#include "grammar.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace {

const int TEMP_MIN = 16, TEMP_MAX = 30;

const std::vector<std::string> CONTACTS = {"mom", "dad", "john", "sarah", "emma", "office"};
const std::map<std::string, std::string> CONTACT_ALIAS = {{"mum", "mom"}, {"mommy", "mom"}, {"mother", "mom"}, {"father", "dad"}, {"sara", "sarah"}, {"jon", "john"}};
const std::vector<std::string> DESTS = {"home", "work", "airport", "petrol", "gym"};
const std::map<std::string, std::string> DEST_ALIAS = {{"office", "work"}, {"fuel", "petrol"}, {"gas", "petrol"}};
const std::vector<std::string> SOURCES = {"spotify", "music", "radio", "podcast"};
const std::map<std::string, std::string> SOURCE_ALIAS = {{"podcasts", "podcast"}, {"spotifi", "spotify"}};
const std::vector<std::string> WINDOWS = {"driver", "passenger", "rear", "front", "all"};

/* Words the grammar anchors on; a transcript word within edit distance 1 of exactly one of these (length >= 5) is corrected. */
const std::vector<std::string> ANCHORS = {"temperature", "navigate", "navigation", "volume", "window", "windows", "airport", "passenger", "driver", "petrol", "contact", "spotify", "podcast", "previous"};

const std::map<std::string, int> NUM = {
    {"zero", 0}, {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}, {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9},
    {"ten", 10}, {"eleven", 11}, {"twelve", 12}, {"thirteen", 13}, {"fourteen", 14}, {"fifteen", 15}, {"sixteen", 16},
    {"seventeen", 17}, {"eighteen", 18}, {"nineteen", 19}, {"twenty", 20}, {"thirty", 30}, {"forty", 40}, {"fifty", 50},
    {"sixty", 60}, {"seventy", 70}, {"eighty", 80}, {"ninety", 90}};

const std::set<std::string> FILLER = {"please", "the", "a", "um", "uh", "hey", "okay", "ok", "car", "my", "now", "can", "you", "could", "just", "would", "to"};

int edit_distance(const std::string &a, const std::string &b) {
    std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = (int)j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= b.size(); j++)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1])});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> out; std::istringstream is(s); std::string w;
    while (is >> w) out.push_back(w);
    return out;
}

/* Lowercase, keep a-z0-9, everything else (incl. hyphens) becomes a space. */
std::string lower_clean(const std::string &in) {
    std::string o;
    for (unsigned char c : in) o += std::isalnum(c) ? (char)std::tolower(c) : ' ';
    return o;
}

/* Merge number words ("twenty one", "twenty-one", "seventeen") into digit tokens. */
std::vector<std::string> merge_numbers(const std::vector<std::string> &t) {
    std::vector<std::string> out;
    for (size_t i = 0; i < t.size(); i++) {
        auto it = NUM.find(t[i]);
        if (it == NUM.end()) { out.push_back(t[i]); continue; }
        int v = it->second;
        if (v >= 20 && v % 10 == 0 && i + 1 < t.size()) {
            auto nx = NUM.find(t[i + 1]);
            if (nx != NUM.end() && nx->second >= 1 && nx->second <= 9) { v += nx->second; i++; }
        }
        out.push_back(std::to_string(v));
    }
    return out;
}

bool is_number(const std::string &s) { return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit) && s.size() <= 4; }

/* Correct near-miss spellings of anchor words. Sets fuzzy when it changed something. */
std::string fix_token(const std::string &w, bool &fuzzy) {
    if (w.size() < 5 || is_number(w)) return w;
    for (const auto &a : ANCHORS) if (w == a) return w;
    std::string best; int n = 0;
    for (const auto &a : ANCHORS) if (edit_distance(w, a) <= 1) { best = a; n++; }
    if (n == 1) { fuzzy = true; return best; }
    return w;
}

bool has(const std::vector<std::string> &t, const char *w) { return std::find(t.begin(), t.end(), w) != t.end(); }
bool has_any(const std::vector<std::string> &t, std::initializer_list<const char *> ws) { for (auto w : ws) if (has(t, w)) return true; return false; }

Command reject(const std::string &reason, const std::string &norm, bool fuzzy) {
    Command c; c.ok = false; c.reason = reason; c.normalised = norm; c.match = fuzzy ? "fuzzy" : "exact"; return c;
}
Command accept(const std::string &intent, const std::string &slots, const std::string &norm, bool fuzzy) {
    Command c; c.ok = true; c.intent = intent; c.slots_json = slots; c.normalised = norm; c.match = fuzzy ? "fuzzy" : "exact"; return c;
}

/* Resolve a word to a canonical entry of `list` via aliases, exact match, or (len >= 4) edit distance 1 with a unique hit. */
bool resolve(const std::string &w, const std::vector<std::string> &list, const std::map<std::string, std::string> &alias, std::string &out, bool &fuzzy) {
    auto al = alias.find(w); if (al != alias.end()) { out = al->second; return true; }
    for (const auto &x : list) if (x == w) { out = x; return true; }
    if (w.size() >= 4) {
        std::string best; int n = 0;
        for (const auto &x : list) if (x.size() >= 4 && edit_distance(w, x) <= 1) { best = x; n++; }
        if (n == 1) { out = best; fuzzy = true; return true; }
    }
    return false;
}

} // namespace

std::string collapse_repeats(const std::string &text) {
    /* sentence = text up to and including . ? ! ; a repeat is a later sentence equal to (or a cut-off prefix of) the first one */
    std::vector<std::string> sent; std::string cur;
    for (char c : text) { cur += c; if (c == '.' || c == '?' || c == '!') { sent.push_back(cur); cur.clear(); } }
    if (!cur.empty()) sent.push_back(cur);
    auto norm = [](const std::string &x) { return lower_clean(x); };
    auto trim = [](std::string x) { size_t a = x.find_first_not_of(' '), b = x.find_last_not_of(' '); return a == std::string::npos ? std::string() : x.substr(a, b - a + 1); };
    if (sent.size() < 2) return text;
    std::string first = trim(norm(sent[0]));
    std::string out = sent[0];
    for (size_t i = 1; i < sent.size(); i++) {
        std::string n = trim(norm(sent[i]));
        if (n == first || (n.size() < first.size() && first.compare(0, n.size(), n) == 0)) continue; /* repeat or truncated repeat */
        out += sent[i];
    }
    return out;
}

std::string json_escape(const std::string &s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}

Command parse_command(const std::string &transcript) {
    bool fuzzy = false;
    auto raw = merge_numbers(split(lower_clean(transcript)));
    std::vector<std::string> t;
    for (auto &w : raw) {
        std::string f = fix_token(w, fuzzy);
        if (FILLER.count(f)) continue;
        t.push_back(f);
    }
    std::string norm; for (auto &w : t) norm += (norm.empty() ? "" : " ") + w;
    if (t.empty()) return reject("no_match", norm, fuzzy);

    int n_set = (int)has(t, "temperature") + (int)has(t, "temp");
    bool temp = n_set > 0;
    bool nav = has_any(t, {"navigate", "navigation", "directions", "drive"});
    bool call = has(t, "call") || has(t, "dial") || has(t, "phone");
    bool vol = has_any(t, {"volume", "louder", "quieter"});
    bool mute = has(t, "mute");
    bool win = has_any(t, {"window", "windows"});

    /* media. "stop" only counts next to a source word ("stop music"); a bare "stop" stays no_match. */
    bool src_word = false;
    for (auto &w : t) { std::string x; bool f2 = false; if (resolve(w, SOURCES, SOURCE_ALIAS, x, f2)) { src_word = true; break; } }
    bool m_play = has(t, "play"), m_pause = has(t, "pause") || (has(t, "stop") && src_word);
    bool m_next = has(t, "skip") || (has(t, "next") && has_any(t, {"track", "tracks", "song", "songs"}));
    bool m_prev = has(t, "previous") && has_any(t, {"track", "tracks", "song", "songs"});
    bool m_partial = (has(t, "next") || has(t, "previous")) && !m_next && !m_prev;
    /* tiny.en hears "play" as "please" before a source word, and "please" is filler, so a lone source word means play_media. */
    bool m_lone = t.size() == 1 && src_word && !m_play;
    int n_media = (int)m_play + (int)m_pause + (int)m_next + (int)m_prev + (int)m_lone + (int)m_partial;
    bool media = n_media > 0;

    int anchors = temp + nav + call + (vol || mute) + win + media;
    if (anchors == 0) return reject("no_match", norm, fuzzy);
    if (anchors > 1) return reject("ambiguous", norm, fuzzy);

    if (media) {
        if (n_media > 1) return reject("ambiguous", norm, fuzzy);
        if (m_partial) return reject("missing_value", norm, fuzzy);
        if (m_pause) return accept("pause_media", "{}", norm, fuzzy);
        if (m_next) return accept("next_track", "{}", norm, fuzzy);
        if (m_prev) return accept("previous_track", "{}", norm, fuzzy);
        std::string src; bool found = false, any = false;
        for (auto &w : t) {
            if (w == "play") continue;
            any = true;
            if (resolve(w, SOURCES, SOURCE_ALIAS, src, fuzzy)) { found = true; break; }
        }
        if (m_lone) fuzzy = true;
        if (!any) return reject("missing_value", norm, fuzzy);
        if (!found) return reject("unknown_source", norm, fuzzy);
        return accept("play_media", "{\"source\":\"" + src + "\"}", norm, fuzzy);
    }
    if (temp) {
        int val = -1;
        for (auto &w : t) if (is_number(w)) { val = std::stoi(w); break; }
        if (val < 0) return reject("missing_value", norm, fuzzy);
        if (val < TEMP_MIN || val > TEMP_MAX) return reject("out_of_range", norm, fuzzy);
        return accept("set_temperature", "{\"celsius\":" + std::to_string(val) + "}", norm, fuzzy);
    }
    if (nav) {
        std::string dest; bool found = false;
        for (auto &w : t) {
            if (w == "navigate" || w == "navigation" || w == "directions" || w == "drive" || w == "take" || w == "me") continue;
            if (resolve(w, DESTS, DEST_ALIAS, dest, fuzzy)) { found = true; break; }
        }
        if (!found) {
            bool any = false;
            for (auto &w : t) if (w != "navigate" && w != "navigation" && w != "directions" && w != "drive" && w != "take" && w != "me") any = true;
            return reject(any ? "unknown_destination" : "missing_value", norm, fuzzy);
        }
        if (dest == "petrol") dest = "petrol_station";
        return accept("navigate", "{\"destination\":\"" + dest + "\"}", norm, fuzzy);
    }
    if (call) {
        std::string who; bool found = false, any = false;
        for (auto &w : t) {
            if (w == "call" || w == "dial" || w == "phone") continue;
            any = true;
            if (resolve(w, CONTACTS, CONTACT_ALIAS, who, fuzzy)) { found = true; break; }
        }
        if (!any) return reject("missing_value", norm, fuzzy);
        if (!found) return reject("unknown_contact", norm, fuzzy);
        return accept("call", "{\"contact\":\"" + who + "\"}", norm, fuzzy);
    }
    if (mute) return accept("mute", "{}", norm, fuzzy);
    if (vol) {
        bool up = has_any(t, {"up", "louder", "increase", "raise"}), down = has_any(t, {"down", "quieter", "lower", "decrease", "softer"});
        if (up == down) return reject(up ? "ambiguous" : "missing_value", norm, fuzzy);
        return accept(up ? "volume_up" : "volume_down", "{}", norm, fuzzy);
    }
    /* window */
    bool open = has_any(t, {"open", "down", "lower"}), close = has_any(t, {"close", "shut", "up", "raise"});
    if (open == close) return reject(open ? "ambiguous" : "missing_value", norm, fuzzy);
    std::string which = "all";
    for (auto &w : t) { std::string x; if (std::find(WINDOWS.begin(), WINDOWS.end(), w) != WINDOWS.end()) { which = w; break; } }
    return accept(open ? "open_window" : "close_window", "{\"which\":\"" + which + "\"}", norm, fuzzy);
}

std::string command_json(const Command &c, const std::string &transcript, long audio_ms, long latency_ms) {
    std::string o = "{\"event\":\"";
    o += c.ok ? "command" : "rejected";
    o += "\",";
    if (c.ok) o += "\"intent\":\"" + c.intent + "\",\"slots\":" + c.slots_json + ",";
    else o += "\"reason\":\"" + c.reason + "\",";
    o += "\"match\":\"" + c.match + "\",\"transcript\":\"" + json_escape(transcript) + "\",\"audio_ms\":" + std::to_string(audio_ms) +
         ",\"latency_ms\":" + std::to_string(latency_ms) + "}";
    return o;
}

const char *command_prompt() {
    /* Was kept to five phrases because under QEMU emulation every prompt token cost about 0.15 s per command.
     * On Graviton (real QNX 8.0) that cost is negligible, so the media phrases are added: without "Play Spotify"
     * in the prompt, tiny.en pulled "play" to "please". Re-run bench-aws.sh when changing this. */
    return "Mute. Volume up. Call Mom. Call Sarah. Navigate home. Navigate to work. Play Spotify. Next track.";
}
