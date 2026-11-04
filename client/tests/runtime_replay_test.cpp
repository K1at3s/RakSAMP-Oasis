#include "../src/main.h"
#include "../src/Security.h"
#include "../src/ReplayHooks.h"
#include <cmath>
#undef isfinite
#undef snprintf
#undef vsnprintf
#include "../src/json.hpp"
#include <fstream>
#include <iostream>
#undef NDEBUG
#include <cassert>
extern bool g_loginPending;
extern unsigned long g_loginPendingUntil;
void ClientMessage(RPCParameters *);
void ScrAddGangZone(RPCParameters *);
void ScrDialogBox(RPCParameters *);
void ScrGameText(RPCParameters *);

static void Message(const std::string &text) {
    RakNet::BitStream data; data.Write((DWORD)0xffffffff); data.Write((DWORD)text.size()); data.Write(text.data(), (int)text.size());
    RPCParameters packet = {}; packet.input = data.GetData(); packet.numberOfBitsOfData = data.GetNumberOfBitsUsed(); ClientMessage(&packet);
}
static void Zone(unsigned short id, float base = 0) {
    RakNet::BitStream data; data.Write(id); data.Write(base); data.Write(base); data.Write(base + 50); data.Write(base + 50); data.Write((unsigned int)0xffffffff);
    RPCParameters packet = {}; packet.input = data.GetData(); packet.numberOfBitsOfData = data.GetNumberOfBitsUsed(); ScrAddGangZone(&packet);
}
static void ZoneDialog(const std::string &text, const std::string &title = "Zone Info") {
    RakNet::BitStream data; data.Write((WORD)7); data.Write((BYTE)0);
    data.Write((BYTE)title.size()); data.Write(title.data(), (int)title.size());
    data.Write((BYTE)2); data.Write("OK", 2); data.Write((BYTE)0);
    stringCompressor->EncodeString(text.c_str(), 2048, &data);
    RPCParameters packet = {}; packet.input = data.GetData(); packet.numberOfBitsOfData = data.GetNumberOfBitsUsed(); ScrDialogBox(&packet);
}
static void GameText(const std::string &text) {
    RakNet::BitStream data; data.Write((int)3); data.Write((int)3000); data.Write((int)text.size()); data.Write(text.data(), (int)text.size());
    RPCParameters packet = {}; packet.input = data.GetData(); packet.numberOfBitsOfData = data.GetNumberOfBitsUsed(); ScrGameText(&packet);
}
static bool Contains(const std::vector<std::string> &values, const std::string &wanted, bool prefix = false) {
    for(const auto &value : values) if(prefix ? value.compare(0, wanted.size(), wanted) == 0 : value == wanted) return true;
    return false;
}
int main(int argc, char **argv) {
    ClientReplay().enabled = true;
    pRakClient = RakNetworkFactory::GetRakClientInterface();
    std::ifstream file(argc > 1 ? argv[1] : "client/tests/fixtures/runtime_scenarios.json");
    assert(file); nlohmann::json scenarios; file >> scenarios;
    for(const auto &scenario : scenarios) {
        std::cout << "Replay: " << scenario.at("name").get<std::string>() << std::endl;
        for(const auto &event : scenario.at("events")) {
            const std::string type = event.at("type");
            if(type == "setup") {
                ClientReplay() = ReplayState(); ClientReplay().enabled = true; ResetAutomationSession(); ResetChatOutQueue();
                iAreWeConnected = iGameInited = iSpawned = 1; g_myPlayerID = 1;
                strcpy_s(g_szNickName, sizeof(g_szNickName), "ReplayBot");
                playerInfo[1].iIsConnected = 1; strcpy_s(playerInfo[1].szPlayerName, sizeof(playerInfo[1].szPlayerName), "ReplayBot");
                g_myGroupName = "TestGroup"; g_isEngineActive = true; g_sessionToken = "replay-token"; g_myHWID = "replay-hwid";
            } else if(type == "message") Message(event.at("text"));
            else if(type == "zone") Zone(event.at("id"), event.value("base", 0.0f));
            else if(type == "zone_dialog") ZoneDialog(event.at("text"), event.value("title", std::string("Zone Info")));
            else if(type == "game_text") GameText(event.at("text"));
            else if(type == "expect_result") {
                assert(!ClientReplay().warResults.empty());
                const auto result = nlohmann::json::parse(ClientReplay().warResults.back());
                assert(result.at("ownGroup") == "TestGroup");
                assert(result.at("wonKnown") == true);
                assert(result.at("won").get<int>() == event.at("won").get<int>());
                assert(result.at("redName") == event.at("redName"));
                assert(result.at("blueName") == event.at("blueName"));
            }
            else if(type == "advance") ClientReplay().tick += event.at("ms").get<unsigned long>();
            else if(type == "sniper_tick") ProcessZtkTick();
            else if(type == "backend_response") ClientReplay().responses[event.at("request")] = {200, event.at("body").dump()};
            else if(type == "command") HandleGroupCommand(event.at("text").get<std::string>().c_str(), g_myPlayerID);
            else if(type == "tick") ProcessAutomationRuntime();
            else if(type == "backend_tick") { ProcessDbSync(); PumpBackendHttp(); }
            else if(type == "disconnect") { resetPools(1, 0); }
            else if(type == "login_tick") { assert(ProcessLoginGate(g_loginPendingUntil)); }
            else if(type == "dialog") {
                sampDialog.bDialogStyle = event.value("style", 2);
                HandleCapturedDialog(7, event.at("title").get<std::string>().c_str(), event.at("text").get<std::string>().c_str());
            } else if(type == "expect_command" || type == "expect_command_prefix") assert(Contains(ClientReplay().commands, event.at("text"), type == "expect_command_prefix"));
            else if(type == "expect_chat_contains") {
                bool found = false; for(const auto &line : ClientReplay().chats) if(line.find(event.at("text").get<std::string>()) != std::string::npos) found = true;
                assert(found);
            } else if(type == "expect") {
                if(event.contains("results")) assert(ClientReplay().warResults.size() == event.at("results").get<size_t>());
                if(event.contains("login")) assert(g_loginPending == event.at("login").get<bool>());
                if(event.contains("war")) assert(g_inZoneWar == event.at("war").get<bool>());
                if(event.contains("connected")) assert((iAreWeConnected != 0) == event.at("connected").get<bool>());
                if(event.contains("group")) assert(g_myGroupName == event.at("group").get<std::string>());
                if(event.contains("queued")) assert(DescribeAutomation().find("queued=" + std::to_string(event.at("queued").get<int>())) != std::string::npos);
            } else assert(false);
        }
    }
    RakNetworkFactory::DestroyRakClientInterface(pRakClient); pRakClient = NULL;
    std::cout << "Runtime replay: OK (actual client handlers; no game or backend connection)\n";
}
