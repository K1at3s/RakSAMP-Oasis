#include "Security.h"
#include <windows.h>
#include <intrin.h>
#include <iostream>
#include <sstream>
#include <string.h>

#include "httplib.h"
#include "json.hpp"

// misc_funcs.cpp icerisinde tanimli (main.h bagimliligini onlemek icin local decl)
void sendChat(char *szMessage);
void Log(char *fmt, ...);

using json = nlohmann::json;

// VDS Backend taban adresi (main.cpp handshake ile ayni kalmali)
static const std::string g_backendBase = "http://127.0.0.1:3000";

std::string g_myHWID = "";
std::string g_sessionToken = "";
std::string g_commandPrefix = "";
std::string g_myGroupId = "";
std::string g_myGroupName = "";
std::string g_enemyGroupName = "";
bool g_isEngineActive = false;
bool g_waitingForGStats = false;
unsigned long g_gstatsReqTime = 0;
// true = /gstats,/stats (ID dialogunda otomatik bos submit -> kendi grubu)
// false = /astats (kullanici account ID girer)
bool g_gstatsAutoSubmit = true;
bool g_gstatsSentFirst = false;
// Discord slash komutundan gelen requesterId: sonuc o kullaniciya reply olarak gonderilir
std::string g_gstatsRequesterId = "";
// Komut parametresi (/gstats 5, /stats 5, /astats 5 gibi) - ID dialogu submit'inde kullanilir
std::string g_gstatsInput = "";
bool g_waitingForPNetStats = false;
unsigned long g_pnetstatsReqTime = 0;
bool g_inZoneWar = false;
int g_plTargetId = -1;
// ?pl istatistik cache'i (netrpc.cpp doldurur, SAMPRPC.cpp okur)
stPlayerCache g_playerCache[PLAYER_CACHE_SIZE] = {0};
bool g_waitingForGst = false;
unsigned long g_gstReqTime = 0;
bool g_waitingForAdmins = false;
unsigned long g_adminsReqTime = 0;
std::string g_adminsRequesterId = "";

bool g_groupChatEnabled = false;
int g_bridgeSuppressEcho = 0;
std::string g_welcomeMessage = "";
bool g_welcomeEnabled = true;   // ?greet acik/kapali (backend welcome_enabled kopyasi)
// Bot mesaj dili: false=English (varsayilan), true=Turkce. Mongo'dan (Guild.lang) gelir.
bool g_langTurkish = false;

static DWORD s_lastGroupChatPoll = 0;

// 1. Panoya HWID Kopyalama
void CopyToClipboard(const std::string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

// 2. HWID ├£retimi (CPU ID + Vol Serial)
std::string GenerateSecureHWID() {
    int cpuinfo[4] = { 0 };
    __cpuid(cpuinfo, 0);
    std::stringstream ss;
    ss << std::hex << cpuinfo[0] << cpuinfo[1] << cpuinfo[2] << cpuinfo[3];

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);
    ss << std::hex << volSerial;

    std::string raw = ss.str();
    std::string hwid = "BURHAN-";
    for (size_t i = 0; i < raw.length() && i < 16; ++i) {
        if (i > 0 && i % 4 == 0) hwid += "-";
        hwid += toupper(raw[i]);
    }
    return hwid;
}

// 3. Sunucu Handshake
bool PerformSecurityHandshake(const std::string& serverUrl) {
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }

    g_myHWID = GenerateSecureHWID();

    httplib::Client cli(serverUrl);
    cli.set_connection_timeout(5);

    json requestBody = { {"hwid", g_myHWID} };
    auto res = cli.Post("/api/handshake", requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data["success"] == true) {
                g_sessionToken = data["sessionToken"].get<std::string>();
                g_commandPrefix = data["config"]["commandPrefix"].get<std::string>();
                g_isEngineActive = true;

                std::cout << "-------------------------------------------\n";
                std::cout << "-£à [LISANS ONAYLANDI] Sunucuya baglaniliyor...\n";
                std::cout << "-------------------------------------------\n";
                return true;
            }
        }
        catch (...) {}
    }

    std::cout << "-------------------------------------------\n";
    std::cout << "[LISANSSIZ CIHAZ] Erisim Yetkiniz Yok!\n";
    CopyToClipboard(g_myHWID);
    std::cout << "[HWID] Panoya kopyalandi: " << g_myHWID << "\n";
    std::cout << "[INFO] Discord uzerinden yoneticiye bu kodu iletin.\n";
    std::cout << "-------------------------------------------\n";

    // Backend ayakta degilse (Discord botu kapali) kullaniciya belirgin uyari goster
    if (!res || res->status != 200) {
        MessageBoxA(NULL,
            "Discord botu kapali oldugu icin RakSAMP acilamiyor.\n\n"
            "Lutfen Discord botunun (backend sunucusunun) calistigindan emin olun ve tekrar deneyin.\n\n"
            "HWID panoya kopyalandi.",
            "RakSAMP - Baslanti Hatasi",
            MB_OK | MB_ICONERROR);
    }

    Sleep(5000);
    ExitProcess(0);
    return false;
}

// 4. Oyun i├gi ?w (War Help) - Discord'a @everyone ile yard─▒m ├ga─sr─▒s─▒ g├nder
void TriggerWarHelp(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName ? playerName : (g_szNickName ? g_szNickName : ""))},
        {"ownGroup", g_myGroupName},
        {"enemyGroup", g_enemyGroupName}
    };

    auto res = cli.Post("/api/warhelp", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "-Uo´©Å [WARHELP] Discord'a yardim cagrisi gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [WARHELP] Discord'a gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 5. Oyun i├gi ?discordchat - MongoDB'deki k├pr├╝ durumunu a├g/kapat
void ToggleDiscordChat() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = { {"hwid", g_myHWID} };

    auto res = cli.Post("/api/groupchat/toggle", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_groupChatEnabled = data.value("enabled", false);
            if (g_groupChatEnabled) {
                std::cout << "­sÆ¼ [GROUPCHAT] Discord grup chat k├pr├╝s├╝ A├cILDI\n";
                // Oyun icinde dile gore geri bildirim (Discord'a tekrar gitmesin)
                g_bridgeSuppressEcho++;
                sendChat((char *)(g_langTurkish ? "!Discord otomasyonu ACIK." : "!Discord automation enabled"));
            } else {
                std::cout << "­sÆ¼ [GROUPCHAT] Discord grup chat k├pr├╝s├╝ KAPANDI\n";
                g_bridgeSuppressEcho++;
                sendChat((char *)(g_langTurkish ? "!Discord otomasyonu KAPALI." : "!Discord automation disabled"));
            }
        } catch (...) {}
    } else {
        std::cout << "-Uá´©Å [GROUPCHAT] Durum degistirilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// ?tr/?en: bot mesaj dilini backend'e (MongoDB Guild.lang) yazar
void ServerSetBotLang(bool turkish) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"lang", turkish ? "tr" : "en"}
    };

    auto res = cli.Post("/api/lang/set", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << (turkish ? "­sîÉ [LANG] Turkce kaydedildi (MongoDB)\n" : "­sîÉ [LANG] English saved (MongoDB)\n");
    } else {
        std::cout << "-Uá´©Å [LANG] Kaydedilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// ?guildid: bu HWID'i Discord sunucusuna baglar. Sonraki tum hwid tabanli
// cagrilar (dil, grup chat, welcome...) o sunucuya cozulur.
bool ServerSetGuildId(const char *guildId) {
    if (!g_isEngineActive || g_sessionToken.empty() || !guildId || !guildId[0]) return false;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"guildId", guildId}
    };

    auto res = cli.Post("/api/guildid", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data.value("success", false)) {
                std::cout << "­soù [GUILDID] Baglandi: " << guildId << "\n";
                return true;
            }
        } catch (...) {}
    }
    std::cout << "-Uá´©Å [GUILDID] Baglanti basarisiz (HTTP " << (res ? res->status : -1) << ")\n";
    return false;
}

// 6. Oyun -> Discord: grup chat'inde ! ile baslayan kendi mesajimizi ilet
void SendGroupChatToDiscord(const char *text) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (text == NULL || text[0] == '\0') return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"author", std::string(g_szNickName ? g_szNickName : "Oyuncu")},
        {"text", text}
    };

    auto res = cli.Post("/api/groupchat", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            (void)data;
        } catch (...) {}
    }
}

// 7. Discord -> Oyun: bekleyen Discord mesajlarini cekip oyuna ilet (2 sn'de bir)
void PollDiscordChat() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    if (s_lastGroupChatPoll != 0 && (dwNow - s_lastGroupChatPoll) < 2000) return;
    s_lastGroupChatPoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/groupchat/poll?hwid=" + g_myHWID;
    // Botun calistigi grubu backend'e bildir (guild kaydina groupName yazilir):
    // "hakixld botu The Youngster'da" bilgisi buradan gelir (oyun ici otomatik ogrenilir).
    if (!g_myGroupName.empty()) {
        url += "&group=" + g_myGroupName;
    }
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_groupChatEnabled = data.value("enabled", g_groupChatEnabled);

        // Bot dilini backend'den senkronla. Backend yalnizca guild bulabildigi
        // zaman lang gonderir; guild yoksa alan yoktur ve lokal secim (?tr/?en)
        // korunur - backend'in "en" varsayilani ?tr'yi ezmesin.
        if (data.contains("lang") && data["lang"].is_string()) {
            std::string lang = data["lang"].get<std::string>();
            bool bTr = (lang == "tr");
            if (bTr != g_langTurkish) {
                g_langTurkish = bTr;
                SaveLang();
                std::cout << (g_langTurkish ? "­sîÉ [LANG] Dil: Turkce (backend poll)\n" : "­sîÉ [LANG] Language: English (backend poll)\n");
            }
        }

        if (data.contains("messages") && data["messages"].is_array()) {
            for (auto &m : data["messages"]) {
                std::string text = m.value("text", "");
                if (text.empty()) continue;

                // Discord tarafindan kimin yazdigi backend'den geliyorsa kullan.
                std::string author = m.value("author", "");
                if (author.empty()) author = "Discord";

                // Oyunda Discord'dan geldigi ve kimin yazdigi belli olsun.
                // "!" on-eki sunucunun bu mesaji gruba yonlendirmesini saglar;
                // HandleGroupCommand "!!" aradigi icin bu tek "!" Discord'a geri gitmez.
                std::string snd = "![Discord] " + author + ": " + text;
                char szBuf[300] = {0};
                strncpy_s(szBuf, sizeof(szBuf) - 1, snd.c_str(), sizeof(szBuf) - 1);
                sendChat(szBuf);
            }
        }
    } catch (...) {}
}

// 8. /gstats dialog verisini Discord gstats kanalina yolla (SAMPRPC.cpp'den cagrilir)
void SendGStatsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"groupId", g_myGroupId},
        {"groupName", g_myGroupName},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"statsText", std::string(szInfo)},
        {"requesterId", g_gstatsRequesterId}
    };

    auto res = cli.Post("/api/gstats", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "­sôè [GSTATS] Discord'a gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [GSTATS] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }

    g_gstatsRequesterId = "";
}

// /admins dialog sonucunu Discord VDS'e aktar (requesterId'ye reply olarak)
void SendAdminsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"adminsText", std::string(szInfo)},
        {"requesterId", g_adminsRequesterId}
    };

    auto res = cli.Post("/api/admins", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "­søí´©Å [ADMINS] Discord'a gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [ADMINS] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }

    g_adminsRequesterId = "";
}

// 9. Oyun i├gi ?1414 - backup ├ga─sr─▒s─▒ -åÆ backup kanal─▒na @here
void SendBackupCall(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName ? playerName : (g_szNickName ? g_szNickName : "Bir oyuncu"))},
        {"ownGroup", g_myGroupName}
    };

    auto res = cli.Post("/api/backup", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "­såI [BACKUP] Discord'a backup cagrisi gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [BACKUP] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 10. Oyun i├gi welcome back - "X logged in" -åÆ welcome kanal─▒na embed
void SendWelcomeBack(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (playerName == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName)},
        {"groupName", g_myGroupName},
        {"welcomeMessage", g_welcomeMessage}
    };

    auto res = cli.Post("/api/welcome", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data.contains("enabled") && !data.value("enabled", true)) {
                std::cout << "Welcome back mesajlari kapali, bildirim atlandi.\n";
            }
        } catch (...) {}
        std::cout << "­sæï [WELCOME] Discord'a welcome back gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [WELCOME] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11a. ?greet - welcome back bildirimlerini a├g/kapat (DB'de saklanir).
// bForceOnOff: -1 = toggle, 0 = off'a zorla, 1 = on'a zorla.
// Sonuc GRUPTA duyurulur (kullanici: "acik mi kapali mi diye mesaj atmiyor").
void ToggleWelcomeBackEx(int bForceOnOff) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = { {"hwid", g_myHWID} };

    auto res = cli.Post("/api/welcome/toggle", body.dump(), "application/json");
    bool enabled = false;
    bool bOk = false;
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            enabled = data.value("enabled", true);
            bOk = true;
        } catch (...) {}
    }

    // on/off ile acildiysa ve sonuc istenen degilse bir kez daha cevir
    if (bOk && bForceOnOff >= 0 && (enabled ? 1 : 0) != bForceOnOff) {
        auto res2 = cli.Post("/api/welcome/toggle", body.dump(), "application/json");
        if (res2 && res2->status == 200) {
            try {
                auto data2 = json::parse(res2->body);
                enabled = data2.value("enabled", !enabled);
            } catch (...) {}
        }
    }
    if (bOk) g_welcomeEnabled = enabled; // yerel kopyayi guncelle (in-game greet bunu kullanir)

    char szMsg[96];
    if (bOk)
        sprintf_s(szMsg, sizeof(szMsg), g_langTurkish ? "!Welcome mesajlari: %s." : "!Welcome messages: %s.",
            enabled ? (g_langTurkish ? "ACIK" : "ON") : (g_langTurkish ? "KAPALI" : "OFF"));
    else
        sprintf_s(szMsg, sizeof(szMsg), g_langTurkish ? "!Welcome mesajlari degistirilemedi." : "!Could not change welcome messages.");
    sendChat(szMsg);
    std::cout << (bOk ? (enabled ? "­sæï [WELCOME] Welcome back ACIK\n" : "­sæï [WELCOME] Welcome back KAPALI\n")
                      : "-Uá´©Å [WELCOME] Durum degistirilemedi\n");
}

// 11b. ?welcomemsg <mesaj> - ki┼siye ├zel welcome mesaj─▒ ayarla (MongoDB'de saklan─▒r)
void SetWelcomePerUser(const char *target, const char *message) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (!target || !target[0]) return;
    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);
    json body = { {"hwid", g_myHWID}, {"target", std::string(target)}, {"message", message ? std::string(message) : ""} };
    auto res = cli.Post("/api/welcome/peruser", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "Per-user (" << target << ") ayarlandi: " << (message ? message : "(silindi)") << "\n";
        Log("[WELCOME] Per-user %s -> %s", target, message && message[0] ? message : "(silindi)");
    } else {
        std::cout << "Per-user ayarlanamadi\n";
    }
}
std::string GetWelcomePerUser(const char *target) {
    if (!g_isEngineActive || g_sessionToken.empty() || !target || !target[0]) return "";
    try {
        httplib::Client cli(g_backendBase);
        cli.set_connection_timeout(3);
        std::string enc; for(char c: std::string(target)){ if(c==' ') enc+="%20"; else enc+=c; }
        std::string url = "/api/welcome/peruser?hwid=" + g_myHWID + "&target=" + enc;
        auto res = cli.Get(url.c_str());
        if (res && res->status == 200) {
            auto data = json::parse(res->body);
            return data.value("message", "");
        }
    } catch(...) {}
    return "";
}
void SetWelcomeMessage(const char *message) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"welcomeMessage", message ? std::string(message) : ""}
    };

    auto res = cli.Post("/api/welcome/message", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
            std::cout << "-£ë´©Å [WELCOME] Kisisele welcome mesaji ayarlandi: " << g_welcomeMessage << "\n";
        } catch (...) {}
    } else {
        std::cout << "-Uá´©Å [WELCOME] Mesaj ayarlanamadi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11c. Backend'den welcome ayarlarini cek (PollCommands'a benzer, ana donguden cagrilir)
void RefreshWelcomeState() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    static DWORD s_lastWelcomePoll = 0;
    if (s_lastWelcomePoll != 0 && (dwNow - s_lastWelcomePoll) < 10000) return;
    s_lastWelcomePoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/welcome/state?hwid=" + g_myHWID;
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
        g_welcomeEnabled = data.value("welcomeEnabled", true); // greet on/off kopyasi
    } catch (...) {}
}

// 12. Discord -> Oyun: /send ve /gstats komutlarini kuyruktan cekip oyunda calistir (2 sn'de bir)
void PollCommands() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    static DWORD s_lastCommandPoll = 0;
    if (s_lastCommandPoll != 0 && (dwNow - s_lastCommandPoll) < 2000) return;
    s_lastCommandPoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/command/poll?hwid=" + g_myHWID;
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        if (data.contains("commands") && data["commands"].is_array()) {
            for (auto &c : data["commands"]) {
                std::string type = c.value("type", "command");
                std::string cmd = c.value("cmd", "");
                std::string requesterId = c.value("requesterId", "");

                if (type == "gmembers") {
                    std::cout << "[COMMAND] Discord'dan /gmembers harvest istendi.\n";
                    StartGmembersCollect();
                } else if (type == "gstats") {
                    g_waitingForGStats = true;
                    g_gstatsReqTime = GetTickCount();
                    g_gstatsRequesterId = requesterId;
                    g_gstatsSentFirst = false;
                    g_gstatsAutoSubmit = true;
                    std::cout << "­sôè [COMMAND] Discord'dan /gstats istendi (" << cmd << ")\n";
                    sendServerCommand((char *)cmd.c_str());
                } else if (type == "admins") {
                    g_waitingForAdmins = true;
                    g_adminsReqTime = GetTickCount();
                    g_adminsRequesterId = requesterId;
                    std::cout << "­søí´©Å [COMMAND] Discord'dan /admins istendi (" << cmd << ")\n";
                    sendServerCommand((char *)cmd.c_str());
                } else if (type == "grole") {
                    std::string target = c.value("target", "");
                    std::string role = c.value("role", "");
                    // Fallback: eski cmd formati "/grole hakixld Member"
                    if(target.empty() && !cmd.empty()){
                        char t[64]={0}, r[64]={0};
                        if(sscanf(cmd.c_str(), "/grole %63s %63[^\r\n]", t, r)==2){ target=t; role=r; }
                        else if(sscanf(cmd.c_str(), "%63s %63[^\r\n]", t, r)==2){ target=t; role=r; }
                    }
                    if(!target.empty() && !role.empty()){
                        std::cout << "­sæñ [COMMAND] Discord'dan /grole otomasyonu: " << target << " -> " << role << "\n";
                        StartGrole(target.c_str(), role.c_str());
                    } else if(!cmd.empty()){
                        std::cout << "­sæñ [COMMAND] Discord'dan /grole direk: " << cmd << "\n";
                        sendServerCommand((char *)cmd.c_str());
                    }
                } else if (type == "grank") {
                    std::string target = c.value("target", "");
                    std::string rank = c.value("rank", "");
                    if(target.empty() && !cmd.empty()){
                        char t[64]={0}, r[64]={0};
                        if(sscanf(cmd.c_str(), "/grank %63s %63[^\r\n]", t, r)==2){ target=t; rank=r; }
                        else if(sscanf(cmd.c_str(), "%63s %63[^\r\n]", t, r)==2){ target=t; rank=r; }
                    }
                    if(!target.empty() && !rank.empty()){
                        std::cout << "­sÅÀ´©Å [COMMAND] Discord'dan /grank otomasyonu: " << target << " -> " << rank << "\n";
                        StartGrank(target.c_str(), rank.c_str());
                    } else if(!cmd.empty()){
                        std::cout << "­sÅÀ´©Å [COMMAND] Discord'dan /grank direk: " << cmd << "\n";
                        sendServerCommand((char *)cmd.c_str());
                    }
                } else if (type == "pm") {
                    std::string target = c.value("target", "");
                    std::string message = c.value("message", "");
                    std::string discordUser = c.value("discordUser", "");
                    std::string reqId = c.value("requesterId", "");
                    std::cout << "[COMMAND] Discord'dan PM: " << target << " <- " << discordUser << "\n";
                    StartPm(target.c_str(), message.c_str(), discordUser.c_str(), reqId.c_str());
                } else if (type == "grole_list") {
                    std::string requesterId = c.value("requesterId", "");
                    std::string guildId = c.value("guildId", "");
                    std::string hwid2 = c.value("hwid", "");
                    std::cout << "­sæÑ [COMMAND] Discord'dan /grole list istendi (req=" << requesterId.substr(0,6) << ")\n";
                    StartGroleList(requesterId.c_str(), guildId.c_str(), hwid2.c_str());
                } else if (type == "grank_list") {
                    std::string requesterId = c.value("requesterId", "");
                    std::string guildId = c.value("guildId", "");
                    std::string hwid2 = c.value("hwid", "");
                    std::cout << "­sæÑ [COMMAND] Discord'dan /grank list istendi (req=" << requesterId.substr(0,6) << ")\n";
                    StartGrankList(requesterId.c_str(), guildId.c_str(), hwid2.c_str());
                } else if (!cmd.empty()) {
                    std::cout << "­sÄ« [COMMAND] Discord'dan komut geldi: " << cmd << "\n";
                    RunCommand((char *)cmd.c_str(), 1);
                }
            }
        }
    } catch (...) {}
}

// 13. GZ sonucu (kill list) - "gz sonucl.txt" dosyasindan okunan metni Discord zonewar kanalina embed olarak yolla
void SendKillList(const char *redBox, const char *blueBox, const char *redName, const char *blueName,
    const char *redTag, const char *blueTag, int redKills, int blueKills, bool won,
    const char *mvpName, const char *feederName, int mvpDmg, int mvpKills, int feederDeaths,
    const char *firstBlood, const char *topWeapons) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (!redBox || !redBox[0] || !blueBox || !blueBox[0]) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"won", won ? 1 : 0},
        {"redName", redName ? redName : ""},
        {"blueName", blueName ? blueName : ""},
        {"redTag", redTag ? redTag : ""},
        {"blueTag", blueTag ? blueTag : ""},
        {"redKills", redKills},
        {"blueKills", blueKills},
        {"redBox", redBox},
        {"blueBox", blueBox},
        {"mvp", mvpName ? mvpName : ""},
        {"mvpDmg", mvpDmg},
        {"mvpKills", mvpKills},
        {"feeder", feederName ? feederName : ""},
        {"feederDeaths", feederDeaths},
        {"firstBlood", firstBlood ? firstBlood : ""},
        {"topWeapons", topWeapons ? topWeapons : ""}
    };

    auto res = cli.Post("/api/killlist", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "-Uo´©Å [KILLLIST] Discord'a GZ sonucu gonderildi!\n";
    } else {
        std::cout << "-Uá´©Å [KILLLIST] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}
