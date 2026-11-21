#include "audio_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef HAVE_QNX_SND
#include <alsa/asoundlib.h>   /* QNX io-snd ships an ALSA-compatible libasound; header path may be <sys/asoundlib.h> */
#endif

enum kind { K_FILE, K_TCP, K_SND };
struct audio_source {
    enum kind kind;
    FILE *fp;
    int fd;
#ifdef HAVE_QNX_SND
    snd_pcm_t *pcm;
#endif
};

static void seterr(char *err, size_t n, const char *m) { if (err && n) snprintf(err, n, "%s", m); }

static int open_wav(audio_source *a, const char *path, char *err, size_t en) {
    a->fp = fopen(path, "rb");
    if (!a->fp) { seterr(err, en, "cannot open wav"); return -1; }
    unsigned char h[12];
    if (fread(h, 1, 12, a->fp) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { seterr(err, en, "not a RIFF/WAVE file"); return -1; }
    int fmt_ok = 0;
    for (;;) {
        unsigned char ch[8];
        if (fread(ch, 1, 8, a->fp) != 8) { seterr(err, en, "no data chunk"); return -1; }
        uint32_t sz = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
        if (!memcmp(ch, "fmt ", 4)) {
            unsigned char f[16];
            if (sz < 16 || fread(f, 1, 16, a->fp) != 16) { seterr(err, en, "bad fmt chunk"); return -1; }
            int tag = f[0] | (f[1] << 8), chn = f[2] | (f[3] << 8);
            uint32_t rate = f[4] | (f[5] << 8) | (f[6] << 16) | ((uint32_t)f[7] << 24);
            int bits = f[14] | (f[15] << 8);
            if (tag != 1 || chn != 1 || rate != AUDIO_RATE || bits != 16) { seterr(err, en, "wav must be 16 kHz mono 16-bit PCM"); return -1; }
            fmt_ok = 1;
            if (sz > 16) fseek(a->fp, (long)(sz - 16), SEEK_CUR);
        } else if (!memcmp(ch, "data", 4)) {
            if (!fmt_ok) { seterr(err, en, "data before fmt"); return -1; }
            return 0; /* positioned at first sample; EOF ends the stream */
        } else {
            fseek(a->fp, (long)sz + (sz & 1), SEEK_CUR);
        }
    }
}

static int open_tcp(audio_source *a, const char *hp, char *err, size_t en) {
    char host[128]; const char *c = strrchr(hp, ':');
    if (!c || (size_t)(c - hp) >= sizeof host) { seterr(err, en, "tcp spec is host:port"); return -1; }
    memcpy(host, hp, c - hp); host[c - hp] = 0;
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, c + 1, &hints, &res) || !res) { seterr(err, en, "getaddrinfo failed"); return -1; }
    a->fd = socket(res->ai_family, res->ai_socktype, 0);
    if (a->fd < 0 || connect(a->fd, res->ai_addr, res->ai_addrlen) < 0) { seterr(err, en, "tcp connect failed"); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);
    return 0;
}

#ifdef HAVE_QNX_SND
static int open_snd(audio_source *a, const char *name, char *err, size_t en) {
    int rc = snd_pcm_open(&a->pcm, *name ? name : "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) { seterr(err, en, snd_strerror(rc)); return -1; }
    /* 16 kHz mono S16_LE, 100 ms latency. io-snd resamples in its plugin layer if the card is fixed-rate. */
    rc = snd_pcm_set_params(a->pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, AUDIO_RATE, 1, 100000);
    if (rc < 0) { seterr(err, en, snd_strerror(rc)); return -1; }
    return 0;
}
#endif

audio_source *audio_open(const char *spec, char *err, size_t en) {
    audio_source *a = calloc(1, sizeof *a);
    a->fd = -1;
    int rc = -1;
    if (!strncmp(spec, "file:", 5)) { a->kind = K_FILE; rc = open_wav(a, spec + 5, err, en); }
    else if (!strncmp(spec, "tcp:", 4)) { a->kind = K_TCP; rc = open_tcp(a, spec + 4, err, en); }
    else if (!strncmp(spec, "snd:", 4)) {
#ifdef HAVE_QNX_SND
        a->kind = K_SND; rc = open_snd(a, spec + 4, err, en);
#else
        seterr(err, en, "built without HAVE_QNX_SND (the SDP 8.0 here ships no libasound/io-snd headers)");
#endif
    } else seterr(err, en, "spec must start with file:, tcp: or snd:");
    if (rc) { audio_close(a); return NULL; }
    return a;
}

long audio_read(audio_source *a, int16_t *buf, long n) {
    long got = 0;
    switch (a->kind) {
    case K_FILE: return (long)fread(buf, 2, (size_t)n, a->fp);
    case K_TCP: {
        char *p = (char *)buf; long want = n * 2, have = 0;
        while (have < want) {
            ssize_t r = recv(a->fd, p + have, (size_t)(want - have), 0);
            if (r < 0 && errno == EINTR) continue;
            if (r < 0) return -1;
            if (r == 0) break;
            have += r;
        }
        return have / 2;
    }
#ifdef HAVE_QNX_SND
    case K_SND:
        while (got < n) {
            snd_pcm_sframes_t r = snd_pcm_readi(a->pcm, buf + got, (snd_pcm_uframes_t)(n - got));
            if (r == -EPIPE || r == -ESTRPIPE) { snd_pcm_recover(a->pcm, (int)r, 1); continue; }
            if (r < 0) return -1;
            got += r;
        }
        return got;
#endif
    default: return -1;
    }
}

void audio_close(audio_source *a) {
    if (!a) return;
    if (a->fp) fclose(a->fp);
    if (a->fd >= 0) close(a->fd);
#ifdef HAVE_QNX_SND
    if (a->pcm) snd_pcm_close(a->pcm);
#endif
    free(a);
}

const char *audio_backend(const audio_source *a) {
    return a->kind == K_FILE ? "file" : a->kind == K_TCP ? "tcp" : "snd";
}
