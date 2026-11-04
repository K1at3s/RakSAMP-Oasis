#ifndef GROUP_LEADERSHIP_H
#define GROUP_LEADERSHIP_H
#include "CommandTracking.h"
#include <vector>
#include <sstream>

enum MemberLeadership { MEMBER_NOT_FOUND, MEMBER_DENIED, MEMBER_ALLOWED };
inline std::string CleanMemberField(const std::string &input) {
    std::string clean;
    for(size_t i = 0; i < input.size(); ++i) {
        if(input[i] == '{' && i + 7 < input.size() && input[i + 7] == '}') { i += 7; continue; }
        clean += input[i];
    }
    const size_t first = clean.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    return clean.substr(first, clean.find_last_not_of(" \t\r\n") - first + 1);
}
inline bool IsLeadershipRole(const std::string &input) {
    std::string role = CleanMemberField(input), key;
    for(char ch : role) if(ch != ' ' && ch != '-' && ch != '_') key += ch;
    return PendingGroupCommand::equal(key, "Leader") || PendingGroupCommand::equal(key, "CoLeader") || PendingGroupCommand::equal(key, "Owner");
}
inline MemberLeadership FindMemberLeadership(const char *page, const std::string &requested) {
    if(!page || requested.empty()) return MEMBER_NOT_FOUND;
    int nameColumn = -1, roleColumn = -1;
    std::istringstream lines(page);
    std::string line;
    while(std::getline(lines, line)) {
        std::vector<std::string> fields;
        size_t begin = 0, end;
        do {
            end = line.find('\t', begin);
            fields.push_back(CleanMemberField(line.substr(begin, end == std::string::npos ? end : end - begin)));
            begin = end == std::string::npos ? end : end + 1;
        } while(end != std::string::npos);
        bool header = false;
        for(size_t i = 0; i < fields.size(); ++i) {
            if(PendingGroupCommand::equal(fields[i], "Name") || PendingGroupCommand::equal(fields[i], "Player") || PendingGroupCommand::equal(fields[i], "Player Name")) { nameColumn = (int)i; header = true; }
            if(PendingGroupCommand::equal(fields[i], "Role") || PendingGroupCommand::equal(fields[i], "Group Role")) { roleColumn = (int)i; header = true; }
        }
        if(header || fields.empty()) continue;
        size_t index = nameColumn >= 0 ? (size_t)nameColumn : 0;
        if(nameColumn < 0 && fields.size() > 1 && !fields[0].empty() && fields[0].find_first_not_of("0123456789. ") == std::string::npos) index = 1;
        if(index >= fields.size()) continue;
        std::string name = fields[index];
        // Some tables combine the row number and name in their first column.
        size_t numEnd = name.find_first_not_of("0123456789");
        if(numEnd > 0 && numEnd != std::string::npos && (name[numEnd] == ' ' || name[numEnd] == '.')) {
            size_t start = name.find_first_not_of(". ", numEnd);
            name = start == std::string::npos ? "" : name.substr(start);
        }
        if(!PendingGroupCommand::equal(name, requested)) continue;
        if(roleColumn >= 0) return (size_t)roleColumn < fields.size() && IsLeadershipRole(fields[roleColumn]) ? MEMBER_ALLOWED : MEMBER_DENIED;
        // Headerless standard rows are Name / custom Rank / Role / Last Active.
        const size_t inferredRole = fields.size() >= index + 4 ? index + 2 : index + 1;
        return inferredRole < fields.size() && IsLeadershipRole(fields[inferredRole]) ? MEMBER_ALLOWED : MEMBER_DENIED;
    }
    return MEMBER_NOT_FOUND;
}
#endif
