#ifndef GROUP_MEMBERSHIP_H
#define GROUP_MEMBERSHIP_H
#include "CommandTracking.h"

inline std::string GroupMessageLower(std::string text) {
    for(size_t i = 0; i < text.size(); ++i) if(text[i] >= 'A' && text[i] <= 'Z') text[i] += 'a' - 'A';
    return text;
}
inline bool IsOwnGroupExit(const std::string &message, const std::string &bot) {
    // Match anchored server notices, never another player's chat or departure.
    std::string lower = GroupMessageLower(message);
    if(lower.compare(0, 6, "group:") == 0) lower.erase(0, 6);
    const size_t first = lower.find_first_not_of(" \t");
    if(first != std::string::npos) lower.erase(0, first);
    if(lower == "you have left the group." || lower == "you left the group." ||
       lower == "you have left your group." || lower == "you have successfully left the group." ||
       lower == "your group has been disbanded." || lower == "your group has been deleted." ||
       lower.find("you have been kicked from the group") == 0 || lower.find("you were kicked from the group") == 0 ||
       lower.find("you have been kicked out of the group") == 0) return true;
    if(bot.empty() || message.compare(0, 6, "GROUP:") != 0) return false;
    GroupCommandEvent event;
    if(GroupCommandEvent::parse(message, event) && event.type == "gkick")
        return PendingGroupCommand::equal(event.target, bot);
    const std::string body = message.substr(6);
    const size_t left = body.find(" left the group");
    const size_t hasLeft = body.find(" has left the group");
    const size_t pos = left != std::string::npos ? left : hasLeft;
    return pos != std::string::npos && PendingGroupCommand::equal(GroupCommandEvent::name(body.substr(0, pos)), bot);
}
inline bool IsGroupLeaveDialog(bool pending, unsigned long elapsed, const char *title, const char *info) {
    if(!pending || elapsed > 20000) return false;
    const std::string text = GroupMessageLower(std::string(title ? title : "") + " " + (info ? info : ""));
    return text.find("leave") != std::string::npos && text.find("group") != std::string::npos;
}
enum { GROUP_LEAVE_YES = 0, GROUP_LEAVE_NO = 1 };
#endif
