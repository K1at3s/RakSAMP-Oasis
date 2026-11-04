#ifndef WAR_RESULT_CONTEXT_H
#define WAR_RESULT_CONTEXT_H
#include "CommandTracking.h"

struct WarResultContext {
    std::string ownGroup, red, blue, zone, id;
    bool wonKnown = false, won = false;
    void begin(const std::string &own, const std::string &left, const std::string &right,
               const std::string &zoneId, const std::string &warId) {
        *this = WarResultContext();
        if(!PendingGroupCommand::equal(own, left) && !PendingGroupCommand::equal(own, right)) return;
        ownGroup = own; red = left; blue = right; zone = zoneId; id = warId;
    }
    bool active() const { return !id.empty() && !ownGroup.empty() && !red.empty() && !blue.empty(); }
    bool ownBlue() const { return PendingGroupCommand::equal(ownGroup, blue); }
    void outcome(bool result) { if(active() && !wonKnown) { won = result; wonKnown = true; } }
    void observe(const std::string &message) {
        if(!active() || wonKnown) return;
        const std::string prefix = "ZONE WAR: ";
        if(message.compare(0, prefix.size(), prefix)) return;
        const std::string body = message.substr(prefix.size());
        const std::string enemy = ownBlue() ? red : blue;
        if(body == ownGroup + " wins against " + enemy || body == enemy + " loses to " + ownGroup) outcome(true);
        else if(body == ownGroup + " loses to " + enemy || body == enemy + " wins against " + ownGroup) outcome(false);
        else if(!zone.empty()) {
            const std::string tail = " zone '# " + zone + "'";
            if(body == ownGroup + " takes over" + tail || body == ownGroup + " keeps" + tail) outcome(true);
            else if(body == enemy + " takes over" + tail || body == enemy + " keeps" + tail) outcome(false);
        }
    }
};
#endif
