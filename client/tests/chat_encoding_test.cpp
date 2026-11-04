#include "../src/GameChatPayload.h"
#undef NDEBUG
#include <cassert>
#include <iostream>

int main() {
    // The game's CP1254 byte for the final Turkish s-cedilla in the crash log.
    const std::string legacy = "bati abi oyunda ge\xFE ";
    bool reproduced = false;
    try {
        nlohmann::json original = { {"author", "washe"}, {"text", legacy} };
        original.dump();
    } catch(const nlohmann::json::type_error &) { reproduced = true; }
    assert(reproduced);

    const auto fixed = BuildGroupChatPayload("TEST-HWID", "washe", legacy.c_str());
    const auto decoded = nlohmann::json::parse(fixed.dump());
    assert(decoded["text"] == "bati abi oyunda ge\xC5\x9F ");
    assert(decoded["author"] == "washe");
    assert(GameChatToUtf8("\xE7\xF0\xFD\xF6\xFE\xFC\xC7\xD0\xDD\xD6\xDE\xDC") ==
        "\xC3\xA7\xC4\x9F\xC4\xB1\xC3\xB6\xC5\x9F\xC3\xBC\xC3\x87\xC4\x9E\xC4\xB0\xC3\x96\xC5\x9E\xC3\x9C");
    const char *utf8 = "pa\xC5\x9F" "a \xF0\x9F\x98\x80";
    assert(GameChatToUtf8(utf8) == utf8);
    assert(GameChatToUtf8("\xFE \xF0\x9F\x98\x80") == "\xC5\x9F \xF0\x9F\x98\x80");
    for(const char *invalid : { "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82" }) {
        const auto safe = BuildGroupChatPayload("TEST", invalid, invalid);
        assert(nlohmann::json::parse(safe.dump()) == safe);
    }
    const std::string longText(4096, '\xFE');
    assert(GameChatToUtf8(longText.c_str()).size() == 8192);
    assert(BuildGroupChatPayload("TEST", NULL, NULL)["author"] == "Oyuncu");
    std::cout << "Game chat encoding: OK - original crash reproduced, CP1254/UTF-8/mixed/invalid text serialized safely\n";
}
