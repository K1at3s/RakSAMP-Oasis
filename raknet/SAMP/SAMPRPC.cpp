/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"
#include <iostream>

// netrpc.cpp'de tanimli: otomatik tag harvest zinciri calisiyor mu
// (harvest /gstats diyaloglari da "UIF - Group Stats" basligini tasir -
// grup adi ogrenme fazi bunlari kendi diyalogu sanip YANLIS isim ogrenmesin).
extern bool g_ghpActive;
// UIF Login dialogu acik mi (netrpc.cpp): kullanici sifre girene kadar tmp hesaba atilmaz
extern bool g_loginPending;
extern unsigned long g_loginPendingUntil;
// /grole / /grank paginated otomasyonu (netrpc.cpp)
extern bool g_groleActive;
extern std::string g_groleTarget;
extern std::string g_groleRole;
extern int  g_grolePhase;
extern unsigned long g_groleStartTime;
extern int  g_grolePage;
extern bool g_grankActive;
extern std::string g_grankTarget;
extern std::string g_grankRank;
extern int  g_grankPhase;
extern unsigned long g_grankStartTime;
extern int  g_grankPage;
extern bool g_groleListActive;
extern std::string g_groleListRequesterId;
extern std::string g_groleListGuildId;
extern std::string g_groleListHwid;
extern std::vector<std::string> g_groleListMembers;
extern int g_groleListPage;
extern unsigned long g_groleListStartTime;
extern bool g_grankListActive;
extern std::string g_grankListRequesterId;
extern std::string g_grankListGuildId;
extern std::string g_grankListHwid;
extern std::vector<std::string> g_grankListMembers;
extern int g_grankListPage;
extern unsigned long g_grankListStartTime;
extern void FinishGrankListAndPost();

// ?gmembers (Google Sheets roster) koleksiyonu (netrpc.cpp)
extern int g_gmPhase;
extern int g_gmPage;
extern int g_gmCount;
extern unsigned long g_gmPhaseAt;
extern int g_gmAstatsIdx;
extern bool g_gmAstatsWaiting;
extern char g_gmAstatsName[64];
extern bool g_gmGvipsWaiting;
extern int OnGmMembersPage(const char *szInfo, int *outButton, int *outItem);
extern void OnGmAstatsResult(const char *szInfo);
extern void OnGmVipsResult(const char *szInfo);

int RPC_ServerJoin = 137;
int RPC_ServerQuit = 138;
int RPC_InitGame = 139;
int RPC_ClientJoin = 25;
int RPC_NPCJoin = 54;
int RPC_Death = 53;
int RPC_RequestClass = 128;
int RPC_RequestSpawn = 129;
int RPC_SetInteriorId = 118;
int RPC_Spawn = 52;
int RPC_Chat = 101;
int RPC_EnterVehicle = 26;
int RPC_ExitVehicle = 154;
int RPC_DamageVehicle = 106;
int RPC_MenuSelect = 132;
int RPC_MenuQuit = 140;
int RPC_ScmEvent = 96;
int RPC_AdminMapTeleport = 255;
int RPC_WorldPlayerAdd = 32;
int RPC_WorldPlayerDeath = 166;
int RPC_WorldPlayerRemove = 163;
int RPC_WorldVehicleAdd = 164;
int RPC_WorldVehicleRemove = 165;
int RPC_SetCheckpoint = 107;
int RPC_DisableCheckpoint = 37;
int RPC_SetRaceCheckpoint = 38;
int RPC_DisableRaceCheckpoint = 39;
int RPC_UpdateScoresPingsIPs = 155;
int RPC_SvrStats = 102;
int RPC_GameModeRestart = 40;
int RPC_ConnectionRejected = 130;
int RPC_ClientMessage = 93;
int RPC_WorldTime = 94;
int RPC_Pickup = 95;
int RPC_DestroyPickup = 63;
int RPC_DestroyWeaponPickup = 97;
int RPC_Weather = 152;
int RPC_SetTimeEx = 255;
int RPC_ToggleClock = 30;
int RPC_ServerCommand = 50;
int RPC_PickedUpPickup = 131;
int RPC_PickedUpWeapon = 255;
int RPC_VehicleDestroyed = 136;
int RPC_DialogResponse = 62;
int RPC_PlayAudioStream = 41;
int RPC_StopAudioStream = 42;
int RPC_ClickPlayer = 23;
int RPC_PlayerUpdate = 60;
int RPC_ClickTextDraw = 83;
int RPC_MapMarker = 119;
int RPC_PlayerGiveTakeDamage = 115;
int RPC_EnterEditObject = 27;
int RPC_EditObject = 117;

int RPC_ScrSetSpawnInfo = 68;
int RPC_ScrSetPlayerTeam = 69;
int RPC_ScrSetPlayerSkin = 153;
int RPC_ScrSetPlayerName = 11;
int RPC_ScrSetPlayerPos = 12;
int RPC_ScrSetPlayerPosFindZ = 13;
int RPC_ScrSetPlayerHealth = 14;
int RPC_ScrPutPlayerInVehicle = 70;
int RPC_ScrRemovePlayerFromVehicle = 71;
int RPC_ScrSetPlayerColor = 72;
int RPC_ScrDisplayGameText = 73;
int RPC_ScrSetInterior = 156;
int RPC_ScrSetCameraPos = 157;
int RPC_ScrSetCameraLookAt = 158;
int RPC_ScrSetVehiclePos = 159;
int RPC_ScrSetVehicleZAngle = 160;
int RPC_ScrVehicleParams = 161;
int RPC_ScrSetCameraBehindPlayer = 162;
int RPC_ScrTogglePlayerControllable = 15;
int RPC_ScrPlaySound = 16;
int RPC_ScrSetWorldBounds = 17;
int RPC_ScrHaveSomeMoney = 18;
int RPC_ScrSetPlayerFacingAngle = 19;
int RPC_ScrResetMoney = 20;
int RPC_ScrResetPlayerWeapons = 21;
int RPC_ScrGivePlayerWeapon = 22;
int RPC_ScrRespawnVehicle = 255;
int RPC_ScrLinkVehicle = 65;
int RPC_ScrSetPlayerArmour = 66;
int RPC_ScrDeathMessage = 55;
int RPC_ScrSetMapIcon = 56;
int RPC_ScrDisableMapIcon = 144;
int RPC_ScrSetWeaponAmmo = 145;
int RPC_ScrSetGravity = 146;
int RPC_ScrSetVehicleHealth = 147;
int RPC_ScrAttachTrailerToVehicle = 148;
int RPC_ScrDetachTrailerFromVehicle = 149;
int RPC_ScrCreateObject = 44;
int RPC_ScrSetObjectPos = 45;
int RPC_ScrSetObjectRotation = 46;
int RPC_ScrDestroyObject = 47;
int RPC_ScrCreateExplosion = 79;
int RPC_ScrShowNameTag = 80;
int RPC_ScrMoveObject = 99;
int RPC_ScrStopObject = 122;
int RPC_ScrNumberPlate = 123;
int RPC_ScrTogglePlayerSpectating = 124;
int RPC_ScrSetPlayerSpectating = 255;
int RPC_ScrPlayerSpectatePlayer = 126;
int RPC_ScrPlayerSpectateVehicle = 127;
int RPC_ScrRemoveComponent = 57;
int RPC_ScrForceSpawnSelection = 74;
int RPC_ScrAttachObjectToPlayer = 75;
int RPC_ScrInitMenu = 76;
int RPC_ScrShowMenu = 77;
int RPC_ScrHideMenu = 78;
int RPC_ScrSetPlayerWantedLevel = 133;
int RPC_ScrShowTextDraw = 134;
int RPC_ScrHideTextDraw = 135;
int RPC_ScrEditTextDraw = 105;
int RPC_ScrAddGangZone = 108;
int RPC_ScrRemoveGangZone = 120;
int RPC_ScrFlashGangZone = 121;
int RPC_ScrStopFlashGangZone = 85;
int RPC_ScrApplyAnimation = 86;
int RPC_ScrClearAnimations = 87;
int RPC_ScrSetSpecialAction = 88;
int RPC_ScrEnableStuntBonus = 104;
int RPC_ScrSetFightingStyle = 89;
int RPC_ScrSetPlayerVelocity = 90;
int RPC_ScrSetVehicleVelocity = 91;
int RPC_ScrToggleWidescreen = 255;
int RPC_ScrSetVehicleTireStatus = 255;
int RPC_ScrSetPlayerDrunkLevel = 35;
int RPC_ScrDialogBox = 61;
int RPC_ScrCreate3DTextLabel = 36;
int RPC_ScrSetWarGroup = 175;

// Anahtar kelimeden sonraki ilk tam sayiyi cikarir (ornek: "Zone: 5" -> 5)
static int ExtractNumberAfter(const char *pszText) {
    if (!pszText) return -1;
    for (const char *p = pszText; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            int val = 0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                p++;
            }
            return val;
        }
    }
    return -1;
}

// /pnetstats dialog g├vdesinden "Packetloss: X%" de─serini ├g─▒kar─▒r
static double ExtractPacketLoss(const char *szText) {
    if (!szText) return -1.0;
    int len = (int)strlen(szText);

    const char *keys[] = { "loss", "lost", "packet", "paket", "kay", "ploss" };
    int bestPos = -1;
    for (int k = 0; k < 6; k++) {
        const char *p = strstr(szText, keys[k]);
        if (p) {
            int pos = (int)(p - szText);
            if (bestPos == -1 || pos < bestPos) bestPos = pos;
        }
    }

    int start, end;
    if (bestPos == -1) { start = 0; end = len; }
    else { start = bestPos - 20; if (start < 0) start = 0; end = bestPos + 30; if (end > len) end = len; }

    for (int i = start; i < end; i++) {
        if (szText[i] == '%') {
            int j = i - 1;
            while (j >= start && ((szText[j] >= '0' && szText[j] <= '9') || szText[j] == '.' || szText[j] == ','))
                j--;
            if (j + 1 < i) {
                char num[32]; int n = 0;
                for (int q = j + 1; q < i && n < 31; q++) num[n++] = szText[q];
                num[n] = 0;
                return atof(num);
            }
        }
    }
    return -1.0;
}

// Buyuk/kucuk harf bagimsiz strstr: /stats diyalo─su "- FPS:\t\t194" buyuk harf
// yazar; eski strstr("fps") bulamiyordu -> ?pl cevabi hep "FPS: 0" cikiyordu.
static const char *StrStrI(const char *szHay, const char *szNeed) {
    if (!szHay || !szNeed) return NULL;
    size_t n = strlen(szNeed);
    for (; *szHay; szHay++) {
        if (_strnicmp(szHay, szNeed, n) == 0) return szHay;
    }
    return NULL;
}

// Genel sayi cikarma: metinde key'e yakin ilk sayiyi dondurur (yoksa -1.0).
// Ornek: "FPS: 60" -> 60, "Ping 45 ms" -> 45. Once ileri, olmazsa geriye arar.
// Key aramasi CASE-INSENSITIVE'tir (FPS/fps/Fps ayni sonuc).
static double ExtractNumberNear(const char *szText, const char *key) {
    if (!szText || !key) return -1.0;
    int len = (int)strlen(szText);
    const char *p = StrStrI(szText, key);
    if (!p) return -1.0;

    const char *q = p - 30; if (q < szText) q = szText;
    const char *end = p + (int)strlen(key) + 30; if (end > szText + len) end = szText + len;

    for (const char *r = p + (int)strlen(key); r < end; r++) {
        if (*r >= '0' && *r <= '9') {
            const char *s = r;
            while (s > q && ((*(s - 1) >= '0' && *(s - 1) <= '9') || *(s - 1) == '.')) s--;
            double d = atof(s);
            if (d >= 0.0) return d;
            break;
        }
    }
    for (const char *r = p - 1; r >= q; r--) {
        if (*r >= '0' && *r <= '9') {
            const char *s = r;
            while (s > q && ((*(s - 1) >= '0' && *(s - 1) <= '9') || *(s - 1) == '.')) s--;
            double d = atof(s);
            if (d >= 0.0) return d;
            break;
        }
    }
    return -1.0;
}

// netrpc.cpp'deki ScrDialogBox'tan cagrilir: dialog dogru decode edildikten sonra
// /pnetstats ve /gstats yakalamasini yapar. Yakaladiysa 1, yakalamadiysa 0 dondurur.
int HandleCapturedDialog(int wDialogID, const char* szTitle, const char* szInfo) {
#ifdef RAKSAMP_CLIENT
    if (!g_isEngineActive || g_sessionToken.empty()) return 0;

    // -------------------------------------------------------------
    // 1. G─░R─░┼S (LOGIN) EKRANI KORUMASI
    // Giri┼s ekran─▒ geldi─sinde diyalogu KAPANMAKTAN korur (0 d├nd├╝r├╝r).
    // -------------------------------------------------------------
    if (szTitle && (strstr(szTitle, "Login") || strstr(szTitle, "Giris") || strstr(szTitle, "Giri┼s") || strstr(szTitle, "Authentication"))) {
        Log("[LOGIN] Giris ekrani algilandi (style=%d, baslik='%s', btn1='%s' btn2='%s'). Bilgi: %.200s",
            sampDialog.bDialogStyle, szTitle, sampDialog.szButton1, sampDialog.szButton2, szInfo ? szInfo : "");
        // UIF Login dialogu: kullanici diyalogda sifreyi elle yazip Login'e basar.
        // Dialogu acik birak (kapatma) — tmp hesaba atilmasin diye spawn akisi bekler.
        g_loginPending = true;
        g_loginPendingUntil = 0; // dialog geldi: kullanici girene kadar beklenir (timeout yok)
        std::cout << "[INFO] Giris ekrani algilandi, diyalog kapatilmadi.\n";
        return 0; // 0 d├nerek diyalogu kapatmaz, RakSAMP'─▒n normal ak─▒┼s─▒na b─▒rak─▒r
    }

    // -------------------------------------------------------------
    // 1b. /changepass / /email INPUT diyalogu (PM'den gelen guvenli komut)
    // PM'de ?changepass <yeni> / ?email <adres> yazilinca UIF input menusu acar;
    // buraya deger yazilip submit edilir. Login/gstats inputlariyla karismasin.
    // -------------------------------------------------------------
    if (g_secCmdPending) {
        if (GetTickCount() > g_secCmdAt) {
            g_secCmdPending = false;
            Log("[SEC] Input diyalogu beklerken zaman asimi.");
            return 0;
        }
        // Pencere icindeki input diyaloglari doldurulur (max 2: ilk + olası onay/tekrar).
        if (!g_loginPending && !g_waitingForGStats && g_secCmdCount < 2 &&
            (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT || sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD)) {
            Log("[SEC] %s input diyalogu yakalandi ('%s'), deger yaziliyor: '%s'",
                g_secCmdIsEmail ? "/email" : "/changepass", szTitle, g_secCmdValue);
            RakNet::BitStream bsResp;
            bsResp.Write((uint16_t)wDialogID);
            bsResp.Write((uint8_t)1);
            bsResp.Write((uint16_t)0xFFFF);
            size_t nLen = strlen(g_secCmdValue);
            bsResp.Write((uint16_t)nLen);
            if (nLen > 0) bsResp.Write(g_secCmdValue, nLen);
            pRakClient->RPC(&RPC_DialogResponse, &bsResp, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            g_secCmdCount++;
            Log("[SEC] %s degeri submit edildi (%d/2).", g_secCmdIsEmail ? "/email" : "/changepass", g_secCmdCount);
            if (g_secCmdCount >= 2) g_secCmdPending = false;
            return 1;
        }
    }

    // -------------------------------------------------------------
    // 2. /pnetstats YAKALAMASI (?pl)
    // -------------------------------------------------------------
    if (g_waitingForPNetStats) {
        if (GetTickCount() - g_pnetstatsReqTime > 15000) {
            g_waitingForPNetStats = false;
            return 0;
        }

        double dLoss = ExtractPacketLoss(szInfo);
        double dFps = ExtractNumberNear(szInfo, "fps");
        double dPing = ExtractNumberNear(szInfo, "ping");
        // /stats <id> dialogu FPS/ping iceriyorsa cache'e yaz: ?pl cevabinda bu taze
        // veriler gosterilir (kullanici: "stats'tan fps+ping cek, cache'e save, ?pl'de yaz").
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS) {
            if (dFps >= 0.0) {
                g_playerCache[g_plTargetId].iFps = (int)dFps;
                g_playerCache[g_plTargetId].dwTime = GetTickCount();
                Log("[PNETSTATS] /stats'tan FPS cache'e yazildi: hedef %d, fps %d", g_plTargetId, (int)dFps);
            }
            if (dPing >= 0.0) {
                g_playerCache[g_plTargetId].iPing = (int)dPing;
                g_playerCache[g_plTargetId].dwTime = GetTickCount();
                Log("[PNETSTATS] /stats'tan Ping cache'e yazildi: hedef %d, ping %d", g_plTargetId, (int)dPing);
            }
        }
        // Erteleme kontrolu merge ONCESI yapilir: cache merge boslugu doldurup
        // (cache dLoss=0.0) ertelemeyi engellememeli. (Erteleme amaci: taze
        // Stats-post'tan gercek FPS almak; cache bayat/deger yok olabilir.)
        bool bDialogEmpty = (dLoss < 0.0);
        if (dPing < 0.0 && g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS)
            dPing = (double)playerInfo[g_plTargetId].dwPing; // scoreboard ping'i

        // UIF /pnetstats FPS icermez (FPS: --) ve kullaniciya ait loss/ping dialogda
        // olmayabilir. "Stats ->" grup mesajlarindan doldurulan cache'ten eksikleri
        // tamamla (netrpc.cpp doldurur, player_cache.txt'ye kalici yazar).
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS && g_playerCache[g_plTargetId].dwTime != 0) {
            if (dLoss < 0.0 && g_playerCache[g_plTargetId].dLoss >= 0.0)
                dLoss = g_playerCache[g_plTargetId].dLoss;
            if (dFps < 0.0 && g_playerCache[g_plTargetId].iFps >= 0)
                dFps = (double)g_playerCache[g_plTargetId].iFps;
            if (dPing < 0.0 && g_playerCache[g_plTargetId].iPing >= 0)
                dPing = (double)g_playerCache[g_plTargetId].iPing;
        }

        Log("[PNETSTATS-DATA] [%s] %.1500s", szTitle ? szTitle : "?", szInfo);

        // UIF /pnetstats "Network Active: 1" gibi BOS doner (loss/fps yok). Asil
        // veri /pnetstats'in tetikledigi "Player [X] Stats ->" grup mesajindadir.
        // Dialogda loss yoksa cevabi O yakalayiciya birak: flag ACIK kalir, dialogu
        // kapat; Stats-post gelince netrpc.cpp taze veriyle cevaplar (cache yoluyla
        // aninda eski FPS gostermek yerine). Zaman asimini ProcessZoneWarAutoJoin
        // toparlar (cache fallback'li).
        if (bDialogEmpty) {
            Log("[PNETSTATS] Dialog veri icermiyor, 'Stats ->' bekleniyor (hedef %d).", g_plTargetId);
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)0);        // Button 0 (─░ptal/Kapat)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint8_t)0);        // Input Text Length
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            std::cout << "[AUTOCLOSE] pnetstats (bos) diyalogu kapatildi, Stats-> bekleniyor\n";
            return 1; // g_waitingForPNetStats ACIK kalir
        }

        g_waitingForPNetStats = false;

        char szResult[192];
        // Hedef adi: canli grup basligindan ogrenilen ad -> cache'teki ad -> "Player <id>".
        // Botun kendi adi (g_szNickName) SADECE hedef botun kendisiyse kullanilir; ?pl <id>
        // cevabi "hakixld's PL" gibi yanlis isimle donemez.
        char szFallbackName[32];
        const char *pPlName = "Player ?";
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS) {
            if (playerInfo[g_plTargetId].szPlayerName[0]) {
                pPlName = playerInfo[g_plTargetId].szPlayerName;
            } else if (g_playerCache[g_plTargetId].szName[0]) {
                pPlName = g_playerCache[g_plTargetId].szName;
            } else if (g_plTargetId == (int)g_myPlayerID) {
                pPlName = g_szNickName;
            } else {
                sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", g_plTargetId);
                pPlName = szFallbackName;
            }
        }
        if (dLoss >= 0.0) {
            unsigned int uiFps = (dFps >= 0.0 && dFps <= 1000.0) ? (unsigned int)dFps : 0;
            int iPing = (dPing >= 0.0 && dPing <= 30000.0) ? (int)dPing : -1;
            char szFps[16], szPing[16];
            sprintf_s(szFps, sizeof(szFps), "%u", uiFps);
            sprintf_s(szPing, sizeof(szPing), "%d", iPing);
            if (iPing < 0) strcpy(szPing, "--");
            if (dFps < 0.0) strcpy(szFps, "--");
            sprintf_s(szResult, sizeof(szResult), "!%s's PL: %g%% | FPS: %s | Ping: %s ms",
                pPlName, dLoss, szFps, szPing);
        }
        else {
            sprintf_s(szResult, sizeof(szResult), "!Packetloss bulunamadi.");
        }

        std::cout << "[pnetstats yakalandi] " << szResult << "\n";

        // Sonucu sadece OYUNDA goster; Discord koprusune yansitma
        // (! ile basladigi icin kopruye duserdi; echo bastirilarak onlenir).
        g_bridgeSuppressEcho++;
        sendChat(szResult);

        // pnetstats diyalogunu ekrana ├gizdirmeden kapat (RPC 62)
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)0);        // Button 0 (─░ptal/Kapat)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        std::cout << "[AUTOCLOSE] pnetstats diyalogu kapatildi!\n";
        return 1;
    }

    // -------------------------------------------------------------
    // 3. /gstats YAKALAMASI (?gst - zone + aktif oyuncu sayisi)
    // -------------------------------------------------------------
    if (g_waitingForGst) {
        if (g_gstReqTime != 0 && GetTickCount() - g_gstReqTime > 20000) {
            g_waitingForGst = false;
            Log("[GST] Bekleme zamana┼s─▒m─▒, flag s─▒f─▒rland─▒.");
            return 0;
        }

        if (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT || sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD) {
            const char *pszInput = "";
            size_t nInputLen = 0;
            Log("[GST] ID giris dialogu algilandi, bos submit gonderiliyor (kendi grubu)...");
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint16_t)nInputLen); // Input Text Length
            if(nInputLen > 0)
                bsResponse.Write(pszInput, nInputLen);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }

        g_waitingForGst = false;
        Log("[GST] Sonuc dialogu yakalandi. Zone + oyuncu sayisi cikariliyor...");
        // Dialogu TAM logla: Log buffer'i 512 byte ile sinirli, bu yuzden parcala.
        {
            const char *pFull = szInfo ? szInfo : "";
            size_t nFull = strlen(pFull);
            for(size_t off = 0; off < nFull; off += 440)
            {
                char szChunk[512];
                size_t nChunk = (nFull - off) < 440 ? (nFull - off) : 440;
                memcpy(szChunk, pFull + off, nChunk);
                szChunk[nChunk] = 0;
                Log("[GST-DATA] %s", szChunk);
            }
        }

        // Dialog formati (logdan ornek):
        //   {00FF00}Zones:{FFFFFF}
        //   1. # 2
        //   2. # 13
        //   ...
        // Zone sayisi = "Zones:" basligi altindaki liste satirlari (# karakteri).
        // (Eski kod "Zone" sonrasi ilk sayiyi aliyordu -> satir numarasi 1 okunuyordu.)
        int nZones = 0, nPlayers = -1;
        const char *pszInfo = szInfo ? szInfo : "";
        const char *pZoneSec = strstr(pszInfo, "Zones:");
        if(!pZoneSec) pZoneSec = strstr(pszInfo, "Zone:");
        if(pZoneSec)
        {
            const char *p = pZoneSec;
            while((p = strchr(p, '#')) != NULL)
            {
                nZones++;
                p++;
            }
        }
        if(nZones == 0)
        {
            const char *pZone = strstr(pszInfo, "Zone");
            if(pZone) nZones = ExtractNumberAfter(pZone + 4);
        }

        // Aktif oyuncu sayisi: "Online Members:" basligi altindaki liste satirlari.
        // ornek:
        //   {00FF00}Online Members:{FFFFFF}
        //   1 - .Punisher(27) - King's Hand
        //   2 - washe(28) - Underboss
        //   ... -> her satir bir aktif uye
        const char *pAktif = strstr(pszInfo, "Online Members:");
        if(pAktif)
        {
            nPlayers = 0;
            const char *p = pAktif + 15; // "Online Members:" sonrasi
            while(*p)
            {
                const char *pNext = strchr(p, '\n');
                const char *pLineEnd = pNext ? pNext : (p + strlen(p));
                size_t nLen = (size_t)(pLineEnd - p);
                const char *pTrim = p;
                // Satir basini trimle: renk kodunu ({RRGGBB}) ve bosluk/satirsonu karakterlerini atla.
                // Djnat: '\n' bir satir sonudur, onu bosluk gibi atlamak bir sonraki satira akar ve
                // ilk uyeyi 2 kez saydirir. Bu yuzden satir sonunu (pLineEnd) sinir al.
                while(pTrim < pLineEnd)
                {
                    if(*pTrim == '{')
                    {
                        while(pTrim < pLineEnd && *pTrim != '}') pTrim++;
                        if(pTrim < pLineEnd && *pTrim == '}') pTrim++;
                    }
                    else if(*pTrim == ' ' || *pTrim == '\t' || *pTrim == '\r')
                        pTrim++;
                    else
                        break;
                }
                // Rakamla baslayan satirlar liste girdileridir ("1 - ...")
                if(pTrim < pLineEnd && *pTrim >= '0' && *pTrim <= '9')
                    nPlayers++;
                else if(pTrim >= pLineEnd)
                    { /* bos / sadece renk kodu olan satir, devam et */ }
                else
                    break; // rakamla baslamayan satir = sonraki bolum basligi
                if(!pNext) break;
                p = pNext + 1;
            }
        }
        else {
            // Fallback: "Online players", "Online", "Active", "Aktif", "Player", "Oyuncu"
            const char *pAktif2 = strstr(pszInfo, "Online players");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Active players");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Online");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Active");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Aktif");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Player");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Oyuncu");
            if(pAktif2)
            {
                const char *pNum = pAktif2;
                while(*pNum && !(*pNum >= '0' && *pNum <= '9')) pNum++;
                if(*pNum) nPlayers = atoi(pNum);
            }
        }

        // Sonucu oyunda grup sohbeti olarak goster (! ile), Discord'a gitmesin
        g_bridgeSuppressEcho++;
        if(nZones > 0) {
            char szResult[256];
            if(nPlayers >= 0)
                sprintf_s(szResult, "!Zone: %d | Aktif: %d", nZones, nPlayers);
            else
                sprintf_s(szResult, "!Zone: %d", nZones);
            Log("[GST] Sonuc: %s", szResult);
            sendChat(szResult);
        } else {
            Log("[GST] Zone bilgisi bulunamadi, ham veri gonderiliyor.");
            char szRaw[256];
            sprintf_s(szRaw, "!%s", pszInfo);
            sendChat(szRaw);
        }
        g_bridgeSuppressEcho--;

        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);
        bsResponse.Write((uint16_t)0xFFFF);
        bsResponse.Write((uint8_t)0);
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
        Log("[GST] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    // -------------------------------------------------------------
    // 3b. /admins YAKALAMASI (Discord /admins slash komutu k├pr├╝s├╝)
    // -------------------------------------------------------------
    if (g_waitingForAdmins) {
        if (g_adminsReqTime != 0 && GetTickCount() - g_adminsReqTime > 20000) {
            g_waitingForAdmins = false;
            Log("[ADMINS] Bekleme zamana┼s─▒m─▒, flag s─▒f─▒rland─▒.");
            return 0;
        }

        g_waitingForAdmins = false;

        Log("[ADMINS] Sonuc dialogu yakalandi. Discord'a gonderiliyor...");
        Log("[ADMINS-DATA] %.250s", szInfo);

        // Security.cpp ├╝zerinden Discord VDS'e aktarma
        SendAdminsToDiscord(szInfo);

        // Sonuc dialogunu ekrana cizdirmeden kapat
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);        // Button 1 (OK)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        Log("[ADMINS] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    // -------------------------------------------------------------
    // 3.5 /groups YAKALAMASI (?zgrp grup ogrenme harvester'i)
    // Sunucu /groups cevabini LIST dialogu olarak acar; her satir "ID. isim".
    // OnGroupsListCaptured parse edip /gstats <ID> zincirini baslatir.
    // -------------------------------------------------------------
    if (g_waitingForGroups) {
        if (g_groupsReqTime != 0 && GetTickCount() - g_groupsReqTime > 15000) {
            g_waitingForGroups = false;
            Log("[GHP] /groups bekleme zamana┼s─▒m─▒, flag s─▒f─▒rland─▒.");
            return 0;
        }
        // /groups cevabi LIST dialog style'inda gelir (veya INFO). Ham olarak gecir:
        // OnGroupsListCaptured kendi parse kuraliyla calisir (format degisikligine dayanikli).
        // UIF iki dialog acar: once "UIF - Group Zone Info" (MSGBOX, kendi /gzinfo'ya benzer -
        // bu /groups cevabi DEGIL, beklemede kal), sonra gercek liste "Online Groups"
        // (TABLIST_HEADERS). Yalnizca liste stili yakalanir.
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT && g_groupsReqTime != 0)) {
            Log("[GHP] /groups cevabi yakalandi (style %d, baslik: %s).", sampDialog.bDialogStyle, szTitle);
            g_waitingForGroups = false;
            OnGroupsListCaptured(szInfo);

            // Cevap dialogunu kapatiyor (ekrana cizdirme)
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint8_t)0);        // Input Text Length
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }
        // Farkli bir dialog geldi (ID giris vs.): bu /groups cevabi degil, beklemede kal
    }

    // -------------------------------------------------------------
    // 3.5b ?gmembers KOLEKSIYONU (Google Sheets roster): faz 1 uye listesi sayfalari
    // -------------------------------------------------------------
    if (g_gmPhase == 1) {
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            sampDialog.bDialogStyle == DIALOG_STYLE_MSGBOX) {
            int iBtn = 1, iItem = -1;
            int r = OnGmMembersPage(szInfo, &iBtn, &iItem);
            Log("[GM] Uye listesi sayfasi (style %d) -> %s (%d uye)", sampDialog.bDialogStyle,
                r == 1 ? "sonraki sayfa" : "bitti", g_gmCount);

            RakNet::BitStream bsGM;
            bsGM.Write((uint16_t)wDialogID);
            if (r == 1) {
                bsGM.Write((uint8_t)iBtn);
                bsGM.Write((uint16_t)(iItem >= 0 ? iItem : 0xFFFF));
            } else {
                bsGM.Write((uint8_t)1);
                bsGM.Write((uint16_t)0xFFFF);
            }
            bsGM.Write((uint8_t)0);
            pRakClient->RPC(&RPC_DialogResponse, &bsGM, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            if (r == 1) g_gmPage++;
            else {
                Log("[GM] Uye listesi tamam: %d uye. Faz 2: /astats basliyor.", g_gmCount);
                g_gmPhase = 2;
                g_gmAstatsIdx = 0;
                g_gmAstatsWaiting = false;
                g_gmPhaseAt = GetTickCount();
            }
            return 1;
        }
        // Uye listesi dialogu degil: beklemede kal (zaman asimini ProcessGmembers isler)
    }

    // -------------------------------------------------------------
    // 3.5c ?gmembers faz 3: /gvips dialogu -> VIP parse, sonra gonderim
    // -------------------------------------------------------------
    if (g_gmPhase == 3 && g_gmGvipsWaiting) {
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            sampDialog.bDialogStyle == DIALOG_STYLE_MSGBOX) {
            Log("[GM] /gvips dialogu yakalandi (style %d), VIP'ler parse ediliyor.", sampDialog.bDialogStyle);
            OnGmVipsResult(szInfo);
            RakNet::BitStream bsGV;
            bsGV.Write((uint16_t)wDialogID);
            bsGV.Write((uint8_t)1);
            bsGV.Write((uint16_t)0xFFFF);
            bsGV.Write((uint8_t)0);
            pRakClient->RPC(&RPC_DialogResponse, &bsGV, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }
    }

    // -------------------------------------------------------------
    // 3.6 /gmembers YAKALAMASI (?zsnip yetki kontrolu)
    // Bot + komutu atan kullanici Co-Leader mi diye /gmembers listesini parse eder.
    // -------------------------------------------------------------
    if (g_waitingForGmembers) {
        if (g_gmembersReqTime != 0 && GetTickCount() - g_gmembersReqTime > 30000) {
            g_waitingForGmembers = false;
            Log("[GHP] /gmembers bekleme zamana┼s─▒m─▒, flag s─▒f─▒rland─▒.");
            return 0;
        }
        // UIF /gmembers cevabi LIST/TABLIST olabilir; bazi sunucularda MSGBOX.
        // SAYFALI: her sayfa OnGmembersPage'e beslenir; GM_NEXT donerse istenen
        // buton/item ile sonraki sayfa istenir (uye sayisina gore sayfa degisir).
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            sampDialog.bDialogStyle == DIALOG_STYLE_MSGBOX) {
            int iBtn = 0, iItem = -1;
            int r = OnGmembersPage(szInfo, &iBtn, &iItem);
            Log("[GHP] /gmembers sayfa yakalandi (style %d, baslik: %s) -> %s",
                sampDialog.bDialogStyle, szTitle, (r == 1) ? "sonraki sayfa" : "finaliz");

            RakNet::BitStream bsR;
            bsR.Write((uint16_t)wDialogID);
            if (r == 1) {
                bsR.Write((uint8_t)iBtn);          // ">>" satiri sec (1) veya '>>' butonu (0)
                bsR.Write((uint16_t)(iItem >= 0 ? iItem : 0xFFFF));
            } else {
                bsR.Write((uint8_t)1);             // bitis: OK ile kapat
                bsR.Write((uint16_t)0xFFFF);
            }
            bsR.Write((uint8_t)0);                 // Input Text Length
            pRakClient->RPC(&RPC_DialogResponse, &bsR, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            if (r == 0)
                OnGmembersFinalize();              // sonuc uygula (baslat/red)
            // r==1: g_waitingForGmembers ACIK kalir, siradaki sayfa diyalo─su buraya gelir
            return 1;
        }
        // Farkli bir dialog geldi: /gmembers cevabi degil, beklemede kal
    }

    // -------------------------------------------------------------
    // 3.7a /grole LIST FETCH (interaktif menu icin tum uyeleri topla)
    // -------------------------------------------------------------
    if (g_groleListActive) {
        if (g_groleListStartTime != 0 && GetTickCount() - g_groleListStartTime > 20000) {
            Log("[GROLE-LIST] Zaman asimi, %d uye ile gonderiliyor.", (int)g_groleListMembers.size());
            FinishGroleListAndPost();
            return 0;
        }
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) {
            Log("[GROLE-LIST] Dialog yakalandi (page=%d baslik='%s')", g_groleListPage, szTitle);
            Log("[GROLE-LIST-DATA] %.1600s", szInfo ? szInfo : "");
            { size_t nD = szInfo ? strlen(szInfo) : 0; if(nD > 700) Log("[GROLE-LIST-TAIL] %.700s", szInfo + nD - 700); }
            int nextRow = -1;
            const char *pL = szInfo;
            int row = 0;
            while(pL && *pL){
                const char *pE = strchr(pL, '\n');
                size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                if(nLen>0 && nLen<300){
                    char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                    char szClean[512]={0}; int c=0;
                    for(size_t i=0; szLine[i] && c<500; i++){
                        if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                        szClean[c++]=szLine[i];
                    } szClean[c]=0;
                    // trim
                    char *s = szClean; while(*s==' '||*s=='\t'||*s=='\r') s++;
                    char *e = s + strlen(s) -1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0;
                    // pagination marker
                    char szLow[512]={0}; for(int i=0; s[i] && i<500; i++) szLow[i]=tolower((unsigned char)s[i]);
                    if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki")){
                        nextRow = row;
                    } else if(s[0]){
                        // header skip
                        if(strstr(szLow, "id") && strstr(szLow, "name")) { /* header */ }
                        else {
                            // extract name: skip leading "1. " / "1: " etc, take first word
                            char *p = s;
                            while(*p && (*p<'0' || *p>'9')){ if(*p=='.'||*p==':'||*p=='-'||*p==' ') {p++; continue;} break; }
                            while(*p>='0' && *p<='9') p++;
                            while(*p=='.'||*p==':'||*p=='-'||*p==' '||*p=='\t') p++;
                            char name[64]={0}; int ni=0;
                            while(p[ni] && p[ni]!=' ' && p[ni]!='\t' && p[ni]!='-' && ni<63){ name[ni]=p[ni]; ni++; }
                            name[ni]=0;
                            // fallback: if no word extracted, use whole trimmed line first token
                            if(!name[0]){
                                int k=0; while(s[k] && s[k]!=' ' && s[k]!='\t' && k<63){ name[k]=s[k]; k++; } name[k]=0;
                            }
                            if(name[0] && strchr(name, '>')==NULL){
                                // avoid duplicates
                                bool dup=false; for(auto &m: g_groleListMembers) if(_stricmp(m.c_str(), name)==0) dup=true;
                                if(!dup) g_groleListMembers.push_back(std::string(name));
                            }
                        }
                    }
                }
                row++;
                if(!pE) break;
                pL = pE + 1;
            }
            RakNet::BitStream bs;
            bs.Write((uint16_t)wDialogID);
            if(nextRow >= 0 && g_groleListPage < 10){
                Log("[GROLE-LIST] Sonraki sayfa (%d) -> satir %d, toplanan %d", g_groleListPage+1, nextRow, (int)g_groleListMembers.size());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)nextRow);
                bs.Write((uint8_t)0);
                g_groleListPage++;
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                return 1;
            } else if(g_groleListPage < 10 && strstr(sampDialog.szButton2, ">>")){
                // '>>' butonuna tikla: sag buton (0) + secili ilk satir (0).
                // 0+0xFFFF = ESC = iptal (sunucu sayfa acmaz).
                Log("[GROLE-LIST] '>>' butonu tiklanıyor, sayfa (%d) -> toplanan %d", g_groleListPage+1, (int)g_groleListMembers.size());
                bs.Write((uint8_t)0);
                bs.Write((uint16_t)0);
                bs.Write((uint8_t)0);
                g_groleListPage++;
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                return 1;
            } else {
                Log("[GROLE-LIST] Liste tamamlandi (%d uye)", (int)g_groleListMembers.size());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                FinishGroleListAndPost();
                return 1;
            }
        }
    }
    // 3.7b /grank LIST FETCH (uye listesi, rank secimi oncesi)
    if (g_grankListActive) {
        if (g_grankListStartTime != 0 && GetTickCount() - g_grankListStartTime > 20000) {
            Log("[GRANK-LIST] Zaman asimi, %d uye ile gonderiliyor.", (int)g_grankListMembers.size());
            FinishGrankListAndPost();
            return 0;
        }
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) {
            Log("[GRANK-LIST] Dialog yakalandi (page=%d baslik='%s')", g_grankListPage, szTitle);
            Log("[GRANK-LIST-DATA] %.500s", szInfo ? szInfo : "");
            int nextRow = -1;
            const char *pL = szInfo;
            int row = 0;
            while(pL && *pL){
                const char *pE = strchr(pL, '\n');
                size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                if(nLen>0 && nLen<300){
                    char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                    char szClean[512]={0}; int c=0;
                    for(size_t i=0; szLine[i] && c<500; i++){
                        if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                        szClean[c++]=szLine[i];
                    } szClean[c]=0;
                    char *s = szClean; while(*s==' '||*s=='\t'||*s=='\r') s++;
                    char *e = s + strlen(s) -1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0;
                    char szLow[512]={0}; for(int i=0; s[i] && i<500; i++) szLow[i]=tolower((unsigned char)s[i]);
                    if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki")){
                        nextRow = row;
                    } else if(s[0]){
                        if(strstr(szLow, "id") && strstr(szLow, "name")) { }
                        else {
                            char *p = s;
                            while(*p && (*p<'0' || *p>'9')){ if(*p=='.'||*p==':'||*p=='-'||*p==' ') {p++; continue;} break; }
                            while(*p>='0' && *p<='9') p++;
                            while(*p=='.'||*p==':'||*p=='-'||*p==' '||*p=='\t') p++;
                            char name[64]={0}; int ni=0;
                            while(p[ni] && p[ni]!=' ' && p[ni]!='\t' && p[ni]!='-' && ni<63){ name[ni]=p[ni]; ni++; }
                            name[ni]=0;
                            if(!name[0]){ int k=0; while(s[k] && s[k]!=' ' && s[k]!='\t' && k<63){ name[k]=s[k]; k++; } name[k]=0; }
                            if(name[0] && strchr(name, '>')==NULL){
                                bool dup=false; for(auto &m: g_grankListMembers) if(_stricmp(m.c_str(), name)==0) dup=true;
                                if(!dup) g_grankListMembers.push_back(std::string(name));
                            }
                        }
                    }
                }
                row++;
                if(!pE) break;
                pL = pE + 1;
            }
            RakNet::BitStream bs;
            bs.Write((uint16_t)wDialogID);
            if(nextRow >= 0 && g_grankListPage < 10){
                Log("[GRANK-LIST] Sonraki sayfa (%d) -> satir %d, toplanan %d", g_grankListPage+1, nextRow, (int)g_grankListMembers.size());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)nextRow);
                bs.Write((uint8_t)0);
                g_grankListPage++;
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                return 1;
            } else if(g_grankListPage < 10 && strstr(sampDialog.szButton2, ">>")){
                // '>>' butonuna tikla: sag buton (0) + secili ilk satir (0).
                // 0+0xFFFF = ESC = iptal (sunucu sayfa acmaz).
                Log("[GRANK-LIST] '>>' butonu tiklanıyor, sayfa (%d) -> toplanan %d", g_grankListPage+1, (int)g_grankListMembers.size());
                bs.Write((uint8_t)0);
                bs.Write((uint16_t)0);
                bs.Write((uint8_t)0);
                g_grankListPage++;
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                return 1;
            } else {
                Log("[GRANK-LIST] Liste tamamlandi (%d uye)", (int)g_grankListMembers.size());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                FinishGrankListAndPost();
                return 1;
            }
        }
    }

    // -------------------------------------------------------------
    // 3.7 /grole PAGINATED OTOMASYONU (Discord -> sayfali uye listesinde ara + rol sec)
    // /grole komutu: sayfali uye listesi (25/sayfa) -> hedefi bul -> tikla -> rol listesi -> sec
    // -------------------------------------------------------------
    if (g_groleActive) {
        if (g_groleStartTime != 0 && GetTickCount() - g_groleStartTime > 45000) {
            g_groleActive = false;
            g_grolePhase = 0;
            Log("[GROLE] Zaman asimi (45sn), otomasyon iptal.");
            return 0;
        }
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            (g_grolePhase == 2 && sampDialog.bDialogStyle == DIALOG_STYLE_MSGBOX)) {
            Log("[GROLE] Dialog yakalandi (phase=%d page=%d baslik='%s')", g_grolePhase, g_grolePage, szTitle);
            Log("[GROLE-DATA] %.1600s", szInfo ? szInfo : "");
            { size_t nD = szInfo ? strlen(szInfo) : 0; if(nD > 700) Log("[GROLE-DATA-TAIL] %.700s", szInfo + nD - 700); }
            Log("[GROLE-BTN] btn1='%s' btn2='%s' style=%d", sampDialog.szButton1, sampDialog.szButton2, sampDialog.bDialogStyle);
            // Hex debug: \n satirlari gercekten var mi, uye satirlari geliyor mu
            {
                char szHex[2200] = {0}; int h = 0;
                for(int i = 0; szInfo && szInfo[i] && h < 1098; i++)
                {
                    if(szInfo[i] == '\n'){ szHex[h++]='<'; szHex[h++]='N'; szHex[h++]='L'; szHex[h++]='>'; }
                    else if(szInfo[i] == '\t'){ szHex[h++]='<'; szHex[h++]='T'; szHex[h++]='A'; szHex[h++]='B'; }
                    else szHex[h++] = szInfo[i];
                }
                szHex[h] = 0;
                Log("[GROLE-DATA-HEX] %s", szHex);
            }
            // Rol diyaloğu geldi (baslik "UIF - Group Role"): uye cift tiklamasi basarili,
            // faz 2'ye gec ve rol sec. (Aday tiklarken gelen rol diyalogu hedef uyenindir:
            // adaylarin tamami hedefin satir/inputtext'iyle tikladi.)
            if(g_grolePhase == 1 && strstr(szTitle, "Role") && (strstr(szInfo, "Co-Leader") || strstr(szInfo, "Member")))
            {
                Log("[GROLE] Rol diyalogu algilandi ('%s'), faz 2'ye gecilir.", szTitle);
                g_grolePhase = 2;
                g_grolePage = 0;
            }
            if (g_grolePhase == 1) {
                // Uye listesi: hedefi ara
                int targetRow = -1, targetItem = -1, nextRow = -1, nextItem = -1;
                char firstCol[128] = {0}; // hedef satirin ilk sutunu (renksiz) — TABLIST inputtext
                const char *pL = szInfo;
                int row = 0;
                int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
                int dataRow = -1; // veri satiri indeksi 0-based: bos satirlar + baslik HARIC (sunucu listitem'i bu)
                while(pL && *pL){
                    const char *pE = strchr(pL, '\n');
                    size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                    if(nLen > 0 && nLen < 300){
                        if(row >= iHeaderRows) dataRow++;
                        char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                        // renk kodlarini temizle {RRGGBB}
                        char szClean[512]={0}; int c=0;
                        for(size_t i=0; szLine[i] && c<500; i++){
                            if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                            szClean[c++]=szLine[i];
                        } szClean[c]=0;
                        // lower
                        char szLow[512]={0};
                        for(int i=0; szClean[i] && i<500; i++) szLow[i]=tolower((unsigned char)szClean[i]);
                        // isim sutununu cikar: "12  washe\tProgrom\tLeader\tNow" -> "washe"
                        char *pN = szClean;
                        while(*pN==' '||*pN=='\t'||*pN=='\r') pN++;
                        while(*pN>='0' && *pN<='9') pN++;
                        while(*pN=='.'||*pN==':'||*pN=='-'||*pN==' '||*pN=='\t') pN++;
                        char name[64]={0}; int ni=0;
                        while(pN[ni] && pN[ni]!=' ' && pN[ni]!='\t' && pN[ni]!='\r' && ni<63){ name[ni]=pN[ni]; ni++; }
                        name[ni]=0;
                        // SADECE isim eslesmesi (renksiz, case-insensitive)
                        if(name[0] && !_stricmp(name, g_groleTarget.c_str())){
                            targetRow = row; targetItem = dataRow;
                            int fc = 0;
                            for(fc = 0; szClean[fc] && szClean[fc] != '\t' && fc < 127; fc++) firstCol[fc] = szClean[fc];
                            firstCol[fc] = 0;
                        }
                        if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki")){ nextRow = row; nextItem = dataRow; }
                    }
                    row++;
                    if(!pE) break;
                    pL = pE + 1;
                }
                RakNet::BitStream bs;
                bs.Write((uint16_t)wDialogID);
                if(targetRow >= 0){
                    Log("[GROLE] Hedef '%s' bulundu (satir %d, item %d).", g_groleTarget.c_str(), targetRow, targetItem);
                    StartGrCandidateClicks(targetRow, targetItem >= 0 ? targetItem : targetRow, (unsigned short)wDialogID, g_grolePage, firstCol);
                    return 1;
                } else if(nextRow >= 0 && g_grolePage < 10){
                    Log("[GROLE] Hedef bu sayfada yok, sonraki sayfa (%d) -> item %d", g_grolePage+1, nextItem);
                    bs.Write((uint8_t)1);
                    bs.Write((uint16_t)(nextItem >= 0 ? nextItem : nextRow));
                    bs.Write((uint8_t)0);
                    g_grolePage++;
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    return 1;
                } else if(g_grolePage < 10 && strstr(sampDialog.szButton2, ">>")){
                    // '>>' butonuna tikla: sag buton (0) + secili ilk satir (0).
                    // 0+0xFFFF = ESC = iptal (sunucu sayfa acmaz); gercek '>>' tiklamasi
                    // secili satirla gider (0+0xFFFF'i iptal sayar).
                    Log("[GROLE] Hedef bulunamadi, '>>' butonu tiklanıyor (sayfa %d)", g_grolePage+1);
                    bs.Write((uint8_t)0);
                    bs.Write((uint16_t)0);
                    bs.Write((uint8_t)0);
                    g_grolePage++;
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    return 1;
                }
                Log("[GROLE] Hedef '%s' bulunamadi, iptal.", g_groleTarget.c_str());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                g_groleActive = false;
                g_grolePhase = 0;
                return 1;
            } else if (g_grolePhase == 2) {
                g_grTargetItem = -1; g_grDlgId = 0; // uye tiklamalari durdu
                // (GUVENLIK notu: buraya yalnizca aday tiklama sonrasi gelen rol diyalogu
                // ile girilir — baslikta uye adi olmaz, kontrol gerekmez.)
                // Rol secimi: Member / Co-Leader / Leader / Owner (LIST, tek sutun)
                int roleRow = -1, roleItem = -1;
                const char *pL = szInfo;
                int row = 0;
                int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
                int dataRow = -1;
                char roleLow[64]={0};
                for(size_t i=0; i<g_groleRole.size() && i<63; i++) roleLow[i]=tolower((unsigned char)g_groleRole[i]);
                while(pL && *pL){
                    const char *pE = strchr(pL, '\n');
                    size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                    if(nLen>0 && nLen<300){
                        if(row >= iHeaderRows) dataRow++;
                        char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                        char szClean[512]={0}; int c=0;
                        for(size_t i=0; szLine[i] && c<500; i++){
                            if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                            szClean[c++]=szLine[i];
                        } szClean[c]=0;
                        char szLow[512]={0};
                        for(int i=0; szClean[i] && i<500; i++) szLow[i]=tolower((unsigned char)szClean[i]);
                        if(roleLow[0] && strstr(szLow, roleLow)){ roleRow = row; roleItem = dataRow; }
                    }
                    row++;
                    if(!pE) break;
                    pL = pE + 1;
                }
                RakNet::BitStream bs;
                bs.Write((uint16_t)wDialogID);
                if(roleRow >= 0){
                    Log("[GROLE] Rol '%s' bulundu (item %d), cift tik (1/2)...", g_groleRole.c_str(), roleItem);
                    bs.Write((uint8_t)1);
                    bs.Write((uint16_t)(roleItem >= 0 ? roleItem : roleRow));
                    BYTE rlen = (BYTE)g_groleRole.size();
                    bs.Write((uint8_t)rlen);
                    bs.Write(g_groleRole.c_str(), rlen);
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    g_grRoleDlgId = (unsigned short)wDialogID;
                    g_grRoleItem = roleItem >= 0 ? roleItem : roleRow;
                    g_grRoleInput[0] = 0;
                    strncpy_s(g_grRoleInput, sizeof(g_grRoleInput), g_groleRole.c_str(), sizeof(g_grRoleInput) - 1);
                    g_grRoleDblAt = GetTickCount() + 300;
                    g_grRoleIsGrank = false;
                    g_grRoleDblPending = true;
                    return 1;
                }
                Log("[GROLE] Rol '%s' bulunamadi, iptal.", g_groleRole.c_str());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                g_groleActive = false;
                g_grolePhase = 0;
                return 1;
            }
        }
    }
    // 3.8 /grank PAGINATED OTOMASYONU (uye listesi + rank secimi)
    if (g_grankActive) {
        if (g_grankStartTime != 0 && GetTickCount() - g_grankStartTime > 45000) {
            g_grankActive = false;
            g_grankPhase = 0;
            Log("[GRANK] Zaman asimi (45sn), otomasyon iptal.");
            return 0;
        }
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            (g_grankPhase == 2 && sampDialog.bDialogStyle == DIALOG_STYLE_MSGBOX)) {
            Log("[GRANK] Dialog yakalandi (phase=%d page=%d baslik='%s')", g_grankPhase, g_grankPage, szTitle);
            Log("[GRANK-DATA] %.400s", szInfo ? szInfo : "");
            // Rank diyalogu geldi (baslik "Group Role"/"Rank"): uye cift tiklamasi basarili, faz 2.
            if(g_grankPhase == 1 && (strstr(szTitle, "Role") || strstr(szTitle, "Rank")) && (strstr(szInfo, "Co-Leader") || strstr(szInfo, "Member")))
            {
                Log("[GRANK] Rank diyalogu algilandi ('%s'), faz 2'ye gecilir.", szTitle);
                g_grankPhase = 2;
                g_grankPage = 0;
            }
            if (g_grankPhase == 1) {
                int targetRow = -1, targetItem = -1, nextRow = -1, nextItem = -1;
                char firstColG[128] = {0}; // grank hedef satirin ilk sutunu
                const char *pL = szInfo;
                int row = 0;
                int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
                int dataRow = -1; // veri satiri indeksi 0-based: bos satirlar + baslik HARIC (sunucu listitem'i bu)
                while(pL && *pL){
                    const char *pE = strchr(pL, '\n');
                    size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                    if(nLen>0 && nLen<300){
                        if(row >= iHeaderRows) dataRow++;
                        char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                        char szClean[512]={0}; int c=0;
                        for(size_t i=0; szLine[i] && c<500; i++){
                            if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                            szClean[c++]=szLine[i];
                        } szClean[c]=0;
                        char szLow[512]={0};
                        for(int i=0; szClean[i] && i<500; i++) szLow[i]=tolower((unsigned char)szClean[i]);
                        // isim sutununu cikar: "12  washe\tProgrom\tLeader\tNow" -> "washe"
                        char *pN = szClean;
                        while(*pN==' '||*pN=='\t'||*pN=='\r') pN++;
                        while(*pN>='0' && *pN<='9') pN++;
                        while(*pN=='.'||*pN==':'||*pN=='-'||*pN==' '||*pN=='\t') pN++;
                        char name[64]={0}; int ni=0;
                        while(pN[ni] && pN[ni]!=' ' && pN[ni]!='\t' && pN[ni]!='\r' && ni<63){ name[ni]=pN[ni]; ni++; }
                        name[ni]=0;
                        // SADECE isim eslesmesi (renksiz, case-insensitive)
                        if(name[0] && !_stricmp(name, g_grankTarget.c_str())){
                            targetRow = row; targetItem = dataRow;
                            int fc = 0;
                            for(fc = 0; szClean[fc] && szClean[fc] != '\t' && fc < 127; fc++) firstColG[fc] = szClean[fc];
                            firstColG[fc] = 0;
                        }
                        if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki")){ nextRow = row; nextItem = dataRow; }
                    }
                    row++;
                    if(!pE) break;
                    pL = pE + 1;
                }
                RakNet::BitStream bs;
                bs.Write((uint16_t)wDialogID);
                if(targetRow >= 0){
                    Log("[GRANK] Hedef '%s' bulundu (satir %d, item %d).", g_grankTarget.c_str(), targetRow, targetItem);
                    StartGrCandidateClicks(targetRow, targetItem >= 0 ? targetItem : targetRow, (unsigned short)wDialogID, g_grankPage, firstColG);
                    return 1;
                } else if(nextRow >= 0 && g_grankPage < 10){
                    Log("[GRANK] Hedef bu sayfada yok, sonraki sayfa -> item %d", nextItem);
                    bs.Write((uint8_t)1);
                    bs.Write((uint16_t)(nextItem >= 0 ? nextItem : nextRow));
                    bs.Write((uint8_t)0);
                    g_grankPage++;
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    return 1;
                } else if(g_grankPage < 10 && strstr(sampDialog.szButton2, ">>")){
                    // '>>' butonuna tikla: sag buton (0) + secili ilk satir (0).
                    // 0+0xFFFF = ESC = iptal (sunucu sayfa acmaz).
                    Log("[GRANK] Hedef bulunamadi, '>>' butonu tiklanıyor (sayfa %d)", g_grankPage+1);
                    bs.Write((uint8_t)0);
                    bs.Write((uint16_t)0);
                    bs.Write((uint8_t)0);
                    g_grankPage++;
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    return 1;
                }
                Log("[GRANK] Hedef '%s' bulunamadi, iptal.", g_grankTarget.c_str());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                g_grankActive = false;
                g_grankPhase = 0;
                return 1;
            } else if (g_grankPhase == 2) {
                g_grTargetItem = -1; g_grDlgId = 0; // uye tiklamalari durdu
                // (Buraya yalnizca aday tiklama sonrasi gelen rank diyalogu ile girilir.)
                int rankRow = -1, rankItem = -1;
                const char *pL = szInfo;
                int row = 0;
                int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
                int dataRow = -1;
                char rankLow[64]={0};
                for(size_t i=0; i<g_grankRank.size() && i<63; i++) rankLow[i]=tolower((unsigned char)g_grankRank[i]);
                while(pL && *pL){
                    const char *pE = strchr(pL, '\n');
                    size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
                    if(nLen>0 && nLen<300){
                        if(row >= iHeaderRows) dataRow++;
                        char szLine[512]; memcpy(szLine, pL, nLen); szLine[nLen]=0;
                        char szClean[512]={0}; int c=0;
                        for(size_t i=0; szLine[i] && c<500; i++){
                            if(szLine[i]=='{' && szLine[i+7]=='}'){ i+=7; continue; }
                            szClean[c++]=szLine[i];
                        } szClean[c]=0;
                        char szLow[512]={0};
                        for(int i=0; szClean[i] && i<500; i++) szLow[i]=tolower((unsigned char)szClean[i]);
                        if(rankLow[0] && strstr(szLow, rankLow)){ rankRow = row; rankItem = dataRow; }
                    }
                    row++;
                    if(!pE) break;
                    pL = pE + 1;
                }
                RakNet::BitStream bs;
                bs.Write((uint16_t)wDialogID);
                if(rankRow >= 0){
                    Log("[GRANK] Rank '%s' bulundu (item %d), cift tik (1/2)...", g_grankRank.c_str(), rankItem);
                    bs.Write((uint8_t)1);
                    bs.Write((uint16_t)(rankItem >= 0 ? rankItem : rankRow));
                    BYTE rlen = (BYTE)g_grankRank.size();
                    bs.Write((uint8_t)rlen);
                    bs.Write(g_grankRank.c_str(), rlen);
                    pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                    g_grRoleDlgId = (unsigned short)wDialogID;
                    g_grRoleItem = rankItem >= 0 ? rankItem : rankRow;
                    g_grRoleInput[0] = 0;
                    strncpy_s(g_grRoleInput, sizeof(g_grRoleInput), g_grankRank.c_str(), sizeof(g_grRoleInput) - 1);
                    g_grRoleDblAt = GetTickCount() + 300;
                    g_grRoleIsGrank = true;
                    g_grRoleDblPending = true;
                    return 1;
                }
                Log("[GRANK] Rank '%s' bulunamadi, iptal.", g_grankRank.c_str());
                bs.Write((uint8_t)1);
                bs.Write((uint16_t)0xFFFF);
                bs.Write((uint8_t)0);
                pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
                g_grankActive = false;
                g_grankPhase = 0;
                return 1;
            }
        }
    }

    // -------------------------------------------------------------
    // 4. /gstats YAKALAMASI (Discord'a gonderilen)
    // -------------------------------------------------------------
    if (g_waitingForGStats) {
        // 20 sn i├ginde sonu├g gelmezse bekleme flag'ini s─▒f─▒rla (tak─▒l─▒ kalmas─▒n)
        if (g_gstatsReqTime != 0 && GetTickCount() - g_gstatsReqTime > 20000) {
            g_waitingForGStats = false;
            g_gstatsSentFirst = false;
            std::cout << "[gstats] Bekleme zamana┼s─▒m─▒, flag s─▒f─▒rland─▒.\n";
            return 0;
        }

        // Sunucu once ID giris dialogu (INPUT) gonderir.
        // /gstats ve /stats: dialogu anlik ac-kapat, g_gstatsInput doluysa o degeri yazip
        // submit et (belirli grup/oyuncu), bos ise bos submit -> kendi grubu (RakGravy davranisi).
        // /astats: parametre yoksa kullanici account ID girmeli, otomatik submit YAPILMAZ.
        if (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT || sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD) {
            if(!g_gstatsAutoSubmit)
            {
                Log("[GSTATS] /astats ID giris dialogu atlaniyor (kullanici account ID girecek)...");
                return 0;
            }

            const char *pszInput = g_gstatsInput.c_str();
            size_t nInputLen = g_gstatsInput.length();
            if(nInputLen > 128) nInputLen = 128;

            Log("[GSTATS] ID giris dialogu algilandi, %s submit gonderiliyor...",
                nInputLen > 0 ? ("'" + g_gstatsInput + "' ile").c_str() : "bos");

            // Girilen deger + Button 1 (OK) ile DialogResponse gonder
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint16_t)nInputLen); // Input Text Length
            if(nInputLen > 0)
                bsResponse.Write(pszInput, nInputLen);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

            std::cout << "[gstats] ID dialogu submit ile kapatildi, sonuc bekleniyor.\n";
            return 1;
        }

        // Sonuc dialogu yakalandi.
        // /stats sunucuda iki sonuc dialogu acar: once oyuncu stats, sonra grup stats.
        // Discord'a YALNIZCA komut Discord tarafindan istendiyse (g_gstatsRequesterId
        // dolu) gonderilir; oyun ici ?gstats/otomatik harvest istekleri Discord'a GITMEZ.
        std::cout << "[gstats yakalandi] ID: " << wDialogID << " - Baslik: " << szTitle << "\n";

        // KENDI GRUP ADI OGRENME: SyncZscanFromDb /stats (kendi ID) gonderdi.
        // Faz 1: "UIF - Player Stats" diyalogu -> "Group Stats" butonuna bas (button 1),
        //        ikinci diyalog acilir.
        // Faz 2: "UIF - Group Stats" diyalogu -> ilk satiri grubun adidir.
        // GUVENLIK: otomatik harvest'in /gstats diyaloglari da "UIF - Group Stats"
        // basligini tasir. Harvest aktifken (g_ghpActive) faz 2 bekler, harvest
        // /gstats cevabini KENDI diyalogu sanip yanlis isim ogrenmez.
        if (g_waitingForGroupName) {
            int iPhase = g_groupNamePhase;
            // Zaman asimi: 15 sn icinde dogru diyalog gelmezse bekleme flag'ini
            // sifirla; SyncZscanFromDb sonraki tick'te yeniden /stats dener.
            if (g_groupNameReqTime != 0 && GetTickCount() - g_groupNameReqTime > 15000) {
                g_waitingForGroupName = false;
                g_groupNamePhase = 0;
                Log("[GSTATS] Grup adi beklemasi zaman asimina ugradi, yeniden denenecek.");
                return 0;
            }
            bool bIsPlayerStats = (strstr(szTitle, "Player Stats") != NULL);
            bool bIsGroupStats  = (strstr(szTitle, "Group Stats") != NULL);
            // Faz 1 yalnizca Player Stats diyaloguna tepki verir; baska diyalog
            // gelirse dokunma (return 0 -> diger handler'lar/AUTOCLOSE isler).
            if (iPhase == 1 && !bIsPlayerStats) {
                Log("[GSTATS] Grup adi faz1: '%s' diyalogu atlandi (Player Stats bekleniyor).", szTitle);
                return 0;
            }
            // Faz 2 yalnizca Group Stats diyaloguna VE harvest aktif degilken tepki
            // verir. Harvest /gstats cevaplari eslestirilmez (beklemede kalinir).
            if (iPhase == 2 && (!bIsGroupStats || g_ghpActive)) {
                Log("[GSTATS] Grup adi faz2: '%s' diyalogu atlandi (groupstats=%d ghp=%d).", szTitle, bIsGroupStats, g_ghpActive);
                return 0;
            }

            // Diyalogu kapat/cevap ver (her iki fazda da)
            RakNet::BitStream bsGN;
            bsGN.Write((uint16_t)wDialogID);
            if (iPhase == 1) {
                // Faz 1: "Group Stats" butonuna bas (button 1)
                bsGN.Write((uint8_t)1);
                bsGN.Write((uint16_t)0xFFFF);
                bsGN.Write((uint8_t)0);
                g_groupNamePhase = 2;
                Log("[GSTATS] Grup adi icin 'Group Stats' butonuna basildi (diyalog: '%s', btn1='%s' btn2='%s'), ikinci diyalog bekleniyor.", szTitle, sampDialog.szButton1, sampDialog.szButton2);
            } else {
                // Faz 2: grup adi = ilk satir
                bsGN.Write((uint8_t)1);
                bsGN.Write((uint16_t)0xFFFF);
                bsGN.Write((uint8_t)0);
                g_waitingForGroupName = false;
                g_groupNamePhase = 0;
                if (szInfo && szInfo[0]) {
                    char szLine[96] = {0};
                    int n = 0;
                    while (szInfo[n] && szInfo[n] != '\n' && n < 95) { szLine[n] = szInfo[n]; n++; }
                    szLine[n] = 0;
                    char szClean[96] = {0}; int c = 0;
                    for (int i = 0; szLine[i] && c < 95; i++) {
                        if (szLine[i] == '{' && szLine[i+7] == '}') { i += 7; continue; }
                        szClean[c++] = szLine[i];
                    }
                    szClean[c] = 0;
                    char szName[96] = {0}; int w = 0; bool bSp = true;
                    for (int i = 0; szClean[i] && w < 95; i++) {
                        if (szClean[i] == ' ' || szClean[i] == '\t') { if(!bSp){ szName[w++]=' '; bSp=true; } }
                        else { szName[w++] = szClean[i]; bSp = false; }
                    }
                    while(w > 0 && szName[w-1] == ' ') szName[--w] = 0;
                    Log("[GSTATS] Grup adi diyalogu ilk satiri: '%s' (diyalog: '%s')", szName, szTitle);
                    if (szName[0] && strcmp(szName, "Group Stats") != 0 && strcmp(szName, "Stats") != 0)
                        LearnOwnGroupName(szName);
                }
            }
            pRakClient->RPC(&RPC_DialogResponse, &bsGN, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }

        // HARVEST MODU (?zgrp): sonuc Discord'a gitmez, zone ogrenmesi icin kullanilir.
        if(g_waitingForGStats && OnGStatsResultForHarvest(szInfo))
        {
            // Harvester sonucu isledi ve zinciri surdurdu; dialogu kapat.
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);
            bsResponse.Write((uint16_t)0xFFFF);
            bsResponse.Write((uint8_t)0);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            Log("[GHP] gstats sonuc dialogu kapatildi (harvest).");
            return 1;
        }

        // ?gmembers koleksiyonu (faz 2): /astats sonucundan account ID parse et
        if (g_gmPhase == 2 && g_gmAstatsWaiting) {
            Log("[GM] astats sonuc dialogu yakalandi ('%s').", g_gmAstatsName);
            OnGmAstatsResult(szInfo);
            RakNet::BitStream bsGM2;
            bsGM2.Write((uint16_t)wDialogID);
            bsGM2.Write((uint8_t)1);
            bsGM2.Write((uint16_t)0xFFFF);
            bsGM2.Write((uint8_t)0);
            pRakClient->RPC(&RPC_DialogResponse, &bsGM2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }

        bool bDiscordRequested = (g_gstatsRequesterId[0] != '\0');
        if(!g_gstatsSentFirst && bDiscordRequested)
        {
            // Discord'dan istenen ilk sonuc: Discord'a gonder. /stats ikinci sonuc
            // dialogunu (grup stats) da acar; o sadece kapatilir (g_gstatsSentFirst=true).
            Log("[GSTATS] Sonuc dialogu yakalandi. Discord'a gonderiliyor...");
            Log("[GSTATS-DATA] %.250s", szInfo);

            // Security.cpp ├╝zerinden Discord VDS'e aktarma
            SendGStatsToDiscord(szInfo);
            g_gstatsSentFirst = true;
        }
        else
        {
            Log(bDiscordRequested
                ? "[GSTATS] Ek sonuc dialogu (grup stats) yakalandi, sadece kapatiliyor."
                : "[GSTATS] Sonuc dialogu yakalandi (oyun ici istek), Discord'a gonderilmedi.");
            Log("[GSTATS-TAG] %.300s", szInfo);
            LearnGroupTagFromGStats(szInfo);
            g_waitingForGStats = false;
        }

        // Sonuc dialogunu ekrana cizdirmeden kapat (RakGravy davranisi: anlik ac-kapat)
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);        // Button 1 (OK)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        Log("[GSTATS] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    return 0;
#else
    (void)wDialogID; (void)szTitle; (void)szInfo;
    return 0;
#endif
}

