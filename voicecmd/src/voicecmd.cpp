// voicecmd: live voice-command daemon. audio source -> energy VAD -> whisper -> fixed grammar -> JSON events on stdout.
#include "audio_source.h"
#include "grammar.h"
#include "vad.h"
#include "whisper.h"
#ifdef VOICECMD_LOAD_BACKENDS
#include "ggml-backend.h" /* host builds with dynamically loaded ggml backends (Homebrew) */
#endif
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static long now_ms() {
    using namespace std::chrono;
    return (long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void usage(const char *p) {
    fprintf(stderr, "usage: %s -m model.bin -a <file:x.wav|tcp:host:port|snd:name> [-t threads] [--audio-ctx-min N] [--no-prompt] [--max N]\n", p);
}

int main(int argc, char **argv) {
    const char *model = nullptr, *spec = nullptr;
    int threads = 4, ctx_min = 100, max_utts = 0; bool use_prompt = true;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-m" && i + 1 < argc) model = argv[++i];
        else if (a == "-a" && i + 1 < argc) spec = argv[++i];
        else if (a == "-t" && i + 1 < argc) threads = atoi(argv[++i]);
        else if (a == "--audio-ctx-min" && i + 1 < argc) ctx_min = atoi(argv[++i]);
        else if (a == "--max" && i + 1 < argc) max_utts = atoi(argv[++i]);
        else if (a == "--no-prompt") use_prompt = false;
        else { usage(argv[0]); return 2; }
    }
    if (!model || !spec) { usage(argv[0]); return 2; }
    setvbuf(stdout, nullptr, _IOLBF, 0);

    char err[160] = "";
    audio_source *src = audio_open(spec, err, sizeof err);
    if (!src) { printf("{\"event\":\"error\",\"what\":\"audio_open\",\"message\":\"%s\"}\n", json_escape(err).c_str()); return 1; }

#ifdef VOICECMD_LOAD_BACKENDS
    ggml_backend_load_all();
#endif
    long t0 = now_ms();
    whisper_context_params cp = whisper_context_default_params();
    cp.use_gpu = false;
    whisper_context *ctx = whisper_init_from_file_with_params(model, cp);
    if (!ctx) { printf("{\"event\":\"error\",\"what\":\"model_load\"}\n"); return 1; }
    printf("{\"event\":\"ready\",\"audio\":\"%s\",\"model_load_ms\":%ld,\"threads\":%d}\n", audio_backend(src), now_ms() - t0, threads);

    const int PRE = 10;  // 200 ms of pre-roll so the first phoneme is not clipped
    std::vector<int16_t> frame(VAD_FRAME), utt;
    std::vector<std::vector<int16_t>> ring; // last PRE frames
    vad_t vad; vad_init(&vad);
    int n_done = 0;
    long t_start_utt = 0;
    for (;;) {
        long n = audio_read(src, frame.data(), VAD_FRAME);
        bool eof = n < VAD_FRAME;
        int ev = VAD_NONE;
        if (!eof) ev = vad_push(&vad, frame.data());
        if (ev == VAD_START) {
            utt.clear();
            for (auto &f : ring) utt.insert(utt.end(), f.begin(), f.end());
            t_start_utt = now_ms();
        }
        if (vad.active || ev == VAD_END) {
            if (!eof) utt.insert(utt.end(), frame.begin(), frame.end());
            if (utt.size() > 8 * AUDIO_RATE) ev = VAD_END; // hard cap at 8 s
        }
        if (!eof) { ring.push_back(frame); if ((int)ring.size() > PRE) ring.erase(ring.begin()); }
        if ((ev == VAD_END || (eof && vad.active)) && !utt.empty()) {
            if (vad.active) { vad.active = 0; vad.loud_run = 0; }
            long audio_ms = (long)utt.size() * 1000 / AUDIO_RATE;
            long t1 = now_ms();
            std::vector<float> pcm(utt.size());
            for (size_t i = 0; i < utt.size(); i++) pcm[i] = utt[i] / 32768.0f;
            whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            p.n_threads = threads; p.language = "en"; p.translate = false;
            p.print_progress = p.print_realtime = p.print_timestamps = p.print_special = false;
            p.no_timestamps = true; p.single_segment = true; p.no_context = true;
            p.max_tokens = 16; /* commands are short; a cap also stops repetition loops seen with a shortened audio_ctx */
            p.temperature = 0.0f; p.temperature_inc = 0.0f; p.suppress_blank = true; p.suppress_nst = true;
            p.initial_prompt = use_prompt ? command_prompt() : nullptr;
            // Encoder work scales with audio_ctx (50 per second). Commands are short, so do not encode a full 30 s window.
            int need = (int)(utt.size() / 320) + 50; // utterance frames + 1 s margin
            p.audio_ctx = need < ctx_min ? ctx_min : (need > 1500 ? 1500 : need);
            // whisper needs >= 1 s of samples: pad with silence
            if (pcm.size() < (size_t)AUDIO_RATE + 1600) pcm.resize((size_t)AUDIO_RATE + 1600, 0.0f);
            std::string text;
            if (whisper_full(ctx, p, pcm.data(), (int)pcm.size()) == 0) {
                int ns = whisper_full_n_segments(ctx);
                for (int s = 0; s < ns; s++) text += whisper_full_get_segment_text(ctx, s);
            }
            text = collapse_repeats(text);
            size_t b = text.find_first_not_of(' '); text = b == std::string::npos ? "" : text.substr(b);
            Command c = parse_command(text);
            printf("%s\n", command_json(c, text, audio_ms, now_ms() - t1).c_str());
            utt.clear();
            if (max_utts && ++n_done >= max_utts) break;
        }
        if (eof) break;
    }
    printf("{\"event\":\"eof\"}\n");
    whisper_free(ctx);
    audio_close(src);
    return 0;
}
