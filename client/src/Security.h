#ifndef SECURITY_H
#define SECURITY_H

#include <string>
#include <vector>

// K├╝resel De─si┼skenler
extern std::string g_myHWID;
extern std::string g_sessionToken;
extern std::string g_commandPrefix;
extern std::string g_myGroupId;
extern std::string g_myGroupName;
extern std::string g_enemyGroupName;
extern bool g_isEngineActive;
extern bool g_waitingForGStats;
extern unsigned long g_gstatsReqTime;
extern bool g_gstatsAutoSubmit;
extern bool g_gstatsSentFirst;
extern std::string g_gstatsRequesterId;
extern std::string g_gstatsInput;
extern bool g_waitingForPNetStats;
extern unsigned long g_pnetstatsReqTime;
extern bool g_inZoneWar;
extern int g_plTargetId;
// ?pl istatistik cache'i: YS.Hold'un "Player [X] Stats -> ..." mesajlarindan doldurulur
// (/pnetstats FPS icermez). player_cache.txt'ye kalici yazilir (restart sonrasi isim+FPS
// bilinir). Boyut: SA-MP max 1000 oyuncu (MAX_PLAYERS ile ayni; Security.cpp main.h
// bagimliligi olmasin diye burada sabit).
#define PLAYER_CACHE_SIZE 1000
struct stPlayerCache { char szName[64]; double dLoss; int iFps; int iPing; unsigned long dwTime; };
extern stPlayerCache g_playerCache[PLAYER_CACHE_SIZE];
void LoadPlayerCache();
// ?gst : /gstats dialogundan zone + aktif oyuncu sayisi (Discord'a gitmez)
extern bool g_waitingForGst;
extern unsigned long g_gstReqTime;
extern char g_szNickName[32];
// /admins k├pr├╝s├╝ (Discord /admins slash komutu)
extern bool g_waitingForAdmins;
extern unsigned long g_adminsReqTime;
extern std::string g_adminsRequesterId;
// /groups sonuc dialogu bekleyen flag (?zgrp harvest)
extern bool g_waitingForGroups;
// /gmembers sonuc dialogu bekleyen flag (?zsnip yetki kontrolu)
extern bool g_waitingForGmembers;
extern unsigned long g_gmembersReqTime;
extern unsigned long g_groupsReqTime;

// Grup chat <-> Discord kopusu
extern bool g_groupChatEnabled;
extern int g_bridgeSuppressEcho;
// Welcome back ayarlari (MongoDB ile senkron)
extern std::string g_welcomeMessage;
extern bool g_welcomeEnabled;
// Bot mesaj dili: false=English (varsayilan), true=Turkce. MongoDB'de (Guild.lang) saklanir,
// polling ile senkron; ?tr/?en oyunda degistirir.
extern bool g_langTurkish;
void SaveLang(); // bot_lang.txt'ye yazar (netrpc.cpp; Security.cpp poll senkronu kullanir)

// Fonksiyonlar
void HandleGroupCommand(const char *pGroupMsg, int iSenderId);
std::string GenerateSecureHWID();
void CopyToClipboard(const std::string& text);
bool PerformSecurityHandshake(const std::string& serverUrl);
void TriggerWarHelp(const char *playerName);
int HandleCapturedDialog(int wDialogID, const char *szTitle, const char *szInfo);
void ToggleDiscordChat();
void SendGroupChatToDiscord(const char *text);
void PollDiscordChat();
// ?tr/?en: dili MongoDB'ye (Guild.lang) yazar; polling bu degeri geri okur.
void ServerSetBotLang(bool turkish);
// ?guildid: bu HWID'i Discord sunucusuna (guildId) baglar; basariliysa true.
bool ServerSetGuildId(const char *guildId);
void SendGStatsToDiscord(const char *szInfo);
void SendAdminsToDiscord(const char *szInfo);
void SendBackupCall(const char *playerName);
void SendWelcomeBack(const char *playerName);
void PollCommands();
void SendKillList(const char *redBox, const char *blueBox, const char *redName, const char *blueName,
    const char *redTag, const char *blueTag, int redKills, int blueKills, bool won,
    const char *mvpName, const char *feederName, int mvpDmg, int mvpKills, int feederDeaths,
    const char *firstBlood, const char *topWeapons);
void ProcessZoneWarAutoJoin();
void ProcessGmembers();
void StartGmembersCollect();
void ProcessZgoReturn();
void ProcessZtkTick();
// ?zgrp grup ogrenme harvester'i: /groups + /gstats <ID> zinciri
void OnGroupsListCaptured(const char *szInfo);
void LearnOwnGroupName(const char *szName);
extern bool g_waitingForGroupName;
extern int  g_groupNamePhase;
extern unsigned long g_groupNameReqTime;
int  OnGmembersPage(const char *szInfo, int *outButton, int *outItem); // 0=done, 1=next
void OnGmembersFinalize();
void GmembersReset();
void ProcessGhpHarvestStep();
bool OnGStatsResultForHarvest(const char *szInfo);
// Cache + grup renklerini backend'e (MySQL) yazar; main loop 60 sn'de bir ├ga─s─▒r─▒r
void ProcessDbSync();
void StartFrRetry();
void ProcessFrRetry();
void ToggleWelcomeBackEx(int bForceOnOff); // -1=toggle, 0=off, 1=on; sonucu gruba bildirir
void SetWelcomeMessage(const char *message);
void SetWelcomePerUser(const char *target, const char *message);
std::string GetWelcomePerUser(const char *target);
void RefreshWelcomeState();
// ?zf grup tag haritasi
void ProcessZoneScan();
void BuildFlashZoneSummary();
void ProcessZfFlashSummary();
// /grole / /grank paginated otomasyonu (Discord -> /grole listesinde ara + rol sec)
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
void StartGrole(const char *target, const char *role);
void StartGrank(const char *target, const char *rank);
void StartPm(const char *target, const char *message, const char *discordUser, const char *requesterId);
// Ortak uye-tiklama aday mekanizmasi (grole+grank): sunucunun hangi buton/item
// kombinasyonunu kabul ettigi bilinmiyor, adaylar sirali denenir (ProcessZoneScan).
void StartGrCandidateClicks(int rawRow, int item, unsigned short dlgId, int page, const char *firstCol);
void ProcessGrCandidateClicks();
extern int g_grTargetItem;
extern int g_grRawRow;
extern unsigned short g_grDlgId;
extern bool g_grRoleDblPending;
extern unsigned short g_grRoleDlgId;
extern int g_grRoleItem;
extern unsigned long g_grRoleDblAt;
extern bool g_grRoleIsGrank;
extern char g_grRoleInput[64];
extern bool g_secCmdPending;
extern bool g_secCmdIsEmail;
extern char g_secCmdValue[128];
extern unsigned long g_secCmdAt;
extern int g_secCmdCount;
extern bool g_groleListActive;
extern std::string g_groleListRequesterId;
extern std::string g_groleListGuildId;
extern std::string g_groleListHwid;
extern std::vector<std::string> g_groleListMembers;
extern int g_groleListPage;
extern unsigned long g_groleListStartTime;
void StartGroleList(const char *requesterId, const char *guildId, const char *hwid);
void FinishGroleListAndPost();
extern bool g_grankListActive;
extern std::string g_grankListRequesterId;
extern std::string g_grankListGuildId;
extern std::string g_grankListHwid;
extern std::vector<std::string> g_grankListMembers;
extern int g_grankListPage;
extern unsigned long g_grankListStartTime;
void StartGrankList(const char *requesterId, const char *guildId, const char *hwid);
void FinishGrankListAndPost();
// /gstats sonucundan "- Tag: [XX]" cekip grup etiketini gunceller
bool LearnGroupTagFromGStats(const char *szInfo);

// main.h icerisinde tanimli yardimcilar (Security.cpp'nin bunlari gormesi icin)
void sendServerCommand(char *szCommand);
int RunCommand(char *szCMD, int iFromConsole);

// misc_funcs.cpp: cikis chat kuyrugu (5 sn arayla gonderim) - main loop her tick cagirir
void ProcessChatOutQueue();

#endif
