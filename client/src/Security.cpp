#include "Security.h"
#include <windows.h>
#include <intrin.h>
#include <iostream>
#include <sstream>
#include <string.h>

#include "httplib.h"
#include "json.hpp"
#include "GameChatPayload.h"
#include "BackendHttp.h"
#include "HardwareIdentity.h"
#include "BackendApi.h"
#include "DurableResults.h"
#include "LogPrivacy.h"
#include "ReplayHooks.h"

// misc_funcs.cpp icerisinde tanimli (main.h bagimliligini onlemek icin local decl)
void sendChat(char *szMessage);
void Log(char *fmt, ...);

using json = nlohmann::json;

// VDS Backend taban adresi (main.cpp handshake ile ayni kalmali)
static const std::string &g_backendBase = ClientOptions().backendUrl;
static BackendHttpQueue &BackendQueue() { static BackendHttpQueue queue(g_backendBase); return queue; }
void PumpBackendHttp() {
    if(ClientReplay().enabled) {
        auto pending = std::move(ClientReplay().completions); ClientReplay().completions.clear();
        for(const auto &callback : pending) callback(); return;
    }
    BackendQueue().Pump();
}
static unsigned long s_backendLinkGeneration = 0;
static void ClearBackendSnapshots();
static DurableResults &WarOutbox();
void InvalidateBackendResponses() { ++s_backendLinkGeneration; ClearBackendSnapshots(); }
static bool QueueBackend(const std::string &method, const std::string &path, const std::string &body,
    BackendHttpQueue::Callback callback, const std::string &key = "") {
    if(ClientReplay().enabled) {
        ClientReplay().http.push_back(method + " " + path + "\n" + body);
        BackendResponse response;
        auto fixture = ClientReplay().responses.find(method + " " + path);
        if(fixture != ClientReplay().responses.end()) {
            response = std::make_shared<httplib::Response>(); response->status = fixture->second.first; response->body = fixture->second.second;
        }
        if(callback) ClientReplay().completions.push_back([callback, response]() { callback(response); });
        return true;
    }
    bool queued = BackendQueue().Submit(method, path, body, callback, key);
    if(!queued && key.empty()) Log("[BACKEND] Request queue full: %s", path.c_str());
    return queued;
}

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
// false = /astats (kullan�c� account ID girer)
bool g_gstatsAutoSubmit = true;
bool g_gstatsSentFirst = false;
// Discord slash komutundan gelen requesterId: sonuc o kullan�c�ya reply olarak g�nderilir
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
bool g_welcomeEnabled = true;   // ?greet a��k/kapal� (backend welcome_enabled kopyasi)
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
    int cpuinfo[4] = {};
    __cpuid(cpuinfo, 0);
    std::ostringstream cpu;
    cpu << std::hex << cpuinfo[0] << ':' << cpuinfo[1] << ':' << cpuinfo[2] << ':' << cpuinfo[3];
    DWORD serial = 0;
    std::ostringstream volume;
    if(GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) volume << std::hex << serial;
    char guid[256] = {};
    DWORD size = sizeof(guid);
    if(RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid",
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, NULL, guid, &size) != ERROR_SUCCESS) guid[0] = 0;
    return BuildHardwareIdentity(cpu.str(), volume.str(), guid);
}

// 3. Sunucu Handshake
bool PerformSecurityHandshake(const std::string& serverUrl, const char *nick) {
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }

    g_myHWID = ScopeHardwareIdentity(GenerateSecureHWID(), nick);
    if(g_myHWID.empty()) {
        MessageBoxA(NULL, "Cihaz kimligi okunamadi. Disk veya MachineGuid erisimini kontrol edin.", "RakSAMP - HWID", MB_OK | MB_ICONERROR);
        return false;
    }
    std::cout << "[HWID] " << g_myHWID << "\n";

    httplib::Client cli(serverUrl);
    cli.set_connection_timeout(5);

    json requestBody = { {"hwid", g_myHWID} };
    SetBackendTimeouts(cli);
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
                InvalidateBackendResponses();
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

    // Backend ayakta degilse (Discord botu kapal�) kullan�c�ya belirgin uyari goster
    if (!res || res->status != 200) {
        MessageBoxA(NULL,
            "Discord botu kapal� oldugu icin RakSAMP acilamiyor.\n\n"
            "Lutfen Discord botunun (backend sunucusunun) calistigindan emin olun ve tekrar deneyin.\n\n"
            "HWID panoya kopyalandi.",
            "RakSAMP - Ba�lanti Hatasi",
            MB_OK | MB_ICONERROR);
    }

    Sleep(5000);
    ExitProcess(0);
    return false;
}

// 4. Oyun i├gi ?w (War Help) - Discord'a @everyone ile yard─▒m ├ga─sr─▒s─▒ g├nder
void TriggerWarHelp(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName ? playerName : (g_szNickName ? g_szNickName : ""))},
        {"ownGroup", g_myGroupName},
        {"enemyGroup", g_enemyGroupName}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/warhelp", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        std::cout << "-Uo´©Å [WARHELP] Discord'a yardim cagrisi g�nderildi!\n";
    } else {
        std::cout << "-Uá´©Å [WARHELP] Discord'a g�nderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
    });
}

// 5. Oyun i├gi ?discordchat - MongoDB'deki k├pr├╝ durumunu a├g/kapat
void ToggleDiscordChat() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    json body = { {"hwid", g_myHWID} };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/groupchat/toggle", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_groupChatEnabled = data.value("enabled", false);
            if (g_groupChatEnabled) {
                std::cout << "­sÆ¼ [GROUPCHAT] Discord grup chat k├pr├╝s├╝ A├cILDI\n";
                // Oyun icinde dile g�re geri bildirim (Discord'a tekrar gitmesin)
                g_bridgeSuppressEcho++;
                sendChat((char *)(g_langTurkish ? "!Discord otomasyonu A�IK." : "!Discord automation enabled"));
            } else {
                std::cout << "­sÆ¼ [GROUPCHAT] Discord grup chat k├pr├╝s├╝ KAPANDI\n";
                g_bridgeSuppressEcho++;
                sendChat((char *)(g_langTurkish ? "!Discord otomasyonu KAPALI." : "!Discord automation disabled"));
            }
        } catch (...) {}
    } else {
        std::cout << "-Uá´©Å [GROUPCHAT] Durum de�i�tirilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
    });
}

// ?tr/?en: bot mesaj dilini backend'e (MongoDB Guild.lang) yazar
static bool s_langPending = false;
static DWORD s_langAttemptAt = 0;
static void TrySaveBotLang() {
    if(!s_langPending || !g_isEngineActive || g_sessionToken.empty()) return;
    const DWORD now = ClientTick();
    if(s_langAttemptAt && now - s_langAttemptAt < 5000) return;
    s_langAttemptAt = now;
    try {

        json body = {{"hwid", g_myHWID}, {"lang", g_langTurkish ? "tr" : "en"}};
        const bool requestedLang = g_langTurkish;
        QueueBackend("POST", "/api/lang/set", body.dump(), [requestedLang](const BackendResponse &res) {
        if(requestedLang != g_langTurkish) return;
        if(res && res->status == 200 && json::parse(res->body).value("success", false)) {
            s_langPending = false;
            Log("[LANG] Saved for HWID %s: %s", g_myHWID.c_str(), g_langTurkish ? "tr" : "en");
        } else Log("[LANG] Save failed; retaining local language and retrying.");
        }, "lang-save");
    } catch(...) { Log("[LANG] Save failed; retaining local language and retrying."); }
}
void ServerSetBotLang(bool turkish) {
    g_langTurkish = turkish;
    s_langPending = true;
    s_langAttemptAt = 0;
    TrySaveBotLang();
}

static bool s_unlinkPending = false;
static DWORD s_unlinkRetryAt = 0;
// ?guildid: bu HWID'i Discord sunucusuna baglar. Sonraki tum hwid tabanli
// cagrilar (dil, grup chat, welcome...) o sunucuya cozulur.
bool ServerSetGuildId(const char *guildId) {
    if(s_unlinkPending || !g_isEngineActive || !guildId || !*guildId) return false;
    const json body = {{"hwid", g_myHWID}, {"guildId", guildId}};
    return QueueBackendRequest("POST", "/api/guildid", body.dump(), [](const BackendResponse &res) {
        bool ok = false;
        try { ok = res && res->status == 200 && json::parse(res->body).value("success", false); } catch(...) {}
        if(ok) InvalidateBackendResponses();
        g_bridgeSuppressEcho++;
        sendChat((char*)(ok ? (g_langTurkish ? "!Discord sunucusuna baglandi." : "!Linked to Discord guild.") : (g_langTurkish ? "!Discord baglantisi basarisiz." : "!Discord link failed.")));
    });
}

void ProcessDiscordUnlink() {
    if(!s_unlinkPending || g_sessionToken.empty()) return;
    const DWORD now = ClientTick();
    if(s_unlinkRetryAt && now - s_unlinkRetryAt < 5000) return;
    s_unlinkRetryAt = now;
    try {

        json body = {{"sessionToken", g_sessionToken}};
        const unsigned long generation = s_backendLinkGeneration;
        QueueBackend("POST", "/api/unlink", body.dump(), [generation](const BackendResponse &res) {
        if(generation != s_backendLinkGeneration) return;
        if(res && res->status == 200 && json::parse(res->body).value("success", false)) {
            s_unlinkPending = false;
            Log("[UNLINK] Discord link removed.");
        } else Log("[UNLINK] Failed; retrying (HTTP %d).", res ? res->status : -1);
        }, "unlink");
    } catch(...) { Log("[UNLINK] Request failed; retrying."); }
}
void RequestDiscordUnlink(bool groupExit) {
    InvalidateBackendResponses();
    s_unlinkPending = true;
    s_unlinkRetryAt = 0;
    g_groupChatEnabled = false;
    g_myGroupId.clear();
    if(groupExit) g_myGroupName.clear();
    ProcessDiscordUnlink();
}

// 6. Oyun -> Discord: grup chat'inde ! ile ba�layan kendi mesaj�mizi ilet
void SendGroupChatToDiscord(const char *text, const char *author) {
    if(s_unlinkPending) return;
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (text == NULL || text[0] == '\0') return;

    try {
        const json body = BuildGroupChatPayload(g_myHWID, author, text);
        const std::string payload = body.dump();

        QueueBackend("POST", "/api/groupchat", payload, [](const BackendResponse &res) {
        if(!res || res->status != 200)
            Log("[GROUPCHAT] Discord delivery failed (HTTP %d), client remains running.", res ? res->status : -1);
        });
    } catch(const std::exception &error) {
        Log("[GROUPCHAT] Discord delivery failed: %s", error.what());
    } catch(...) {
        Log("[GROUPCHAT] Discord delivery failed: unexpected error, client remains running.");
    }
}

// 7. Discord -> Oyun: bekleyen Discord mesajlarini cekip oyuna ilet (2 sn'de bir)
void PollDiscordChat() {
    if(s_unlinkPending) return;
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = ClientTick();
    if (s_lastGroupChatPoll != 0 && (dwNow - s_lastGroupChatPoll) < ClientOptions().pollIntervalMs) return;
    s_lastGroupChatPoll = dwNow;

    std::string url = "/api/groupchat/poll?hwid=" + g_myHWID;
    // Botun calistigi grubu backend'e bildir (guild kaydina groupName yazilir):
    // "hakixld botu The Youngster'da" bilgisi buradan gelir (oyun ici otomatik ��renilir).
    if (!g_myGroupName.empty()) {
        url += "&group=" + BackendQueryValue(GameChatToUtf8(g_myGroupName.c_str()));
    }
    const unsigned long generation = s_backendLinkGeneration;
    const std::string group = g_myGroupName;
    QueueBackend("GET", url, "", [generation, group](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration || group != g_myGroupName || s_unlinkPending) return;
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_groupChatEnabled = data.value("enabled", g_groupChatEnabled);

        // Bot dilini backend'den senkronla. Backend yalnizca guild bulabildigi
        // zaman lang g�nderir; guild yoksa alan yoktur ve lokal secim (?tr/?en)
        // korunur - backend'in "en" varsayilani ?tr'yi ezmesin.
        if (!s_langPending && data.contains("lang") && data["lang"].is_string()) {
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
                // "!" on-eki sunucunun bu mesaj� gruba yonlendirmesini saglar;
                // HandleGroupCommand "!!" aradigi icin bu tek "!" Discord'a geri gitmez.
                std::string snd = "![Discord] " + author + ": " + text;
                char szBuf[300] = {0};
                strncpy_s(szBuf, sizeof(szBuf) - 1, snd.c_str(), sizeof(szBuf) - 1);
                sendChat(szBuf);
            }
        }
    } catch (...) {}
    }, "PollDiscordChat");
}

// 8. /gstats dialog verisini Discord gstats kanalina yolla (SAMPRPC.cpp'den cagrilir)
void SendGStatsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    json body = {
        {"hwid", g_myHWID},
        {"groupId", g_myGroupId},
        {"groupName", g_myGroupName},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"statsText", std::string(szInfo)},
        {"requesterId", g_gstatsRequesterId}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/gstats", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        std::cout << "­sôè [GSTATS] Discord'a g�nderildi!\n";
    } else {
        std::cout << "-Uá´©Å [GSTATS] G�nderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }


    });
    g_gstatsRequesterId = "";
}

// /admins dialog sonucunu Discord VDS'e aktar (requesterId'ye reply olarak)
void SendAdminsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"adminsText", std::string(szInfo)},
        {"requesterId", g_adminsRequesterId}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/admins", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        std::cout << "­søí´©Å [ADMINS] Discord'a g�nderildi!\n";
    } else {
        std::cout << "-Uá´©Å [ADMINS] G�nderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }


    });
    g_adminsRequesterId = "";
}

// 9. Oyun i├gi ?1414 - backup ├ga─sr─▒s─▒ -åÆ backup kanal─▒na @here
void SendBackupCall(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName ? playerName : (g_szNickName ? g_szNickName : "Bir oyuncu"))},
        {"ownGroup", g_myGroupName}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/backup", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        std::cout << "­såI [BACKUP] Discord'a backup cagrisi g�nderildi!\n";
    } else {
        std::cout << "-Uá´©Å [BACKUP] G�nderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
    });
}

// 10. Oyun i├gi welcome back - "X logged in" -åÆ welcome kanal─▒na embed
void SendWelcomeBack(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (playerName == NULL) return;

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName)},
        {"groupName", g_myGroupName},
        {"welcomeMessage", g_welcomeMessage}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/welcome", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data.contains("enabled") && !data.value("enabled", true)) {
                std::cout << "Welcome back mesajlari kapal�, bildirim atlandi.\n";
            }
        } catch (...) {}
        std::cout << "­sæï [WELCOME] Discord'a welcome back g�nderildi!\n";
    } else {
        std::cout << "-Uá´©Å [WELCOME] G�nderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
    });
}

// 11a. ?greet - welcome back bildirimlerini a├g/kapat (DB'de saklanir).
// bForceOnOff: -1 = toggle, 0 = off'a zorla, 1 = on'a zorla.
// Sonuc GRUPTA duyurulur (kullan�c�: "a��k mi kapal� mi diye mesaj atmiyor").
void ToggleWelcomeBackEx(int force) {
    if(!g_isEngineActive || g_sessionToken.empty()) return;
    json body = {{"hwid", g_myHWID}};
    if(force >= 0) body["enabled"] = force != 0;
    QueueBackendRequest("POST", "/api/welcome/toggle", body.dump(), [](const BackendResponse &res) {
        bool ok = false;
        try {
            if(res && res->status == 200) { const json data = json::parse(res->body); ok = data.value("success", false); if(ok) g_welcomeEnabled = data.value("enabled", g_welcomeEnabled); }
        } catch(...) {}
        g_bridgeSuppressEcho++;
        sendChat((char*)(ok ? (g_welcomeEnabled ? (g_langTurkish ? "!Karsilama mesajlari: ACIK" : "!Welcome messages: ON") : (g_langTurkish ? "!Karsilama mesajlari: KAPALI" : "!Welcome messages: OFF")) : (g_langTurkish ? "!Karsilama ayari degistirilemedi." : "!Could not change welcome messages.")));
    });
}

// 11b. ?welcomemsg <mesaj> - ki┼siye ├zel welcome mesaj─▒ ayarla (MongoDB'de saklan─▒r)
void SetWelcomePerUser(const char *target, const char *message) {
    if(!g_isEngineActive || !target || !*target) return;
    json body = {{"hwid", g_myHWID}, {"target", target}, {"message", message ? message : ""}};
    QueueBackendRequest("POST", "/api/welcome/peruser", body.dump(), [](const BackendResponse &res) {
        Log("[WELCOME] Per-user setting response: HTTP %d", res ? res->status : 0);
    });
}
std::string GetWelcomePerUser(const char *target) {
    if(!g_isEngineActive || !target || !*target) return "";
    try {
        const auto res = GetBackendSnapshot("/api/welcome/peruser/list?hwid=" + g_myHWID, 10000);
        if(res && res->status == 200) {
            const json data = json::parse(res->body);
            if(data.contains("list") && data["list"].is_array()) for(const auto &row : data["list"])
                if(_stricmp(row.value("target_name", "").c_str(), target) == 0) return row.value("custom_message", "");
        }
    } catch(...) {}
    return "";
}
void SetWelcomeMessage(const char *message) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    json body = {
        {"hwid", g_myHWID},
        {"welcomeMessage", message ? std::string(message) : ""}
    };

    const unsigned long generation = s_backendLinkGeneration;
    QueueBackend("POST", "/api/welcome/message", body.dump(), [generation](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration) return;
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
            std::cout << "-£ë´©Å [WELCOME] Kisisele welcome mesaj� ayarlandi: " << g_welcomeMessage << "\n";
        } catch (...) {}
    } else {
        std::cout << "-Uá´©Å [WELCOME] Mesaj ayarlanamadi (HTTP " << (res ? res->status : -1) << ")\n";
    }
    });
}

// 11c. Backend'den welcome ayarlarini cek (PollCommands'a benzer, ana donguden cagrilir)
void RefreshWelcomeState() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    GetBackendSnapshot("/api/welcome/peruser/list?hwid=" + g_myHWID, 10000);

    DWORD dwNow = ClientTick();
    static DWORD s_lastWelcomePoll = 0;
    if (s_lastWelcomePoll != 0 && (dwNow - s_lastWelcomePoll) < 10000) return;
    s_lastWelcomePoll = dwNow;

    std::string url = "/api/welcome/state?hwid=" + g_myHWID;
    const unsigned long generation = s_backendLinkGeneration;
    const std::string group = g_myGroupName;
    QueueBackend("GET", url, "", [generation, group](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration || group != g_myGroupName || s_unlinkPending) return;
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
        g_welcomeEnabled = data.value("welcomeEnabled", true); // greet on/off kopyasi
    } catch (...) {}
    }, "RefreshWelcomeState");
}

// 12. Discord -> Oyun: /send ve /gstats komutlar�ni kuyruktan cekip oyunda calistir (2 sn'de bir)
// Retry acknowledgements in order if the local backend is temporarily unavailable.
static std::vector<json> s_commandStatusOutbox;
static bool s_statusInFlight = false;
// Oasis backend retains its existing pending-interaction delivery flow.
static void FlushDiscordCommandStatuses() {}
void SendDiscordCommandStatus(const char *requesterId, const char *status, const char *reason) {
    if(requesterId && requesterId[0]) Log("[COMMAND] request=%s status=%s reason=%s",requesterId,status ? status : "",reason ? reason : "");
}
static unsigned long long CurrentUnixMilliseconds() {
    FILETIME value; GetSystemTimeAsFileTime(&value);
    ULARGE_INTEGER time; time.LowPart = value.dwLowDateTime; time.HighPart = value.dwHighDateTime;
    return time.QuadPart / 10000ULL - 11644473600000ULL;
}
static void ExecuteDiscordCommand(const json &c) {
    const unsigned long long expiry = c.value("expiresAt", 0ULL);
    if(expiry && CurrentUnixMilliseconds() >= expiry) {
        SendDiscordCommandStatus(c.value("requesterId", "").c_str(), "timeout", "expired"); return;
    }                std::string type = c.value("type", "command");
                std::string cmd = c.value("cmd", "");
                std::string requesterId = c.value("requesterId", "");

                if (type == "gmembers") {
                    std::cout << "[COMMAND] Discord'dan /gmembers harvest istendi.\n";
                    StartGmembersCollect(requesterId.c_str());
                } else if (type == "gstats") {
                    g_waitingForGStats = true;
                    g_gstatsReqTime = ClientTick();
                    g_gstatsRequesterId = requesterId;
                    g_gstatsSentFirst = false;
                    g_gstatsAutoSubmit = true;
                    std::cout << "­sôè [COMMAND] Discord'dan /gstats istendi (" << RedactLogSecrets(cmd) << ")\n";
                    sendServerCommand((char *)cmd.c_str());
                } else if (type == "admins") {
                    g_waitingForAdmins = true;
                    g_adminsReqTime = ClientTick();
                    g_adminsRequesterId = requesterId;
                    std::cout << "­søí´©Å [COMMAND] Discord'dan /admins istendi (" << RedactLogSecrets(cmd) << ")\n";
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
                        std::cout << "­sæñ [COMMAND] Discord'dan /grole direk: " << RedactLogSecrets(cmd) << "\n";
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
                        std::cout << "­sÅÀ´©Å [COMMAND] Discord'dan /grank direk: " << RedactLogSecrets(cmd) << "\n";
                        sendServerCommand((char *)cmd.c_str());
                    }
                } else if (type == "ginvite") {
                    std::string target = c.value("target", "");
                    if(target.empty() && !cmd.empty()){
                        char t[64]={0};
                        if(sscanf(cmd.c_str(), "/ginvite %63s", t)==1){ target=t; }
                        else if(sscanf(cmd.c_str(), "%63s", t)==1){ target=t; }
                    }
                    if(!target.empty()){
                        std::cout << "[COMMAND] Discord'dan /ginvite: " << target << "\n";
                        char szCmd[80]={0};
                        sprintf_s(szCmd, sizeof(szCmd), "/ginvite %s", target.c_str());
                            sendServerCommand(szCmd);
                    } else if(!cmd.empty()){
                        sendServerCommand((char *)cmd.c_str());
                    }
                } else if (type == "gkick_list") {
                    std::string requesterId = c.value("requesterId", "");
                    std::string guildId = c.value("guildId", "");
                    std::string hwid2 = c.value("hwid", "");
                    std::cout << "[COMMAND] Discord'dan /gkick list istendi" << std::endl;
                    StartGkickList(requesterId.c_str(), guildId.c_str(), hwid2.c_str());
                } else if (type == "gkick") {
                    std::string target = c.value("target", "");
                    std::string reason = c.value("reason", "");
                    if(!target.empty()){
                        std::cout << "[COMMAND] Discord'dan /gkick otomasyonu: " << target << std::endl;
                            StartGkick(target.c_str(), reason.c_str());
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
                    std::cout << "­sÄ« [COMMAND] Discord'dan komut geldi: " << RedactLogSecrets(cmd) << "\n";
                    RunCommand((char *)cmd.c_str(), 1);
                }
}

void PollCommands() {
    TrySaveBotLang();
    ProcessDiscordUnlink();
    if(s_unlinkPending) return;
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = ClientTick();
    static DWORD s_lastCommandPoll = 0;
    if (s_lastCommandPoll != 0 && (dwNow - s_lastCommandPoll) < ClientOptions().pollIntervalMs) return;
    s_lastCommandPoll = dwNow;
    FlushDiscordCommandStatuses();

    std::string url = "/api/command/poll?hwid=" + g_myHWID;
    const unsigned long generation = s_backendLinkGeneration;
    const std::string group = g_myGroupName;
    QueueBackend("GET", url, "", [generation, group](const BackendResponse &res) {
    if(generation != s_backendLinkGeneration || group != g_myGroupName || s_unlinkPending) return;
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        if (data.contains("commands") && data["commands"].is_array()) {
            for (auto &c : data["commands"]) {
                const json command = c;
                const std::string key = "discord:" + c.value("type", "command") + ":" + c.value("requesterId", "") + ":" + c.value("cmd", "");
                if(!QueueAutomationAction(key, [command]() { ExecuteDiscordCommand(command); }))
                    SendDiscordCommandStatus(c.value("requesterId", "").c_str(), "failed", "busy");
            }
        }
    } catch (...) {}
    }, "PollCommands");
}

// 13. GZ sonucu (kill list) - "gz sonucl.txt" dosyasindan okunan metni Discord zonewar kanalina embed olarak yolla
void SendKillList(const char *redBox, const char *blueBox, const char *redName, const char *blueName,
    const char *redTag, const char *blueTag, int redKills, int blueKills, bool won,
    const char *mvpName, const char *feederName, int mvpDmg, int mvpKills, int feederDeaths,
    const char *firstBlood, const char *topWeapons, const char *ownGroup, const char *zone, const char *warId, bool wonKnown) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (!redBox || !redBox[0]) return; // Oasis provides one scoreboard box.

    try {


    json body = {
        {"hwid", g_myHWID},
        {"ownGroup", ownGroup ? ownGroup : ""}, {"zone", zone ? zone : ""},
        {"warId", warId ? warId : ""}, {"wonKnown", wonKnown},
        {"lang", g_langTurkish ? "tr" : "en"},
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

    for(auto it = body.begin(); it != body.end(); ++it) {
        if(it.value().is_string()) { const std::string raw = it.value().get<std::string>(); it.value() = GameChatToUtf8(raw.c_str()); }
    }
    if(ClientReplay().enabled) { ClientReplay().warResults.push_back(body.dump()); return; }
    DurableResults &outbox = WarOutbox();
    if(!outbox.Enqueue(body, ClientOptions().resultQueueLimit)) {
        Log("[OUTBOX] Result could not be persisted: %s", outbox.Error().c_str()); return;
    }
    Log("[OUTBOX] War result saved; pending=%d.", (int)outbox.Size());
    ProcessDurableResults();    } catch(const std::exception &err) { Log("[KILLLIST] Delivery error: %s", err.what()); }
}

#include "BackendRuntime.inc"
static void ClearBackendSnapshots() { s_backendReads.clear(); }
