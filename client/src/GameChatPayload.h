#ifndef GAME_CHAT_PAYLOAD_H
#define GAME_CHAT_PAYLOAD_H
#include <string>
#include <windows.h>
#include "json.hpp"

// SA-MP text uses CP1254, whereas Discord/JSON require UTF-8. Preserve valid
// UTF-8 sequences too, since game and backend text can share the same buffer.
inline std::string GameChatToUtf8(const char *input) {
    const std::string text = input ? input : "";
    std::string result;
    result.reserve(text.size() * 3);
    for(size_t i = 0; i < text.size();) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if(ch < 0x80) { result += text[i++]; continue; }
        int length = ch >= 0xC2 && ch <= 0xDF ? 2 : ch >= 0xE0 && ch <= 0xEF ? 3 : ch >= 0xF0 && ch <= 0xF4 ? 4 : 0;
        wchar_t wide[2] = {};
        if(length && i + length <= text.size() &&
           MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data() + i, length, wide, 2) > 0) {
            result.append(text, i, length);
            i += length;
            continue;
        }
        wchar_t legacy = 0;
        char converted[4] = {};
        const int chars = MultiByteToWideChar(1254, 0, text.data() + i, 1, &legacy, 1);
        const int bytes = chars ? WideCharToMultiByte(CP_UTF8, 0, &legacy, 1, converted, sizeof(converted), NULL, NULL) : 0;
        if(bytes > 0) result.append(converted, bytes);
        else result.append("\xEF\xBF\xBD");
        ++i;
    }
    return result;
}

inline nlohmann::json BuildGroupChatPayload(const std::string &hwid, const char *author, const char *text) {
    return { {"hwid", hwid}, {"author", GameChatToUtf8(author && author[0] ? author : "Oyuncu")},
        {"text", GameChatToUtf8(text)} };
}
#endif
