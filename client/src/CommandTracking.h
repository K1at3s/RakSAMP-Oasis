#ifndef COMMAND_TRACKING_H
#define COMMAND_TRACKING_H
#include <string>

struct GroupCommandEvent {
    std::string type, actor, target, value;
    static bool hasAccountId(const std::string &text) {
        size_t start = text.find('(');
        const size_t first = text.find_first_not_of(" \t");
        // Both Name(id) and (Name)(id) are used by group system messages.
        if(start == first && start != std::string::npos) {
            const size_t end = text.find(')', start);
            start = end == std::string::npos ? end : text.find('(', end + 1);
        }
        if(start == std::string::npos) return false;
        size_t pos = start + 1;
        bool nonzero = false;
        while(pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            nonzero = nonzero || text[pos] != '0';
            ++pos;
        }
        return nonzero && pos > start + 1 && pos < text.size() && text[pos] == ')';
    }
    static std::string name(std::string text) {
        const size_t first = text.find_first_not_of(" \t");
        if(first == std::string::npos) return "";
        text.erase(0, first);
        if(text[0] == '(') {
            size_t end = text.find(')');
            return end == std::string::npos ? "" : text.substr(1, end - 1);
        }
        return text.substr(0, text.find_first_of("( \t\r\n"));
    }
    static bool parse(const std::string &message, GroupCommandEvent &out) {
        size_t first = message.find_first_not_of(" \t");
        if(first == std::string::npos || message.compare(first, 6, "GROUP:") != 0) return false;
        const std::string body = message.substr(first + 6);
        const char *phrases[] = { " changed the role of ", " changed the rank of ", " kicked ", " invited ", " invites ", " invite " };
        const char *types[] = { "grole", "grank", "gkick", "ginvite", "ginvite", "ginvite" };
        for(size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); ++i) {
            size_t pos = body.find(phrases[i]);
            if(pos == std::string::npos) continue;
            GroupCommandEvent event;
            event.type = types[i];
            event.actor = name(body.substr(0, pos));
            std::string target = body.substr(pos + std::string(phrases[i]).size());
            if(event.type == "ginvite" && target.compare(0, 4, "the ") == 0) target.erase(0, 4);
            event.target = name(target);
            if(event.type == "ginvite" && (!hasAccountId(body.substr(0, pos)) || !hasAccountId(target))) return false;
            if(i < 2) {
                size_t valueAt = target.find(" to '");
                if(valueAt == std::string::npos) return false;
                valueAt += 5;
                size_t end = target.find('\'', valueAt);
                if(end == std::string::npos) return false;
                event.value = target.substr(valueAt, end - valueAt);
            }
            if(event.actor.empty() || event.target.empty()) return false;
            out = event;
            return true;
        }
        return false;
    }
};

// Correlate a server event with exactly one pending Discord action.
struct PendingGroupCommand {
    std::string requesterId, type, target, value;
    unsigned long started = 0;
    static bool equal(const std::string &a, const std::string &b) {
        if(a.size() != b.size()) return false;
        for(size_t i = 0; i < a.size(); ++i) {
            char x = a[i], y = b[i];
            if(x >= 'A' && x <= 'Z') x += 'a' - 'A';
            if(y >= 'A' && y <= 'Z') y += 'a' - 'A';
            if(x != y) return false;
        }
        return true;
    }
    bool active() const { return !requesterId.empty(); }
    bool matches(const char *eventType, const char *actor, const char *bot,
                 const char *eventTarget, const char *eventValue) const {
        return active() && type == eventType && bot && bot[0] && actor &&
            equal(actor, bot) && eventTarget && equal(target, eventTarget) &&
            (value.empty() || (eventValue && equal(value, eventValue)));
    }
    bool expired(unsigned long now) const { return active() && now - started > 60000; }
};
#endif
