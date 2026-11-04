#ifndef MUTE_NOTICE_H
#define MUTE_NOTICE_H
#include <string>

// Only a server notice addressed to this client is a mute signal. Player chat
// can contain any of these words and must never cause a reconnect.
inline bool IsOwnMuteNotice(const char *input) {
    if(!input) return false;
    const std::string source = input;
    std::string text;
    for(size_t i = 0; i < source.size(); ++i) {
        if(source[i] == '~' && i + 2 < source.size() && source[i + 2] == '~') {
            if(source[i + 1] == 'n' || source[i + 1] == 'N') text += ' ';
            i += 2;
            continue;
        }
        if(source[i] == '{' && i + 7 < source.size() && source[i + 7] == '}') {
            bool color = true;
            for(size_t j = i + 1; j < i + 7; ++j) {
                const char c = source[j];
                if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) color = false;
            }
            if(color) { i += 7; continue; }
        }
        char c = source[i];
        if(c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if(c == '\r' || c == '\n' || c == '\t') c = ' ';
        if(c != ' ' || (!text.empty() && text.back() != ' ')) text += c;
    }
    if(!text.empty() && text.back() == ' ') text.pop_back();
    if(text.compare(0, 7, "ERROR: ") == 0) text.erase(0, 7);
    if(text.compare(0, 3, ">> ") == 0) text.erase(0, 3);
    const char *prefixes[] = { "YOU ARE COMMAND MUTED", "YOU ARE MUTED", "YOU HAVE BEEN MUTED", "YOU HAVE BEEN COMMAND MUTED", "YOU ARE CURRENTLY MUTED" };
    for(const char *prefix : prefixes) {
        const std::string phrase = prefix;
        if(text.compare(0, phrase.size(), phrase) != 0) continue;
        const std::string rest = text.substr(phrase.size());
        if(rest.empty() || rest == "." || rest == "!" || rest.compare(0, 4, " FOR") == 0 || rest.compare(0, 6, " UNTIL") == 0) return true;
    }
    return false;
}
#endif
