/* Fixed in-car command grammar: free transcript text in, one structured command (or a typed rejection) out.
 * Pure C++17, no whisper dependency, so it is unit-tested on the host. */
#ifndef GRAMMAR_H
#define GRAMMAR_H
#include <string>

struct Command {
    bool ok = false;          /* true: a valid command; false: rejected */
    std::string intent;       /* set_temperature, navigate, call, volume_up, volume_down, mute, open_window, close_window */
    std::string slots_json;   /* JSON object text, e.g. {"celsius":21} */
    std::string reason;       /* when !ok: no_match, out_of_range, missing_value, unknown_contact, unknown_destination, ambiguous */
    std::string match;        /* "exact" or "fuzzy" (a near-miss word was corrected) */
    std::string normalised;   /* the transcript after normalisation, for logs */
};

Command parse_command(const std::string &transcript);
/* Full JSON event line for the daemon: includes transcript and the supplied timings. */
std::string command_json(const Command &c, const std::string &transcript, long audio_ms, long latency_ms);
/* Whisper with a shortened audio window tends to repeat the phrase until max_tokens: keep one copy. */
std::string collapse_repeats(const std::string &text);
std::string json_escape(const std::string &s);
/* The vocabulary prompt given to whisper as its initial prompt. */
const char *command_prompt();
#endif
