#ifndef REPLAY_HOOKS_H
#define REPLAY_HOOKS_H
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <windows.h>
// Enabled only by the offline test executable. No game command enables replay.
struct ReplayState {
    bool enabled = false;
    bool asyncReads = false;
    unsigned long tick = 100000;
    std::vector<std::string> commands, chats, logs, http, warResults;
    std::map<std::string, std::pair<int, std::string>> responses;
    std::vector<std::function<void()>> completions;
};
inline ReplayState &ClientReplay() { static ReplayState state; return state; }
inline unsigned long ClientTick() { return ClientReplay().enabled ? ClientReplay().tick : GetTickCount(); }
#endif
