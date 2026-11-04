#ifndef SETTINGS_ARGUMENTS_H
#define SETTINGS_ARGUMENTS_H
#include <string>
#include <vector>
#include <cstring>

inline bool ParseServerPort(const std::string &text, int &port) {
    if(text.empty()) return false;
    int value = 0;
    for(size_t i = 0; i < text.size(); ++i) {
        if(text[i] < '0' || text[i] > '9') return false;
        value = value * 10 + (text[i] - '0');
        if(value > 65535) return false;
    }
    if(value == 0) return false;
    port = value;
    return true;
}

// Read full Windows command lines as tokens. Never scan switches inside argv[0]
// (for example RakSAMPClient-postfix.exe) or inside another option's value.
inline std::vector<std::string> SettingsArgumentTokens(const char *commandLine) {
    std::vector<std::string> tokens;
    if(!commandLine) return tokens;
    const char *p = commandLine;
    while(*p) {
        while(*p == ' ' || *p == '\t') ++p;
        if(!*p) break;
        std::string token;
        bool quoted = false;
        while(*p && (quoted || (*p != ' ' && *p != '\t'))) {
            if(*p == '"') quoted = !quoted;
            else token += *p;
            ++p;
        }
        tokens.push_back(token);
    }
    return tokens;
}

template<size_t N>
inline void CopySettingsArgument(char (&destination)[N], const std::string &value) {
    const size_t count = value.size() < N - 1 ? value.size() : N - 1;
    std::memcpy(destination, value.data(), count);
    destination[count] = 0;
}

template<typename Server>
inline void ApplyServerArguments(const char *commandLine, Server &server) {
    const std::vector<std::string> args = SettingsArgumentTokens(commandLine);
    for(size_t i = 1; i < args.size(); ++i) {
        const std::string &option = args[i];
        if(option.size() != 2 || (option[0] != '-' && option[0] != '/')) continue;
        const char key = option[1];
        if(key != 'h' && key != 'p' && key != 'n' && key != 'z') continue;
        if(i + 1 >= args.size()) continue;
        // A missing value must not consume the following option.
        if(args[i + 1].size() == 2 && (args[i + 1][0] == '-' || args[i + 1][0] == '/') &&
           std::strchr("hpnz", args[i + 1][1])) continue;
        const std::string &value = args[++i];
        if(key == 'p') ParseServerPort(value, server.iPort);
        else if(key == 'h' && !value.empty()) CopySettingsArgument(server.szAddr, value);
        else if(key == 'n' && !value.empty()) CopySettingsArgument(server.szNickname, value);
        else if(key == 'z') CopySettingsArgument(server.szPassword, value);
    }
}
#endif
