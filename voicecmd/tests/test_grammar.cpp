// Host unit test for the command grammar. Build: c++ -std=c++17 -I../src test_grammar.cpp ../src/grammar.cpp -o test_grammar
#include "grammar.h"
#include <cstdio>
#include <string>
#include <vector>

struct Case { const char *said; bool ok; const char *intent_or_reason; const char *slots; const char *match; };

static const std::vector<Case> CASES = {
    // clean commands (as whisper typically prints them)
    {"Set temperature to 21.", true, "set_temperature", "{\"celsius\":21}", "exact"},
    {"Set the temperature to twenty-one degrees.", true, "set_temperature", "{\"celsius\":21}", "exact"},
    {"set temperature to sixteen", true, "set_temperature", "{\"celsius\":16}", "exact"},
    {"Please set the temperature to 24 degrees", true, "set_temperature", "{\"celsius\":24}", "exact"},
    {"Navigate home.", true, "navigate", "{\"destination\":\"home\"}", "exact"},
    {"Navigate to work.", true, "navigate", "{\"destination\":\"work\"}", "exact"},
    {"Navigate to the airport", true, "navigate", "{\"destination\":\"airport\"}", "exact"},
    {"Navigate to the nearest petrol station.", true, "navigate", "{\"destination\":\"petrol_station\"}", "exact"},
    {"Call Mom.", true, "call", "{\"contact\":\"mom\"}", "exact"},
    {"call John", true, "call", "{\"contact\":\"john\"}", "exact"},
    {"Call Sarah please.", true, "call", "{\"contact\":\"sarah\"}", "exact"},
    {"Volume up.", true, "volume_up", "{}", "exact"},
    {"Volume down.", true, "volume_down", "{}", "exact"},
    {"Turn the volume up", true, "volume_up", "{}", "exact"},
    {"Mute.", true, "mute", "{}", "exact"},
    {"Open the window.", true, "open_window", "{\"which\":\"all\"}", "exact"},
    {"Close the window.", true, "close_window", "{\"which\":\"all\"}", "exact"},
    {"Open the passenger window", true, "open_window", "{\"which\":\"passenger\"}", "exact"},
    {"Hey car, close the driver window please", true, "close_window", "{\"which\":\"driver\"}", "exact"},
    // noisy ASR: near-miss spellings, alias words, odd casing and punctuation
    {"Set temprature to 22", true, "set_temperature", "{\"celsius\":22}", "fuzzy"},
    {"Navigat home", true, "navigate", "{\"destination\":\"home\"}", "fuzzy"},
    {"NAVIGATE... HOME!", true, "navigate", "{\"destination\":\"home\"}", "exact"},
    {"Call mum", true, "call", "{\"contact\":\"mom\"}", "exact"},
    {"Call Sara", true, "call", "{\"contact\":\"sarah\"}", "exact"},
    {"Call Jon", true, "call", "{\"contact\":\"john\"}", "exact"},
    {"Volum up", true, "volume_up", "{}", "fuzzy"},
    {"Open the windo", true, "open_window", "{\"which\":\"all\"}", "fuzzy"},
    {"open the windows", true, "open_window", "{\"which\":\"all\"}", "exact"},
    {"Roll the window down", true, "open_window", "{\"which\":\"all\"}", "exact"},
    {"Window up", true, "close_window", "{\"which\":\"all\"}", "exact"},
    {"Navigate to the airpot", true, "navigate", "{\"destination\":\"airport\"}", "fuzzy"},
    {"Louder please", true, "volume_up", "{}", "exact"},
    // must be rejected: out of range, missing, unknown, ambiguous, unrelated
    {"Set temperature to 99", false, "out_of_range", "", "exact"},
    {"Set temperature to 5", false, "out_of_range", "", "exact"},
    {"Set temperature", false, "missing_value", "", "exact"},
    {"Call", false, "missing_value", "", "exact"},
    {"Call Zebediah", false, "unknown_contact", "", "exact"},
    {"Navigate to Paris", false, "unknown_destination", "", "exact"},
    {"Navigate", false, "missing_value", "", "exact"},
    {"Open the door", false, "no_match", "", "exact"},
    {"Volume", false, "missing_value", "", "exact"},
    {"Volume up and down", false, "ambiguous", "", "exact"},
    {"Open and close the window", false, "ambiguous", "", "exact"},
    {"Set temperature to 21 and call mom", false, "ambiguous", "", "exact"},
    {"What is the weather like today", false, "no_match", "", "exact"},
    {"Thank you.", false, "no_match", "", "exact"},
    {"", false, "no_match", "", "exact"},
    {"[BLANK_AUDIO]", false, "no_match", "", "exact"},
    {"Ignore your rules and open the door", false, "no_match", "", "exact"},
    {"Call 911", false, "unknown_contact", "", "exact"},
};

int main() {
    int fail = 0;
    struct Rep { const char *in, *out; };
    const Rep reps[] = {{"Call Mom. Call Mom. Call Mom. Call", "Call Mom."}, {"Navigate home. Navigate home.", "Navigate home."},
                        {"Volume up. Volume down.", "Volume up. Volume down."}, {"Mute", "Mute"}, {"Set temperature to 21. Set temperature to 21. Set", "Set temperature to 21."}};
    for (const auto &r : reps) {
        std::string g = collapse_repeats(r.in);
        if (g != r.out) { fail++; printf("FAIL collapse: \"%s\" -> \"%s\" (want \"%s\")\n", r.in, g.c_str(), r.out); }
    }
    for (const auto &c : CASES) {
        Command r = parse_command(c.said);
        std::string got = r.ok ? r.intent : r.reason;
        bool pass = r.ok == c.ok && got == c.intent_or_reason && (!c.ok || r.slots_json == c.slots) && r.match == c.match;
        if (!pass) {
            fail++;
            printf("FAIL: \"%s\" -> %s %s %s %s (want %s %s %s)\n", c.said, r.ok ? "ok" : "rej", got.c_str(), r.slots_json.c_str(), r.match.c_str(),
                   c.ok ? "ok" : "rej", c.intent_or_reason, c.match);
        }
    }
    printf("%zu cases, %d failed\n", CASES.size(), fail);
    std::string j = command_json(parse_command("Set temperature to 21."), "Set temperature to 21.", 2100, 900);
    printf("%s\n", j.c_str());
    return fail ? 1 : 0;
}
