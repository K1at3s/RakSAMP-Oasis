/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"

#include <cmath>   // json.hpp'nin std::isfinite gereksinimi
#include <math.h>
#undef isfinite    // MSVC math.h makrosu std::isfinite'i k-‚ñírar (Security.cpp'de sorun yok; burada main.h getiriyor)
#include <stdio.h>
#undef snprintf    // MSVC stdio.h makrosu std::snprintf'i k-‚ñírar
#undef vsnprintf
#include "httplib.h"
#include "json.hpp"
using json = nlohmann::json;

int iNetModeNormalOnfootSendRate, iNetModeNormalIncarSendRate, iNetModeFiringSendRate, iNetModeSendMultiplier;

char g_szHostName[256];
BYTE m_bLagCompensation;

PLAYERID imitateID = -1;
bool iGettingNewName=false;

int iMoney, iDrunkLevel, iLocalPlayerSkin;

struct stGTAMenu GTAMenu;

struct stSAMPDialog sampDialog;
HFONT hSAMPDlgFont = NULL;
HANDLE hDlgThread = NULL;
HWND hwndSAMPDlg = NULL;

PLAYER_SPAWN_INFO SpawnInfo;

BOOL bIsSpectating = 0;

// Grup komutu cooldown'lari ve durum (GetTickCount ms)
static DWORD g_lastWarHelpTime = 0;
static DWORD g_lastPLTime = 0;
static DWORD g_lastGStatsTime = 0;
static DWORD g_lastBackupTime = 0;
static DWORD g_lastZoneWarTime = 0;
static DWORD g_lastZoneWarMsgTime = 0; // war-end tespiti icin AYRI zaman (g_lastZoneWarTime'i ?za/?gz paylasir)
static DWORD g_lastPingTime = 0;
static bool g_zwLearningGroup = false;
static DWORD g_zwLearnTick = 0;
static DWORD g_lastGztTime = 0;
static int g_knownGroupMembers[MAX_PLAYERS];

// Zone war s-‚ïùresi: sabit 10 dk (sunucu kural-‚ñí). ?gzt kalan s-‚ïùreyi bundan hesaplar;
// "Type /gz2 to join" bildirimi sava-s-‚ñín ba-slang-‚ñí-g an-‚ñíd-‚ñír (g_zoneWarStartTick).
#define ZONE_WAR_TOTAL_MS 600000
#define KILL_LIST_CAPTURE_WINDOW_MS 90000 // sonu-g textdraw'lar-‚ñí i-gin toplama penceresi (son 60 sn giri˛ + bitis textdraw'lari)
static DWORD g_zoneWarStartTick = 0;

// Kill list yakalay-‚ñíc-‚ñí durumu (GZ sonucu textdraw'lar-‚ñí -> dosya + Discord).
// Kill-list textdraw'lari sadece GZ -‚ñë-c-‚ñëNDEK-‚ñëLERE gelir; bot son 00:02'de /gz2 ile girer ve kalir.
static bool g_zwCapturingKillList = false;  // textdraw toplama aktif mi
static bool g_zwKillListSent = false;       // sonu-g zaten g-nderildi mi
static DWORD g_zwCaptureStart = 0;          // toplama ba-slang-‚ñíc-‚ñí
static DWORD g_zwFinalizeAt = 0;            // sava˛ bitisi finalize zamani (wins/loses blo-su kurar)
static std::string g_zwKillListText;        // birikmi-s sonu-g metni (renk kodlar-‚ñí korunarak toplan-‚ñír, finalize'da ayr-‚ñí-st-‚ñír-‚ñíl-‚ñír)
static char g_zwWarZone[16] = {0};          // aktif sava-s-‚ñín zone no'su ("146"); 60 sn'lik tekrar duyurusu bununla elenir
static std::string g_zwWarTeamLeft;         // "X vs Y" duyurusundan SOL grup (kirmizi takim)
static std::string g_zwWarTeamRight;        // "X vs Y" duyurusundan SAG grup (mavi takim)
static bool g_zwWelcomed[MAX_PLAYERS];      // bu sava-sta oyuncu ba-s-‚ñína bir kez welcome (sava-s ba-s-‚ñínda s-‚ñíf-‚ñírlan-‚ñír)

// Kill list textdraw'lari AYRI AYRI gelir: her kolon/satir ayri textdraw (ID 70 "Name",
// 71 "Kills", 72 "Assists", 73 "Deaths", 74 "Dmg" basliklari Y=80; veri satirlari Y=95+,
// X kolon hizasi). Ayni Y'deki textdraw'lar X'e gˆre yan yana dizilir -> satir olusturur.
#define KILL_TD_MAX 768
struct KillTdItem { float fX, fY; char szText[64]; DWORD dwColor; };
static KillTdItem g_zwKillTds[KILL_TD_MAX];
static int g_zwKillTdCount = 0;
void FinalizeKillList(); // ileri bildirim (sava˛-basi kalintisini kapatmak icin)
// Kill list istatistikleri (Discord embed icin)
static int g_zwBestK = 0, g_zwBestDmg = 0, g_zwBestD = 0;
static char g_zwMvpName[64] = {0}, g_zwFeederName[64] = {0};
static int g_zwMineK = 0, g_zwMineA = 0, g_zwMineD = 0, g_zwMineDmg = 0;
static int g_zwEnemyK = 0, g_zwEnemyA = 0, g_zwEnemyD = 0, g_zwEnemyDmg = 0;
static bool g_zwWeWon = true; // sava˛ sonucu: biz kazandik mi (war-end blogu kurar)
static char g_zwFirstBlood[160] = {0}; // GZ ilk kill: "isim (Weapon: X) (Distance: Ym)"

// Top Weapons: RPC 55 (DeathMessage - sagdaki kill feed) sava˛ boyunca toplanir.
// "ZONE WAR: X vs Y" duyurusundaki konum takimimizi belirler (sol=kirmizi, sag=mavi);
// finalize'da scoreboard isimleriyle filtrelenir, top 5 silah Discord'a gider.
static bool g_zwWeAreBlue = false;      // isim eslesmesi: grubumuz "vs" de sagda mi (mavi)
static bool g_zwWeAreBlueKnown = false; // vs duyurusundan belirlendi mi (renk fallback'i icin)
#define WAR_KILL_MAX 1024
struct WarKill { char szKiller[32]; BYTE weapon; };
static WarKill g_zwWarKills[WAR_KILL_MAX];
static int g_zwWarKillCount = 0;

// GZ giri-sleri: sava-s ba-s-‚ñínda tak-‚ñím rengi --srenmek i-gin k-‚ñísa giri-s; son 00:02'de kill-list giri-si.
static bool   g_zwReturnPending = false;   // k-‚ñísa giri-s aktif ([gz2] -> [fr] ile geri d-n-‚ïùlecek)
static DWORD  g_zwReturnAt = 0;
static bool   g_zwKillListStay = false;    // kill-list giri-sinde GZ'de kalinir (sava˛ bitince finalize + /fr)
static bool   g_zwAutoJoinSent = false;    // son 00:02'de /gz2 g-nderildi mi (kill-list i-gin i-geride kal-‚ñín-‚ñír)
static bool   g_zwCheckJoinSent = false;   // 09:02 kontrol giri-si yap-‚ñíld-‚ñí m-‚ñí (sava-s aktif mi diye bakar)
static unsigned int g_zwMyColor = 0;       // GZ'de bizim tak-‚ñím rengi ({RRGGBB}, 0 = bilinmiyor)
// Esir-alma ile grup adi ˆrenimi: "ZONE WAR: X takes over zone '# N'" -> zone N'nin
// sahipligi (RPC 108 ile) guncellenir; o guncellemede X -> zone N esle (pending).
// (renk teyit pending mekanizmasi kald˝rildi - sahiplik ZONE WAR mesaj˝ndan
//  direkt baglanir, renk kullanilmaz)

// Sava-s zamanlay-‚ñíc-‚ñí (UIF textdraw ID 60): "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> skor + kalan s-‚ïùre.
// Bot GZ i-ginde k-‚ñísa giri-slerde (sava-s ba-s-‚ñí renk --srenme) timer'i de yakalayabilir.
static DWORD g_warTimerTick = 0;            // son yakalama zaman-‚ñí
static int  g_warRemainingSec = -1;         // yakalanan kalan s-‚ïùre (sn)

// Gang zone yakalay-‚ñíc-‚ñí: UIF zone numaralar-‚ñí haritan-‚ñín -‚ïùst solundan (Tierra Robada)
// ba-slayarak #0'dan itibaren s-‚ñíral-‚ñí olu-sturulur -> gang zone ID == zone # kabul edilir.
// RPC_ScrAddGangZone (108) geldik-ge koordinatlar buraya kaydedilir (?zgo i-gin).
#define MAX_GANG_ZONES 2048
static float g_zoneMinX[MAX_GANG_ZONES];
static float g_zoneMinY[MAX_GANG_ZONES];
static float g_zoneMaxX[MAX_GANG_ZONES];
static float g_zoneMaxY[MAX_GANG_ZONES];
// (g_zoneColor dizisi kald˝rildi - eslesme tamamen isim bazli, renk saklanmaz/kullanilmaz)
static bool g_zoneFlash[MAX_GANG_ZONES];
// zone -> sahip grup adi (/gzinfo "owned by group X" ile ˆrenilir). Renk bagimsiz
// eslesme: YS.Hold "bot renk bagimli olmasin, sadece flash bagimli olsun" - renk geri
// donusumunde ayni renk farkli gruplara baglaniyordu; isim eslesmesi bunu cozer.
static char g_zoneOwnerName[MAX_GANG_ZONES][80];
static int g_zoneCount = 0;

// ?zgo: bot zone'a isinlanip 40 sn bekler, sonra eski pozisyonuna doner.
#define ZGO_STAY_MS 40000
#define ZGO_TELEPORT_Z 100.0f // yuksekten dus, zemin Z'sine otur (sabit 25 bazi bolgelerde yerin dibinde kaliyor)
static bool g_zgoActive = false;            // isinlama + bekleme aktif mi
static DWORD g_zgoReturnTick = 0;           // geri donus zamani
static float g_zgoReturnPos[3] = {0, 0, 0}; // donulecek (spawn) pozisyon

// ?gzt: geri sayim duyurulari (YS botu gibi). Esikler SABIT DOSYADAN degil,
// sava˛˝n gercek suresinden otomatik hesaplanir (kullan˝c˝: "ilk zaman-‚ñí -gekip
// ona g-re ayarla"): yarim nokta + 60/30/10 sn. Sunucu sureyi zaten surekli
// veriyor (timer textdraw + Time left mesajlari) - CaptureWarTimer yakalar.
static int   g_gztCountdownAnnounced = 999;
static int   g_gztLastRem = -1;          // onceki kontrol anindaki kalan saniye (esik gecisi tespiti)
static bool  g_warTimerViaTdw = false;    // timer TEXTDRAW'dan mi geldi? (kendi sava˛imiz
                                          // GZ'sinde g-r-‚ïùn-‚ïùr; uL.Hold'un baska sava˛lar icin
                                          // attigi "Time left:" chat mesajlari sayilmaz -
                                          // yoksa bizde sava˛ yokken geri sayim basliyordu)
// ?gzt on/off: geri sayim duyurulari aÁ˝k mi? (varsayilan ON = mevcut davranis)
static bool g_gztOn = true;

// ?za: /gz sonrasi yetki kontrolu - co-leader/leader degilse UIF bir textdraw
// gosterir. Bu pencere icinde gelen textdraw'larda "leader" gecen metin yakalanir
// ve gruba dile gˆre "yetkim yok" mesaj˝ atilir.
static bool  g_zaPermActive = false;
static DWORD g_zaPermUntil = 0;

// ?zsnip (zone sniper): hedef zone'a isinla, o zonede sava˛ bitene kadar bekle,
// bitince /gz gˆnder (oraya saldiri). Isim (grup tag'i) veya sayisal zone id alir.
static bool  g_ztkActive = false;
static int   g_ztkZoneId = -1;
static bool  g_ztkWaitAnnounced = false;
// Zincir saldiri (?zsnip + ?yes): grubun diger zone'larina sirasiyla saldir.
static char  g_snipGroupName[96] = {0}; // hedef grubun adi
static int   g_snipZones[64];           // grubun saldiri sirasi zone listesi
static int   g_snipZoneCount = 0;
static int   g_snipZoneIdx = 0;         // siradaki zone (son saldirilan = idx-1)
static bool  g_ztkAskNext = false;      // saldiri bitti, ?yes bekleniyor
static bool  g_ztkAttackPending = false; // /gz gonderildi, won/lost sonucu bekleniyor
static int   g_ztkOutcome = 0;            // 0=bilinmiyor, 1=kazanildi, 2=kaybedildi
static bool  g_ztkTargetWar = false;      // hedef zonede dusman savasi suruyor (mesaj bazli takip)
static bool IsSniperTargetZone(int zid); // ileri bildirim (zsnip zone eslesmesi)
static int CollectGroupZones(const char *szGroupName, int *outZones, int maxZones); // ileri bildirim

// ?zsnip yetki kontrolu: /gmembers dialogundan bot + komutu atan kullan˝c˝
// Co-Leader/Leader rolune sahip mi? Degilse sniper (ba˛latma ve off) CALISTIRILMAZ.
// NOT: /gmembers SAYFALI dialogdur; bu surum ilk sayfayi okur (isimler sonraki
// sayfalardaysa yetki dogrulanamaz, reddedilir - guvenli varsayilan).
static bool  g_snipPermPending = false;  // /gmembers cevabi bekleniyor
static int   g_snipAction = 0;           // 0 = sniper ba˛lat, 1 = sniper kapat
static int   g_snipZone = -1;            // bekleyen hedef zone
static int   g_snipSenderId = -1;        // komutu atan kullan˝c˝ (playerInfo'dan ismi alinir)
bool g_waitingForGmembers = false;       // SAMPRPC.cpp kullanir (extern)
unsigned long g_gmembersReqTime = 0;     // SAMPRPC.cpp kullanir (extern)

// ?gmembers (Google Sheets roster): /gmembers sayfali liste + /astats (account ID)
// + /gvips (VIP) toplanir, backend'e POST edilir, ornek tablo formatinda yazilir.
#define GM_MAX_MEMBERS 160
struct GmMemberRow {
	char szName[64];
	char szRank[32];    // rol/rank (Leader/Co-Leader/Member/Elder/Veteran/Recruit)
	char szRole[32];    // sheet'te ayri kolon; veri ayri degilse rank ile ayni
	char szLast[32];    // son aktif (parse edilebilirse)
	char szAccount[24]; // /astats'tan account ID
	char szVip[24];     // /gvips'ten VIP tipi
};
static GmMemberRow g_gmRows[GM_MAX_MEMBERS];
int g_gmCount = 0;
int g_gmPhase = 0;            // 0=kapal˝ 1=uyeler 2=astats 3=gvips 4=gˆnderim (SAMPRPC extern)
int g_gmPage = 0;
unsigned long g_gmPhaseAt = 0;
int g_gmAstatsIdx = 0;
bool g_gmAstatsWaiting = false;
char g_gmAstatsName[64] = {0};
bool g_gmGvipsWaiting = false;
static bool g_gmSendDone = false;
#define GMCOL_RESULT_DONE 0
#define GMCOL_RESULT_NEXT 1
static const char *g_szBackendBase = "http://127.0.0.1:3000"; // OnGmSend erken kullanir (sablon: satir 955'teki)

// Renk kodlarini temizle + trim (gmembers/gvips satirlari icin)
static void GmCleanLine(const char *szIn, char *szOut, int nOut)
{
	int c = 0;
	for(int i = 0; szIn[i] && c < nOut - 1; i++)
	{
		if(szIn[i] == '{' && szIn[i + 7] == '}') { i += 7; continue; }
		szOut[c++] = szIn[i];
	}
	szOut[c] = 0;
	char *s = szOut;
	while(*s == ' ' || *s == '\t' || *s == '\r') s++;
	char *e = s + strlen(s) - 1;
	while(e >= s && (*e == ' ' || *e == '\t' || *e == '\r')) *e-- = 0;
	if(s != szOut) memmove(szOut, s, strlen(s) + 1);
}

void StartGmembersCollect()
{
	if(g_gmPhase != 0)
	{
		Log("[GM] Koleksiyon zaten Áal˝˛˝yor (faz %d).", g_gmPhase);
		return;
	}
	g_gmPhase = 1;
	g_gmCount = 0; g_gmPage = 0;
	g_gmAstatsIdx = 0; g_gmAstatsWaiting = false;
	g_gmGvipsWaiting = false; g_gmSendDone = false;
	g_gmPhaseAt = GetTickCount();
	Log("[GM] ?gmembers koleksiyonu ba˛ladi (faz 1: uye listesi).");
	sendServerCommand((char *)"/gmembers");
}

// Faz 1 (uye listesi) sayfa parse'i: SAMPRPC.cpp cagirir. GMCOL_RESULT_DONE=0 / NEXT=1.
int OnGmMembersPage(const char *szInfo, int *outButton, int *outItem)
{
	*outButton = 1; *outItem = -1;
	Log("[GM-MEMBERS-DATA] Sayfa %d: %.800s", g_gmPage + 1, szInfo ? szInfo : "");
	if(!szInfo || !szInfo[0]) return GMCOL_RESULT_DONE;

	const char *pL = szInfo;
	int iRow = 0;
	while(pL && *pL && g_gmCount < GM_MAX_MEMBERS)
	{
		const char *pE = strchr(pL, '\n');
		size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
		if(nLen > 0 && nLen < 255)
		{
			char szLine[256], szClean[256];
			memcpy(szLine, pL, nLen); szLine[nLen] = 0;
			GmCleanLine(szLine, szClean, sizeof(szClean));
			if(!szClean[0]) { iRow++; if(!pE) break; pL = pE + 1; continue; }

			char szLow[256] = {0};
			for(int i = 0; szClean[i] && i < 255; i++) szLow[i] = (char)tolower((unsigned char)szClean[i]);

			// Sayfalama isareti: ">>" / next / sonraki
			if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki"))
			{
				*outItem = iRow;
				iRow++; if(!pE) break; pL = pE + 1; continue;
			}
			// Baslik satiri: "#/ID/Name/Rank" gibi
			if((strstr(szLow, "id") && strstr(szLow, "name")) || strstr(szLow, "rank") && strstr(szLow, "role"))
			{ iRow++; if(!pE) break; pL = pE + 1; continue; }

			// Isim: ba˛langictaki sira numarasini ("1. ", "1: ", "1 ") atla
			char *p = szClean;
			while(*p && !((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_' || *p == '.')) p++;
			while(*p >= '0' && *p <= '9') p++;
			while(*p == '.' || *p == ':' || *p == '-' || *p == ' ' || *p == '\t') p++;
			char szName[64] = {0}; int ni = 0;
			while(p[ni] && p[ni] != ' ' && p[ni] != '\t' && p[ni] != '-' && p[ni] != '\r' && ni < 63)
			{ szName[ni] = p[ni]; ni++; }
			szName[ni] = 0;
			if(!szName[0])
			{ // fallback: satirin ilk kelimesi
				int k = 0;
				while(szClean[k] && szClean[k] != ' ' && szClean[k] != '\t' && k < 63) { szName[k] = szClean[k]; k++; }
				szName[k] = 0;
			}
			if(!szName[0] || strchr(szName, '>')) { iRow++; if(!pE) break; pL = pE + 1; continue; }

			// Rank/Role anahtar kelimesi tara
			const char *szRanks[] = { "Co-Leader", "Leader", "Member", "Elder", "Veteran", "Recruit" };
			char szRank[32] = {0};
			for(int r = 0; r < 6; r++)
				if(strstr(szClean, szRanks[r])) { strncpy_s(szRank, sizeof(szRank), szRanks[r], sizeof(szRank) - 1); break; }

			// Son aktif: satirin son kelimesi (tarih ise)
			char szLast[32] = {0};
			{
				const char *pT = strrchr(szClean, ' ');
				const char *pT2 = strrchr(szClean, '\t');
				const char *pW = pT && (!pT2 || pT > pT2) ? pT : pT2;
				if(pW && pW[1] && (isdigit((unsigned char)pW[1]) || pW[1] == '-'))
					strncpy_s(szLast, sizeof(szLast), pW + 1, sizeof(szLast) - 1);
			}

			// Tekrar kontrolu
			bool bDup = false;
			for(int i = 0; i < g_gmCount; i++)
				if(_stricmp(g_gmRows[i].szName, szName) == 0) { bDup = true; break; }
			if(!bDup)
			{
				strncpy_s(g_gmRows[g_gmCount].szName, sizeof(g_gmRows[0].szName), szName, sizeof(g_gmRows[0].szName) - 1);
				strncpy_s(g_gmRows[g_gmCount].szRank, sizeof(g_gmRows[0].szRank), szRank, sizeof(g_gmRows[0].szRank) - 1);
				strncpy_s(g_gmRows[g_gmCount].szRole, sizeof(g_gmRows[0].szRole), szRank, sizeof(g_gmRows[0].szRole) - 1);
				strncpy_s(g_gmRows[g_gmCount].szLast, sizeof(g_gmRows[0].szLast), szLast, sizeof(g_gmRows[0].szLast) - 1);
				g_gmCount++;
				Log("[GM] Uye #%d: '%s' (rank=%s, son=%s)", g_gmCount, szName, szRank[0] ? szRank : "?", szLast[0] ? szLast : "?");
			}
		}
		iRow++;
		if(!pE) break;
		pL = pE + 1;
	}

	if(*outItem >= 0 && g_gmPage < 12)
		return GMCOL_RESULT_NEXT;
	return GMCOL_RESULT_DONE;
}

// Faz 2: /astats sonuc dialogundan account ID parse et (format dogrulanacak, log basar)
void OnGmAstatsResult(const char *szInfo)
{
	Log("[GM-ASTATS-DATA] '%s' -> %.400s", g_gmAstatsName, szInfo ? szInfo : "");
	if(!szInfo || !szInfo[0] || g_gmAstatsIdx >= g_gmCount) { g_gmAstatsWaiting = false; g_gmAstatsIdx++; return; }

	char szClean[600];
	GmCleanLine(szInfo, szClean, sizeof(szClean));
	char szAcc[24] = {0};

	// "ID: 123456" / "Account ID: 123456" / "Id : 123"
	const char *pId = strstr(szClean, "ID:");
	if(!pId) pId = strstr(szClean, "Id:");
	if(!pId) pId = strstr(szClean, "id:");
	if(!pId) pId = strstr(szClean, "Account ID");
	if(pId)
	{
		const char *pN = pId + (strstr(szClean, "Account ID") ? strlen("Account ID") : 3);
		if(*pN == ':') pN++;
		while(*pN == ' ' || *pN == '\t') pN++;
		int n = 0;
		while(pN[n] >= '0' && pN[n] <= '9' && n < 23) { szAcc[n] = pN[n]; n++; }
	}
	// Fallback: ilk uzun sayi (>=5 hane)
	if(!szAcc[0])
	{
		const char *pN = szClean;
		while(*pN)
		{
			if(*pN >= '0' && *pN <= '9')
			{
				const char *pS = pN; int n = 0;
				while(pS[n] >= '0' && pS[n] <= '9') n++;
				if(n >= 5 && n <= 23) { memcpy(szAcc, pS, n); szAcc[n] = 0; break; }
				pN += n;
			}
			else pN++;
		}
	}
	if(g_gmAstatsIdx < g_gmCount)
		strncpy_s(g_gmRows[g_gmAstatsIdx].szAccount, sizeof(g_gmRows[0].szAccount), szAcc, sizeof(g_gmRows[0].szAccount) - 1);
	Log("[GM] astats '%s' -> account ID: %s", g_gmAstatsName, szAcc[0] ? szAcc : "(bulunamadi)");
	g_gmAstatsWaiting = false;
	g_gmAstatsIdx++;
}

// Faz 3: /gvips dialogundan isim -> VIP tipi parse et
void OnGmVipsResult(const char *szInfo)
{
	Log("[GM-VIPS-DATA] %.800s", szInfo ? szInfo : "");
	if(!szInfo) { g_gmGvipsWaiting = false; g_gmPhase = 4; g_gmPhaseAt = GetTickCount(); return; }

	const char *pL = szInfo;
	while(pL && *pL)
	{
		const char *pE = strchr(pL, '\n');
		size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
		if(nLen > 0 && nLen < 255)
		{
			char szLine[256], szClean[256];
			memcpy(szLine, pL, nLen); szLine[nLen] = 0;
			GmCleanLine(szLine, szClean, sizeof(szClean));
			if(!szClean[0]) { if(!pE) break; pL = pE + 1; continue; }

			char szLow[256] = {0};
			for(int i = 0; szClean[i] && i < 255; i++) szLow[i] = (char)tolower((unsigned char)szClean[i]);
			if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki"))
			{ if(!pE) break; pL = pE + 1; continue; }
			if(strstr(szLow, "vip") && (strstr(szLow, "name") || strstr(szLow, "isim")))
			{ if(!pE) break; pL = pE + 1; continue; }

			// Isim (ilk kelime) + VIP tipi (diamond/gold/silver/platinum/bronze)
			char szName[64] = {0};
			int k = 0;
			while(szClean[k] && szClean[k] != ' ' && szClean[k] != '\t' && szClean[k] != '-' && k < 63) { szName[k] = szClean[k]; k++; }
			szName[k] = 0;
			const char *szVips[] = { "diamond", "gold", "silver", "platinum", "bronze" };
			char szVip[24] = {0};
			for(int v = 0; v < 5; v++)
			{
				const char *pV = strstr(szLow, szVips[v]);
				if(pV) { szVip[0] = (char)toupper(szVips[v][0]); strncpy_s(szVip + 1, sizeof(szVip) - 1, szVips[v] + 1, sizeof(szVip) - 2); break; }
			}
			if(szName[0])
			{
				for(int i = 0; i < g_gmCount; i++)
					if(_stricmp(g_gmRows[i].szName, szName) == 0 && szVip[0])
					{
						strncpy_s(g_gmRows[i].szVip, sizeof(g_gmRows[0].szVip), szVip, sizeof(g_gmRows[0].szVip) - 1);
						break;
					}
				Log("[GM] VIP: '%s' -> %s", szName, szVip[0] ? szVip : "?");
			}
		}
		if(!pE) break;
		pL = pE + 1;
	}
	g_gmGvipsWaiting = false;
	g_gmPhase = 4;
	g_gmPhaseAt = GetTickCount();
}

// Faz 4: toplanan veriyi backend'e POST et (Google Sheets'e yazar)
void OnGmSend()
{
	if(g_gmSendDone) return;
	g_gmSendDone = true;
	try
	{
		json j;
		j["hwid"] = g_myHWID;
		j["playerName"] = g_szNickName ? g_szNickName : "";
		j["group"] = g_myGroupName;
		j["rows"] = json::array();
		for(int i = 0; i < g_gmCount; i++)
		{
			j["rows"].push_back({
				{"name", g_gmRows[i].szName},
				{"accountId", g_gmRows[i].szAccount},
				{"rank", g_gmRows[i].szRank},
				{"role", g_gmRows[i].szRole},
				{"vip", g_gmRows[i].szVip},
				{"lastActive", g_gmRows[i].szLast}
			});
		}
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(5);
		auto res = cli.Post("/api/gmembers/sheet", j.dump(), "application/json");
		if(res && res->status == 200)
		{
			std::string strResp = res->body;
			bool bOk = (strResp.find("\"success\":true") != std::string::npos);
			Log("[GM] Google Sheets gˆnderimi: %s (%d uye).", bOk ? "BASARILI" : "HATA", g_gmCount);
			if(!bOk) Log("[GM] Backend cevabi: %.200s", strResp.c_str());
		}
		else
			Log("[GM] Backend'e ulasilamadi (status %d).", res ? res->status : -1);
	}
	catch(...) { Log("[GM] Gˆnderim hatasi."); }
	g_gmPhase = 0;
	Log("[GM] Koleksiyon tamam (%d uye).", g_gmCount);
}

// Ana donguden (main.cpp) her iterasyonda cagrilir.
void ProcessGmembers()
{
	if(g_gmPhase == 0) return;
	DWORD dwNow = GetTickCount();

	if(g_gmPhase == 1)
	{
		if(dwNow - g_gmPhaseAt > 90000)
		{
			Log("[GM] Uye listesi zaman asimi (sayfa %d, %d uye) - devam ediliyor.", g_gmPage, g_gmCount);
			g_gmPhase = 2; g_gmAstatsIdx = 0; g_gmAstatsWaiting = false; g_gmPhaseAt = dwNow;
		}
		return; // sayfalar SAMPRPC'de islenir
	}

	if(g_gmPhase == 2)
	{
		// tum uyeler bitti mi? -> gvips
		if(g_gmAstatsIdx >= g_gmCount && !g_gmAstatsWaiting)
		{
			Log("[GM] /astats tamam (%d uye), faz 3: /gvips.", g_gmCount);
			g_gmPhase = 3; g_gmGvipsWaiting = false; g_gmPhaseAt = dwNow;
			sendServerCommand((char *)"/gvips");
			return;
		}
		if(!g_gmAstatsWaiting && g_gmAstatsIdx < g_gmCount)
		{
			g_gmAstatsWaiting = true;
			strncpy_s(g_gmAstatsName, sizeof(g_gmAstatsName), g_gmRows[g_gmAstatsIdx].szName, sizeof(g_gmAstatsName) - 1);
			// Mevcut gstats makinesi input dialogunu auto-submit eder (g_gstatsAutoSubmit)
			g_waitingForGStats = true;
			g_gstatsReqTime = GetTickCount();
			g_gstatsAutoSubmit = true;
			g_gstatsInput = g_gmRows[g_gmAstatsIdx].szName;
			g_gstatsRequesterId = "";
			g_gstatsSentFirst = false;
			char szCmd[80];
			sprintf_s(szCmd, sizeof(szCmd), "/astats %s", g_gmRows[g_gmAstatsIdx].szName);
			Log("[GM] astats %d/%d: %s", g_gmAstatsIdx + 1, g_gmCount, g_gmRows[g_gmAstatsIdx].szName);
			sendServerCommand(szCmd);
			g_gmPhaseAt = dwNow;
		}
		else if(g_gmAstatsWaiting && dwNow - g_gmPhaseAt > 20000)
		{
			Log("[GM] astats timeout: %s (gecti)", g_gmAstatsName);
			g_gmAstatsWaiting = false;
			g_gmAstatsIdx++;
			g_gmPhaseAt = dwNow;
		}
		return;
	}

	if(g_gmPhase == 3)
	{
		if(dwNow - g_gmPhaseAt > 30000)
		{
			Log("[GM] /gvips zaman asimi.");
			g_gmPhase = 4; g_gmPhaseAt = dwNow;
			OnGmSend();
			return;
		}
		return; // dialog SAMPRPC'de islenir
	}

	if(g_gmPhase == 4)
	{
		if(!g_gmSendDone) OnGmSend(); // gvips'ten faz 4'e gecildiyse gˆnderimi ba˛lat
		if(dwNow - g_gmPhaseAt > 30000)
		{
			Log("[GM] Gˆnderim zaman asimi.");
			g_gmPhase = 0;
		}
	}
}
static int g_zgoZoneId = -1;                // hedef zone (geri donus bildirimi icin)

// ?zf: /gzinfo asenkron oldugu icin flash paketleri (ScrFlashGangZone -> g_zoneFlash)
// birka-g yuz ms sonra gelir. Aninda BuildFlashZoneSummary cagirmak BO-S/ESKI veri okur.
// 1 sn sonra ana donguden cagrilir (ProcessZfFlashSummary).
static DWORD g_zfFlashAt = 0;               // 0: bekleyen istek yok
static bool  g_zfFlashPendingFlag = false;  // BuildFlashZoneSummary bir kez calistirilir

// ?zgrp harvest durumu: ?zf (ve ?zgo) tag eksikse otomatik /groups harvest ba˛latir;
bool  g_ghpActive = false;                     // harvest devam ediyor mu (SAMPRPC extern)
static bool g_forceZscanAfterHarvest = false; // harvest bitti -> yeni veri varsa zone scan tetikle (zscan off olsa bile)
static bool  g_ghpWaitingGstats = false;     // /gstats sonucu bekleniyor mu
// /grole / /grank paginated otomasyonu (Discord -> /grole listesinde ara + rol sec)
bool g_groleActive = false;
std::string g_groleTarget;
std::string g_groleRole;
int  g_grolePhase = 0;            // 1=uye listesi, 2=rol secimi
unsigned long g_groleStartTime = 0;
int  g_grolePage = 0;
static bool  g_groleDirectPending = false; // once direkt "?grole <isim> <rol>" denendi, onay bekleniyor
static DWORD g_groleDirectAt = 0;          // direkt onay icin son tarih (4 sn sonra dialog otomasyonuna gec)
// Ortak uye-tiklama adaylari (grole + grank): uL bot yapimcisinin ipucu ‚Äî liste
// formatina gˆre sunucunun hangi buton/item kabul ettigi bilinmiyor. Adaylar
// 1.5 sn arayla sirali denenir; faz-2 diyalog basliginda hedef adi dogrulanir.
int   g_grTargetItem = -1;
int   g_grRawRow = -1;        // hedefin ham satir indeksi (baslik+bos satirlar dahil)
char  g_grRowText[128] = {0}; // hedef satirin ILK SUTUNU (renksiz) ‚Äî TABLIST inputtext olarak gˆnderilir
unsigned short g_grDlgId = 0;
static int   g_grClickAttempt = -1;
static unsigned long g_grClickAt = 0;
static int   g_grPage = 0;
static int   g_grDblPhase = 0; // cift tik durumu: 0=hazir, 1=ilk tik gitti, 2=cift tik tamam
// Faz-2 rol/rank secimi de cift tik ister: ilk tik faz-2 handler'inda, ikinci tik ProcessZoneScan'de.
bool  g_grRoleDblPending = false;
unsigned short g_grRoleDlgId = 0;
int   g_grRoleItem = 0;
unsigned long g_grRoleDblAt = 0;
bool  g_grRoleIsGrank = false;
char  g_grRoleInput[64] = {0}; // rol/rank secim inputtext'i (LIST secili satir metni)
bool g_grankActive = false;
std::string g_grankTarget;
std::string g_grankRank;
int  g_grankPhase = 0;
unsigned long g_grankStartTime = 0;
int  g_grankPage = 0;
// /grole list fetch (interaktif menu icin tum uyeleri topla)
bool g_groleListActive = false;
std::string g_groleListRequesterId;
std::string g_groleListGuildId;
std::string g_groleListHwid;
std::vector<std::string> g_groleListMembers;
int g_groleListPage = 0;
unsigned long g_groleListStartTime = 0;
// /grank list fetch (uye listesi; rank secimi ayri)
bool g_grankListActive = false;
std::string g_grankListRequesterId;
std::string g_grankListGuildId;
std::string g_grankListHwid;
std::vector<std::string> g_grankListMembers;
int g_grankListPage = 0;
unsigned long g_grankListStartTime = 0;
 // UIF komut mute riski: /gstats art arda hizli giderse sunucu komutlar˝ susturur.
// Adimlar arasina 3 sn tempolu bekleme koyulur; sonuc aninda degil deadline sonrasi
// siradaki komut gˆnderilir. Kilit cozum de (ProcessZoneScan) ayni deadline'i bekler.
#define GHP_STEP_DELAY_MS 5000
static int   g_ghpCount = 0;
static int   g_ghpIdx = 0;              // siradaki /gstats cagrisi
static DWORD g_ghpNextStepAt = 0;            // siradaki /gstats ne zaman gˆnderilebilir
static int   g_ghpStaleRetries = 0;          // ayni adimin tekrar deneme sayisi
static void ProcessGhpHarvestStep();         // ?zgrp harvest zincirini surdur (ProcessZoneScan kilidinden)
#define GHP_AUTO_PERIOD_MS 900000    // oto ?zgrp periyodu: 15 dk'da bir eksik tag toplama
static DWORD g_ghpLastRun = 0;            // son oto harvest ba˛langici
static int   g_ghpLearnedCount = 0;       // bu zincirde kac tag ˆrenildi (spam'siz bildirim icin)
static bool  g_zgrpAuto = false;          // ?zgrp on/off: oto harvest ‚Äî DEFAULT KAPALI (elle ?zgrp/?tagscan ile calisir)

void ProcessZfFlashSummary()
{
	if(!g_zfFlashPendingFlag) return;
	if(GetTickCount() < g_zfFlashAt) return;
	g_zfFlashPendingFlag = false;
	g_zfFlashAt = 0;
	Log("[ZF] /gzinfo sonrasi 1 sn gecti, flashing zone dagilimi gˆnderiliyor.");
	BuildFlashZoneSummary();
}

// ?zf: flashing zone'lari grup tag'larina gˆre sayip gruba bildirir.

// /gmembers SAYFALI dialog: uye sayisina gˆre sayfa sayisi degisir. Bot+kullan˝c˝
// bulunana kadar sayfalar taranir (max 8); "Sonraki/Next" satiri varsa ona tiklanir,
// yoksa ikincil buton ile sayfa cevrilir.
#define GM_MAX_PAGES 8
static int  s_iGmPage = 1;
static bool s_bGmBotOk = false;
static bool s_bGmUserOk = false;
void GmembersReset() { s_iGmPage = 1; s_bGmBotOk = false; s_bGmUserOk = false; }

// Oto zone taramasi: flashing zone'lara isinlanip /gzinfo'dan grup adini ˆrenir
// (zgo mantigi). Sonuc group_colors.txt'ye yazilir; ?zf etiketleri ve
// "serbest zone" tespiti bu haritadan gelir.
#define ZONE_SCAN_PER_SAMPLE_MS 4000  // /gzinfo cevabi icin bekleme (mute korumali)
#define ZONE_SCAN_TELEPORT_SYNC_MS 3500 // /pos sonrasi teleport tamamlanana kadar bekle (stale /gzinfo + yanlis isim onlenir)
#define ZONE_SCAN_PERIOD_MS      900000 // oto tarama periyodu: yeni grup adlari icin 15 dk'da bir
static int   g_zoneScanQueue[MAX_GANG_ZONES] = {0};
static int   g_zoneScanQ = 0;          // kuyruktaki ornek zone sayisi
static int   g_zoneScanIdx = -1;       // -1: tarama yok; 0..: siradaki ornek
static DWORD g_zoneScanDeadline = 0;   // ornek icin zaman asimi
static bool  g_zoneScanWaitingTeleport = false; // /pos gˆnderildi, /gzinfo henuz gˆnderilmedi
static float g_zoneScanReturnPos[3] = {0, 0, 0};
static bool  g_zoneScanDone = false;   // oturumda bir kez
static DWORD g_zoneScanWaitStart = 0;  // oto-ba˛latma sayaci (spawn'dan 30 sn sonra)
static DWORD g_zoneScanLastRun = 0;    // son oto tarama zamani (periyod kontrolu)
bool g_loginPending = false;           // UIF Login dialogu aÁ˝k (SAMPRPC extern); kullan˝c˝ ˛ifre girene
                                       // kadar spawn/akislar bekler, tmp hesaba atilmaz.
unsigned long g_loginPendingUntil = 0; // login bekleme timeout (10 sn) - dialog hic gelmezse devam et
static bool  g_zscanAuto = false;      // ?zscan on/off: otomatik aralikli tarama aÁ˝k mi
// Komut mutesi (".?zscan sirasinda sunucu /pos//gzinfo mute'lar"): mute textdraw'i
// gˆrununce bir kez reconnect tetikle (ayn-‚ñí mute penceresinde tekrar etmemesi icin bayrak).
static bool  g_muteReconnectPending = false;

// Freeroam'a gecis: /fr'yi oturum basina EN FAZLA bir kez gˆnder (cmd mute riski yok).
// Birincil tetik: spawn men¸s¸ "Freeroam" cevabi (ScrDialogBox). Yedek tetik: spawn'dan
// 10 sn sonra tek seferlik zaman tabanli gˆnderim (menu hic gelmezse / RPC_Spawn gelmezse).
// SpawnConfirmed (RPC_Spawn) de /fr gˆnderebilir; hepsi g_frSentOnce ile korunur.
static DWORD g_dwFrFallbackStart = 0;

void StartFrRetry()
{
	// Gerek yok: tetikler olay tabanli. (Yedek tetik ProcessFrRetry'de.)
}

void ProcessFrRetry()
{
	if(g_frSentOnce) return;
	if(!iSpawned) { g_dwFrFallbackStart = 0; return; } // yeniden spawn'da timer'i kur
	if(g_dwFrFallbackStart == 0)
		g_dwFrFallbackStart = GetTickCount();
	else if(GetTickCount() - g_dwFrFallbackStart >= 10000)
	{
		g_frSentOnce = true;
		Log("[SPAWN] Spawn men¸s¸ gelmedi/zaman asimi, tek seferlik /fr gˆnderimi.");
		sendServerCommand((char *)"/fr");
	}
}

// Spawn men¸s¸ ("Where would you like to go?") son otomatik secilme zamani.
static DWORD g_dwSpawnMenuHandled = 0;

// -------------------------------------------------------------
// ?zf GRUP TAG HARITASI (isim -> grup tag'i)
// Eslesme YALNIZCA ISIM + TAG ile: zone sahibi /gzinfo'dan ˆrenilen ISIM,
// tag /groups+/gstats harvest'iyla ˆrenilir; group_colors.txt'ye kalici yazilir,
// DB'ye push edilir (?zf flashing zone'lari bu haritayla gruplar). RENK KULLANILMAZ.
// -------------------------------------------------------------
#define MAX_GROUP_COLORS 512 // eskiden 128: DB dolunca harvest yeni gruplari ekleyemiyordu
static struct { char szTag[64]; char szName[64]; } g_groupColors[MAX_GROUP_COLORS];
static int g_groupColorCount = 0;
static const char *g_szColorsFile = "group_colors.txt";
static const char *g_szLangFile    = "bot_lang.txt";
static void SyncPlayerCacheToDb();
static void SyncGroupColorsToDb();

// {RRGGBB} renk kodlarini adlardan temizle + bosluklari normalize et. Tanim asagida.
static void NormalizeGroupName(char *sz);

static int FindGroupColorByTag(const char *szTag)
{
	if(!szTag) return -1;
	// 1) Kisa etiket (tag) eslesmesi
	for(int i = 0; i < g_groupColorCount; i++)
		if(!_stricmp(g_groupColors[i].szTag, szTag)) return i;
	// 2) Tag bulunamazsa TAM ISIM ile dene (kullan˝c˝ tag yerine isim yazabilir):
	//    normalize (bosluk/format farki tolere) + case-insensitive.
	char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), szTag, sizeof(szNeed) - 1);
	NormalizeGroupName(szNeed);
	for(int i = 0; i < g_groupColorCount; i++)
	{
		char szDb[80]; strncpy_s(szDb, sizeof(szDb), g_groupColors[i].szName, sizeof(szDb) - 1);
		NormalizeGroupName(szDb);
		if(!_stricmp(szDb, szNeed)) return i;
	}
	return -1;
}

static void NormalizeGroupName(char *sz); // tanim asagida; FindGroupColorByName'den cagrilir

// Isimle eslesen grup kaydini bul (renk bagimsiz eslesme: /gzinfo isim -> DB isim).
static int FindGroupColorByName(const char *szName)
{
	if(!szName || !szName[0]) return -1;
	char szN[80]; strncpy_s(szN, sizeof(szN), szName, sizeof(szN) - 1);
	NormalizeGroupName(szN);
	for(int i = 0; i < g_groupColorCount; i++)
	{
		char szDb[80]; strncpy_s(szDb, sizeof(szDb), g_groupColors[i].szName, sizeof(szDb) - 1);
		NormalizeGroupName(szDb);
		if(!_stricmp(szDb, szN)) return i;
	}
	return -1;
}

// ?zgo <gruptag>: grubun adini/tag'ini bularak o gruba ait 1 bos (flash etmeyen)
// zone'u doner. Grubun hic zonu yoksa ya da hepsi sava˛taysa -1.
// "Free" -> sahipsiz (sahibi bilinmeyen) ilk zon.
// TAG CAKISMASI: ayni kisa tag'i birden fazla grup kullaniyorsa (sunucu gercegi,
// orn. 'dEV' -> The Devils Organizations + The Devilz Dead Industries) rastgele
// secim yanlis zon gˆnderir. Bu yuzden cakisma varsa -2 doner; cagiran yer
// isimle netlestirme ister (??zgo <tam isim> kullan).
static int FindZoneByGroupTag(const char *szTag)
{
	if(!szTag || !szTag[0]) return -1;
	if(!_stricmp(szTag, "Free"))
	{
		// SADECE taramada gercekten sahipsiz ˆrenilmis zone'lar ("Free" isaretli).
		// Ismi HIC bilinmeyen zone dahil EDILMEZ: bilinmeyen = sahipsiz degil
		// (?zgo Free #2'ye gitti, zone Hall of Malevolence'inisti).
		for(int zid = 0; zid < g_zoneCount; zid++)
			if(!_stricmp(g_zoneOwnerName[zid], "Free"))
				return zid;
		return -1;
	}
	// Tag e-slesmesi: kac grup bu tag'i tasiyor? (tag'ler temiz saklanir, strip gerekmez)
	int iFirst = -1, iCount = 0;
	for(int i = 0; i < g_groupColorCount; i++)
	{
		if(!g_groupColors[i].szTag[0]) continue;
		if(_stricmp(g_groupColors[i].szTag, szTag) == 0)
		{
			if(iFirst < 0) iFirst = i;
			iCount++;
		}
	}
	// Tam isim ile dene (tag yerine isim yazilabilir): normalize + case-insensitive
	if(iCount == 0)
	{
		char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), szTag, sizeof(szNeed) - 1);
		NormalizeGroupName(szNeed);
		for(int i = 0; i < g_groupColorCount; i++)
		{
			char szDb[80]; strncpy_s(szDb, sizeof(szDb), g_groupColors[i].szName, sizeof(szDb) - 1);
			NormalizeGroupName(szDb);
			if(!_stricmp(szDb, szNeed)) { iFirst = i; iCount = 1; break; }
		}
	}
	if(iFirst < 0) return -1;
	if(iCount > 1) return -2; // cakisma: isimle netlestir
	int gi = iFirst;
	// ISIM BAZLI: grubun adini bul, o adi tasiyan zone'u don (flashlanan onecelikli,
	// flash yoksa normal zone). ?zgo o grubun DB'deki zone'una gider.
	const char *pName = g_groupColors[gi].szName;
	if(!pName || !pName[0]) return -1;
	// Bosluk/format farkini onlemek icin normalize karsilastirma.
	char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), pName, sizeof(szNeed) - 1);
	NormalizeGroupName(szNeed);
	int iFallback = -1;
	for(int zid = 0; zid < g_zoneCount; zid++)
	{
		if(!g_zoneOwnerName[zid][0]) continue;
		char szHave[80]; strncpy_s(szHave, sizeof(szHave), g_zoneOwnerName[zid], sizeof(szHave) - 1);
		NormalizeGroupName(szHave);
		if(_stricmp(szHave, szNeed) != 0) continue;
		if(g_zoneFlash[zid]) return zid;   // flashlanan zone ˆncelikli
		if(iFallback < 0) iFallback = zid;
	}
	if(iFallback < 0)
	{
		// Bellek restore basarisiz olabilir (paket kaybi / saldiri): grubun
		// DB deki zone listesini dogrudan backend den iste (geri yazilir).
		const char *pDbName = (iFirst >= 0 && iFirst < g_groupColorCount) ? g_groupColors[iFirst].szName : szNeed;
		if(pDbName && pDbName[0])
		{
			try
			{
				std::string url = std::string("/api/groups/zone?name=") + pDbName;
				for(size_t i = 0; i < url.size(); i++)
					if(url[i] == ' ') url.replace(i, 1, "%20");
				httplib::Client cli(g_szBackendBase);
				cli.set_connection_timeout(4);
				auto res = cli.Get(url.c_str());
				if(res && res->status == 200)
				{
					json j = json::parse(res->body);
					if(j.contains("zones") && j["zones"].is_array())
					{
						for(const auto &z : j["zones"])
						{
							int zid = z.is_number() ? z.get<int>() : atoi(z.get<std::string>().c_str());
							if(zid >= 0 && zid < MAX_GANG_ZONES)
							{
								strncpy_s(g_zoneOwnerName[zid], sizeof(g_zoneOwnerName[0]),
									pDbName, sizeof(g_zoneOwnerName[0]) - 1);
								Log("[ZONE] %s zone DB den bulundu: #%d", pDbName, zid);
								return zid;
							}
						}
					}
				}
			}
			catch(...) {}
		}
	}
	return iFallback;
}

// {RRGGBB} renk kodlarini adlardan temizle (UIF grup adlarinda tasiyabiliyor).
static void StripColorCodes(char *sz)
{
	if(!sz) return;
	char *pW = sz;
	for(const char *pR = sz; *pR; )
	{
		if(*pR == '{')
		{
			const char *pE = strchr(pR, '}');
			if(pE && pE - pR == 7) { pR = pE + 1; continue; } // {RRGGBB} atla
		}
		if(*pR == '<')
		{
			// UIF grup isimlerinde <RRGGBB> / <ffffff> renk kodu da olabiliyor
			const char *pE = strchr(pR, '>');
			if(pE && (pE - pR == 7 || pE - pR == 8)) { pR = pE + 1; continue; }
		}
		if(*pR == '(')
		{
			// (RRGGBB) parantezli renk kodu (UIF "Reu(ffffff)nion" tarzi sizinti)
			const char *pE = strchr(pR, ')');
			if(pE && pE - pR == 7)
			{
				bool bHex = true;
				for(const char *q = pR + 1; q < pE; q++)
					if(!isxdigit((unsigned char)*q)) { bHex = false; break; }
				if(bHex) { pR = pE + 1; continue; }
			}
		}
		*pW++ = *pR++;
	}
	*pW = 0;
}

// Grup adini normalize et: renk kodlarini temizle + icteki coklu bosluklari tek bosluga
// indir + bas/son bosluklari kirp. /gzinfo "owned by group" ile /gstats grup adi arasinda
// bosluk/format farki oluyordu ("The  Death  Squad" vs "The Death Squad") -> isim eslesmesi
// yapilamayip tag "renk bekleniyor" (color 0) kaydediliyordu. Bu normalizasyon renk<->tag
// eslesmesini duzeltir (filtreye gerek kalmaz).
static void NormalizeGroupName(char *sz)
{
	if(!sz) return;
	StripColorCodes(sz);
	char *pW = sz;
	bool bSpace = true;
	for(const char *pR = sz; *pR; pR++)
	{
		if(*pR == ' ' || *pR == '\t' || *pR == '\r' || *pR == '\n')
		{
			if(!bSpace) { *pW++ = ' '; bSpace = true; }
		}
		else
		{
			*pW++ = *pR; bSpace = false;
		}
	}
	*pW = 0;
	// sondaki boslugu kirp
	while(pW > sz && (pW[-1] == ' ')) { pW--; *pW = 0; }
}

static void LoadGroupColors()
{
	// Kullan˝c˝: text dosyasina kayit YOK - DB tek kaynak. (LoadGroupsFromDb
	// ProcessDbSync uzerinden acilista DB'den geri yukler; bu fonksiyon bos.)
	g_groupColorCount = 0;
	Log("[ZF] Text dosyasi yuklemesi kald˝rildi (DB tek kaynak).");
}

// Bugunun tarihi YYYYMMDD sayisi (gunluk zone verisi sifirlama icin).
static int CurrentDayStamp()
{
	time_t t = time(nullptr);
	struct tm lt;
	localtime_s(&lt, &t);
	return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

// balant˝ basinda cagrilir (misc_funcs.sampConnect): yeniden ˆrenme mute riski
void InitGroupColorsFromDisk()
{
	LoadGroupColors();
	// Kendi grup adi kalici kayit DOSYADA DEGIL - DB'de (bots.group_name).
	// SyncZscanFromDb acilista DB'den okur, yoksa /stats ile ˆrenip yazar.
	// Zone->sahip haritasi ARTIK DB'de (gangs.zones JSON): acilista LoadGroupsFromDb
	// geri y-‚ïùkler (ProcessDbSync tetikler). zones.txt KALDIRILDI - DB tek dogru kaynak.
	// Yeniden ba˛latma sonrasi sava˛/timer restore: sava˛ bitis ani (sistem tick)
	// dosyada. GetTickCount() sistem uptime'i oldugu icin restart sonrasi da gecerlidir;
	// bitis ani gecmediyse sava˛ hala suruyor demektir -> kalan sure restore edilir
	// (geri sayim duyurusu + kill list giri˛ii bu sureye baglidir).
	// war_timer.txt KALDIRILDI (kullan˝c˝: dosya yok). Sava˛ durumu sadece hafizada;
	// yeniden ba˛latmada sava˛ bilgisi restore edilmez, taze oturum ba˛lar.
}

static void SaveGroupColors()
{
	// Kullan˝c˝: text dosyasina kayit YOK - DB tek kaynak. Veri ProcessDbSync'in
	// /api/groups push'uyla (60 sn'de bir) DB'ye yazilir.
	Log("[ZF] Text dosyasi kaydi kald˝rildi (DB tek kaynak, %d grup).", g_groupColorCount);
}

// --- DB PUSH (MySQL backend) ---
// player_cache.txt / group_colors.txt'nin kopyasini backend'e de yazar (?pl ve zone
// etiketleri DB'de de dursun). Dosya tek dogru kaynak olmaya devam eder; DB kopyadir.
// Push, main loop'taki ProcessDbSync (60 sn'de bir) tarafindan tetiklenir; buradaki
// iki cagri periyodik akistan, Save*tarafindan degil.
// (g_szBackendBase erken tanim: ?gmembers OnGmSend'in onceki satirlarda kullandigi)

// --- Admin + grup log kaydi ---
// Chat'teki "ADMIN: X banned/muted/kicked/disarmed Y(id) ... (REASON: Z)"
// ve "GROUP: (A)(accid) changed the rank of B(accid) from 'x' to 'y'"
// mesajlari DB'ye kaydedilir (Discord tarafi /alog ile gosterir).
static char g_lastGlogActor[64] = {0}, g_lastGlogTarget[64] = {0};
static char g_lastGlogOld[64] = {0}, g_lastGlogNew[64] = {0};
static long long g_lastGlogActorAcc = 0, g_lastGlogTargetAcc = 0;
static int g_lastGlogType = 0; // 1=rol dei˛iklii, 2=rank dei˛iklii, 3=kick
// ?post 30 sn gerekce penceresi: ?post yazinca beklenir, grup mesaj˝ gerekce olur.
static bool  g_pushPending = false;
static DWORD g_pushUntil = 0;
static char  g_pushReason[256] = {0};
static int   g_pushRequester = -1;
// /pm: Discord'dan hedef oyuncuya mesaj + yanit takibi (yanit gelince Discord'a iletilir).
static bool  g_pmPending = false;
static char  g_pmTarget[64] = {0};    // hedef (oyuncu adi veya ID - kullan˝c˝ yazdigi gibi)
static char  g_pmName[64] = {0};      // cozulen oyuncu ADI (yanit isimle eslesir)
static int   g_pmId = -1;             // cozulen oyuncu ID (yanit (id) ile eslesir)
static char  g_pmRequesterId[64] = {0};
static DWORD g_pmUntil = 0;
// GUVENLIK (/changepass / /email): PM'den deger alinir, /changepass gˆnderilir,
// UIF input diyalogu acar; SAMPRPC diyaloga degeri yazip submit eder.
bool  g_secCmdPending = false;
bool  g_secCmdIsEmail = false;
char  g_secCmdValue[128] = {0};
DWORD g_secCmdAt = 0;
int   g_secCmdCount = 0; // kac diyalog dolduruldu (max 2: yeni + onay/tekrar)

// ASCII token'i kopyala (src'ten ba˛lar, tokenden once durur).
static size_t CopyToken(char *dst, size_t dstLen, const char *src, const char *pEnd)
{
	if(!src || !dst || dstLen == 0) return 0;
	size_t n = 0;
	while(src + n < pEnd && src[n] && n < dstLen - 1)
	{
		dst[n] = src[n];
		n++;
	}
	dst[n] = 0;
	return n;
}

// Admin eylem mesaj˝ni DB'ye POST et: "ADMIN: gandalf banned Bowtac(163) from CNR Mode..."
// Eylem "from X" icermiyorsa (disarm gibi) mod bos kalir - mesaj formatinda mod yoktur.
static void SendAdminLog(const char *admin, const char *target, long long targetId,
	const char *action, const char *mode, int durationMin, const char *reason)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!target || !target[0] || !action || !action[0]) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"hwid", g_myHWID},
			{"adminName", admin ? admin : ""},
			{"targetName", target},
			{"targetId", targetId},
			{"action", action},
			{"mode", mode ? mode : ""},
			{"durationMin", durationMin},
			{"reason", reason ? reason : ""}
		};
		auto res = cli.Post("/api/adminlog", body.dump(), "application/json");
		// Server guild bulamazsa 200 + {success:false} doner (INSERT yapilmaz);
		// client yalnizca status'e bakarsa yanlis "kaydedildi" der. success'i de kontrol et.
		bool bOk = false;
		if(res && res->status == 200)
		{
			try { bOk = json::parse(res->body).value("success", false); } catch(...) { bOk = false; }
		}
		if(bOk)
			Log("[ALOG] Admin log kaydedildi: %s %s(%lld) [%s] (%s)", action, target, targetId, mode ? mode : "-", reason ? reason : "-");
		else
			Log("[ALOG] Admin log KAYDEDILEMEDI (guild bulunamadi/hata): %s %s(%lld) [%s] (%s)", action, target, targetId, mode ? mode : "-", reason ? reason : "-");
	}
	catch(...) {}
}

// Grup rol/rank/kick dei˛iklii mesaj˝ni DB'ye POST et + son kaydi sakla (?post icin).
static void SendGroupLog(const char *actor, long long actorAcc,
	const char *target, long long targetAcc, const char *oldRank, const char *newRank, int type)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!actor || !actor[0] || !target || !target[0]) return;
	strncpy_s(g_lastGlogActor, sizeof(g_lastGlogActor), actor, _TRUNCATE);
	strncpy_s(g_lastGlogTarget, sizeof(g_lastGlogTarget), target, _TRUNCATE);
	strncpy_s(g_lastGlogOld, sizeof(g_lastGlogOld), oldRank ? oldRank : "", _TRUNCATE);
	strncpy_s(g_lastGlogNew, sizeof(g_lastGlogNew), newRank ? newRank : "", _TRUNCATE);
	g_lastGlogActorAcc = actorAcc;
	g_lastGlogTargetAcc = targetAcc;
	g_lastGlogType = type;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"hwid", g_myHWID},
			{"actorName", actor},
			{"actorAcc", actorAcc},
			{"targetName", target},
			{"targetAcc", targetAcc},
			{"oldRank", oldRank ? oldRank : ""},
			{"newRank", newRank ? newRank : ""}
		};
		auto res = cli.Post("/api/grouplog", body.dump(), "application/json");
		if(res && res->status == 200)
			Log("[GLOG] Grup log kaydedildi: %s(%lld) -> %s(%lld) '%s'->'%s'", actor, actorAcc, target, targetAcc, oldRank ? oldRank : "", newRank ? newRank : "");
	}
	catch(...) {}
}

// /pm: Discord'dan hedef oyuncuya mesaj gˆnder (grup chat'inde gˆrunur).
// Format: "[Discord][kullan˝c˝]: mesaj". Yanit gelince Discord'a iletilir.
void StartPm(const char *target, const char *message, const char *discordUser, const char *requesterId)
{
	if(!target || !target[0] || !message || !message[0]) return;

	// Mesaj icerigi: "[Discord][kullan˝c˝]: mesaj" - ozel mesaj olarak gider.
	char szOut[256];
	sprintf_s(szOut, sizeof(szOut), "[Discord][%s]: %s",
		(discordUser && discordUser[0]) ? discordUser : "Discord", message);

	// HEDEF: UIF /pm <oyuncuID> <mesaj> kullanir. Discord'da isim girildiyse
	// player ID sine cevir; rakam girildiyse (ID) aynen kullan. Not: playerInfo
	// bot baglanmadan once oyunda olanlari TANIMAZ (isim bos) - g_playerCache de
	// taranir.
	int iTargetId = -1;
	bool bNumeric = true;
	for(const char *q = target; *q; q++) if(*q < '0' || *q > '9') { bNumeric = false; break; }
	const char *pResolvedName = NULL;
	if(bNumeric)
	{
		int nId = atoi(target);
		if(nId > 0 && nId < MAX_PLAYERS)
		{
			iTargetId = nId;
			if(playerInfo[nId].szPlayerName[0])
				pResolvedName = playerInfo[nId].szPlayerName;
			else if(g_playerCache[nId].szName[0])
				pResolvedName = g_playerCache[nId].szName;
		}
	}
	if(iTargetId < 0)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(playerInfo[i].szPlayerName[0] && _stricmp(playerInfo[i].szPlayerName, target) == 0)
			{ iTargetId = i; pResolvedName = playerInfo[i].szPlayerName; break; }
			if(g_playerCache[i].szName[0] && _stricmp(g_playerCache[i].szName, target) == 0)
			{ iTargetId = i; pResolvedName = g_playerCache[i].szName; break; }
		}
	}

	char szCmd[300];
	if(iTargetId >= 0)
		sprintf_s(szCmd, sizeof(szCmd), "/pm %d %s", iTargetId, szOut);
	else
		sprintf_s(szCmd, sizeof(szCmd), "/pm %s %s", target, szOut); // isimle dene
	g_bridgeSuppressEcho++;
	sendServerCommand(szCmd);
	Log("[PM] Discord'dan ozel mesaj gˆnderildi: hedef '%s' (id %d): %s", target, iTargetId, szOut);

	// Yanit takibi: hedef ADI ve ID ile eslesir (PM yanitinda ikisi de gorunur).
	g_pmName[0] = 0;
	if(pResolvedName && pResolvedName[0])
		strncpy_s(g_pmName, sizeof(g_pmName), pResolvedName, sizeof(g_pmName) - 1);
	g_pmId = iTargetId;
	strncpy_s(g_pmTarget, sizeof(g_pmTarget), target, sizeof(g_pmTarget) - 1);
	strncpy_s(g_pmRequesterId, sizeof(g_pmRequesterId), requesterId ? requesterId : "", sizeof(g_pmRequesterId) - 1);
	g_pmUntil = GetTickCount() + 15 * 60 * 1000; // 15 dk yanit penceresi
	g_pmPending = true;

	// Discord daki ilk embed hedefin GERCEK oyun ici adini gostersin
	// (Discord da "80" yazildiysa embed "cb650r" gostersin).
	if(requesterId && requesterId[0])
	{
		try
		{
			httplib::Client cli(g_szBackendBase);
			cli.set_connection_timeout(3);
			json body = {
				{"requesterId", requesterId},
				{"target", target},
				{"targetName", g_pmName[0] ? g_pmName : target},
				{"message", message}
			};
			cli.Post("/api/pm/sent", body.dump(), "application/json");
		}
		catch(...) {}
	}
}

// ?post: son yakalanan grup aksiyonunu (rol/rank/kick) gerekceyle Discord'a embed gˆnder.
static void PushLastGroupLog(const char *reason)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!g_lastGlogTarget[0]) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"hwid", g_myHWID},
			{"actorName", g_lastGlogActor},
			{"actorAcc", g_lastGlogActorAcc},
			{"targetName", g_lastGlogTarget},
			{"targetAcc", g_lastGlogTargetAcc},
			{"actionType", g_lastGlogType},
			{"oldRank", g_lastGlogOld},
			{"newRank", g_lastGlogNew},
			{"reason", reason ? reason : ""}
		};
		auto res = cli.Post("/api/grouppost", body.dump(), "application/json");
		if(res && res->status == 200)
			Log("[POST] Discord'a gˆnderildi: %s -> %s (tip %d, gerekce: %s).", g_lastGlogActor, g_lastGlogTarget, g_lastGlogType, (reason && reason[0]) ? reason : "-");
		else
			Log("[POST] Discord'a GONDERILEMEDI (HTTP %d).", res ? res->status : -1);
	}
	catch(...) {}
}

// "from 'Eski' to 'Yeni'" kismini ayikla. pFrom 'from ' birakilmis sekilde.
static void ParseRankTransition(const char *pFrom, char *oldRank, size_t oldLen, char *newRank, size_t newLen)
{
	oldRank[0] = 0; newRank[0] = 0;
	if(!pFrom) return;
	const char *pOld = strstr(pFrom, "from '");
	if(!pOld) return;
	pOld += strlen("from '");
	const char *pOldEnd = strchr(pOld, '\'');
	if(!pOldEnd) return;
	CopyToken(oldRank, oldLen, pOld, pOldEnd);
	const char *pTo = strstr(pOldEnd, "to '");
	if(!pTo) return;
	pTo += strlen("to '");
	const char *pToEnd = strchr(pTo, '\'');
	if(pToEnd) CopyToken(newRank, newLen, pTo, pToEnd);
}

// "GROUP: (Harm)(3716972) changed the rank of [YS]FamilyRoots(3752380) from 'F' to 'F'"
static void CheckGroupLogMessage(const char *msg)
{
	const char *pChg = strstr(msg, "changed the rank of");
	int iType = 2;
	if(!pChg){ pChg = strstr(msg, "changed the role of"); iType = 1; }
	if(!pChg)
	{
		// kick: "GROUP: (Ad)(acc) kicked Hedef(acc) ..." ‚Äî rol/rank degil, kick yakala
		const char *pK = strstr(msg, " kicked ");
		if(!pK) return;
		char actor[64] = {0}, target[64] = {0};
		long long actorAcc = 0, targetAcc = 0;
		const char *p1 = strchr(msg, '(');
		const char *p2 = p1 ? strchr(p1 + 1, ')') : NULL;
		const char *p3 = p2 ? strchr(p2 + 1, '(') : NULL;
		const char *p4 = p3 ? strchr(p3 + 1, ')') : NULL;
		if(p1 && p2 && p3 && p4 && p2 < pK)
		{
			CopyToken(actor, sizeof(actor), p1 + 1, p2);
			actorAcc = _strtoi64(p3 + 1, NULL, 10);
		}
		else
		{
			// "(Ad)(acc)" yoksa: mesajdaki ilk kelime (GROUP: on-eki sonrasi)
			const char *q = strstr(msg, "GROUP:") ? strstr(msg, "GROUP:") + 6 : msg;
			while(*q == ' ' || *q == '\t') q++;
			int n = 0; while(q[n] && q[n] != ' ' && q[n] != '\t' && q[n] != '(' && n < 63){ actor[n] = q[n]; n++; } actor[n] = 0;
		}
		// hedef: " kicked " sonrasi paranteze kadar
		const char *pT = pK + strlen(" kicked ");
		while(*pT == ' ') pT++;
		const char *pTO = strchr(pT, '(');
		int tn = 0;
		for(const char *q = pT; *q && *q != ' ' && *q != '\t' && q != pTO && tn < 63; q++) target[tn++] = *q;
		target[tn] = 0;
		const char *pTClose = pTO ? strchr(pTO + 1, ')') : NULL;
		if(pTClose) targetAcc = _strtoi64(pTO + 1, NULL, 10);
		if(!actor[0] || !target[0]) return;
		Log("[GLOG] Kick yakalandi: %s(%lld) -> %s(%lld)", actor, actorAcc, target, targetAcc);
		SendGroupLog(actor, actorAcc, target, targetAcc, "", "Kicked", 3);
		return;
	}
	// yapan: ilk "(Ad)(accid)" cifti
	char actor[64] = {0}, target[64] = {0}, oldRank[64] = {0}, newRank[64] = {0};
	long long actorAcc = 0, targetAcc = 0;
	const char *p1 = strchr(msg, '(');
	const char *p2 = p1 ? strchr(p1 + 1, ')') : NULL;
	const char *p3 = p2 ? strchr(p2 + 1, '(') : NULL;
	const char *p4 = p3 ? strchr(p3 + 1, ')') : NULL;
	if(p1 && p2 && p3 && p4)
	{
		CopyToken(actor, sizeof(actor), p1 + 1, p2);
		actorAcc = _strtoi64(p3 + 1, NULL, 10);
	}
	// hedef: "changed the rank of " sonrasi "Ad(accid)" - ad paranteze kadar
	const char *pTgt = pChg + (strstr(pChg, "changed the rank of") ? strlen("changed the rank of") : strlen("changed the role of"));
	while(*pTgt == ' ') pTgt++;
	const char *pTOpen = strchr(pTgt, '(');
	if(pTOpen)
	{
		CopyToken(target, sizeof(target), pTgt, pTOpen);
		const char *pTClose = strchr(pTOpen + 1, ')');
		if(pTClose) targetAcc = _strtoi64(pTOpen + 1, NULL, 10);
	}
	else
	{
		// ad bilinmiyor olabilir; ilk bosluga kadar al
		const char *pEnd = pTgt;
		while(*pEnd && *pEnd != ' ') pEnd++;
		CopyToken(target, sizeof(target), pTgt, pEnd);
	}
	ParseRankTransition(pChg, oldRank, sizeof(oldRank), newRank, sizeof(newRank));

	if(!actor[0] || !target[0]) return;
	Log("[GLOG] Rol dei˛iklii yakalandi: %s(%lld) -> %s(%lld)", actor, actorAcc, target, targetAcc);
	SendGroupLog(actor, actorAcc, target, targetAcc, oldRank, newRank, iType);

	// --- /grole DIREKT onayi: bot "?grole <isim> <rol>" gˆnderdiyse ve hedef eslesiyorsa ---
	if(g_groleDirectPending && g_szNickName[0] && !_stricmp(actor, g_szNickName) && !_stricmp(target, g_groleTarget.c_str()))
	{
		Log("[GROLE] DIREKT onay alindi: %s rol degisti ('%s' -> '%s'), otomasyon tamam.", target, oldRank, newRank);
		g_groleActive = false;
		g_grolePhase = 0;
		g_groleDirectPending = false;
		g_grTargetItem = -1; g_grDlgId = 0;
	}
}

// "ADMIN: gandalf banned Bowtac(163) from CNR Mode for 180 minutes (REASON: ...)"
static void CheckAdminLogMessage(const char *msg)
{
	static const char *verbs[] = {" banned ", " muted ", " kicked ", " disarmed "};
	const char *pVerb = NULL; const char *verb = NULL;
	for(int i = 0; i < 4; i++)
	{
		const char *p = strstr(msg, verbs[i]);
		if(p && (!pVerb || p < pVerb)) { pVerb = p; verb = verbs[i]; }
	}
	if(!pVerb) return;
	// bazen "ADMIN: isim banned" bazen "isim banned" - basligi temizle
	// admin: pVerb oncesi son kelime ("ADMIN:" on-eki varsa at)
	char admin[64] = {0};
	{
		const char *pStart = msg;
		const char *pColon = strstr(msg, ": ");
		if(pColon && pColon < pVerb) pStart = pColon + 2;
		const char *pEnd = pVerb;
		const char *pLast = NULL;
		for(const char *q = pStart; q < pEnd; q++)
			if(*q == ' ' || *q == '\t') pLast = q;
		CopyToken(admin, sizeof(admin), pLast ? pLast + 1 : pStart, pEnd);
	}
	// hedef: verb sonrasi "Ad(123)" - ad paranteze kadar
	char target[64] = {0}; long long targetId = 0;
	{
		const char *pT = pVerb + strlen(verb);
		while(*pT == ' ') pT++;
		const char *pOpen = strchr(pT, '(');
		const char *pEnd = pOpen ? pOpen : pT;
		while(pEnd > pT && pEnd[-1] == ' ') pEnd--;
		CopyToken(target, sizeof(target), pT, pEnd);
		if(pOpen)
		{
			const char *pClose = strchr(pOpen + 1, ')');
			if(pClose) targetId = _strtoi64(pOpen + 1, NULL, 10);
		}
	}
	if(!target[0]) return;
	char action[16] = {0};
	CopyToken(action, sizeof(action), verb + 1, verb + strlen(verb));
	// mod: "from <Mod> for" veya "from <Mod> ..." - dict'ten sonra.
	// disarm gibi "from X" icermeyen eylemlerde mod bos kalir (mesaj formatinda yok).
	char mode[64] = {0}; int durationMin = 0; char reason[128] = {0};
	{
		const char *pFrom = strstr(pVerb, " from ");
		if(pFrom)
		{
			pFrom += strlen(" from ");
			const char *pEnd = strstr(pFrom, " for ");
			if(!pEnd) pEnd = strstr(pFrom, " (REASON");
			if(!pEnd) pEnd = pFrom + strlen(pFrom);
			CopyToken(mode, sizeof(mode), pFrom, pEnd);
		}
		const char *pFor = strstr(pVerb, " for ");
		if(pFor)
		{
			pFor += strlen(" for ");
			durationMin = atoi(pFor);
		}
		const char *pReason = strstr(pVerb, "(REASON: ");
		if(pReason)
		{
			pReason += strlen("(REASON: ");
			const char *pEnd = strchr(pReason, ')');
			if(pEnd) CopyToken(reason, sizeof(reason), pReason, pEnd);
			else strncpy_s(reason, sizeof(reason), pReason, _TRUNCATE);
		}
	}
	Log("[ALOG] Admin eylem yakalandi: %s %s %s(%lld) mod=%s sure=%d dk sebep=%s", admin, action, target, targetId, mode, durationMin, reason);
	SendAdminLog(admin, target, targetId, action, mode, durationMin, reason);
}

// oyun ici "?alog <id|accid|isim>" sorgusu: DB'den sayi ozeti iste ve oyuna yaz
static void QueryAdminLogSummary(const char *q)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!q || !q[0]) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(5);
		json body = {{"hwid", g_myHWID}, {"q", q}};
		auto res = cli.Post("/api/alog", body.dump(), "application/json");
		if(res && res->status == 200)
		{
			// "!" prefix gruba iletim icin; ozet uL.Hold formatinda (backend uretir)
		std::string out = "!" + std::string(res->body);
			g_bridgeSuppressEcho++;
			sendChat((char *)out.c_str());
			Log("[ALOG] ?alog yaniti: %s", res->body.c_str());
		}
		else
			Log("[ALOG] ?alog sorgusu ba˛ar˝s˝z (HTTP %d)", res ? res->status : -1);
	}
	catch(...) {}
}

static void SyncPlayerCacheToDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;

	json arr = json::array();
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(g_playerCache[i].dwTime == 0) continue;
		arr.push_back({
			{"id", i},
			{"name", g_playerCache[i].szName[0] ? g_playerCache[i].szName : "?"},
			{"packetloss", g_playerCache[i].dLoss},
			{"fps", g_playerCache[i].iFps},
			{"ping", g_playerCache[i].iPing}
		});
	}
	if(arr.empty()) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {{"players", arr}};
		auto res = cli.Post("/api/player/cache", body.dump(), "application/json");
		if(res && res->status == 200) Log("[PL-CACHE] %d oyuncu DB'ye push edildi.", (int)arr.size());
	}
	catch(...) {}
}

// Acilista DB'den BUGUNKU grup+tag+zone haritasini geri y-‚ïùkler.
// GET /api/groups sunucu tarafinda DATE(updated_at)=CURDATE() filtreli - dunun
// verisi otomatik elenir. Tag'ler de buradan gelir.
// RESTORE TAMAMLANANA KADAR tum DB yazimlari KILITLIDIR (asagidaki bayrak):
// yoksa bos hafiza DB'ye yazip var olan zone haritasini siliyordu.
static bool s_bGroupDbRestored = false;
static DWORD s_dwRestoreLastTry = 0;

static bool LoadGroupsFromDb()
{
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		auto res = cli.Get("/api/groups");
		if(!res || res->status != 200) return false;
		json j = json::parse(res->body);
		if(!j.value("success", false)) return false;
		int nZones = 0, iMaxId = -1;
		if(j.contains("groups") && j["groups"].is_array())
		{
			for(const auto &g : j["groups"])
			{
				std::string name = g.value("name", "");
				std::string tag = g.value("tag", "");
				if(name.empty()) continue;
				// Grup kaydini garanti et (tag ile birlikte)
				int gi = FindGroupColorByName(name.c_str());
				if(gi < 0 && g_groupColorCount < MAX_GROUP_COLORS)
				{
					gi = g_groupColorCount++;
					strncpy_s(g_groupColors[gi].szName, sizeof(g_groupColors[0].szName),
						name.c_str(), sizeof(g_groupColors[0].szName) - 1);
					strncpy_s(g_groupColors[gi].szTag, sizeof(g_groupColors[0].szTag),
						tag.c_str(), sizeof(g_groupColors[0].szTag) - 1);
				}
				else if(gi >= 0 && tag[0] && strcmp(g_groupColors[gi].szTag, tag.c_str()) != 0)
					strncpy_s(g_groupColors[gi].szTag, sizeof(g_groupColors[0].szTag),
						tag.c_str(), sizeof(g_groupColors[0].szTag) - 1);
				// Kalici zone->sahip haritasini geri y-‚ïùkle
				if(g.contains("zones") && g["zones"].is_array())
				{
					for(const auto &zv : g["zones"])
					{
						int zid = zv.is_number() ? zv.get<int>() : atoi(zv.get<std::string>().c_str());
						if(zid < 0 || zid >= MAX_GANG_ZONES) continue;
						strncpy_s(g_zoneOwnerName[zid], sizeof(g_zoneOwnerName[0]),
							name.c_str(), sizeof(g_zoneOwnerName[0]) - 1);
						if(zid > iMaxId) iMaxId = zid;
						nZones++;
					}
				}
			}
		}
		if(iMaxId + 1 > g_zoneCount)
			g_zoneCount = iMaxId + 1;
		SaveGroupColors(); // DB'den gelen tagler yerel dosyaya da yazilsin
		Log("[ZF] DB'den geri y-‚ïùklendi: %d grup, %d zone sahipligi.", g_groupColorCount, nZones);
		return true;
	}
	catch(...) { return false; }
}

static void SyncGroupColorsToDb()
{
	// Ogrenilen grup adi + kisa tag + KALICI sahip olunan TUM zone id'ler tek
	// satir (group_name upsert). Flashing filtresi YOK - kalici harita. zones.txt
	// YOK: DB acilista GET /api/groups ile geri y-‚ïùklenir (LoadGroupsFromDb).
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	// KILIT: DB'den ilk restore tamamlanmadan YAZMA. Bos hafiza ile yazim,
	// DB'deki zone haritasini silip botu her acilista sifirdan tarama zorluyordu.
	if(!s_bGroupDbRestored) return;

	json arr = json::array();
	for(int i = 0; i < g_groupColorCount; i++)
	{
		if(!g_groupColors[i].szName[0]) continue;
		json zids = json::array();
		for(int z = 0; z < MAX_GANG_ZONES; z++)
			if(g_zoneOwnerName[z][0] && _stricmp(g_zoneOwnerName[z], g_groupColors[i].szName) == 0)
				zids.push_back(z);
		arr.push_back({{"name", g_groupColors[i].szName}, {"tag", g_groupColors[i].szTag}, {"zones", zids}});
	}
	if(arr.empty()) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {{"groups", arr}};
		auto res = cli.Post("/api/groups", body.dump(), "application/json");
		if(res && res->status == 200) Log("[ZF] %d grup/tag + zone listesi DB'ye push edildi (1 grup = 1 satir).", (int)arr.size());
	}
	catch(...) {}
}

// Flash'tan dusen zone: kirmizi yanip somneyi birakti (grup rengine dond-‚ïù). Zone
// DB'de kayitli degil (tag gruplara ait); sadece yerel durum sifirlanir ve
// zones.txt guncellenir.
void RemoveZoneFromDb(int zid)
{
	if(zid < 0 || zid >= MAX_GANG_ZONES) return;
	// Yerel durumu sifirla: flash bitti. "Free" isaretli zone sahipsiz kalmaya
	// devam eder - isaret korunur (yoksa bilinmeyen sayilip tekrar taranirdi).
	g_zoneFlash[zid] = false;
	if(_stricmp(g_zoneOwnerName[zid], "Free") != 0)
		g_zoneOwnerName[zid][0] = 0;
	// Kalici zone->sahip haritasi DB'de; degisim periyodik sync ile yazilir.
}


// Her botun XML'deki ismiyle (g_szNickName) eslestigi satiri DB'ye upsert eder.
// "data" JSON: botun anlik durum verileri (/api/bot/status, backend bots tablosu).
static bool g_zscanDbLoaded = false; // toggle DB'den okundu mu (asagi: SyncZscanFromDb)
static void SyncBotStatusToDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!g_szNickName[0]) return;

	// Botun verileri: bilinen oyuncu sayisi + aktif zone war + bot adi.
	int nPlayers = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
		if(playerInfo[i].szPlayerName[0]) nPlayers++;
	json data = {
		{"bot", g_szNickName},
		{"playerId", g_myPlayerID},
		{"oyuncuSayisi", nPlayers},
		{"zoneWar", g_inZoneWar}
	};
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {{"name", g_szNickName}, {"data", data}};
		// groupName YALNIZCA doluyken gˆnder: bos gˆnderilirse DB deki kayitli
		// grup adi her acilista silinir, bot yeniden ˆrenmek zorunda kalir.
		if(!g_myGroupName.empty())
			body["groupName"] = g_myGroupName;
		// zscan/gzt/zgrp tercihlerini SADECE DB'den okunduktan SONRA gˆnder.
		// (Yoksa acilis aninda varsayilan degerler DB'deki kayitli tercihi ezdi:
		// ?zscan on sonraki restart'ta "kapal˝" olarak gˆruluyordu.)
		if(g_zscanDbLoaded)
		{
			body["zscan"] = g_zscanAuto ? 1 : 0;
			body["gzt"] = g_gztOn ? 1 : 0;
			body["zgrp"] = g_zgrpAuto ? 1 : 0;
		}
		auto res = cli.Post("/api/bot/status", body.dump(), "application/json");
		if(res && res->status == 200) Log("[BOT] '%s' durumu DB'ye yazildi.", g_szNickName);
	}
	catch(...) {}
}

// ?zscan on/off tercihini gruba gˆre DB'den oku (acilis/ilgili zamanlarda). Grup
void StartZoneScan(bool bAuto); // tanim asagida (netrpc.cpp); SyncZscanFromDb'den cagrilir
// adi biliniyorsa GET /api/bot/toggles ile cekilip g_zscanAuto/g_gztOn uygulanir.
bool g_waitingForGroupName = false;   // /stats sonucundan kendi grup adi okunuyor (SAMPRPC extern)
int  g_groupNamePhase = 0;            // 0=idle, 1=ilk diyalog, 2=ikinci diyalog (grup adi)
DWORD g_groupNameReqTime = 0;         // zaman asimi (SAMPRPC extern)

// /stats sonucundan ˆrenilen KENDI GRUP ADI. (SAMPRPC cagirir - diyalog ilk satiri.)
void LearnOwnGroupName(const char *szName)
{
	if(!szName || !szName[0]) return;
	// Mevcut ad ayniysa is yok; farkliysa guncelle + DB'ye yaz.
	if(!g_myGroupName.empty() && _stricmp(g_myGroupName.c_str(), szName) == 0) return;
	bool bChanged = !g_myGroupName.empty();
	g_myGroupName = szName;
	Log("[GSTATS] Kendi grup adi ˆrenildi: %s%s", szName,
		bChanged ? " (GRUP DEGISTI - toggle yeniden okunacak)" : "");
	if(bChanged)
		g_zscanDbLoaded = false; // grup degisti: yeni grubun zscan/gzt/zgrp tercihi okunsun
	SyncBotStatusToDb(); // groupName'i DB'ye yaz (zscan/gzt/zgrp dogru kaydedilsin)
}
void StartGrole(const char *target, const char *role)
{
	if(!target || !target[0] || !role || !role[0]) return;
	g_groleActive = true;
	g_groleTarget = target;
	g_groleRole = role;
	g_grolePhase = 1;
	g_grolePage = 0;
	g_groleStartTime = GetTickCount();
	g_grTargetItem = -1; g_grDlgId = 0; g_grClickAttempt = -1; g_grClickAt = 0;
	// Direkt "?grole <isim> <rol>" chat komutu UIF'de calismiyor (test edildi);
	// /grole dialog otomasyonu kullanilir.
	g_groleDirectPending = false;
	g_groleDirectAt = 0;
	Log("[GROLE] Otomasyon ba˛latildi: '%s' -> '%s' (/grole gˆnderiliyor)...", target, role);
	sendServerCommand((char *)"/grole");
}
void StartGrank(const char *target, const char *rank)
{
	if(!target || !target[0] || !rank || !rank[0]) return;
	g_grankActive = true;
	g_grankTarget = target;
	g_grankRank = rank;
	g_grankPhase = 1;
	g_grankPage = 0;
	g_grankStartTime = GetTickCount();
	g_grTargetItem = -1; g_grDlgId = 0; g_grClickAttempt = -1; g_grClickAt = 0; g_grDblPhase = 0;
	Log("[GRANK] Otomasyon ba˛latildi: '%s' -> '%s' (/grank gˆnderiliyor)...", target, rank);
	sendServerCommand((char *)"/grank");
}
void StartGroleList(const char *requesterId, const char *guildId, const char *hwid)
{
	if(!requesterId || !requesterId[0]) return;
	g_groleListActive = true;
	g_groleListRequesterId = requesterId;
	g_groleListGuildId = guildId ? guildId : "";
	g_groleListHwid = hwid ? hwid : "";
	g_groleListMembers.clear();
	g_groleListPage = 0;
	g_groleListStartTime = GetTickCount();
	Log("[GROLE-LIST] Uye listesi cekiliyor (req=%s)...", requesterId);
	sendServerCommand((char *)"/grole");
}
void FinishGroleListAndPost()
{
	if(g_groleListMembers.empty()){
		Log("[GROLE-LIST] Liste bos, gˆnderilmiyor.");
		g_groleListActive = false;
		return;
	}
	Log("[GROLE-LIST] Toplam %d uye toplandi, Discord'a gˆnderiliyor...", (int)g_groleListMembers.size());
	try{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"requesterId", g_groleListRequesterId},
			{"guildId", g_groleListGuildId},
			{"hwid", g_groleListHwid},
			{"members", g_groleListMembers}
		};
		auto res = cli.Post("/api/grole/list", body.dump(), "application/json");
		if(res && res->status==200) Log("[GROLE-LIST] Discord'a gˆnderildi.");
		else Log("[GROLE-LIST] Gˆnderilemedi (HTTP %d)", res?res->status:-1);
	}catch(...){}
	g_groleListActive = false;
	g_groleListMembers.clear();
	g_groleListPage = 0;
}
void StartGrankList(const char *requesterId, const char *guildId, const char *hwid)
{
	if(!requesterId || !requesterId[0]) return;
	g_grankListActive = true;
	g_grankListRequesterId = requesterId;
	g_grankListGuildId = guildId ? guildId : "";
	g_grankListHwid = hwid ? hwid : "";
	g_grankListMembers.clear();
	g_grankListPage = 0;
	g_grankListStartTime = GetTickCount();
	Log("[GRANK-LIST] Uye listesi cekiliyor (req=%s)...", requesterId);
	sendServerCommand((char *)"/grole");
}
void FinishGrankListAndPost()
{
	if(g_grankListMembers.empty()){
		Log("[GRANK-LIST] Liste bos, gˆnderilmiyor.");
		g_grankListActive = false;
		return;
	}
	Log("[GRANK-LIST] Toplam %d uye toplandi, Discord'a gˆnderiliyor...", (int)g_grankListMembers.size());
	try{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"requesterId", g_grankListRequesterId},
			{"guildId", g_grankListGuildId},
			{"hwid", g_grankListHwid},
			{"members", g_grankListMembers}
		};
		auto res = cli.Post("/api/grank/list", body.dump(), "application/json");
		if(res && res->status==200) Log("[GRANK-LIST] Discord'a gˆnderildi.");
		else Log("[GRANK-LIST] Gˆnderilemedi (HTTP %d)", res?res->status:-1);
	}catch(...){}
	g_grankListActive = false;
	g_grankListMembers.clear();
	g_grankListPage = 0;
}
static void SyncZscanFromDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(g_zscanDbLoaded) return;
	// Grup adi bilinmiyorsa: /stats (kendi ID'miz) ile ˆren. /stats once oyuncu
	// stats diyalogu acar, "Group Stats" butonuna (button 1) basinca grup adini
	// gosteren ikinci diyalog gelir. (NOT: "logged in - Ominous" ranktir, grup
	// adi degil - ona guvenme.)
	if(g_myGroupName.empty())
	{
		// Zaman asimi: 15 sn icinde ˆrenilemezse beklemeyi sifirla; sonraki tick'te
		// yeniden /stats dener. SAMPRPC gstats timeout'undan (20sn) BAGIMSIZ - yoksa
		// gstats kapaninca bu da sonsuza dek kilitlenirdi (wait=1 takilmasi).
		if(g_waitingForGroupName && (GetTickCount() - g_groupNameReqTime > 15000))
		{
			g_waitingForGroupName = false;
			g_groupNamePhase = 0;
			Log("[GSTATS] Grup adi beklemasi zaman asimina ugradi, yeniden deneniyor.");
		}
		// SPAWN GATE: /stats yalnizca spawn sonrasi gˆnderilir. Spawn men¸s¸/
		// gecisi sirasinda gˆnderilirse sunucu grup stats diyalogunu bastiriyor.
		if(!g_waitingForGroupName && iSpawned && g_zoneScanIdx < 0 && !g_ghpActive && !g_inZoneWar)
		{
			// ONCE DB'DEN OKU (bots.group_name - dosya yok, DB tek kaynak). Kayitli
			// grup adi varsa /stats ˆrenmesi ATLANIR; yoksa asagida /stats ile ˆrenilir.
			bool bLoadedFromDb = false;
			if(g_szNickName[0])
			{
				try
				{
					httplib::Client cli(g_szBackendBase);
					cli.set_connection_timeout(3);
					std::string url = std::string("/api/bot/status?name=") + g_szNickName;
					auto res = cli.Get(url.c_str());
					if(res && res->status == 200)
					{
						json j;
						try { j = json::parse(res->body); }
						catch(...)
						{
							Log("[GSTATS] DB yukleme: JSON parse hatasi: %.120s", res->body.c_str());
						}
						std::string dbName = j.value("groupName", "");
						if(!dbName.empty())
						{
							g_myGroupName = dbName;
							Log("[GSTATS] Grup adi DB'den yuklendi: '%s'", dbName.c_str());
							bLoadedFromDb = true;
						}
						else
							Log("[GSTATS] DB yukleme: groupName bos (cevap: %.120s)", res->body.c_str());
					}
					else
						Log("[GSTATS] DB yukleme: HTTP %d", res ? res->status : -1);
				}
				catch(...) { Log("[GSTATS] DB yukleme: balant˝ hatasi."); }
			}
			else
				Log("[GSTATS] DB yukleme atlandi: g_szNickName bos.");
			if(!bLoadedFromDb)
			{
				g_waitingForGroupName = true;
				g_groupNamePhase = 1;
				g_groupNameReqTime = GetTickCount();
				// /stats akisi SAMPRPC g_waitingForGStats blogunda islenir: flag'leri
				// cmds.cpp'deki gibi kur (yoksa Player Stats diyalogu AUTOCLOSE'a dusuyor).
				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
				g_gstatsAutoSubmit = true;
				g_gstatsInput = "";
				g_gstatsRequesterId = "";
				g_gstatsSentFirst = false;
				char szCmd[64];
				sprintf_s(szCmd, sizeof(szCmd), "/stats %d", g_myPlayerID);
				Log("[GSTATS] Grup adi icin /stats %d gˆnderiliyor (Group Stats butonu bekleniyor)...", g_myPlayerID);
				sendServerCommand(szCmd);
			}
		}
		else if(!g_waitingForGroupName)
		{
			// Spawn beklenirken log spam'i olmasin: 2 sn'de bir bildir.
			static DWORD s_lastGroupNameBlkLog = 0;
			if(s_lastGroupNameBlkLog == 0 || GetTickCount() - s_lastGroupNameBlkLog >= 2000)
			{
				s_lastGroupNameBlkLog = GetTickCount();
				Log("[GSTATS] Grup adi ˆrenme bekliyor (spawn=%d scan=%d ghp=%d war=%d)...",
					iSpawned, g_zoneScanIdx, g_ghpActive, g_inZoneWar);
			}
		}
		return; // grup adi bilinene kadar bekler, sonraki tick'te tekrar dener
	}
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		std::string url = std::string("/api/bot/toggles?group=") + g_myGroupName;
		auto res = cli.Get(url.c_str());
		if(res && res->status == 200)
		{
			json j = json::parse(res->body);
			bool bOn = (j.value("zscan", 0) == 1);
			bool bGzt = (j.value("gzt", 1) == 1);
			bool bZgrp = (j.value("zgrp", 0) == 1); // default KAPALI: oto harvest elle (?zgrp/?tagscan) calisir
			g_zscanAuto = bOn;
			g_gztOn = bGzt;
			g_zgrpAuto = bZgrp;
			g_zscanDbLoaded = true;
			Log("[BOT] Grup '%s' icin kayitli durum: zscan=%s, gzt=%s, zgrp=%s.",
				g_myGroupName.c_str(), bOn ? "A«IK" : "KAPALI", bGzt ? "A«IK" : "KAPALI", bZgrp ? "A«IK" : "KAPALI");
			if(bOn && s_bGroupDbRestored && g_zoneScanIdx < 0 && iSpawned)
				StartZoneScan(true); // restore sonrasi ilk tarama (bilinmeyen-only)
		}
	}
	catch(...) {}
}

// Periyodik (main loop, her tick'te): restore + zscan toggle + cache'leri DB'ye yazar.
static DWORD g_lastDbSyncTick = 0;
void ProcessDbSync(){
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	// BACKEND SAGLIK KONTROLU: Discord botu (backend) calisma sirasinda kapanirsa
	// client de kendini kapatir (60 sn'de bir kontrol). Backend'siz bot islevsizdir.
	static DWORD s_lastBackendCheck = 0;
	if(s_lastBackendCheck == 0 || GetTickCount() - s_lastBackendCheck >= 60000)
	{
		s_lastBackendCheck = GetTickCount();
		bool bAlive = false;
		try
		{
			httplib::Client cli(g_szBackendBase);
			cli.set_connection_timeout(3);
			auto res = cli.Get("/api/bot/status");
			if(res && res->status == 200) bAlive = true;
		}
		catch(...) {}
		if(!bAlive)
		{
			Log("[BACKEND] Discord botu (backend) yanit vermiyor, client kapatiliyor.");
			MessageBoxA(NULL,
				"Discord botu (backend sunucusu) calismayi durdurdu.\n\n"
				"RakSAMP kapatiliyor. Backend'i yeniden ba˛latin ve tekrar deneyin.",
				"RakSAMP - Balant˝ Kesildi",
				MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}
	}
	// Login beklentisi main.cpp ana dongusunde yonetilir (g_loginPending iken bu
	// fonksiyon cagrilmaz). Acilis: BUGUNKU grup+tag+zone haritasini DB'den geri y-‚ïùkle (bir kez, 5 sn
	// aralikli retry). Bu BASARILI OLANA KADAR SyncGroupColorsToDb kilitli -
	// boylece var olan DB verisi bos push ile silinemez.
	if(!s_bGroupDbRestored)
	{
		DWORD dwNowR = GetTickCount();
		if(s_dwRestoreLastTry == 0 || dwNowR - s_dwRestoreLastTry >= 5000)
		{
			s_dwRestoreLastTry = dwNowR;
			s_bGroupDbRestored = LoadGroupsFromDb();
		}
	}
	// Gun degisiminde eski zone verisini temizle (kullan˝c˝: 22 agustos verisi
	// 23 agustosta silinmeli). GET /api/groups zaten bugunku kayitlari verir;
	// burada yerel temizlik + push yapilir.
	static int s_dbLastDay = CurrentDayStamp();
	int iToday = CurrentDayStamp();
	if(iToday != s_dbLastDay)
	{
		s_dbLastDay = iToday;
		Log("[ZF] Gun degisti (%d): eski zone sahiplik verisi temizleniyor.", iToday);
		for(int z = 0; z < MAX_GANG_ZONES; z++)
			g_zoneOwnerName[z][0] = 0;
		SyncGroupColorsToDb(); // bos zone listeleriyle bugunun kayitlari yazilir
	}
	// zscan/gzt/zgrp toggle'ini HER TICK'te dene (60 sn throttle YOK): grup adi
	// sonradan ˆrenilirse (ornek: bot acilip gruba girdikten sonra) bayrak kurulsun.
	// g_zscanDbLoaded true olunca iceride tekrar denenmez (bir kez).
	SyncZscanFromDb();
	DWORD dwNow = GetTickCount();
	if(dwNow - g_lastDbSyncTick < 60000) return;
	g_lastDbSyncTick = dwNow;
	SyncPlayerCacheToDb();
	SyncGroupColorsToDb();
	SyncBotStatusToDb();
}

// Grup dili: ?tr / ?en ile dei˛tirilir; varsayilan INGILIZCE. Tek dogru kaynak
// MongoDB'dir (Guild.lang); Security.cpp polling ile senkronlar. bot_lang.txt
// sadece boot anindaki ilk deger (poll gelene kadar) icindir.
static const char *LM(const char *tr, const char *en) { return g_langTurkish ? tr : en; }

static void LoadLang()
{
	FILE *f = fopen(g_szLangFile, "r");
	if(f)
	{
		char szBuf[16] = {0};
		if(fgets(szBuf, sizeof(szBuf), f) && szBuf[0] == 't')
			g_langTurkish = true;
		fclose(f);
	}
	Log("[LANG] Grup dili: %s", g_langTurkish ? "Turkce" : "English");
}

void SaveLang()
{
	FILE *f = fopen(g_szLangFile, "w");
	if(f) { fputs(g_langTurkish ? "tr\n" : "en\n", f); fclose(f); }
	Log("[LANG] Grup dili kaydedildi: %s", g_langTurkish ? "tr" : "en");
}

// Bir zone'un sahibini verilen grup adiyla esle ve kalici kaydet (ISIM bazli, renk yok).
static void BindZoneGroup(int iZone, const char *szName)
{
	if(!szName || !szName[0] || iZone < 0 || iZone >= MAX_GANG_ZONES) return;

	char szBuf[80];
	strncpy_s(szBuf, sizeof(szBuf), szName, _TRUNCATE);
	NormalizeGroupName(szBuf);
	while(szBuf[0] && szBuf[strlen(szBuf)-1] == ' ') szBuf[strlen(szBuf)-1] = 0;
	if(!szBuf[0]) return;

	// zone -> sahip isim (renk bagimsiz eslesme). Dikkat: bu, /gstats "ilk zone"
	// eslemesinden gelir - GUVENILMEZ olabiliyor (yanlis gruba yaziyordu: #26'ya Kosmk).
	// Sahibi /gzinfo taramasiyla zaten dogru ˆrenildiyse (doluysa) EZME; bos ise fallback yaz.
	if(iZone >= 0 && iZone < MAX_GANG_ZONES && g_zoneOwnerName[iZone][0] == 0)
		strncpy_s(g_zoneOwnerName[iZone], sizeof(g_zoneOwnerName[0]), szBuf, sizeof(g_zoneOwnerName[0]) - 1);

	// Isim DB'de varsa ek is yok; yoksa yeni kayit (tag BOS - harvest ogrenir).
	// Eskiden tag=isim konuyordu: DB tag==isim placeholder doluyor, ?zf isim
	// gosteriyordu. Bos tag = "bilinmiyor", harvest /gstats ile gercegini ogrenir.
	if(FindGroupColorByName(szBuf) < 0 && g_groupColorCount < MAX_GROUP_COLORS)
	{
		strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
			szBuf, sizeof(g_groupColors[0].szName) - 1);
		g_groupColors[g_groupColorCount].szTag[0] = 0;
		g_groupColorCount++;
		SaveGroupColors();
		Log("[ZFU:LEARN] '%s' ˆrenildi (zone #%d)", szBuf, iZone);
	}
}

// -------------------------------------------------------------
// ?pl ISTATISTIK CACHE'I
// UIF'in /pnetstats cevabi FPS icermez (FPS: --); FPS dahil tam istatistik YS.Hold'un
// "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w" grup mesajlarinda gelir. Bu cache
// ?pl cevabina FPS'i (ve eksik loss/ping'i) koyar; player_cache.txt ile restart'a dayanir.
// Isimler de buraya yazilir: grup basligindan ˆrenilen ad, ?pl cevabinda "hakixld"
// yerine gercek kullan˝c˝ adinin gˆrunmesini saglar.
// -------------------------------------------------------------
static const char *g_szPlayerCacheFile = "player_cache.txt";

void LoadPlayerCache()
{
	memset(g_playerCache, 0, sizeof(g_playerCache));
	int nLoaded = 0;
	FILE *f = fopen(g_szPlayerCacheFile, "r");
	if(f)
	{
		char szLine[256];
		while(fgets(szLine, sizeof(szLine), f))
		{
			int iId = -1; char szName[64] = {0}; double dLoss = -1.0; int iFps = -1, iPing = -1;
			if(sscanf(szLine, "%d %63s %lf %d %d", &iId, szName, &dLoss, &iFps, &iPing) == 5
				&& iId >= 0 && iId < MAX_PLAYERS)
			{
				g_playerCache[iId].dwTime = 1; // "biliniyor" isareti (0 = yok)
				strncpy_s(g_playerCache[iId].szName, sizeof(g_playerCache[0].szName), szName, _TRUNCATE);
				g_playerCache[iId].dLoss = dLoss;
				g_playerCache[iId].iFps = iFps;
				g_playerCache[iId].iPing = iPing;
				nLoaded++;
			}
		}
		fclose(f);
	}
	Log("[PL-CACHE] %d oyuncu istatistigi dosyadan yuklendi.", nLoaded);
}

static void SavePlayerCache()
{
	FILE *f = fopen(g_szPlayerCacheFile, "w");
	if(f)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(g_playerCache[i].dwTime == 0) continue;
			fprintf(f, "%d %s %.2f %d %d\n", i, g_playerCache[i].szName,
				g_playerCache[i].dLoss, g_playerCache[i].iFps, g_playerCache[i].iPing);
		}
		fclose(f);
	}
}

// "Anahtar: deger" deseninden ilk sayiyi ceker; sayi yoksa -1.0 (FPS: N/A gibi)
static double NumberAfter(const char *pText, const char *szKey)
{
	if(!pText || !szKey) return -1.0;
	const char *p = strstr(pText, szKey);
	if(!p) return -1.0;
	p += strlen(szKey);
	while(*p == ' ' || *p == '\t') p++;
	if(!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return -1.0;
	return atof(p);
}

// ?zf: flashing zone'lari grup tag'larina gˆre sayip gruba bildirir (YS bot gibi).
// Cikti TEK mesajdir (SA-MP 128 karakter siniri): en buyuk gruplar once, tasarsa
// kesilir. Bilinmeyen grup rengi gˆruldugunde kendi flash taramamiz (/gzinfo)
// ba˛latilir - YS botu gibi yeni gruplari kendi kendine ˆrenir.

// ?zf sonrasi: bilinmeyen renkli flashing zone'lar icin tarama ba˛latildiysa
// tarama bittiginde guncel ozet TEKRAR gˆnderilir (artik renkler biliniyor).
static bool g_zfScanResend = false;
static int  g_zfFollowUps = 0;         // tarama sonu takip turu sayisi (sonsuz dongu kilidi)
static void StartFlashScan();          // ?zf: sahibi bilinmeyen flashing zone'lari ˆren

// ?zf cikti parcasi: "!Flashing Zones: Tag(n) | ..." (Tek veya [1/2] [2/2]).
// UIF'te "!" on eki = gruba mesaj (hakixld: "!..." -> (GROUP) kanali).
struct FlashZoneItem { char szLabel[64]; int n; };
static void SendFlashSlice(const FlashZoneItem items[], int iFrom, int iTo, int iNum, int iOf)
{
	char szOut[140]; int nOut = 0;
	// Tarama/kill-list capture suruyorsa "capturing" belirt (kullan˝c˝: "capture ediyorsa
	// capturing yazsin"). g_zoneScanIdx >= 0 = zone taramasi aktif; g_zwCapturingKillList = kill list.
	const char *pCap = (g_zoneScanIdx >= 0 || g_zwCapturingKillList) ? " (capturing)" : "";
	// YS botu gibi: parca sayisi 1 ise [1/1] GOSTERMEZ (duz baslik), birden fazlaysa [iNum/iOf].
	if(iOf > 1)
		nOut += sprintf_s(szOut + nOut, sizeof(szOut) - nOut, "![%d/%d] Flashing Zones%s: ", iNum, iOf, pCap);
	else
		nOut += sprintf_s(szOut + nOut, sizeof(szOut) - nOut, "!Flashing Zones%s: ", pCap);
	bool bFirst = true;
	for(int i = iFrom; i < iTo; i++)
	{
		char szOne[96]; int nL = sprintf_s(szOne, sizeof(szOne), "%s%s(%d)", bFirst ? "" : " | ", items[i].szLabel, items[i].n);
		if(nOut + nL >= 120) break; // parca siniri (SA-MP 128): kalanini diger parca yazar
		bFirst = false;
		sprintf_s(szOut + nOut, sizeof(szOut) - nOut, "%s", szOne);
		nOut += nL;
	}
	g_bridgeSuppressEcho++;
	Log("[ZF] Dagilim[%d/%d]: %s", iNum, iOf, szOut);
	sendChat(szOut);
}

void BuildFlashZoneSummary()
{
	struct { int gi; int n; } agg[256];
	int aggCount = 0, total = 0;
	for(int zid = 0; zid < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue;
		// RENK BAGIMSIZ eslesme: flashing zone'un sahibi /gzinfo'dan ˆrenilen ISIM
		// (g_zoneOwnerName) ile DB'deki isim->tag eslenir. Renk geri donusumu ayni rengi
		// farkli gruplara bagliyordu; isim eslesmesi bunu cozer (YS.Hold: "only
		// dependent on flash"). gi: -1 = isim bilinmiyor (?).
		int gi = -1;
		if(g_zoneOwnerName[zid][0] != 0)
		{
			gi = FindGroupColorByName(g_zoneOwnerName[zid]);
			if(gi < 0) gi = -1;
		}
		int a = 0;
		for(; a < aggCount; a++) if(agg[a].gi == gi) break;
		if(a == aggCount && aggCount < 256) { agg[aggCount].gi = gi; agg[aggCount].n = 0; aggCount++; }
		if(a < aggCount) { agg[a].n++; total++; }
	}

	// count'a gˆre buyukten kucuge
	for(int i = 0; i < aggCount; i++)
		for(int j = i + 1; j < aggCount; j++)
			if(agg[j].n > agg[i].n)
			{
				int ti = agg[i].gi; int tn = agg[i].n;
				agg[i].gi = agg[j].gi; agg[i].n = agg[j].n;
				agg[j].gi = ti; agg[j].n = tn;
			}

	// Bilinmeyen (sahipli ama haritada olmayan) renk var mi? Varsa bu renklerin
	// flashing zone'larina tek tek teleport olup /gzinfo ile ˆren (YS botu gibi
	// kendi taramamiz). Ozet ONCE mevcut bilgiyle gˆnderilir, tarama bitince tekrar.
	bool bUnknown = false;
	for(int a = 0; a < aggCount && !bUnknown; a++)
	{
		if(agg[a].gi == -2) continue; // renk 0 = sahipsiz/free, ˆrenmeye gerek yok
		if(agg[a].gi < 0) bUnknown = true; // -1: hicbir gruba eslesmeyen renk
	}

	// YS botu gibi: "Flashing Zones: Tag(n) | ..." - etiketler once toplanir;
	// uzunluga gˆre Tek veya [1/2]+[2/2] iki mesaj.
	FlashZoneItem items[256];
	int nItems = 0;
	for(int a = 0; a < aggCount && nItems < 256; a++)
	{
		int idx = agg[a].gi; // -2 free, -1 bilinmeyen, >=0 grup indeksi
		const char *pszLabel = "Free";
		// "NULL"/"-"/"~" = UIF'te tag'siz grup (gercek tag yok), isimle gˆsterilir.
		bool bNoTag = (idx >= 0 && !g_groupColors[idx].szTag[0])
			|| (idx >= 0 && (!_stricmp(g_groupColors[idx].szTag, "NULL") || !strcmp(g_groupColors[idx].szTag, "-")
				|| !strcmp(g_groupColors[idx].szTag, "~")));
		if(agg[a].gi == -2 || (idx >= 0 && !g_groupColors[idx].szTag[0] && !g_groupColors[idx].szName[0]))
			pszLabel = "Free";
		else if(idx >= 0 && !bNoTag)
			pszLabel = g_groupColors[idx].szTag;   // kisa etiket ˆncelikli
		else if(idx >= 0)
			pszLabel = g_groupColors[idx].szName;  // tag yoksa tam ad
		else
			pszLabel = "capturing";              // bilinmeyen: tarama sonrasi etiket ˆrenilir
		char szLabelClean[64] = {0};
		strncpy_s(szLabelClean, sizeof(szLabelClean), pszLabel, sizeof(szLabelClean)-1);
		StripColorCodes(szLabelClean); // {RRGGBB} / <RRGGBB> renk kodlarini temizle
		strncpy_s(items[nItems].szLabel, sizeof(items[0].szLabel), szLabelClean, sizeof(items[0].szLabel) - 1);
		items[nItems].n = agg[a].n;
		nItems++;
	}

	// nItems==0: gˆrunur bolgede bilinen grup yok - BO-S mesaj atma (bo-s "Flashing
	// Zones: " gruba gidip "mesaj atm-‚ñíyor" hissi verir). Bilinmeyen renk varsa
	// tarama ba˛lat (yalnizca flashing zonelere gider); ˆrenilince tekrar ozet atilir.
	if(nItems == 0)
	{
		Log("[ZF] Bilinen flashing grup yok (bolge bos veya hepsi bilinmeyen), mesaj atilmadi. Toplam %d zone.", total);
		// Tag ˆrenme yalnizca ?zgrp ile (elle): otomatik /gstats spam'i mute riski.
		return;
	}

	// YS botu gibi dinamik parcalama: 1 mesaja ~5 grup / ~105 char sigar. Grup sayisi
	// artarsa otomatik [1/N] [2/N]... bolesin (her zaman [1/1] degil). Parcalari once
	// char hesabiyla planla, sonra sirayla gˆnder.
	struct { int from, to; } parts[16];
	int nParts = 0, curLen = 0, curStart = 0;
	for(int i = 0; i < nItems && nParts < 16; i++)
	{
		int len = (int)strlen(items[i].szLabel) + 6; // "Tag(n) | " tahmini
		if(curLen + len > 105 && i > curStart)
		{
			parts[nParts].from = curStart; parts[nParts].to = i;
			nParts++; curStart = i; curLen = 0;
		}
		curLen += len;
	}
	if(nParts < 16) { parts[nParts].from = curStart; parts[nParts].to = nItems; nParts++; }
	for(int p = 0; p < nParts; p++)
		SendFlashSlice(items, parts[p].from, parts[p].to, p + 1, nParts);
	Log("[ZF] Toplam %d flashing zone (gˆrunen bolge), %d parca.", total, nParts);
	// Bilinmeyen renk ˆrenme yalnizca ?zgrp (elle): otomatik /gstats spam'i mute riski.
}

// --- Oto renk taramasi: renk -> grup adi (gzinfo), zgo mantigiyla ---

static void GangZoneScanNext();
static void GangZoneScanGo(int iZone);

// /gzinfo dialogu geldi: "Zone Name: # N" -> zone id, "owned by group "X"" -> ad.
// Sahipsiz cikarsa renk "Free" (bos tag) olarak isaretlenir; tekrar kontrol edilmez.
static void GangZoneScanGzDialog(const char *szInfo)
{
	int iZone = -1;
	const char *pZone = strstr(szInfo, "Zone Name: #");
	if(pZone)
	{
		pZone += strlen("Zone Name: #");
		while(*pZone == ' ') pZone++;
		iZone = atoi(pZone);
	}

	// Ogrenme guard'i: dialog baska/eski bir zone icinse ˆrenme (iZone -1 = parse hatasi
	// dahil). Stale dialog atlanir; siradaki ornege gecisi 7 sn'lik deadline yapar.
	if(g_zoneScanIdx >= 0 && iZone != g_zoneScanQueue[g_zoneScanIdx])
	{
		Log("[ZF:SCAN] Dialog zone #%d, beklenen ornek #%d degil; stale dialog atlaniyor.",
			iZone, g_zoneScanQueue[g_zoneScanIdx]);
		return;
	}

	char szOwner[80] = {0};
	const char *pOwn = strstr(szInfo, "owned by group");
	// Not: sahip adi renk bilinse de ayrismali - bilinen kisa etiket (GeMs gibi)
	// buradan tam isme (The Youngsters Family gibi) yukseltilir.
	if(pOwn)
	{
		const char *q = strchr(pOwn, '"');
		const char *r = q ? strchr(q + 1, '"') : NULL;
		if(q && r && r > q + 1)
		{
			int n = (int)(r - q - 1);
			if(n > 79) n = 79;
			memcpy(szOwner, q + 1, n);
			szOwner[n] = 0;
			NormalizeGroupName(szOwner);
		}
	}
	// ISIM BAZLI ˆrenme: /gzinfo'nun "owned by group X" ismi -> DB isim->tag eslemesi.
	// Sahipsiz zone -> "Free" sahte grubu: ?zgo free bu zone'lara gider, DB'ye de
	// Free grubunun zone listesi olarak kaydedilir.
	if(!szOwner[0]) strncpy_s(szOwner, sizeof(szOwner), "Free", _TRUNCATE);
	if(g_groupColorCount < MAX_GROUP_COLORS)
	{
		int idx = FindGroupColorByName(szOwner);
		if(idx < 0)
		{
			// Yeni grup: isim + tag BOS (tag harvest ile ogrenilir; placeholder
			// tag=isim DB ye isim olarak kayit ediliyordu - ?zf isim gosteriyordu).
			strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
				szOwner, sizeof(g_groupColors[0].szName) - 1);
			g_groupColors[g_groupColorCount].szTag[0] = 0;
			g_groupColorCount++;
			SaveGroupColors();
			Log("[ZF:SCAN] Yeni grup ˆrenildi: '%s'", szOwner);
		}
	}
	Log("[ZF:SCAN] Ornek zone #%d: %s", iZone, szOwner);

	// zone -> sahip grup adi: ?zf flashing zone'lari bu isimle DB'deki tag'e esler.
	if(iZone >= 0 && iZone < MAX_GANG_ZONES)
		strncpy_s(g_zoneOwnerName[iZone], sizeof(g_zoneOwnerName[0]), szOwner, sizeof(g_zoneOwnerName[0]) - 1);

	GangZoneScanNext();
}

static void GangZoneScanNext()
{
	if(g_zoneScanIdx < 0) return;
	g_zoneScanIdx++;
	if(g_zoneScanIdx >= g_zoneScanQ)
	{
		g_zoneScanIdx = -1;
		g_zoneScanDone = true;
		g_muteReconnectPending = false; // tarama bitti, sonraki scan/mute icin sifirla
		Log("[ZF:SCAN] Tarama tamam (%d grup haritada).", g_groupColorCount);

		// Serbest zone listesi: sahibi bilinmeyen veya "Free" isaretli olanlar
		// (renk kullanilmaz - eslesme tamamen isim bazli).
		char szFree[2048]; int nF = 0;
		for(int zid = 0; zid < g_zoneCount; zid++){
			if(g_zoneOwnerName[zid][0] == 0 || !_stricmp(g_zoneOwnerName[zid], "Free")){
				int r = sprintf_s(szFree + nF, sizeof(szFree) - nF, " #%d", zid);
				if(r < 0) break;
				nF += r;
				if(nF >= (int)sizeof(szFree)-16) break;
			}
		}
		szFree[nF < (int)sizeof(szFree) ? nF : (int)sizeof(szFree)-1] = 0;
		Log("[ZF:SCAN] Serbest zone'lar:%s", nF > 0 ? szFree : " (yok)");

		// ?zf bilinmeyen renkler icin tarama ba˛latmissa: guncel ozeti tekrar gˆnder
		// (artik renkler ˆrenildi, "?" yerine isimler gˆrunecek).
		// SPAM ONLEME: ?zf komutu disinda otomatik ozet ATILMAZ (her tag ˆreniminde
		// gruba mesaj gidiyordu). Yalnizca isaret sifirlanir; kullan˝c˝ ?zf atinca gˆrur.
		g_zfScanResend = false;

		// Tarama bitti: ˆrenilen zone->grup+tag haritasini DB'ye kaydet (kalici harita).
		// (bot yeniden ba˛layinca zoneler "sifirlanmasin").
		SyncGroupColorsToDb();

		char szCmd[128];
		sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f",
			g_zoneScanReturnPos[0], g_zoneScanReturnPos[1], g_zoneScanReturnPos[2]);
		sendServerCommand(szCmd);
		// Tag'ler /groups + /gstats ile YALNIZCA ?zgrp / ?tagscan (elle) ile ˆrenilir;
		// tarama bitince otomatik /groups GONDERILMEZ (her seferinde tag bulmaya calisip
		// /gstats spam'i + mute riski yaratiyordu).
		// Tarama surerken flashlanip atlanan bilinmeyen zone kald˝ysa eve donusten
		// SONRA bir tur daha (#147 vakasi: "Atlanildi (scan aktif)" -> 15 dk'ya kaliyordu).
		// Sinirsiz dongu olmasin diye oturum basina 5 takip turu.
		if(g_zfFollowUps < 5)
		{
			g_zfFollowUps++;
			StartFlashScan();
		}
		return;
	}
	// Komut mute korumasi: /pos + /gzinfo cift komut hizli arka arkaya gidince sunucu
	// mute atiyordu. Zone'lar arasi minimum bekleme (Sleep) mute'yi onler; flashing-only
	// tarama az zone oldugu icin toplam sure yine makul (<1-2 dk).
	Sleep(2000);
	GangZoneScanGo(g_zoneScanQueue[g_zoneScanIdx]);
}

static void GangZoneScanGo(int iZone)
{
	float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
	float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
	// 1. faz: teleport. Sunucu konumu i-sleyene kadar /gzinfo G-√ªNDER-‚ñëLMEZ -
	// ayni anda giderse cevap eski konumun zone'unu dondurur (stale zinciri).
	g_zoneScanWaitingTeleport = true;
	g_zoneScanDeadline = GetTickCount() + ZONE_SCAN_TELEPORT_SYNC_MS;
	Log("[ZF:SCAN] Ornek zone #%d taran˝yor -> /pos (%s)", iZone, szCmd);
	sendServerCommand(szCmd);
}

void StartZoneScan(bool bAuto)
{
	if(g_zoneScanIdx >= 0 || !iSpawned || g_inZoneWar)
	{
		Log("[ZF:SCAN] StartZoneScan atlandi (scanAktif=%d spawned=%d war=%d).",
			g_zoneScanIdx >= 0, iSpawned, g_inZoneWar);
		return;
	}

	// Tarama dengesi (kullan˝c˝): bilinen zone'lari HER turda yeniden taramak
	// bosu bosuna vakit kaybi; hic taramamak da veriyi bozar. Cozum:
	// Normal tur  -> YALNIZCA sahibi bilinmeyen flashing zone'lar.
	// Her 4. tur  -> TAM KONTROL: bilinenler de yeniden dogrulanir (bayat veri).
	// ILK GIRIS: s_iScanPass=1'den ba˛lar -> ilk tur bilinmeyen-only (DB'de var
	// olanlar atlanir, sadece eksikler ˆrenilir). Yarinki giri˛ gun degisince
	// LoadGroupsFromDb bos donecek, tum zone'lar "bilinmeyen" sayilip taraniacak.
	static int s_iScanPass = 1;
	bool bFullVerify = (s_iScanPass % 4 == 0);
	s_iScanPass++;

	g_zoneScanQ = 0;
	for(int zid = 0; zid < g_zoneCount && g_zoneScanQ < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue; // YALNIZCA flashing zone'lar
		if(g_zoneOwnerName[zid][0] != 0) continue; // DB'de var olani atla, sadece yeni flashlanan
		g_zoneScanQueue[g_zoneScanQ++] = zid;
	}
	if(g_zoneScanQ == 0)
	{
		g_zoneScanLastRun = GetTickCount(); // periyodu yenile: sonraki deneme ZONE_SCAN_PERIOD_MS sonra
		Log("[ZF:SCAN] Tarama icin zone yok (%s, periyod 15 dk'ya resetlendi).",
			bFullVerify ? "tam kontrol" : "bilinmeyen");
		return;
	}
	Log("[ZF:SCAN] Tarama basliyor (%s mod, %d zone).", bFullVerify ? "TAM KONTROL" : "bilinmeyen-only", g_zoneScanQ);

	float fPos[3] = {0, 0, 0};
	if(!getPlayerPos(g_myPlayerID, fPos))
	{
		g_zoneScanWaitStart = 0; // oto-ba˛latma 30 sn sonra tekrar deneyecek
		Log("[ZF:SCAN] Pozisyon alinamadi, tarama ba˛latilmadi (oto-retry).");
		return;
	}
	g_zoneScanReturnPos[0] = fPos[0];
	g_zoneScanReturnPos[1] = fPos[1];
	g_zoneScanReturnPos[2] = fPos[2];

	g_zoneScanIdx = 0;
	g_zoneScanLastRun = GetTickCount(); // oto aralikli tarama icin son ba˛latma anii
	Log("[ZF:SCAN] %d zone taramas-‚ñí basliyor (%s).", g_zoneScanQ, bAuto ? "oto" : "manuel ?zscan");
	GangZoneScanGo(g_zoneScanQueue[0]);
}

// ?zf: mevcut flashing zone'lardan sahibi BILINMEYEN olanlari tek tek ornek se-gip
// teleport + /gzinfo ile ˆrenir (YS botu yerine kendi taramamiz). Tarama bitince
// GangZoneScanNext guncel ozeti tekrar gˆnderir.
static void StartFlashScan()
{
	if(g_zoneScanIdx >= 0 || !iSpawned) { Log("[ZF:FLASHSCAN] Atlanildi (scan aktif=%d spawned=%d).", g_zoneScanIdx >= 0, iSpawned); return; }

	g_zoneScanQ = 0;
	for(int zid = 0; zid < g_zoneCount && g_zoneScanQ < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue;      // yalnizca flashing zone'lar
		if(g_zoneOwnerName[zid][0] != 0) continue; // isim biliniyor, ˆrenmeye gerek yok
		bool bDup = false;
		for(int q = 0; q < g_zoneScanQ; q++)
			if(g_zoneScanQueue[q] == zid) { bDup = true; break; }
		if(!bDup)
			g_zoneScanQueue[g_zoneScanQ++] = zid;
	}
	if(g_zoneScanQ == 0) return; // bilinmeyen flashing zone yok

	float fPos[3] = {0, 0, 0};
	if(!getPlayerPos(g_myPlayerID, fPos)) { Log("[ZF:FLASHSCAN] Pozisyon alinamadi, tarama ba˛latilmadi."); return; }
	g_zoneScanReturnPos[0] = fPos[0];
	g_zoneScanReturnPos[1] = fPos[1];
	g_zoneScanReturnPos[2] = fPos[2];

	g_zoneScanIdx = 0;
	g_zfScanResend = true; // tarama bitince guncel ozet yeniden gˆnderilir (artik tag'ler biliniyor)
	Log("[ZF:FLASHSCAN] %d sahibi bilinmeyen flashing zone ˆreniliyor.", g_zoneScanQ);
	GangZoneScanGo(g_zoneScanQueue[0]);
}

// Her ana dongu tick'inde: oto-tarama (spawn'dan 30 sn sonra, sonra her 15 dk'da bir) +
// stale renk temizligi. Zone war sirasinda tarama bekletilir (teleport yok).
void StartGrCandidateClicks(int rawRow, int item, unsigned short dlgId, int page, const char *firstCol)
{
	g_grRawRow = rawRow;
	g_grTargetItem = item;
	g_grDlgId = dlgId;
	g_grPage = page;
	g_grClickAttempt = -1;
	g_grClickAt = 0;
	g_grDblPhase = 0;
	g_grRowText[0] = 0;
	if(firstCol && firstCol[0]) strncpy_s(g_grRowText, sizeof(g_grRowText), firstCol, sizeof(g_grRowText) - 1);
	Log("[GROLE] Uye tiklama adaylari ba˛latildi (ham satir=%d, item=%d, sayfa=%d, sutun='%s').", rawRow, item, page, g_grRowText);
}

static void SendGrClick(int item, const char *szInput)
{
	RakNet::BitStream bs;
	bs.Write((uint16_t)g_grDlgId);
	bs.Write((uint8_t)1);
	bs.Write((uint16_t)item);
	BYTE len = szInput ? (BYTE)strlen(szInput) : 0;
	bs.Write((uint8_t)len);
	if(len) bs.Write(szInput, len);
	pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
}

// Uye tiklama adaylari: BUTON 0 = sayfalama (log'da kanitlandi), secim BUTON 1 (OK)
// ile olur ve UIF'de CI FT TIKLAMA gerekir. TABLIST inputtext = satirin ILK SUTUNU
// (renksiz) ‚Äî sunucu uyeyi bundan taniyor olabilir. Adaylar: (item, input) kombinasyonlari.
void ProcessGrCandidateClicks()
{
	if(g_grTargetItem < 0 || g_grDlgId == 0) return;
	if(!g_groleActive && !g_grankActive) return;
	if(g_groleActive && g_grolePhase != 1) return;
	if(g_grankActive && g_grankPhase != 1) return;
	if(g_grClickAttempt < 0){ g_grClickAttempt = 0; g_grDblPhase = 0; g_grClickAt = GetTickCount() + 100; return; }
	if(g_grClickAttempt >= 5 || GetTickCount() < g_grClickAt) return;
	int item;
	const char *szInput;
	const char *szName = g_groleActive ? g_groleTarget.c_str() : g_grankTarget.c_str();
	switch(g_grClickAttempt){
		case 0: item = g_grRawRow;                              szInput = g_grRowText; break; // 29 + ilk sutun
		case 1: item = g_grTargetItem;                          szInput = g_grRowText; break; // 14 + ilk sutun
		case 2: item = g_grTargetItem + g_grPage * 25 + 1;      szInput = g_grRowText; break; // 40 + ilk sutun
		case 3: item = g_grTargetItem;                          szInput = szName;      break; // 14 + isim
		default: item = g_grRawRow;                             szInput = szName;      break; // 29 + isim
	}
	if(g_grDblPhase == 0){
		Log("[GROLE] Aday %d/5: item=%d input='%s' cift tik (1/2)...", g_grClickAttempt + 1, item, szInput);
		SendGrClick(item, szInput);
		g_grDblPhase = 1;
		g_grClickAt = GetTickCount() + 300;
	}
	else if(g_grDblPhase == 1){
		Log("[GROLE] Aday %d/5: item=%d input='%s' cift tik (2/2)...", g_grClickAttempt + 1, item, szInput);
		SendGrClick(item, szInput);
		g_grDblPhase = 2;
		g_grClickAt = GetTickCount() + 1500;
	}
	else{
		g_grClickAttempt++;
		g_grDblPhase = 0;
		g_grClickAt = GetTickCount() + 100;
	}
}

// Faz-2 ikinci tik: rol/rank secimini tamamlar (cift tik = UIF'de onay).
static void SendGrRoleSecondClick()
{
	RakNet::BitStream bs;
	bs.Write((uint16_t)g_grRoleDlgId);
	bs.Write((uint8_t)1);
	bs.Write((uint16_t)g_grRoleItem);
	BYTE len = (BYTE)strlen(g_grRoleInput);
	bs.Write((uint8_t)len);
	if(len) bs.Write(g_grRoleInput, len);
	pRakClient->RPC(&RPC_DialogResponse, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
	g_grRoleDblPending = false;
	if(g_grRoleIsGrank){
		Log("[GRANK] Otomasyon tamam: %s -> %s", g_grankTarget.c_str(), g_grankRank.c_str());
		g_grankActive = false;
		g_grankPhase = 0;
	}
	else{
		Log("[GROLE] Otomasyon tamam: %s -> %s", g_groleTarget.c_str(), g_groleRole.c_str());
		g_groleActive = false;
		g_grolePhase = 0;
	}
}

void ProcessZoneScan()
{
	// ?post gerekce penceresi doldu mu? (30 sn) ‚Äî gerekce yazilmadiysa bos gˆnderilir,
	// Discord tarafi "gerekce verilmedi" yazar.
	if(g_pushPending && GetTickCount() > g_pushUntil)
	{
		g_pushPending = false;
		Log("[POST] 30 sn doldu, gˆnderiliyor (gerekce: '%s').", g_pushReason[0] ? g_pushReason : "-");
		PushLastGroupLog(g_pushReason[0] ? g_pushReason : "");
	}
	// Faz-2 rol/rank cift tik ikinci tiklamasi.
	if(g_grRoleDblPending && GetTickCount() >= g_grRoleDblAt)
		SendGrRoleSecondClick();
	// /grole DIREKT komut ("?grole <isim> <rol>") 4 sn icinde "changed the role of"
	// onayi uretmediyse: /grole dialog otomasyonuna gec (hala phase=1 page=0'sa).
	// (Direkt komut diyalog acmissa phase 1 zaten ilerliyordur; tekrar /grole gˆnderilmez.)
	if(g_groleActive && g_groleDirectPending && GetTickCount() > g_groleDirectAt && g_grolePhase == 1 && g_grolePage == 0)
	{
		g_groleDirectPending = false;
		Log("[GROLE] Direkt komut onay vermedi, /grole dialog otomasyonuna gecilir (hedef '%s').", g_groleTarget.c_str());
		sendServerCommand((char *)"/grole");
	}
	// Hedef bulunduysa uye tiklama adaylarini dene (buton/item bilinmiyor).
	ProcessGrCandidateClicks();

	// ?zgrp harvest kilidi cozum: /gstats sonucu gelmezse SAMPRPC g_waitingForGStats'i
	// sifirlar (yalnizca dialog geldiginde kontrol eder - hic dialog gelmezse flag A«IK
	// kalir) ama g_ghpWaitingGstats true kalir -> zincir takilir. Mutlak zaman asimi:
	// istekten 50 sn gectiyse beklemeyi birak. Dialog cevabi sunucuda gecikebiliyor;
	// ayni komutu tekrar gˆnderince spam/mute riski artar, SADECE bir kez tekrar dener.
	if(g_ghpActive && g_ghpWaitingGstats)
	{
		bool bHadTimeout = (g_gstatsReqTime != 0 && GetTickCount() - g_gstatsReqTime > 50000);
		if(bHadTimeout || !g_waitingForGStats)
		{
			if(g_ghpStaleRetries < 1)
			{
				g_ghpStaleRetries++;
				Log("[GHP] gstats bekleyisi zaman asimi, adim bir kez tekrar deneniyor.");
				g_ghpWaitingGstats = false;
				g_ghpNextStepAt = 0;
				ProcessGhpHarvestStep();
			}
			else
			{
				// Ikinci deneme de cevapsiz: adimi atlas (tag'i olmayan grup, sonraki
				// oturumda tekrar denenir). Komut spam'i mute getirir.
				Log("[GHP] gstats yanit vermedi (mute/sunucu), adim atlaniyor.");
				g_ghpWaitingGstats = false;
				g_ghpNextStepAt = GetTickCount() + GHP_STEP_DELAY_MS;
				g_ghpIdx++;
				ProcessGhpHarvestStep();
			}
		}
	}

	// Komut mute korumasi: siradaki /gstats ancak GHP_STEP_DELAY_MS gecikme
	// dolunca gˆnderilir (OnGStatsResultForHarvest deadline'i kurar).
	if(g_ghpActive && !g_ghpWaitingGstats && g_ghpNextStepAt != 0 && GetTickCount() >= g_ghpNextStepAt)
	{
		ProcessGhpHarvestStep();
	}

	if(g_zoneScanIdx < 0)
	{
		// Otomatik ?zgrp: g_zgrpAuto aÁ˝kken TEK SEferlik calisir (spawn sonrasi
		// ilk tur). Periyodik DEGIL: harvest bitince g_zgrpAuto otomatik kapanir.
		// SADECE eksik tag olan grup varsa ba˛lar: restore sonrasi g_groupColors
		// dolu ama tag'i bos/placeholder (tag==isim) olan grup varsa harvest ile
		// kisa tag cekilir. Tum tagler tamamsa GEREKSIZ /groups+ /gstats spam'i
		// atilmaz (kullan˝c˝ istegi).
		DWORD dwNow3 = GetTickCount();
		bool bNeedHarvest = false;
		// GRUP ADI GATE: harvest yalnizca kendi grup adi ˆrenildikten SONRA ba˛lar
		// (ˆrenme 1 kereliktir, /stats -> Group butonu; harvest /gstats diyaloglari
		// ˆrenme faziyla ayni basligi tasir, cakisirlardi).
		if(g_zgrpAuto && s_bGroupDbRestored && iSpawned && !g_myGroupName.empty() && (g_ghpLastRun == 0 || GetTickCount() - g_ghpLastRun >= GHP_AUTO_PERIOD_MS)
			&& g_spawnTick != 0 && (GetTickCount() - g_spawnTick) >= 60000) // spawn dan 1 dk koruma
		{
			// Eksik tag tespiti: tag bos veya placeholder (tag == isim) olan grup
			for(int gi = 0; gi < g_groupColorCount; gi++)
			{
				if(!g_groupColors[gi].szName[0]) continue;
				if(!g_groupColors[gi].szTag[0]
					|| _stricmp(g_groupColors[gi].szTag, g_groupColors[gi].szName) == 0)
				{ bNeedHarvest = true; break; }
			}
		}
		if(bNeedHarvest && !g_ghpActive && !g_inZoneWar && !g_zgoActive)
		{
			g_ghpLastRun = dwNow3;
			g_ghpLearnedCount = 0;
			Log("[GHP] Eksik tag'li grup var, otomatik harvest basliyor (/groups, tek seferlik)%s.",
				g_zscanAuto ? " - bitince zone taramasi gelecek" : "");
			g_waitingForGroups = true;
			g_groupsReqTime = GetTickCount();
			sendServerCommand((char *)"/groups");
		}
		// ?zscan on ise otomatik aralikli tarama: spawn'dan sonra ve ZONE_SCAN_PERIOD_MS
		// aralikla zone'lar taranir (?zscan off ile kapanir). Tarama 25+ dk surebilir,
		// bu yuzden periyod genis tutulur; sava˛/zgo/harvest sirasinda bekler.
		// DB RESTORE ONCE: restore tamamlanmadan tarama ba˛lamaz (yoksa var olan
		// zone verisi "bilinmiyor" sanilip yeniden taran˝yordu).
		// GRUP ADI GATE: tarama da kendi grup adi ˆrenilmeden ba˛lamaz (ˆrenme
		// ˆnceliklidir, sonra harvest/zscan devam eder).
		// HARVEST SONRASI OTOMATIK: harvest bitti, yeni veri varsa zone scan tetikle (zscan off olsa bile).
		else if(s_bGroupDbRestored && !g_myGroupName.empty() && iSpawned && !g_ghpActive && !g_inZoneWar && !g_zgoActive && (g_zscanAuto || g_forceZscanAfterHarvest))
		{
			bool bForce = g_forceZscanAfterHarvest;
			g_forceZscanAfterHarvest = false;
			if(bForce || g_zoneScanLastRun == 0 || dwNow3 - g_zoneScanLastRun >= ZONE_SCAN_PERIOD_MS)
			{
				StartZoneScan(true);
			}
		}
		return;
	}
	if(g_inZoneWar || g_zgoActive) return; // sava˛/zgo surecinde tarama bekler
	DWORD dwNow = GetTickCount();
	if(dwNow <= g_zoneScanDeadline) return;

	// 1. faz bitti (teleport sync): simdi /gzinfo gˆnder, cevap icin yeni deadline.
	if(g_zoneScanWaitingTeleport)
	{
		g_zoneScanWaitingTeleport = false;
		Log("[ZF:SCAN] Ornek zone #%d icin /gzinfo gˆnderiliyor.", g_zoneScanQueue[g_zoneScanIdx]);
		g_zoneScanDeadline = dwNow + ZONE_SCAN_PER_SAMPLE_MS;
		sendServerCommand((char *)"/gzinfo");
		return;
	}

	// 2. faz: /gzinfo cevabi gelmedi, siradaki ornege gec.
	Log("[ZF:SCAN] Ornek zone #%d icin dialog gelmedi, atlaniyor.", g_zoneScanQueue[g_zoneScanIdx]);
	GangZoneScanNext();
}

void ServerJoin(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CHAR szPlayerName[256];
	PLAYERID playerId;
	BYTE byteNameLen=0;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	
	bsData.Read(playerId);
	int iUnk = 0;
	bsData.Read(iUnk);
	BYTE bIsNPC = 0;
	bsData.Read(bIsNPC);
	bsData.Read(byteNameLen);
	if(byteNameLen > 20) return;
	bsData.Read(szPlayerName,byteNameLen);
	szPlayerName[byteNameLen] = '\0';
	
	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 1;
	playerInfo[playerId].byteIsNPC = bIsNPC;
	strcpy((char *)playerInfo[playerId].szPlayerName, szPlayerName);

	// Zone war aktifken GRUP UYESI bir oyuncu baglanirsa gruba gz2 bildirimi at.
	// (g_knownGroupMembers: kendi grubumuzun sohbetinde gˆrulen uyeler - botun grubu.
	//  Oyunun TUM oyuncularina degil, sadece kendi grubuna bildirilir.)
	if(g_inZoneWar && playerId != g_myPlayerID && g_knownGroupMembers[playerId] && !g_zwWelcomed[playerId])
	{
		g_zwWelcomed[playerId] = true;
		Log("[WELCOME] Grup uyesi (%d) %s baglandi, sava˛ aktif (%s).", playerId, szPlayerName, g_enemyGroupName.c_str());
		static char szWarNotice[192];
		sprintf_s(szWarNotice, sizeof(szWarNotice),
			LM("!%s oyuna kat˝ld˝. Zone war devam ediyor! /gz2 yazarak sava˛a katil.",
			   "!%s joined. Zone war is ongoing! Type /gz2 to join the fight."),
			szPlayerName);
		sendChat(szWarNotice);
	}

	//Log("***[JOIN] (%d) %s", playerId, szPlayerName);
}

void ServerQuit(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteReason;

	bsData.Read(playerId);
	bsData.Read(byteReason);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 0;
	playerInfo[playerId].byteIsNPC = 0;
	//Log("***[QUIT:%d] (%d) %s", byteReason, playerId, playerInfo[playerId].szPlayerName);
	memset(playerInfo[playerId].szPlayerName, 0, 20);	
}

void InitGame(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsInitGame((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID MyPlayerID;
	bool bLanMode, bStuntBonus;
	BYTE byteVehicleModels[212];

	bool m_bZoneNames, m_bUseCJWalk, m_bAllowWeapons, m_bLimitGlobalChatRadius;
	float m_fGlobalChatRadius, m_fNameTagDrawDistance;
	bool m_bDisableEnterExits, m_bNameTagLOS, m_bManualVehicleEngineAndLight;
	bool m_bShowPlayerTags;
	int m_iShowPlayerMarkers;
	BYTE m_byteWorldTime, m_byteWeather;
	float m_fGravity;
	int m_iDeathDropMoney;
	bool m_bInstagib;

	bsInitGame.ReadCompressed(m_bZoneNames);
	bsInitGame.ReadCompressed(m_bUseCJWalk);
	bsInitGame.ReadCompressed(m_bAllowWeapons);
	bsInitGame.ReadCompressed(m_bLimitGlobalChatRadius);
	bsInitGame.Read(m_fGlobalChatRadius);
	bsInitGame.ReadCompressed(bStuntBonus);
	bsInitGame.Read(m_fNameTagDrawDistance);
	bsInitGame.ReadCompressed(m_bDisableEnterExits);
	bsInitGame.ReadCompressed(m_bNameTagLOS);
	bsInitGame.ReadCompressed(m_bManualVehicleEngineAndLight); // 
	bsInitGame.Read(iSpawnsAvailable);
	bsInitGame.Read(MyPlayerID);
	bsInitGame.ReadCompressed(m_bShowPlayerTags);
	bsInitGame.Read(m_iShowPlayerMarkers);
	bsInitGame.Read(m_byteWorldTime);
	bsInitGame.Read(m_byteWeather);
	bsInitGame.Read(m_fGravity);
	bsInitGame.ReadCompressed(bLanMode);
	bsInitGame.Read(m_iDeathDropMoney);
	bsInitGame.ReadCompressed(m_bInstagib);

	// Server's send rate restrictions
	if(!settings.uiForceCustomSendRates)
	{
		bsInitGame.Read(iNetModeNormalOnfootSendRate);
		bsInitGame.Read(iNetModeNormalIncarSendRate);
		bsInitGame.Read(iNetModeFiringSendRate);
		bsInitGame.Read(iNetModeSendMultiplier);
	}
	else
		bsInitGame.SetReadOffset(bsInitGame.GetReadOffset() + 4*32);

	bsInitGame.Read(m_bLagCompensation);

	BYTE unk;
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);

	BYTE byteStrLen;
	bsInitGame.Read(byteStrLen);
	if(byteStrLen)
	{
		memset(g_szHostName,0,sizeof(g_szHostName));
		bsInitGame.Read(g_szHostName, byteStrLen);
	}
	g_szHostName[byteStrLen] = '\0';

	bsInitGame.Read((char *)&byteVehicleModels[0],212);

	g_myPlayerID = MyPlayerID;

	char szTitle[64];
	if(settings.iConsole)
	{
		sprintf(szTitle, "%s (%d) - %.16s - RakSAMP %s", g_szNickName, g_myPlayerID, g_szHostName, RAKSAMP_VERSION);
		SetConsoleTitle(szTitle);
		Log("Connected to %.64s\n", g_szHostName);
	}
	else
	{
		sprintf(szTitle, "%s (%d) - RakSAMP %s", g_szNickName, g_myPlayerID, RAKSAMP_VERSION);
		SetWindowText(hwnd, szTitle);
		Log("Connected to %.64s", g_szHostName);
	}

	iGameInited = 1;
	// UIF Login: balant˝ sonrasi spawn istegi GONDERILMEZ (tmp hesap onlemi).
	// Login dialogu gelir (g_loginPending=true) -> kullan˝c˝ ˛ifreyle girer ->
	// "You logged in" mesaj˝ gelince false olur, spawn/akis ba˛lar.
	// Login dialogu HIC gelmezse (sunucu tmp'e zorluyorsa) 10 sn sonra otomatik
	// devam et (takilip sonsuza dek bekleme).
	g_loginPending = true;
	g_loginPendingUntil = GetTickCount() + 10000;
	Log("[LOGIN] Balant˝ tamam, login bekleniyor (tmp hesap onleniyor)...");
}

void WorldPlayerAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	BYTE byteFightingStyle=4;
	BYTE byteTeam=0;
	int iSkin=0;
	float vecPos[3];
	float fRotation=0;
	DWORD dwColor=0;

	bsData.Read(playerId);
	bsData.Read(byteTeam);
	bsData.Read(iSkin);
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);
	bsData.Read(fRotation);
	bsData.Read(dwColor);
	bsData.Read(byteFightingStyle);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 1;
	playerInfo[playerId].onfootData.vecPos[0] = 
	playerInfo[playerId].incarData.vecPos[0] = vecPos[0];
	playerInfo[playerId].onfootData.vecPos[1] = 
	playerInfo[playerId].incarData.vecPos[1] = vecPos[1];
	playerInfo[playerId].onfootData.vecPos[2] =
	playerInfo[playerId].incarData.vecPos[2] = vecPos[2];

	//Log("[WORLD ADD] Player [%d]", playerId);
}

void WorldPlayerDeath(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	//Log("[PLAYER_DEATH] %d", playerId);
}

// RPC 55 (DeathMessage): SA-MP'in sagdaki kill feed'i. Sunucu HER kill icin tum
// oyunculara gˆnderir (FR'deyken bile) - format: BYTE silah, WORD kurban, WORD katil.
// Sava-s boyunca katil adi + silah ID'si toplanir; Top Weapons finalize'da
// scoreboard uyeleriyle filtrelenip sayilir.
void ScrDeathMessage(RPCParameters *rpcParams)
{
	if(!iGameInited || !g_inZoneWar || g_zwKillListSent) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteWeapon = 0;
	WORD wVictimId = 0, wKillerId = 0;
	bsData.Read(byteWeapon);
	bsData.Read(wVictimId);
	bsData.Read(wKillerId);

	if(wKillerId >= MAX_PLAYERS || wVictimId >= MAX_PLAYERS) return;
	const char *pKiller = playerInfo[wKillerId].szPlayerName;
	if(!pKiller || !pKiller[0]) return;

	if(g_zwWarKillCount < WAR_KILL_MAX)
	{
		strncpy_s(g_zwWarKills[g_zwWarKillCount].szKiller, sizeof(g_zwWarKills[0].szKiller),
			pKiller, sizeof(g_zwWarKills[0].szKiller) - 1);
		g_zwWarKills[g_zwWarKillCount].weapon = byteWeapon;
		g_zwWarKillCount++;
		if(g_zwWarKillCount == 1 || (g_zwWarKillCount % 50) == 0)
			Log("[KILLLIST] WAR-KILL birikimi: %d (son: %s, silah %d)", g_zwWarKillCount, pKiller, byteWeapon);
	}
}

void WorldPlayerRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId=0;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 0;
	playerInfo[playerId].incarData.vecPos[0] = 0.0f;
	playerInfo[playerId].incarData.vecPos[1] = 0.0f;
	playerInfo[playerId].incarData.vecPos[2] = 0.0f;

	//Log("[PLAYER_REMOVE] %d", playerId);
}

void WorldVehicleAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	NEW_VEHICLE NewVehicle;

	bsData.Read((char *)&NewVehicle,sizeof(NEW_VEHICLE));

	if(NewVehicle.VehicleId < 0 || NewVehicle.VehicleId >= MAX_VEHICLES) return;

	vehiclePool[NewVehicle.VehicleId].iDoesExist = 1;
	vehiclePool[NewVehicle.VehicleId].fPos[0] = NewVehicle.vecPos[0];
	vehiclePool[NewVehicle.VehicleId].fPos[1] = NewVehicle.vecPos[1];
	vehiclePool[NewVehicle.VehicleId].fPos[2] = NewVehicle.vecPos[2];
	vehiclePool[NewVehicle.VehicleId].iModelID = NewVehicle.iVehicleType;

	//Log("[VEHICLE_ADD:%d] ModelID: %d, Position: %0.2f, %0.2f, %0.2f",
	//	NewVehicle.VehicleId, NewVehicle.iVehicleType, NewVehicle.vecPos[0], NewVehicle.vecPos[1], NewVehicle.vecPos[2]);
}

void WorldVehicleRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;

	bsData.Read(VehicleID);

	if(VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

	vehiclePool[VehicleID].iDoesExist = 0;
	vehiclePool[VehicleID].fPos[0] = 0.0f;
	vehiclePool[VehicleID].fPos[1] = 0.0f;
	vehiclePool[VehicleID].fPos[2] = 0.0f;

	//Log("[VEHICLE_REMOVE] %d", VehicleID);
}

void ConnectionRejected(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	BYTE byteRejectReason;

	bsData.Read(byteRejectReason);

	if(byteRejectReason==REJECT_REASON_BAD_VERSION)
	{
		Log("[RAKSAMP] Bad SA-MP version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_NICKNAME)
	{
		char szNewNick[32], randgen[4];

		iGettingNewName = true;

		gen_random(randgen, 4);
		sprintf(szNewNick, "%s_%s", g_szNickName, randgen);

		Log("[RAKSAMP] Bad nickname. Changing name to %s", szNewNick);

		strcpy(g_szNickName, szNewNick);
		resetPools(1, 0);
	}
	else if(byteRejectReason==REJECT_REASON_BAD_MOD)
	{
		Log("[RAKSAMP] Bad mod version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_PLAYERID)
	{
		Log("[RAKSAMP] Bad player ID.");
	}
	else
		Log("ConnectionRejected: unknown");
}

// Sava-s ba-slang-‚ñíc-‚ñínda k-‚ñísa GZ giri-si: kendi tak-‚ñím rengimizi --srenmek i-gin /gz2,
// birka-g saniye i-geride kal (skorboard textdraw'lar-‚ñínda kendi ad-‚ñím-‚ñíz-‚ñín rengi = tak-‚ñím rengi),
// sonra /fr ile freeroam'a geri d-n. /pos bu modda -gal-‚ñí-smaz; -g-‚ñík-‚ñí-s /fr ile yap-‚ñíl-‚ñír.
static void ZwJoinCaptureReturn()
{
	g_zwReturnPending = true;
	g_zwKillListStay = false;
	// 2 sn: takim rengi + war timer textdraw'inin ilk edit'i gelene kadar kal,
	// sonra /fr ile geri don. (Uzun kalma GZ'de takilma/ban riski tasir.)
	g_zwReturnAt = GetTickCount() + 2000;
	Log("[ZONEWAR] Kisa GZ giri˛ yapiliyor, takim rengi ˆrenilip /fr ile geri donulecek.");
	sendServerCommand((char *)"/gz2");
}

void ClientMessage(RPCParameters *rpcParams)
{
	//if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	DWORD dwStrLen, dwColor;
	char szMsg[257];
	memset(szMsg, 0, 257);

	bsData.Read(dwColor);
	bsData.Read(dwStrLen);
	if(dwStrLen > 256) return;

	bsData.Read(szMsg, dwStrLen);
	szMsg[dwStrLen] = 0;

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr(szMsg, settings.findItems[i].szFind))
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
		}
	}

	char szNonColorEmbeddedMsg[257];
	int iNonColorEmbeddedMsgLen = 0;

	for (size_t pos = 0; pos < strlen(szMsg) && szMsg[pos] != '\0'; pos++)
	{
		if (!((*(unsigned char*)(&szMsg[pos]) - 32) >= 0 && (*(unsigned char*)(&szMsg[pos]) - 32) < 224))
			continue;

		if(pos+7 < strlen(szMsg))
		{
			if (szMsg[pos] == '{' && szMsg[pos+7] == '}')
			{
				pos += 7;
				continue;
			}
		}

		szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = szMsg[pos];
		iNonColorEmbeddedMsgLen++;
	}

	szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = 0;

	Log("[CMSG] %s", szNonColorEmbeddedMsg);

	// --- Admin + grup rol dei˛iklii loglari (DB'ye kayit) ---
	// "(GROUP)" prefix'siz gelen sistem mesajlari: "ADMIN: X banned Y(id) ..."
	// ve "GROUP: (...) changed the rank of ..." buradan yakalanir.
	CheckAdminLogMessage(szNonColorEmbeddedMsg);
	CheckGroupLogMessage(szNonColorEmbeddedMsg);

	// First Blood: GZ kill feedi "X(pid) has killed Y(pid) (Weapon: Z) (Distance: Nm)"
	// (Most Wanted formati "* X killed Y" ‚Äî * √∂nekli olanlar degil). Sava˛ basinda
	// bot 3 sn GZ'ye girer; o penceredeki ilk kill = first blood.
	if(g_inZoneWar && !g_zwFirstBlood[0] && szNonColorEmbeddedMsg[0] != '*')
	{
		const char *pK = strstr(szNonColorEmbeddedMsg, " has killed ");
		if(pK && strstr(pK, "(Weapon:"))
		{
			char szKiller[64] = {0};
			const char *pOp = strchr(szNonColorEmbeddedMsg, '(');
			if(pOp){ size_t n = (size_t)(pOp - szNonColorEmbeddedMsg); if(n > 63) n = 63; memcpy(szKiller, szNonColorEmbeddedMsg, n); szKiller[n] = 0; }
			char szWpn[48] = {0};
			const char *pW = strstr(pK, "(Weapon:"), *pWE = NULL;
			if(pW) pWE = strchr(pW, ')');
			if(pW && pWE){ const char *pS = pW + 8; while(*pS == ' ') pS++; size_t n = (size_t)(pWE - pS); if(n > 47) n = 47; memcpy(szWpn, pS, n); szWpn[n] = 0; }
			char szDist[16] = {0};
			const char *pD = strstr(pK, "(Distance:"), *pDE = NULL;
			if(pD) pDE = strchr(pD, 'm');
			if(pD && pDE){ const char *pS = pD + 10; while(*pS == ' ') pS++; size_t n = (size_t)(pDE - pS); if(n > 15) n = 15; memcpy(szDist, pS, n); szDist[n] = 0; }
			if(szKiller[0])
			{
				sprintf_s(g_zwFirstBlood, sizeof(g_zwFirstBlood), "%s (Weapon: %s) (Distance: %sm)", szKiller, szWpn, szDist);
				Log("[KILLLIST] First Blood: %s", g_zwFirstBlood);
			}
		}
	}

	// --- Zone War chat dedektoru (RakGravy desenleri) ---
	// "keeps zone" / "takes over" / "wins against" / "loses to" iceren mesajlar
	// Discord zonewar kanalina bildirilir. 60 sn cooldown, cift tetiklenmeyi onler.
	// Yalnizca botun kendi grubunun mesajlari yakalanir.
	// Kendi grup adi /gz2 sonrasi gelen ilk "ZONE WAR: X" mesaj˝ndan otomatik ˆrenilir
	// (oyun RPC'sinden gelen isim guvenilmez; bot baskalari tarafindan da kullanilacak).
	{
		// YS.Hold'un periyodik "Time left: MM:SS" duyurusu: timer textdraw'ina
		// bagimli olmadan kalan sureyi verir (GZ'de olmaya gerek yok - grup chat'i).
		// Textdraw gelmedigi zamanlarda (restart sonrasi) sava˛ suresini bilmek icin
		// tek guvenilir kaynak. CaptureWarTimer ile ayni parse mantigi.
		const char *pTimeLeft = strstr(szNonColorEmbeddedMsg, "Time left:");
		if(pTimeLeft)
		{
			const char *pTL = pTimeLeft + strlen("Time left:");
			while(*pTL == ' ' || *pTL == '\t') pTL++;
			int iM = atoi(pTL), iS = 0;
			const char *pColon = strchr(pTL, ':');
			if(pColon && pColon[1] >= '0' && pColon[1] <= '9') iS = atoi(pColon + 1);
			if(iM >= 0 && iM <= 120 && iS >= 0 && iS <= 59 && g_inZoneWar)
			{
				g_warRemainingSec = iM * 60 + iS;
				g_warTimerTick = GetTickCount();
				// Chat kaynagi (uL.Hold tum sava˛larin suresini yazar): ?gzt time
				// gostergesi icin tutulur ama GERI SAYIM BUNA GORE ATILMAZ -
				// bizim sava˛imiz degilse phantom duyurular cikiyordu.
				g_warTimerViaTdw = false;
				if(g_warRemainingSec > 0)
				{
					DWORD dwEnd = g_warTimerTick + (DWORD)g_warRemainingSec * 1000;
					DWORD dwStart = dwEnd - ZONE_WAR_TOTAL_MS;
					if((int)dwStart <= 0 || dwStart > g_warTimerTick)
						dwStart = g_warTimerTick - (DWORD)g_warRemainingSec * 1000;
					g_zoneWarStartTick = dwStart;
				}
				Log("[WAR:TIMER] Chat duyurusundan sure: %d:%02d (%d sn)", iM, iS, g_warRemainingSec);
			}
		}

		const char *pZoneMsg = szNonColorEmbeddedMsg;
		const char *pAction = NULL;
		const char *pTail = NULL;

		// "Type /gz2 to join" bildirimi: botun grubunun sava˛i ba˛ladi.
		// Duyuru bazen "ZONE WAR: A$AP vs uL ... Type /gz2 to join" gibi GRUP
		// ADLARINI icerir, bazen sadece "Zone: '# N'.  Type /gz2 to join." olur.
		// Kural: "X vs Y" varsa grubumuz icinde degilse baska grubun sava˛i, yoksay.
		// "vs" yoksa mesaj grubumuza ozel gˆnderilmistir -> bizim sava˛imiz.
		// "Type /gz2 to join" bildirimi: sava˛ duyurusu. KRITIK: sunucu HER grubun
		// sava˛ duyurusunu chate yazar. Sadece "ZONE WAR: X vs Y" icinde grubumuz
		// geciyorsa BIZIM sava˛imizdir. Ciplak "Zone: '# N'.  Type /gz2 to join."
		// tek basina sava˛ BASLATMAZ (baska grubun sava˛i olabilir); yalnizca
		// bizim sava˛imiz AKTIFKEN zone numarasini verir.
		bool bJoinNotice = (strstr(pZoneMsg, "Type /gz2 to join") != NULL);
		bool bNamedOurWar = false;
		const char *pVS = strstr(pZoneMsg, " vs ");
		if(pVS && !g_myGroupName.empty())
		{
			char szLeft[80] = {0}, szRight[80] = {0};
			const char *pStart = pZoneMsg + 9; // "ZONE WAR: " sonrasi
			while(*pStart == ' ') pStart++;
			int nL = (int)(pVS - pStart);
			if(nL > 0 && nL < 80) { memcpy(szLeft, pStart, nL); szLeft[nL] = 0; }
			const char *pR = pVS + 4;
			while(*pR == ' ') pR++;
			strncpy_s(szRight, sizeof(szRight), pR, sizeof(szRight) - 1);
			for(char *q = szRight; *q; q++) if(*q == '\r' || *q == '\n') { *q = 0; break; }
			NormalizeGroupName(szLeft); NormalizeGroupName(szRight);
			bNamedOurWar = (_stricmp(g_myGroupName.c_str(), szLeft) == 0
						|| _stricmp(g_myGroupName.c_str(), szRight) == 0);
			Log("[ZONEWAR] Duyuru: '%s' vs '%s' - bizim sava˛: %s", szLeft, szRight,
				bNamedOurWar ? "EVET" : "HAYIR");
			if(!bNamedOurWar)
				Log("[ZONEWAR] Baska grubun sava˛ duyurusu yoksayildi (%s).", pZoneMsg);
		}
		// Ciplak zone prompt u: sava˛imiz AKTIFSE zone no sunu guncelle (yoksa yoksay)
		// ZSNIP: hedef zonede savas basladi (ciplak zone notice) -> mesaj bazli bekle
		if(bJoinNotice && !pVS && g_ztkActive)
		{
			const char *pZ2 = strstr(pZoneMsg, "Zone: '#");
			if(pZ2)
			{
				pZ2 += strlen("Zone: '#");
				while(*pZ2 == ' ') pZ2++;
				int nZ2 = atoi(pZ2);
				if(nZ2 >= 0 && IsSniperTargetZone(nZ2))
				{
					if(nZ2 == g_ztkZoneId)
					{
						if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
						{
							g_snipZoneIdx++;
							int nNext = g_snipZones[g_snipZoneIdx];
							g_ztkZoneId = nNext;
							g_ztkTargetWar = false;
							float fCX = (g_zoneMinX[nNext] + g_zoneMaxX[nNext]) / 2.0f;
							float fCY = (g_zoneMinY[nNext] + g_zoneMaxY[nNext]) / 2.0f;
							char szCmd2[128];
							sprintf_s(szCmd2, sizeof(szCmd2), "/pos %.2f %.2f %.2f", fCX, fCY, ZGO_TELEPORT_Z);
							sendServerCommand(szCmd2);
							Log("[ZSNIP] Hedef zone #%d savasta, atlanip #%d'e geciliyor.", nZ2, nNext);
						}
						else
						{
							g_ztkTargetWar = true;
							Log("[ZSNIP] Hedef zone #%d savasi basladi, bitisi bekleniyor (mesaj bazli).", nZ2);
						}
					}
					else
					{
						g_ztkTargetWar = true;
						if(g_ztkZoneId < 0) g_ztkZoneId = nZ2;
						Log("[ZSNIP] Hedef zone #%d savasi basladi, bitisi bekleniyor (mesaj bazli).", nZ2);
					}
				}
			}
		}
		if(bJoinNotice && !pVS && g_inZoneWar)
		{
			const char *pZ = strstr(pZoneMsg, "Zone: '#");
			if(pZ)
			{
				pZ += strlen("Zone: '#");
				while(*pZ == ' ') pZ++;
				char szZ[16] = {0}; int nZ = 0;
				while(*pZ >= '0' && *pZ <= '9' && nZ < 15) szZ[nZ++] = *pZ++;
				szZ[nZ] = 0;
				if(szZ[0]) strncpy_s(g_zwWarZone, sizeof(g_zwWarZone), szZ, sizeof(g_zwWarZone) - 1);
				Log("[ZONEWAR] Aktif sava˛ zone #%s duyurusu alindi.", g_zwWarZone);
			}
		}
		if(bNamedOurWar)
		{
			// Duyurunun zone'sunu cikar: "Zone: '# 146'." -> "146"
			// Canli format "Zone: '# 157'" - t-‚ñírnak sonrasi bo-sluk olabilir, atla.
			char szZoneTag[16] = {0};
			const char *pZ = strstr(pZoneMsg, "Zone: '#");
			if(pZ)
			{
				pZ += strlen("Zone: '#");
				while(*pZ == ' ') pZ++;
				int nZ = 0;
				while(*pZ >= '0' && *pZ <= '9' && nZ < 15) szZoneTag[nZ++] = *pZ++;
				szZoneTag[nZ] = 0;
			}
			// Ayni sava˛˝n tekrar eden duyurusu: yoksay. (Yoksa g_zoneWarStartTick her
			// tekrarda sifirlanir ve ?gzt kalan sureyi yanlis hesaplar.) Sava˛ zaten
			// aktifse gelen her yeni join duyurusu tekrar sayilir; zone tag'i uyu-smuyorsa
			// (bos tag dahil) da ayni sava˛˝n devami kabul edilir.
			if(g_inZoneWar && g_zoneWarStartTick != 0
				&& (!szZoneTag[0] || !strcmp(g_zwWarZone, szZoneTag)))
			{
				// Tekrar duyurusu. Bot yeniden ba˛ladiysa / oturum ortasinda sava˛a
				// denk geldiyse timer hic cekilmemis olabilir (g_warRemainingSec<0):
				// o zaman kisa GZ giri˛ yapip timer'-‚ñí + takim rengini yeniden ˆren.
				if(g_warRemainingSec < 0 && !g_zwReturnPending)
				{
					g_warTimerTick = 0;
					Log("[ZONEWAR] Sava˛ ortasinda timer bilinmiyor, kisa giri˛ ile yeniden ˆreniliyor.");
					ZwJoinCaptureReturn();
				}
				else
					Log("[ZONEWAR] Sava˛ duyurusu tekrari yoksayildi (zone #%s aktif).", g_zwWarZone);
			}
			else
			{
				pAction = "join";
				strncpy_s(g_zwWarZone, sizeof(g_zwWarZone), szZoneTag, sizeof(g_zwWarZone) - 1);
				// Sava˛ ba˛langic anini kaydet: kalan sure = ZONE_WAR_TOTAL_MS - gecen sure.
				g_zoneWarStartTick = GetTickCount();
				g_inZoneWar = true;
				g_gztCountdownAnnounced = 999; // yeni sava˛: geri sayim esikleri sifirdan
				g_gztLastRem = -1;
				// Taze sayac: eski sava˛tan kalan tick/kalan sure yeni sava˛i zehirlememeli.
				// Zehirsiz eski degerle geri sayim sifirdan biler "00:01" duyurup zinciri
				// tum sava˛ boyunca susturuyordu (#gzt duyuru atmiyor vakasi).
				g_warRemainingSec = ZONE_WAR_TOTAL_MS / 1000;
				g_warTimerTick = g_zoneWarStartTick;
				g_warTimerViaTdw = false; // yeni sava˛: textdraw yakalanana kadar duyuru YOK
				memset(g_zwWelcomed, 0, sizeof(g_zwWelcomed)); // yeni sava˛: welcome tekrari olmasin
				// Yeni sava˛: onceki yakalayici/kill list durumunu sifirla
				// ONCE bekleyen kill list varsa (onceki sava˛ sonucu) finalize et ‚Äî
				// yoksa yeni sava˛˝n reset'i veriyi siler, sonuc Discord'a hic gitmez.
				if(g_zwCapturingKillList && g_zwKillTdCount > 0 && !g_zwKillListSent)
				{
					Log("[KILLLIST] Yeni sava˛ ba˛ladi, onceki sava˛˝n kill listi finalize ediliyor...");
					FinalizeKillList();
				}
				g_zwCapturingKillList = false;
				g_zwKillListSent = false;
				g_zwKillListText.clear(); g_zwKillTdCount = 0;
				g_zwAutoJoinSent = false;
				g_zwCheckJoinSent = false;
				g_zwKillListStay = false;
				g_zwMyColor = 0;
				g_zwBestK = 0; g_zwBestDmg = 0; g_zwBestD = 0;
				g_zwMvpName[0] = 0; g_zwFeederName[0] = 0;
				g_zwMineK = 0; g_zwMineA = 0; g_zwMineD = 0; g_zwMineDmg = 0;
				g_zwEnemyK = 0; g_zwEnemyA = 0; g_zwEnemyD = 0; g_zwEnemyDmg = 0;
				g_zwWeWon = true;
				g_zwFirstBlood[0] = 0;
				g_zwWeAreBlue = false; g_zwWeAreBlueKnown = false;
				g_zwWarKillCount = 0; // Top Weapons: yeni sava˛, yeni kill feed
				// Kill list textdraw'lari (Name/Kills/Assists/Deaths/Dmg) GZ'de
				// olmaya gerek kalmadan da gelebiliyor (SaveTextDrawData logladi).
				// Sava˛ boyunca yakalayiciyi A«IK tut; bitince finalize et.
				g_zwCapturingKillList = true;
				g_zwCaptureStart = GetTickCount();
				g_zwFinalizeAt = 0; // yeni sava˛: bitis finalize'i yeniden kurulacak
				Log("[KILLLIST] Yakalayici sava˛ boyunca aÁ˝k (kill list textdraw'lari toplanacak).");
				// Kisa GZ giri˛: takim rengimizi ˆren (skorboard textdraw'inda kendi adimizin
				// rengi bizim takim rengidir). 4 sn iceride kal, sonra /pos ile geri don.
				g_warRemainingSec = -1;
				g_warTimerTick = 0;
				ZwJoinCaptureReturn();
				Log("[ZONEWAR] Sava˛ ba˛ladi (zone #%s), sure sayaci ba˛latildi.", g_zwWarZone);
				// Kill list takim basliklari icin "X vs Y" duyurusundaki iki grubu sakla:
				// SOL grup = kirmizi, SAG grup = mavi (kullan˝c˝ kurali). UIF duyurusu
				// "ZONE WAR: <GrupA> vs <GrupB>" bicimindedir.
				{
					const char *pVS = strstr(pZoneMsg, " vs ");
					if(pVS)
					{
						// Soldaki isim: "ZONE WAR: " sonrasi, " vs " oncesi
						const char *pStart = pZoneMsg + 9;
						while(*pStart == ' ') pStart++;
						int nL = (int)(pVS - pStart);
						if(nL > 0 && nL < 80)
						{
							char szL[80] = {0};
							memcpy(szL, pStart, nL); szL[nL] = 0;
							NormalizeGroupName(szL);
							g_zwWarTeamLeft = szL;   // kirmizi
						}
						// Sagdaki isim: " vs " sonrasi satir sonuna kadar
						const char *pR = pVS + 4;
						while(*pR == ' ') pR++;
						{
							char szR[80] = {0};
							strncpy_s(szR, sizeof(szR), pR, sizeof(szR) - 1);
							for(char *q = szR; *q; q++) if(*q == '\r' || *q == '\n') { *q = 0; break; }
							NormalizeGroupName(szR);
							g_zwWarTeamRight = szR;  // mavi
						}
						Log("[ZONEWAR] Takimlar: SOL(kirmizi)='%s' | SAG(mavi)='%s'",
							g_zwWarTeamLeft.c_str(), g_zwWarTeamRight.c_str());
					}
					// Isim eslesmesi: grubumuz solda ise KIRMIZI, sagda ise MAVI biziz.
					// (Renk tahmininden daha guvenilir - "vs" duyurusu sol=kirmizi/sag=mavi.)
					if(!g_myGroupName.empty())
					{
						if(!g_zwWarTeamLeft.empty() && _stricmp(g_myGroupName.c_str(), g_zwWarTeamLeft.c_str()) == 0)
						{
							g_zwWeAreBlue = false;
							g_zwWeAreBlueKnown = true;
							Log("[ZONEWAR] Takimimiz: KIRMIZI (sol). Dusman: '%s'", g_zwWarTeamRight.c_str());
						}
						else if(!g_zwWarTeamRight.empty() && _stricmp(g_myGroupName.c_str(), g_zwWarTeamRight.c_str()) == 0)
						{
							g_zwWeAreBlue = true;
							g_zwWeAreBlueKnown = true;
							Log("[ZONEWAR] Takimimiz: MAVI (sag). Dusman: '%s'", g_zwWarTeamLeft.c_str());
						}
					}
				}
				// Grubumuzun adini henuz bilmiyorsak ˆrenme moduna gir.
				// Sonraki ilk "ZONE WAR: X keeps/takes..." mesaj˝ndaki X bizim grubumuzdur.
				if(g_myGroupName.empty() && !g_zwLearningGroup)
				{
					g_zwLearningGroup = true;
					g_zwLearnTick = GetTickCount();
					Log("[ZONEWAR] /gz2 bildirimi alindi, kendi grup adi ˆrenilecek...");
				}
			}
		}

		// --- Zone sahipligi ˆrenimi (SADECE isim bazli; RENK KULLANILMAZ) ---
		// "ZONE WAR: X takes over zone '# N'"  -> X o zone'un YENI sahibi: direkt bagla.
		// "ZONE WAR: X keeps zone '# N'"       -> X mevcut sahibi: direkt bagla.
		// (Eski akis takes-over'da RPC 108 RENK guncellemesi bekliyordu - renkle
		// isimiz yok; sunucu mesaj˝nin kendisi yeterli ve aninda ˆrenir.)
		{
			const char *pTakes = strstr(pZoneMsg, "takes over zone '#");
			const char *pKeeps = strstr(pZoneMsg, "keeps zone '#");
			const char *pCk = pTakes ? pTakes : pKeeps;
			if(pCk && strncmp(pZoneMsg, "ZONE WAR:", 9) == 0)
			{
				const char *pName = pZoneMsg + 9;
				while(*pName == ' ') pName++;
				int n = (int)(pCk - pName);
				if(n > 0 && n < 80)
				{
					char szName[80] = {0};
					memcpy(szName, pName, n);
					szName[n] = 0;
					NormalizeGroupName(szName);
					int zid = atoi(pCk + (pTakes ? strlen("takes over zone '#") : strlen("keeps zone '#")));
					if(zid >= 0 && zid < MAX_GANG_ZONES && szName[0])
					{
						BindZoneGroup(zid, szName);
						// ZSNIP: hedef zonede savas bitti ("takes over zone '# N'")
						if(g_ztkActive && zid == g_ztkZoneId)
						{
							g_ztkTargetWar = false;
							// O zone bitti (kim alirsa alsin - grubun datasindan duser):
							// SONRAKI zone'a aninda gec + saldir (Faz1 /gz atar).
							Log("[ZSNIP] Hedef zone #%d savasi bitti (takes over), sonraki zone'a geciliyor.", zid);
							if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
							{
								g_snipZoneIdx++;
								int nNext = g_snipZones[g_snipZoneIdx];
								g_ztkZoneId = nNext;
								g_ztkWaitAnnounced = false;
								float fCX = (g_zoneMinX[nNext] + g_zoneMaxX[nNext]) / 2.0f;
								float fCY = (g_zoneMinY[nNext] + g_zoneMaxY[nNext]) / 2.0f;
								char szCmd[128];
								sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCX, fCY, ZGO_TELEPORT_Z);
								sendServerCommand(szCmd);
								g_bridgeSuppressEcho++;
								char szMsg[128];
								sprintf_s(szMsg, sizeof(szMsg),
									LM("!Zone #%d bitti, sonraki zone #%d saldiriliyor...",
										"!Zone #%d ended, attacking next zone #%d..."), zid, nNext);
								sendChat(szMsg);
							}
							else
							{
								g_ztkActive = false;
								g_ztkZoneId = -1;
								g_snipZoneCount = 0;
								g_snipZoneIdx = 0;
								g_snipGroupName[0] = 0;
								Log("[ZSNIP] Zincir tamamlandi (baska zone yok).");
							}
						}
					}
				}
			}
		}

		if((pTail = strstr(pZoneMsg, "keeps zone")) != NULL) { pAction = "keeps"; }
		else if((pTail = strstr(pZoneMsg, "takes over")) != NULL) { pAction = "takes"; }
		else if((pTail = strstr(pZoneMsg, "wins against")) != NULL) { pAction = "wins"; }
		else if((pTail = strstr(pZoneMsg, "loses to")) != NULL) { pAction = "loses"; }

		// Ogrenme modundaysak ve "ZONE WAR: <Grup> ..." mesaj˝ geldiyse, grup adini cek.
		// /gz2'den sonra gelen ilk zone mesaj˝ bizim grubumuzun mesaj˝dir.
		if(pAction && g_zwLearningGroup && (strncmp(pZoneMsg, "ZONE WAR:", 9) == 0))
		{
			if(GetTickCount() - g_zwLearnTick <= 60000)
			{
				const char *pName = pZoneMsg + 9;
				while(*pName == ' ') pName++;

				// Grup adi, action kelimesinden ("keeps zone", "takes over", ...)
				// ONCEKI kisimdir. Ornek: "ZONE WAR: swan of gours takes over zone '# 222'"
				// -> "swan of gours". pTail bu keyword'un basina isaret eder.
				std::string strLearned;
				if(pTail && pTail > pName)
					strLearned.assign(pName, (size_t)(pTail - pName));

				// Sondaki bosluklari temizle
				while(!strLearned.empty() && strLearned[strLearned.size()-1] == ' ')
					strLearned.erase(strLearned.size()-1);

				if(!strLearned.empty())
				{
					g_myGroupName = strLearned;
					g_zwLearningGroup = false;
					Log("[ZONEWAR] Kendi grup adi otomatik ˆrenildi: %s", g_myGroupName.c_str());
				}
			}
			else
			{
				g_zwLearningGroup = false;
			}
		}

		// Kendi grubumuzun adini biliyorsak, mesajda o ad gecmiyorsa yoksay.
		// Grup adi henuz ˆrenilmediyse (bos) hicbir zone mesaj˝ yakalanmaz;
		// Kullan˝c˝: wins/loses her grup icin tetiklenirse botun sava˛i karisir.
		// Sadece botun grubunun adi mesaja dahilse isle (takes over dahil).
		if(pAction && !bJoinNotice)
		{
			if(g_myGroupName.empty() || strstr(pZoneMsg, g_myGroupName.c_str()) == NULL)
				pAction = NULL;
		}

		if(pAction)
		{
			DWORD dwNow = GetTickCount();
			// 60 sn anti-spam: YALNIZCA zone-war mesajlari bu zamanlayiciyi gunceller.
			// (?za/?gz kendi g_lastZoneWarTime'i kullanir ‚Äî paylasilirsa "loses to"
			// war-end mesaj˝ sessizce yutulur, sava˛ hic bitmezdi.)
			if(g_lastZoneWarMsgTime == 0 || (dwNow - g_lastZoneWarMsgTime) >= 60000)
			{
				g_lastZoneWarMsgTime = dwNow;

// Sava˛ bitti (wins/loses/takes over): sure sayacini sifirla.
				if(!strcmp(pAction, "wins") || !strcmp(pAction, "loses") || !strcmp(pAction, "takes over"))
				{
					// Sabit 10 dk kurali gecerli (kullan˝c˝ onayi); sureyi olcmeye gerek yok.
					g_zoneWarStartTick = 0;
					g_inZoneWar = false;
					g_gztCountdownAnnounced = 999; // sava˛ bitti: sonraki sava˛ icin sifir
					g_gztLastRem = -1;
					g_zwWarZone[0] = 0; // sonraki sava˛ yeni duyuru sayilir
					g_zwWarTeamLeft.clear();  // sonraki sava˛ takimlari yeniden parse edilir
					g_zwWarTeamRight.clear();
					g_warRemainingSec = -1;
					g_warTimerTick = 0;
					g_warTimerViaTdw = false;

					// Kill list toplanacaksa GZ'de KAL (textdraw'lar bitis duyurusundan
					// sonra gelir); finalize edilince /fr ile donulur. Kill list yoksa
					// kisa giri˛ donusu hemen yapilir.
if(g_zwCapturingKillList && !g_zwKillListSent)
				{
					// Bitis textdraw'larini bekle; g_zwReturnPending true kalir,
					// finalize sonrasi d-n-‚ïùs (ProcessZoneWarAutoJoin) /fr gˆnderir.
					g_zwFinalizeAt = GetTickCount() + 3000;
					Log("[KILLLIST] Sava˛ bitti (%s), GZ'de kalinip sonuc textdraw'lari bekleniyor...", pAction);
				}
				else if(g_zwReturnPending)
				{
					g_zwReturnPending = false;
					Log("[ZONEWAR] Sava˛ bitti, /fr ile freeroam'a donuluyor.");
					sendServerCommand((char *)"/fr");
				}

				// NOT: g_zwKillListSent burada AYARLANMAZ ‚Äî g_zwFinalizeAt=+3sn
				// finalize'i tetikler; KillListSent'i erken true yapmak finalize'i
				// engelliyordu (sonuc Discord'a hic gitmiyordu). FinalizeKillList
				// isini bitirince kendisi true yapar.
					Log("[ZONEWAR] Sava˛ bitti (%s), sure sayaci s˝f˝rland˝.", pAction);

					// Kayip bildirimi: "X wins against Y" - Y bizim grubumuzsa kaybettik.
					// "X loses to Y" / "X loses against Y" formatinda da calisir: rakip
					// tarafinda (kaybeden degil) olan taraf biziz demektir.
					if(!g_myGroupName.empty())
					{
						bool bWeLost = false;
						const char *pAgainst = strstr(pZoneMsg, "against");
						if(pAgainst && strstr(pAgainst + 7, g_myGroupName.c_str()))
							bWeLost = true; // "X wins against BIZ" -> kaybettik
						else if(!pAgainst && strstr(pZoneMsg, "loses") && strstr(pZoneMsg, g_myGroupName.c_str()))
						{
							// "X loses" mesaj˝nda bizim ad yoksa (rakip kaybetti) degil;
							// "loses" sonrasi bizim ad varsa kaybettik. YS format: "X loses to Y".
							const char *pLoses = strstr(pZoneMsg, "loses");
							if(pLoses && strstr(pLoses + 6, g_myGroupName.c_str()))
								bWeLost = true;
						}
						if(bWeLost)
						{
							g_zwWeWon = false;
							Log("[ZONEWAR] Grubumuz sava˛i KAYBETTI (%s).", pZoneMsg);
							g_bridgeSuppressEcho++;
							sendChat((char *)LM("!Your group lost the zone war!", "!Your group lost the zone war!"));
						}
					}
				}

				Log("[ZONEWAR] Chat mesaj˝ yakalandi (%s): %s", pAction, pZoneMsg);
				// Discord zonewar kanalina embed gˆnderilmez (kullan˝c˝ istegi: gereksiz spam).
				// Zone sahipligi ˆrenimi (ZFU: takes over/keeps -> BindZoneGroup) calismaya devam eder.
			}
		}
	}

	// --- Welcome back dedektoru: "GROUP: X logged in" ---
	// Oyun ici gruplarda "X logged in" mesaj˝ gelince Discord welcome kanalina bildir.
	{
		// Sistemsel ">> You logged in to your account": sunucu oturumu hatirliyor
		// (kullan˝c˝ daha once girmis). Login beklemeyi KALDIRMAZ ‚Äî sunucuya login
		// i˛lemini bitirmesi icin 3 sn verilir; ancak ondan SONRA spawn akisi serbest.
		// (Not: bu mesaj tmp hesapta da geliyor; tek basina gercek login degildir.)
		if(strstr(szNonColorEmbeddedMsg, "You logged in to your account") && g_loginPending)
		{
			// Sunucu login'i islesin: 3 sn sonra akis devam etsin.
			if(g_loginPendingUntil == 0)
				g_loginPendingUntil = GetTickCount() + 3000;
			Log("[LOGIN] 'You logged in' mesaj˝ alindi, 3 sn sonra akis devam edecek.");
		}
		const char *pLogged = strstr(szNonColorEmbeddedMsg, "logged in");
		// Sistemsel ">> You logged in to your account" mesaj˝ greet TETIKLEMEZ:
		// yalnizca "GROUP: X(id) logged in" formatli grup uyesi giri˛leri.
		if(pLogged && (pLogged == szNonColorEmbeddedMsg || pLogged[-1] == ' ')
			&& strncmp(szNonColorEmbeddedMsg, "GROUP:", 6) == 0)
		{
			// Isim cikar: gercek format "GROUP: X(id) logged in - Ominous" -> isim
			// ':' sonrasi, '(' oncesi (eski format varsayimi yanlisti, greet hic
			// tetiklenmiyordu).
			const char *pColon = strchr(szNonColorEmbeddedMsg, ':');
			const char *pName = pColon ? pColon + 1 : szNonColorEmbeddedMsg;
			while(*pName == ' ' || *pName == '\t') pName++;
			const char *pOpen = strchr(pName, '(');
			size_t nLen = pOpen ? (size_t)(pOpen - pName) : strlen(pName);
			char szName[64] = {0};
			if(nLen > 0 && nLen < 64)
			{
				memcpy(szName, pName, nLen); szName[nLen] = 0;
				// Sondaki boslugu kirp
				while(nLen > 0 && szName[nLen-1] == ' ') szName[--nLen] = 0;
			}

			if(szName[0] && strcmp(szName, g_szNickName) != 0)
			{
				Log("[WELCOME] Welcome back tespit edildi: %s", szName);
				SendWelcomeBack(szName);

				// OYUN ICINI otomatik greet (uL.Hold tarzi) - yalnizca ?greet A«IK ise.
				if(!g_welcomeEnabled)
				{
					Log("[WELCOME] Greet KAPALI, oyun ici mesaj atlanmadi (Discord da atlar).");
				}
				else
				{
					// - Sava˛ varsa: " Welcome <isim>! We are in a ZONE WAR against <rakip>! Type /gz2 for backup!"
					// - Ozel mesaj ayarliysa (?welcomemsg): o mesaj gˆnderilir
					// - Bot dili TR ise: " Tekrar ho˛ geldin <isim>!" / EN: " Welcome back <isim>!"
					char szGreet[224];
					if(g_inZoneWar)
				{
					// Rakip adi: once "X vs Y" duyurusundan (bizim olmayan taraf),
					// yoksa WARGROUP RPC'sinden gelen enemy.
					std::string strEnemy = g_enemyGroupName;
					if(!g_zwWarTeamLeft.empty() && !g_zwWarTeamRight.empty())
					{
						bool bLeftOurs = (!g_myGroupName.empty()
							&& _stricmp(g_zwWarTeamLeft.c_str(), g_myGroupName.c_str()) == 0);
						const std::string &strPick = bLeftOurs ? g_zwWarTeamRight : g_zwWarTeamLeft;
						if(!strPick.empty()) strEnemy = strPick;
					}
					if(strEnemy.empty())
						sprintf_s(szGreet, sizeof(szGreet), LM("!Welcome %s! Grup sava˛˝nday˝z! Yedek icin /gz2 yaz!", "!Welcome %s! We are in a ZONE WAR! Type /gz2 for backup!"), szName);
					else
						sprintf_s(szGreet, sizeof(szGreet), "!Welcome %s! We are in a ZONE WAR against %s! Type /gz2 for backup!", szName, strEnemy.c_str());
				}
				else
				{
					std::string per = GetWelcomePerUser(szName);
					if(!per.empty()){
						if(strstr(per.c_str(), "%s")){
							char tmp[224]; sprintf_s(tmp, sizeof(tmp), per.c_str(), szName);
							sprintf_s(szGreet, sizeof(szGreet), "!%s", tmp);
						} else {
							sprintf_s(szGreet, sizeof(szGreet), "!%s", per.c_str());
						}
					} else if(!g_welcomeMessage.empty())
					{
						sprintf_s(szGreet, sizeof(szGreet), "!%s", g_welcomeMessage.c_str());
					}
					else
					{
						sprintf_s(szGreet, sizeof(szGreet), LM("!Tekrar ho˛ geldin %s!", "!Welcome back %s!"), szName);
					}
				}
g_bridgeSuppressEcho++;
				sendChat(szGreet);
				}
			}
		}
	}

	// GUVENLIK KOMUTLARI: ?changepass / ?email ‚Äî yalnizca PM ile ve yetkili isimlerden.
	// Format: ">> PM from <isim>(<id>) (tag): mesaj". ?help'te gˆsterilmez.
	if(!strncmp(szNonColorEmbeddedMsg, ">> PM from", 10))
	{
		const char *pNm = szNonColorEmbeddedMsg + 10;
		while(*pNm == ' ') pNm++;
		const char *pOp = strchr(pNm, '(');
		const char *pColon = strchr(pNm, ':');
		char szPmName[64] = {0};
		if(pOp){ size_t n = (size_t)(pOp - pNm); if(n > 63) n = 63; memcpy(szPmName, pNm, n); szPmName[n] = 0; }
		else if(pColon){ size_t n = (size_t)(pColon - pNm); if(n > 63) n = 63; memcpy(szPmName, pNm, n); szPmName[n] = 0; }
		const char *pMsg = pColon ? pColon + 1 : "";
		while(*pMsg == ' ') pMsg++;

		// Yetkili isimler (case-insensitive)
		bool bAuth = szPmName[0]
			&& (!_stricmp(szPmName, "Oreyoo") || !_stricmp(szPmName, "hakixld") || !_stricmp(szPmName, "cb650r"));

		if(bAuth && !strncmp(pMsg, "?changepass", 11) && (pMsg[11] == ' ' || pMsg[11] == '\0'))
		{
			// UIF /changepass INPUT men¸s¸ acar; degeri sakla, komutu gˆnder, menuyu bekle.
			const char *pVal = pMsg + 11;
			while(*pVal == ' ') pVal++;
			strncpy_s(g_secCmdValue, sizeof(g_secCmdValue), pVal, sizeof(g_secCmdValue) - 1);
			g_secCmdIsEmail = false;
			g_secCmdPending = true;
			g_secCmdAt = GetTickCount() + 15000;
			g_secCmdCount = 0;
			Log("[SEC] ?changepass PM'de (yetkili: %s), deger: '%s'", szPmName, g_secCmdValue);
			sendServerCommand((char *)"/changepass");
			// Onay GRUBA degil PM yaniti (/r) olarak gider.
			sendServerCommand((char *)"/r ﬁifre dei˛tirme men¸s¸ bekleniyor...");
			return;
		}
		if(bAuth && !strncmp(pMsg, "?email", 6) && (pMsg[6] == ' ' || pMsg[6] == '\0'))
		{
			const char *pVal = pMsg + 6;
			while(*pVal == ' ') pVal++;
			strncpy_s(g_secCmdValue, sizeof(g_secCmdValue), pVal, sizeof(g_secCmdValue) - 1);
			g_secCmdIsEmail = true;
			g_secCmdPending = true;
			g_secCmdAt = GetTickCount() + 15000;
			g_secCmdCount = 0;
			Log("[SEC] ?email PM'de (yetkili: %s), deger: '%s'", szPmName, g_secCmdValue);
			sendServerCommand((char *)"/email");
			sendServerCommand((char *)"/r E-posta men¸s¸ bekleniyor...");
			return;
		}

		// Discord /pm yaniti: hedef oyuncu PM ile cevap verirse Discord'a ilet
		// (format: ">> PM from <isim>(<id>) (...): mesaj"). Isim VEYA ID ile
		// eslesir: g_pmName/g_pmId cozulmus hedef (Discord da ID yazilsa calisir).
		int iPmId = -1;
		if(pOp)
		{
			const char *pId = pOp + 1;
			iPmId = atoi(pId);
		}
		if(g_pmPending && GetTickCount() < g_pmUntil && g_pmRequesterId[0]
			&& szPmName[0] && pMsg[0]
			&& ((g_pmName[0] && _stricmp(szPmName, g_pmName) == 0)
				|| (g_pmTarget[0] && _stricmp(szPmName, g_pmTarget) == 0)
				|| (g_pmId >= 0 && iPmId >= 0 && g_pmId == iPmId)))
		{
			g_pmPending = false;
			Log("[PM] Yanit Discord'a iletiliyor (PM): %s: '%s'", szPmName, pMsg);
			try
			{
				httplib::Client cli(g_szBackendBase);
				cli.set_connection_timeout(3);
				json body = {
					{"hwid", g_myHWID},
					{"requesterId", g_pmRequesterId},
					{"reply", pMsg},
					{"author", szPmName}
				};
				cli.Post("/api/pm/reply", body.dump(), "application/json");
			}
			catch(...) {}
		}
	}

	// Grup chat tespiti: "(GROUP) [20] isim(oyuncuID): mesaj"
	// KENDI oyuncunun ?w / ?pl / ?gstats komutlar˝ni yakalar (cooldown ile).
	// Normal chat (RakSAMP konsol inputu) buraya girmez, tetiklemez.
	if(!strncmp(szNonColorEmbeddedMsg, "(GROUP)", 7))
	{
		const char *pCloseBracket = strchr(szNonColorEmbeddedMsg, ']'); // "[20]" kapanisi
		const char *pOpenParen = pCloseBracket ? strchr(pCloseBracket, '(') : NULL; // oyuncu id parantezi
		if(pOpenParen)
		{
			int iSenderId = atoi(pOpenParen + 1);
			const char *pColon = strchr(pOpenParen, ':');
			const char *pGroupMsg = pColon ? pColon + 1 : "";
			while(*pGroupMsg == ' ') pGroupMsg++;

			// "(GROUP) [20] isim(oyuncuID): mesaj" basligindan gˆnderici adini ˆren.
			// playerInfo[yeniKatilan].szPlayerName sadece ServerJoin ile dolar; botun
			// baglanmasindan ONCE oyunda olanlar bos kalir. Bu yuzden ?pl cevabinda
			// bakilan kullan˝c˝nin adi g_szNickName'e (bot adi) dusuyordu. Grup
			// basligindaki ad, kimligin dogru kaynagidir (rename edilmis kisilerde de
			// guncel kalir).
			if(pCloseBracket && pCloseBracket + 1 < pOpenParen && iSenderId >= 0 && iSenderId < MAX_PLAYERS)
			{
				const char *pNameStart = pCloseBracket + 1;
				while(*pNameStart == ' ') pNameStart++;
				size_t nLen = (size_t)(pOpenParen - pNameStart);
				if(nLen > 0 && nLen <= 20)
				{
					char szGrpName[64] = {0};
					memcpy(szGrpName, pNameStart, nLen);
					szGrpName[nLen] = 0;
					StripColorCodes(szGrpName); // renk kodlarini temizle ({RRGGBB} / <RRGGBB>)
					if(szGrpName[0])
					{
						playerInfo[iSenderId].iIsConnected = 1;
						strncpy_s(playerInfo[iSenderId].szPlayerName,
							sizeof(playerInfo[iSenderId].szPlayerName), szGrpName, _TRUNCATE);
						// Isim cache'ini de guncelle (restart sonrasi ?pl ismi bilinsin)
						if(g_playerCache[iSenderId].dwTime == 0
							|| strcmp(g_playerCache[iSenderId].szName, szGrpName) != 0)
						{
							strncpy_s(g_playerCache[iSenderId].szName,
								sizeof(g_playerCache[0].szName), szGrpName, _TRUNCATE);
							g_playerCache[iSenderId].dwTime = 1;
							SavePlayerCache();
						}
					}
				}
			}

			// Grup ID'si: "(GROUP) [20]" -> 20
			if(pCloseBracket && pCloseBracket > szNonColorEmbeddedMsg + 8)
			{
				char szGrpId[16] = {0};
				int n = 0;
				for(const char *q = szNonColorEmbeddedMsg + 8; q < pCloseBracket && n < 15; q++)
					szGrpId[n++] = *q;
				szGrpId[n] = 0;
				int iGrp = atoi(szGrpId);

				// Kendi grubumuzu ilk gˆrdugumuzde kaydet
				if(iSenderId == g_myPlayerID && iGrp > 0)
					g_myGroupId = szGrpId;

				// Ayni grupta gˆrdugumuz oyuncular -> grup uyesi
				if(g_myGroupId != "" && szGrpId == g_myGroupId)
					g_knownGroupMembers[iSenderId] = 1;
			}

			// Bot kendi sendChat ile gˆnderdigi mesaj˝ (GROUP) olarak echo
			// edilirse tekrar islenmesin (cevap/otomatik mesajlar).
			// NOT: Oyun ici (GTA SA) sohbet kutusundan yazilan mesajlar sunucu
			// tarafindan gˆnderan oyuncunun ID'siyle yayinlanir. Kullan˝c˝
			// bot hesabi (hakixld) degil, oyun hesabiyla (orn. washe) yazdigi
			// icin iSenderId != g_myPlayerID olur. Bu yuzden sadece bot'un
			// KENDI echo'larini engelliyoruz, diger grup uyelerinin mesajlarini
			// HandleGroupCommand'e veriyoruz.
			if(iSenderId == g_myPlayerID) return;

			// Sunucu kendi mesaj˝mizi (GROUP) olarak geri echo etmedigi icin komut
			// mantigi HandleGroupCommand'e tasindi (konsol inputu RunCommand'den de cagrilir).
			HandleGroupCommand(pGroupMsg, iSenderId);
		}
	}
}

// Grup chat komutlar˝ni lokal isler (?help, ?w, ?ping, ?pl, ?gst, ?gstats,
// ?greet, ?welcomemsg, ?gzt, ?discordchat, ?1414, ?gz ve !<mesaj>).
// Sunucu kendi mesaj˝ni (GROUP) olarak geri echo etmedigi icin bu mantik hem
// ClientMessage yakalamasindan hem de RunCommand (konsol inputu) icin cagrilir.
// iSenderId < 0 ise konsol/gelen sender yok anlaminda g_myPlayerID varsayilir.
void HandleGroupCommand(const char *pGroupMsg, int iSenderId)
{
			// ?post gerekce penceresi: ?post sonrasi 30 sn icinde yazilan normal grup
			// mesaj˝ gerekce olarak yakalanir (komutlar haric).
			if(g_pushPending && pGroupMsg[0] != '?' && pGroupMsg[0] != '!' && pGroupMsg[0] != '/' && pGroupMsg[0] != '\0')
			{
				strncpy_s(g_pushReason, sizeof(g_pushReason), pGroupMsg, sizeof(g_pushReason) - 1);
				Log("[POST] Gerekce alindi: '%s'", g_pushReason);
			}
			// /pm yaniti: hedef oyuncu grup chat'inde normal mesaj atarsa Discord'a ilet
			// (Discord'da /pm'i kullanan kisiye etiketli + ephemeral gˆsterilir).
			if(g_pmPending && GetTickCount() < g_pmUntil && g_pmTarget[0] && g_pmRequesterId[0]
				&& iSenderId >= 0 && iSenderId < MAX_PLAYERS && iSenderId != g_myPlayerID
				&& pGroupMsg[0] != '?' && pGroupMsg[0] != '!' && pGroupMsg[0] != '/' && pGroupMsg[0] != '\0')
			{
				const char *pPmName = playerInfo[iSenderId].szPlayerName;
				if(pPmName && pPmName[0] && _stricmp(pPmName, g_pmTarget) == 0)
				{
					g_pmPending = false;
					Log("[PM] Yanit Discord'a iletiliyor: %s: '%s'", pPmName, pGroupMsg);
					try
					{
						httplib::Client cli(g_szBackendBase);
						cli.set_connection_timeout(3);
						json body = {
							{"hwid", g_myHWID},
							{"requesterId", g_pmRequesterId},
							{"reply", pGroupMsg},
							{"author", pPmName}
						};
						cli.Post("/api/pm/reply", body.dump(), "application/json");
					}
					catch(...) {}
				}
			}
			// YS.Hold'un periyodik "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w"
			// mesaj˝: FPS dahil tam istatistiktir. /pnetstats FPS icermediginden bunu
			// cache'e kaydet (?pl cevabinda kullanilir), player_cache.txt'ye yaz.
			if(!strncmp(pGroupMsg, "Player [", 8) && strstr(pGroupMsg, " Stats ->"))
			{
				int iPid = atoi(pGroupMsg + 8);
				if(iPid >= 0 && iPid < MAX_PLAYERS)
				{
					bool bNew = (g_playerCache[iPid].dwTime == 0);
					double dLoss = NumberAfter(pGroupMsg, "Packetloss:");
					double dFps  = NumberAfter(pGroupMsg, "FPS:");
					double dPing = NumberAfter(pGroupMsg, "Ping:");
					if(dLoss >= 0.0) g_playerCache[iPid].dLoss = dLoss;
					if(dFps  >= 0.0) g_playerCache[iPid].iFps = (int)dFps;
					if(dPing >= 0.0) g_playerCache[iPid].iPing = (int)dPing;
					g_playerCache[iPid].dwTime = GetTickCount();
					SavePlayerCache();
					if(bNew)
						Log("[PL-CACHE] %d (%s) stats cache: loss %.2f%% fps %d ping %d",
							iPid, g_playerCache[iPid].szName[0] ? g_playerCache[iPid].szName : "?",
							g_playerCache[iPid].dLoss, g_playerCache[iPid].iFps, g_playerCache[iPid].iPing);

					// ?pl bekliyorsa ve hedef bu oyuncuysa cevabi simdi gˆnder:
					// /pnetstats dialogu "Network Active: 1" gibi BOS doner; asil
					// loss/fps/ping bu "Stats ->" mesaj˝ndadir (SAMPRPC dialogda loss
					// bulamazsa cevabi buraya birakir, flag aÁ˝k kalir).
					if(g_waitingForPNetStats && g_plTargetId == iPid
						&& GetTickCount() - g_pnetstatsReqTime <= 15000)
					{
						g_waitingForPNetStats = false;
						const char *pName = "Player ?";
						char szFallbackName[32];
						if(playerInfo[iPid].szPlayerName[0]) pName = playerInfo[iPid].szPlayerName;
						else if(g_playerCache[iPid].szName[0]) pName = g_playerCache[iPid].szName;
						else { sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", iPid); pName = szFallbackName; }
						// Isimde renk kodu varsa temizle (playerInfo/player_cache ham gelebilir)
						char szCleanName[64] = {0};
						strncpy_s(szCleanName, sizeof(szCleanName), pName, sizeof(szCleanName)-1);
						StripColorCodes(szCleanName);
						pName = szCleanName;
						char szFps[16], szPing[16];
						if(g_playerCache[iPid].iFps >= 0) sprintf_s(szFps, sizeof(szFps), "%d", g_playerCache[iPid].iFps); else strcpy(szFps, "--");
						if(g_playerCache[iPid].iPing >= 0) sprintf_s(szPing, sizeof(szPing), "%d", g_playerCache[iPid].iPing); else strcpy(szPing, "--");
						char szOut[192];
						sprintf_s(szOut, sizeof(szOut), "!%s's PL: %g%% | FPS: %s | Ping: %s ms",
							pName, g_playerCache[iPid].dLoss, szFps, szPing);
						Log("[PNETSTATS] Stats-post ile cevap: %s", szOut);
						g_bridgeSuppressEcho++;
						sendChat(szOut);
					}
				}
				return;
			}

			int iCmdSender = iSenderId;
			if(iCmdSender < 0 || iCmdSender >= MAX_PLAYERS)
				iCmdSender = g_myPlayerID;

			// Komutu kullananin oyun ici adi (?w, ?1414, ?pl icin Discord'a isim gider).
			const char *pCmdSenderName = "";
			if(playerInfo[iCmdSender].iIsConnected && playerInfo[iCmdSender].szPlayerName[0])
				pCmdSenderName = playerInfo[iCmdSender].szPlayerName;
			else
				pCmdSenderName = g_szNickName;

			// --- ?help : komut listesini oyunda goster (Discord'a gitmez) ---
			if(!strncmp(pGroupMsg, "?help", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[HELP] ?help yakalandi, komut listesi gˆnderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Komutlar: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo ?grole <isim> <memb|co> ?grank <isim> <rank>", "!Commands: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo ?grole <name> <memb|co> ?grank <name> <rank>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Ayarlar: ?greet ?welcomemsg ?gm <mesaj> ?discordchat", "!Settings: ?greet ?welcomemsg ?gm <message> ?discordchat"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bilgi: ?info ?disc ?test ?m ?astats <id> ?alogs <id>", "!Info: ?info ?disc ?test ?m ?astats <id> ?alogs <id>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a mesaj: !<mesaj> yaz", "!Send Discord message: !<message>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Nas˝l: ?help", "!How: ?help"));
			}
			// --- ?test : balant˝ kontrolu, bot cevap verir ---
			else if(!strncmp(pGroupMsg, "?test", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[TEST] ?test yakalandi, cevap gˆnderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bot Áal˝˛˝yor. ?help ile komutlar˝ gˆr.", "!Bot is running. Use ?help to see commands."));
			}
			// --- ?info : bot surum bilgisi ---
			else if(!strncmp(pGroupMsg, "?info", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[INFO] ?info yakalandi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!RakSAMP bot v1.0 - ?help ile komut listesi", "!RakSAMP bot v1.0 - ?help for commands"));
			}
			// --- ?disc : Discord sunucu linki ---
			else if(!strncmp(pGroupMsg, "?disc", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[DISC] ?disc yakalandi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Discord: uifserver.net/discord");
			}
			// --- ?w : Discord war help, 5 dk cooldown (spam onlemek icin; 30 sn iken Discord'a yagmur gibi iniyordu) ---
			else if(!strncmp(pGroupMsg, "?w", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastWarHelpTime != 0 && (dwNow - g_lastWarHelpTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastWarHelpTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?w)", "!Cooldown: %d secs left (?w)"), iLeft);
					Log("[WARHELP] Cooldown aktif (%d sn kald˝), ?w yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastWarHelpTime = dwNow;
				Log("[WARHELP] Grup chat'inde ?w yakalandi. %s Discord'a gˆnderiliyor...", pCmdSenderName);
				TriggerWarHelp(pCmdSenderName);
				// uL.Hold gibi gˆnderim onayi: "The call was sent to Discord."
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a gˆnderildi.", "!The call was sent to Discord."));
				return;
			}
			// --- ?ping : yanit verir, 30 sn cooldown (RakGravy ile ayni) ---
			if(!strncmp(pGroupMsg, "?ping", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPingTime != 0 && (dwNow - g_lastPingTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastPingTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?ping)", "!Cooldown: %d secs left (?ping)"), iLeft);
					Log("[PING] Cooldown aktif (%d sn kald˝), ?ping yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastPingTime = dwNow;
				Log("[PING] ?ping yakalandi, Pong cevabi gˆnderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Pong!");
			}
			// --- ?pl [id] : /pnetstats dialogundan packetloss, 30 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?pl", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPLTime != 0 && (dwNow - g_lastPLTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastPLTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?pl)", "!Cooldown: %d secs left (?pl)"), iLeft);
					Log("[PNETSTATS] Cooldown aktif (%d sn kald˝), ?pl yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastPLTime = dwNow;

				// id verilmezse: komutu KULLANAN KISININ istatistigi (grup basligindan
				// gelen iSenderId). Konsol/lokal cagrilarinda iSenderId zaten bot'tur.
				int iTarget = (iSenderId >= 0 && iSenderId < MAX_PLAYERS) ? iSenderId : g_myPlayerID;
				const char *pArg = pGroupMsg + 3;
				while(*pArg == ' ') pArg++;
				if(*pArg != '\0')
				{
					// Balant˝ kontrolu YOK: istenen id her zaman sorgulanir (UIF tum
					// oyuncularin istatistiklerini bilir). Bagliyiz sandigimiz kisi yanlis
					// olabilir (ServerJoin'i kacirmisiz); sunucu cozer.
					int i = atoi(pArg);
					if(i >= 0 && i < MAX_PLAYERS)
						iTarget = i;
				}

				g_plTargetId = iTarget;
				g_waitingForPNetStats = true;
				g_pnetstatsReqTime = GetTickCount();

				char szCmd[64];
				if(iTarget == g_myPlayerID)
					sprintf_s(szCmd, sizeof(szCmd), "/pnetstats");
				else
					sprintf_s(szCmd, sizeof(szCmd), "/pnetstats %d", iTarget);

				Log("[PNETSTATS] /stats gˆnderiliyor (hedef: %d)...", iTarget);
				char szStatsCmd[64];
				if(iTarget == g_myPlayerID)
					sprintf_s(szStatsCmd, sizeof(szStatsCmd), "/stats");
				else
					sprintf_s(szStatsCmd, sizeof(szStatsCmd), "/stats %d", iTarget);
				sendServerCommand(szStatsCmd);
				Log("[PNETSTATS] /pnetstats gˆnderiliyor (hedef: %d)...", iTarget);
				sendServerCommand(szCmd);
				// DB'deki oyuncu cache'ini de hemen tazele (eski veri backend'de silinir)
				SyncPlayerCacheToDb();
			}
			// --- ?gst : /gstats dialogundan zone + aktif oyuncu sayisini oyunda goster (Discord'a gitmez) ---
			else if(!strncmp(pGroupMsg, "?gst", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastGStatsTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?gst)", "!Cooldown: %d secs left (?gst)"), iLeft);
					Log("[GST] Cooldown aktif (%d sn kald˝), ?gst yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGst = true;
				g_gstReqTime = GetTickCount();
				Log("[GST] /gstats gˆnderiliyor (zone + aktif oyuncu sayisi icin)...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?az : attackable (free/sahipsiz) zone sayisini aninda goster (YS bot gibi) ---
			else if(!strncmp(pGroupMsg, "?az", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				// Isim bazli: sahibi bilinmeyen veya "Free" isaretli zone'lar sayilir.
				int nFree = 0;
				for(int zid = 0; zid < g_zoneCount; zid++)
					if(g_zoneOwnerName[zid][0] == 0 || !_stricmp(g_zoneOwnerName[zid], "Free"))
						nFree++;
				char szOut[96];
				sprintf_s(szOut, sizeof(szOut), "!Attackable zones: %d", nFree);
				Log("[AZ] Attackable zones: %d.", nFree);
				g_bridgeSuppressEcho++;
				sendChat(szOut);
			}
			// --- ?gstats / ?act : /gstats dialogunu Discord'a yolla, 30 sn cooldown ---
			else if((!strncmp(pGroupMsg, "?gstats", 7) && (pGroupMsg[7] == '\0' || pGroupMsg[7] == ' '))
			     || (!strncmp(pGroupMsg, "?act", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' ')))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastGStatsTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?gstats)", "!Cooldown: %d secs left (?gstats)"), iLeft);
					Log("[GSTATS] Cooldown aktif (%d sn kald˝), ?gstats yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
				g_gstatsSentFirst = false;
				g_gstatsRequesterId = ""; // oyun ici istek: Discord'a gˆnderilmez
				Log("[GSTATS] /gstats gˆnderiliyor...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?greet [on|off] : welcome back mesajlarini ac/kapat (DB'de saklanir).
			// Argsiz = toggle. Sonuc GRUPTA bildirilir (aÁ˝k/kapal˝).
			else if(!strncmp(pGroupMsg, "?greet", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				const char *pArg = pGroupMsg + 6;
				while(*pArg == ' ') pArg++;
				int iForce = -1;
				if(!_stricmp(pArg, "on")) iForce = 1;
				else if(!_stricmp(pArg, "off")) iForce = 0;
				Log("[WELCOME] ?greet yakalandi (arg='%s').", pArg);
				ToggleWelcomeBackEx(iForce);
			}
			// --- ?welcomemsg [isim] <mesaj> : kisi bazli (B) veya global welcome ayarla ---
			// B secenegi: ?welcomemsg cb650r Hosgeldin kral!  -> sadece cb650r icin
			//             ?welcomemsg list                 -> per-user listesi
			//             ?welcomemsg <mesaj>              -> global (herkes icin)
			else if(!strncmp(pGroupMsg, "?welcomemsg", 11) && (pGroupMsg[11] == '\0' || pGroupMsg[11] == ' '))
			{
				const char *pArg = pGroupMsg + 11;
				while(*pArg == ' ') pArg++;
				if(*pArg == '\0')
				{
					Log("[WELCOME] ?welcomemsg bos, global siliniyor.");
					SetWelcomeMessage("");
				}
				else if(!_stricmp(pArg, "list"))
				{
					// Per-user listesini Discord'a goster (log + chat)
					try{
						httplib::Client cli(g_szBackendBase);
						cli.set_connection_timeout(3);
						std::string url = "/api/welcome/peruser/list?hwid=" + g_myHWID;
						auto res = cli.Get(url.c_str());
						if(res && res->status==200){
							auto j=json::parse(res->body);
							auto list=j.value("list", json::array());
							if(list.empty()){
								g_bridgeSuppressEcho++; sendChat((char*)"!Per-user welcome yok.");
							} else {
								std::string out="!Per-user: ";
								for(auto &e: list){
									std::string t=e.value("target_name",""), m=e.value("custom_message","");
									out += t + "->" + m.substr(0,20) + " | ";
									if(out.size()>100) break;
								}
								g_bridgeSuppressEcho++; sendChat((char*)out.c_str());
							}
						}
					}catch(...){}
				}
				else
				{
					// Per-user self: gˆnderen kisi kendi adina mesaj ayarlar
					// Ornek: cb650r ?welcomemsg test -> cb650r icin "test" kaydedilir
					char senderName[64]={0};
				if(iSenderId >=0 && iSenderId < MAX_PLAYERS && playerInfo[iSenderId].szPlayerName[0])
					strncpy_s(senderName, sizeof(senderName), playerInfo[iSenderId].szPlayerName, sizeof(senderName)-1);
				else if(iSenderId >=0 && iSenderId < MAX_PLAYERS && g_playerCache[iSenderId].szName[0])
					strncpy_s(senderName, sizeof(senderName), g_playerCache[iSenderId].szName, sizeof(senderName)-1);
				if(!senderName[0]){
						// Fallback global (eski davranis)
						char szMsg[256] = {0};
						strncpy_s(szMsg, sizeof(szMsg) - 1, pArg, sizeof(szMsg) - 1);
						Log("[WELCOME] Global welcomemsg ayarlaniyor: %s", szMsg);
						SetWelcomeMessage(szMsg);
					} else if(!_stricmp(pArg, "clear")){
						Log("[WELCOME] Per-user %s siliniyor", senderName);
						SetWelcomePerUser(senderName, "");
						g_bridgeSuppressEcho++; char tmp[128]; sprintf_s(tmp,"!Per-user %s silindi.", senderName); sendChat(tmp);
					} else {
						Log("[WELCOME] Per-user %s (self) -> %s", senderName, pArg);
						SetWelcomePerUser(senderName, pArg);
						g_bridgeSuppressEcho++; char tmp[128]; sprintf_s(tmp,"!Per-user %s ayarlandi.", senderName); sendChat(tmp);
					}
				}
			}
			// --- ?gm <mesaj> : ?welcomemsg alias (E1 uyumlu) ---
			else if(!strncmp(pGroupMsg, "?gm", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				const char *pArg = pGroupMsg + 3;
				while(*pArg == ' ') pArg++;
				if(*pArg == '\0')
				{
					Log("[WELCOME] ?gm icin mesaj bos, ayar siliniyor.");
					SetWelcomeMessage("");
				}
				else
				{
					char szMsg[256] = {0};
					strncpy_s(szMsg, sizeof(szMsg) - 1, pArg, sizeof(szMsg) - 1);
					Log("[WELCOME] ?gm ayarlaniyor: %s", szMsg);
					SetWelcomeMessage(szMsg);
				}
			}
			// --- ?gzt : zone war timer - on/off + kalan sureyi gruba bildir ---
			else if(!strncmp(pGroupMsg, "?gzt", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				// ?gzt on / ?gzt off: geri sayim duyurularini ac/kapat (YS botu gibi)
				const char *pArg = pGroupMsg + 5;
				while(*pArg == ' ') pArg++;
				if(!_stricmp(pArg, "on"))
				{
					g_gztOn = true;
					SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
					Log("[GZT] Timer ON.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: A«IK. Son 10/5/3/1 saniyede duyuru yap˝lacak.", "!War timer: ON. Announcements at last 10/5/3/1 secs."));
					return;
				}
				if(!_stricmp(pArg, "off"))
				{
					g_gztOn = false;
					SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
					Log("[GZT] Timer OFF.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: KAPALI. Duyuru yapilmayacak.", "!War timer: OFF. No announcements."));
					return;
				}

				// ?gzt time: su anki sava˛ suresini goster - cooldown yok.
				if(_stricmp(pArg, "time") != 0 && pArg[0])
				{
					Log("[GZT] Bilinmeyen arguman: '%s' (on/off/time).", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullan˝m: ?gzt | ?gzt time | ?gzt off", "!Usage: ?gzt | ?gzt time | ?gzt off"));
					return;
				}

				// Argumansiz ?gzt: durumu bildir (KAPALI ise "kapal˝" uyarisi). "?gzt time"
				// aÁ˝k olsa da olmasa da su anki sureyi gosterir (kullan˝c˝ istegi).
				bool bExplicitTime = !_stricmp(pArg, "time");
				if(!g_gztOn && !bExplicitTime)
				{
					Log("[GZT] Argumansiz ama timer KAPALI, 'kapal˝' uyarisi gˆnderildi.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: KAPALI. AÁmak icin ?gzt on | sure: ?gzt time",
						"!War timer: OFF. Enable with ?gzt on | time: ?gzt time"));
					return;
				}
				DWORD dwNow = GetTickCount();
				// Kalan sure: ˆncelikle yakalanan GZ timer textdraw'ina dayanir
				// (gercek kalan sure; chat duyuru ani ile sava˛ basi ayni degil).
				// Yoksa ba˛langic tick'inden hesaplanan deger kullanilir.
				int iRemainingSec = -1;
				if(!g_inZoneWar)
				{
					Log("[GZT] Sava˛ aktif degil, War time bilgisi gˆnderilemiyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War time: yok", "!War time: none"));
					return;
				}
				if(g_warTimerTick != 0 && dwNow - g_warTimerTick < ZONE_WAR_TOTAL_MS)
					iRemainingSec = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
				else if(g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick < ZONE_WAR_TOTAL_MS)
					iRemainingSec = (int)((ZONE_WAR_TOTAL_MS - (dwNow - g_zoneWarStartTick)) / 1000);

				if(iRemainingSec >= 0)
				{
					int iMin = iRemainingSec / 60;
					int iSec = iRemainingSec % 60;
					char szWarTime[192];
					sprintf_s(szWarTime, sizeof(szWarTime), "!War time: %d:%02d", iMin, iSec);
					Log("[GZT] Kalan sure hesaplandi: %d:%02d", iMin, iSec);
					g_bridgeSuppressEcho++;
					sendChat(szWarTime);
				}
				else
				{
					// Sava˛ ba˛langici bilinmiyor ya da coktan bitti.
					Log("[GZT] Sava˛ ba˛langici yok, War time bilgisi gˆnderilemiyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War time: yok", "!War time: none"));
				}
			}
			// --- ?tr / ?en : grup dili (varsayilan INGILIZCE; ?tr ile Turkce'ye gec) ---
			else if(!strncmp(pGroupMsg, "?tr", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				g_langTurkish = true;
				SaveLang();
				ServerSetBotLang(true); // MongoDB Guild.lang'a yaz
				Log("[LANG] ?tr ile Turkce secildi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Dil: Turkce. Mesajlar artik Turkce atilacak.");
			}
			else if(!strncmp(pGroupMsg, "?en", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				g_langTurkish = false;
				SaveLang();
				ServerSetBotLang(false); // MongoDB Guild.lang'a yaz
				Log("[LANG] ?en ile Ingilizce secildi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Language: English. Messages will now be sent in English.");
			}
			// --- ?guildid <id> : bu botu kullanan Discord sunucusunu (guild) bagla ---
			else if(!strncmp(pGroupMsg, "?guildid", 8) && (pGroupMsg[8] == ' ' || pGroupMsg[8] == '\0'))
			{
				const char *pArg = pGroupMsg + 8;
				while(*pArg == ' ') pArg++;
				if(!pArg[0])
				{
					Log("[GUILDID] Arguman yok, kullan˝m: ?guildid <discord sunucu id>");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullan˝m: ?guildid <discord sunucu id>",
						"!Usage: ?guildid <discord guild id>"));
				}
				else if(ServerSetGuildId(pArg))
				{
					Log("[GUILDID] Grup Discord sunucusuna baglandi: %s", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Discord sunucusuna baglandi.", "!Linked to Discord guild."));
				}
				else
				{
					Log("[GUILDID] Balant˝ ba˛ar˝s˝z: %s", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Balant˝ ba˛ar˝s˝z: sunucu botu eklememis olabilir.",
						"!Link failed: the guild may not have added the bot."));
				}
			}
			// --- ?discordchat : Discord grup chat kopusunu ac/kapat (MongoDB'de saklanir) ---
			else if(!strncmp(pGroupMsg, "?discordchat", 12) && (pGroupMsg[12] == '\0' || pGroupMsg[12] == ' '))
			{
				Log("[GROUPCHAT] ?discordchat komutu yakalandi, durum dei˛tiriliyor...");
				ToggleDiscordChat();
			}
			// --- ?m : game-modes listesi sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?m", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				Log("[MODES] ?m yakalandi, /gamemodes gˆnderiliyor.");
				sendServerCommand((char *)"/gamemodes");
			}
			// --- ?astats <id> : account stats sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?astats", 7) && (pGroupMsg[7] == ' ' || pGroupMsg[7] != '\0'))
			{
				Log("[ASTATS] ?astats yakalandi.");
				char szCmd[64];
				sprintf_s(szCmd, sizeof(szCmd), "/astats %s", pGroupMsg + 7);
				sendServerCommand(szCmd);
			}
			// --- ?alogs <id|isim> : admin loglari DB'den (uL.Hold formatinda) ---
			else if(!strncmp(pGroupMsg, "?alogs", 6) && (pGroupMsg[6] == ' ' || pGroupMsg[6] != '\0'))
			{
				Log("[ALOGS] ?alogs yakalandi, DB sorgusu baslatiliyor.");
				QueryAdminLogSummary(pGroupMsg + 6);
			}
			// --- ?alog <id|accid|isim> : DB'deki admin loglarindan sayi ozeti ---
			else if(!strncmp(pGroupMsg, "?alog", 5) && (pGroupMsg[5] == ' ' || pGroupMsg[5] != '\0'))
			{
				Log("[ALOG] ?alog yakalandi, DB sorgusu ba˛latiliyor.");
				QueryAdminLogSummary(pGroupMsg + 5);
			}
			// --- ?gname : kendi grup adini YENIDEN ˆren (1 kerelik /stats -> Group
			// butonu akisi; ˆrenileni DB'ye yazar). Grup degistiginde kullanilir. ---
			else if(!strncmp(pGroupMsg, "?gname", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				Log("[GNAME] ?gname yakalandi, grup adi yeniden ˆreniliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Grup adi yeniden ˆreniliyor...", "!Re-learning group name..."));
				// Mevcut adi sifirla -> SyncZscanFromDb yeniden ˆrenir (DB'de yoksa
				// /stats akisi; varsa DB'den yuklenir). Zorla /stats icin DB de sifirlanir.
				g_myGroupName.clear();
				g_zscanDbLoaded = false;
				g_waitingForGroupName = false;
				g_groupNamePhase = 0;
				// DB'deki eski adi da temizle ki /stats ile YENIDEN ˆrensin
				try
				{
					httplib::Client cli(g_szBackendBase);
					cli.set_connection_timeout(3);
					json body = {{"name", g_szNickName}, {"data", json::object()}, {"groupName", ""}};
					cli.Post("/api/bot/status", body.dump(), "application/json");
				}
				catch(...) {}
				g_bridgeSuppressEcho++;
			}
			// --- ?post [gerekce] : son grup aksiyonunu (rol/rank/kick) Discord log kanalina embed gˆnder ---
			// Gerekce inline yazilmazsa 30 sn beklenir; bu surede yazilan grup mesaj˝ gerekce olur.
			else if(!strncmp(pGroupMsg, "?post", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				if(!g_lastGlogTarget[0])
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Log yok: once bir rol/rank/kick dei˛iklii yakalanmali.", "!No log: a role/rank/kick change must be detected first."));
				}
				else
				{
					const char *pReason = pGroupMsg + 5;
					while(*pReason == ' ') pReason++;
					if(*pReason != '\0')
					{
						Log("[POST] ?post gerekceli: %s", pReason);
						PushLastGroupLog(pReason);
					}
					else
					{
						Log("[POST] ?post: gerekce bekleniyor (30 sn)...");
						g_pushPending = true;
						g_pushUntil = GetTickCount() + 30000;
						g_pushReason[0] = 0;
						g_pushRequester = iSenderId;
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Gerekce yazabilirsin (30 sn).", "!You can type a reason (30 secs)."));
					}
				}
			}
			// --- ?tag <isim> <tag> : grup tagini elle kaydet (yerel + DB isim eslesmeli) ---
			// (?tagscan ayri komuttur: buraya takilmamasi icin haric tutulur.)
			else if(!strncmp(pGroupMsg, "?tag", 4) && strncmp(pGroupMsg, "?tagscan", 8) != 0
				&& (pGroupMsg[4] == ' ' || pGroupMsg[4] != '\0'))
			{
				Log("[TAG] ?tag yakalandi.");
				// Yerel + DB'ye isim eslesmeli tag kaydet: "?tag <isim> <tag>"
				// Tag son kelime, isim geri kalan (isimde bosluk olabilir: "didnt need a group").
				const char *pArg = pGroupMsg + 4;
				while(*pArg == ' ') pArg++;
				const char *pLast = NULL;
				for(const char *q = pArg; *q; q++) if(*q == ' ') pLast = q;
				if(pLast && pLast > pArg)
				{
					char szName[80] = {0}, szTag[32] = {0};
					size_t nN = pLast - pArg;
					if(nN < 80)
					{
						memcpy(szName, pArg, nN); szName[nN] = 0;
						const char *pT = pLast + 1; while(*pT == ' ') pT++;
						strncpy_s(szTag, sizeof(szTag), pT, sizeof(szTag) - 1);
						StripColorCodes(szName);
						NormalizeGroupName(szName);
						StripColorCodes(szTag);
						if(szName[0] && szTag[0])
						{
							int gi = FindGroupColorByName(szName);
							// Uzun grup isimlerinin kismi yazimi eslesin:
							// "didnt need" -> "Didn't need a group" (apostrof yok sayilir).
							// Tek eslesme bulunursa onu kullan.
							if(gi < 0)
							{
								char szNeedL[80]; int nl = 0;
								for(int k = 0; szName[k] && nl < 79; k++){ if(szName[k] == '\'') continue; szNeedL[nl++] = tolower((unsigned char)szName[k]); }
								szNeedL[nl] = 0;
								int iFound = -1;
								for(int i = 0; i < g_groupColorCount; i++)
								{
									if(!g_groupColors[i].szName[0]) continue;
									char szDbL[80]; int dl = 0;
									for(int k = 0; g_groupColors[i].szName[k] && dl < 79; k++){ if(g_groupColors[i].szName[k] == '\'') continue; szDbL[dl++] = tolower((unsigned char)g_groupColors[i].szName[k]); }
									szDbL[dl] = 0;
									if(strstr(szDbL, szNeedL))
									{
										if(iFound >= 0) { iFound = -2; break; } // birden fazla eslesme: belirsiz
										iFound = i;
									}
								}
								if(iFound >= 0)
								{
									gi = iFound;
									Log("[TAG] Kismi eslesme: '%s' -> '%s'.", szName, g_groupColors[iFound].szName);
								}
							}
							if(gi < 0 && g_groupColorCount < MAX_GROUP_COLORS)
							{
								gi = g_groupColorCount++;
								strncpy_s(g_groupColors[gi].szName, sizeof(g_groupColors[0].szName),
									szName, sizeof(g_groupColors[0].szName) - 1);
							}
							if(gi >= 0)
							{
								strncpy_s(g_groupColors[gi].szTag, sizeof(g_groupColors[0].szTag),
									szTag, sizeof(g_groupColors[0].szTag) - 1);
								SaveGroupColors();
								SyncGroupColorsToDb(); // DB'ye isimle push (elle kayit)
								Log("[TAG] '%s' -> '[%s]' yerel + DB'ye kaydedildi.", szName, szTag);
								g_bridgeSuppressEcho++;
								char szOk[96];
								sprintf_s(szOk, sizeof(szOk), "!%s -> [%s] kaydedildi.", szName, szTag);
								sendChat(szOk);
							}
						}
					}
				}
			}
			// --- ?zlist / ?zwho : bulundugu zone bilgisi -√•√Ü /gzinfo (hangi grup, ne zaman bosalir) ---
			else if((!strncmp(pGroupMsg, "?zlist", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			     || (!strncmp(pGroupMsg, "?zwho", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' ')))
			{
				Log("[ZONE] %s yakalandi, /gzinfo gˆnderiliyor.", pGroupMsg);
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?zscan on/off: otomatik aralikli tarama. ?zscan on -> taramayi ba˛lat +
			// aralikli tekrarla; ?zscan off -> durdur. Argumansiz -> durumu bildir. ---
			else if(!strncmp(pGroupMsg, "?zscan", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				const char *pArg = pGroupMsg + 7;
				while(*pArg == ' ') pArg++;
				if(!_stricmp(pArg, "on"))
				{
					g_zscanAuto = true;
					SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
					// TARAMA ANINDA BASLATILMAZ: sadece bayrak acilir. Tarama ilk
					// giri˛ sonrasi + periyodik akista (ProcessZoneScan) ba˛lar.
					// Test icin surekli ac/kapa yapilinca her toggle'da tarama+DB push
					// tetikleniyordu; bu hem DB'yi yoruyor hem var olan veriyi karistiriyordu.
					g_bridgeSuppressEcho++;
					if(g_inZoneWar)
						// Sava˛ sirasinda tarama bilin-gli bekletilir (teleport = sava˛ bugu);
						// kullan˝c˝ya S-√ªYLE, sessiz no-op olmasin.
						sendChat((char *)LM("!Zone war aktif - tarama sava˛ bitince otomatik ba˛layacak.",
							"!Zone war active - scan will start automatically after the war."));
					else
						sendChat((char *)LM("!Zone tarama: A«IK (periyodik).", "!Zone scan: ON (periodic)."));
				}
				else if(!_stricmp(pArg, "off"))
				{
					g_zscanAuto = false;
					g_zoneScanIdx = -1;   // aktif taramayi da durdur (kuyruk temizlenir)
					g_zoneScanQ = 0;
					SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Zone tarama: KAPALI.", "!Zone scan: OFF."));
				}
				else
				{
					// Argumansiz/bilinmeyen: durumu bildir + kullan˝m.
					Log("[ZONE] ?zscan yakalandi (arg='%s'), durum bildiriliyor.", pArg);
					g_bridgeSuppressEcho++;
					if(pArg[0] && _stricmp(pArg, "on") && _stricmp(pArg, "off"))
						sendChat((char *)LM("!Kullan˝m: ?zscan on | ?zscan off", "!Usage: ?zscan on | ?zscan off"));
					else
						sendChat((char *)LM(g_zscanAuto ? "!Zone tarama: A«IK." : "!Zone tarama: KAPALI.",
							g_zscanAuto ? "!Zone scan: ON." : "!Zone scan: OFF."));
				}
			}
			// --- ?zgo <#N | gruptag> : zone'a isinlan, 40 sn bekle, sonra geri don (gruba bildirir) ---
			// Sayi verilirse o zone; tag verilirse o grubun rengine ait 1 zone'a gider.
			else if(!strncmp(pGroupMsg, "?zgo", 4) && (pGroupMsg[4] == ' ' || pGroupMsg[4] == '#' || (pGroupMsg[4] >= '0' && pGroupMsg[4] <= '9')))
			{
				const char *pArg = pGroupMsg + 4;
				while(*pArg == ' ' || *pArg == '#') pArg++;
				int iZone = -1;
				g_snipZoneCount = 0; g_snipZoneIdx = 0; g_snipGroupName[0] = 0;
				if(pArg[0] >= '0' && pArg[0] <= '9')
					iZone = atoi(pArg);
				else
				{
					iZone = FindZoneByGroupTag(pArg);
					// Zincir icin grubu ve TUM zone'larini yakala
					int gi = FindGroupColorByName(pArg);
					if(gi < 0)
					{
						for(int i = 0; i < g_groupColorCount; i++)
							if(g_groupColors[i].szTag[0] && _stricmp(g_groupColors[i].szTag, pArg) == 0) { gi = i; break; }
					}
					if(gi >= 0)
					{
						strncpy_s(g_snipGroupName, sizeof(g_snipGroupName), g_groupColors[gi].szName, sizeof(g_snipGroupName) - 1);
						g_snipZoneCount = CollectGroupZones(g_groupColors[gi].szName, g_snipZones, 64);
						Log("[ZSNIP] Zincir icin %d zone bulundu (grup: %s).", g_snipZoneCount, g_snipGroupName);
					}
				}
				if(iZone == -2)
				{
					// TAG CAKISMASI: bu tag birden fazla gruba ait (orn. dEV ->
					// The Devils Organizations + The Devilz Dead Industries). Rastgele
					// secim yanlis zone gˆnderir; tam ISIM ile netlestir.
					Log("[ZGO] Tag '%s' birden fazla gruba ait, isimle netlestiriliyor.", pArg);
					g_bridgeSuppressEcho++;
					char szCol[160];
					sprintf_s(szCol, sizeof(szCol),
						LM("!'%s' tag'i birden fazla gruba ait - tam isim yaz (??zgo <tam isim>).",
							"!'%s' tag belongs to multiple groups - use full name (??zgo <full name>)."), pArg);
					sendChat(szCol);
					return;
				}
				if(iZone >= 0 && iZone < g_zoneCount)
				{
					// Eski (spawn) pozisyonu kaydet
					float fPos[3] = {0, 0, 0};
					if(!getPlayerPos(g_myPlayerID, fPos))
					{
						Log("[ZGO] Mevcut pozisyon alinamadi, geri donus noktasi kullanilamayacak.");
					}
					g_zgoReturnPos[0] = fPos[0];
					g_zgoReturnPos[1] = fPos[1];
					g_zgoReturnPos[2] = fPos[2];

					float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
					float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
					char szCmd[128];
					sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
					Log("[ZGO] ?zgo %s -> zone #%d merkezine isinlaniliyor (%s), 40 sn beklenip donulecek.", pArg, iZone, szCmd);

					g_zgoActive = true;
					g_zgoZoneId = iZone;
					g_zgoReturnTick = GetTickCount() + ZGO_STAY_MS;
					sendServerCommand(szCmd);

					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d de. /go yazarak yanima ˝˛˝nlanabilirsin, 40 sn sonra dˆn¸yorum.",
				"!At zone #%d. Type /go to teleport to me, returning in 40 sec."), iZone);
					g_bridgeSuppressEcho++;
					sendChat(szMsg);
				}
				else
				{
					// Tag/isim bilinmiyorsa: kendi flash taramamizla (teleport + /gzinfo)
					// ˆrenmesini ba˛lat; ?zgo tekrar deneyince bulur.
					Log("[ZONE] ?zgo %s icin zone bulunamadi (grup bilinmiyor), flash tarama ba˛latiliyor.", pArg);
					StartFlashScan();
					g_bridgeSuppressEcho++;
					char szMsg[160];
					sprintf_s(szMsg, sizeof(szMsg),
						LM("!%s icin zon taran˝yor, ~30 sn sonra ?zgo %s tekrar dene.",
						   "!Scanning zones for %s, retry ?zgo %s in ~30 sec."), pArg, pArg);
					sendChat(szMsg);
				}
			}
			// --- ?zgo (argumansiz) : zone listesi bilgisi ---
			else if(!strncmp(pGroupMsg, "?zgo", 4) && (pGroupMsg[4] == '\0'))
			{
				Log("[ZONE] ?zgo (argumansiz), /gzinfo gˆnderiliyor.");
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?za / ?zattack [<zoneid | gruptag>] : zone saldirisi - id veya tag ile
// hedef zonu bulup gider, sonra /gz gˆnderir (bot co-leader/leader olmali; yetki sunucuda). ---
			else if((!strncmp(pGroupMsg, "?za", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			     || (!strncmp(pGroupMsg, "?zattack", 8) && (pGroupMsg[8] == '\0' || pGroupMsg[8] == ' ')))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?za)", "!Cooldown: %d secs left (?za)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kald˝), ?za yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONE] %s yakalandi. /gz sunucu komutu gˆnderiliyor...", pGroupMsg);
				// <arg> verildiyse hedefi bul: sayisal = zone id, harf = grup tag'i.
				const char *pA = pGroupMsg + (strncmp(pGroupMsg, "?zattack", 8) == 0 ? 8 : 3);
				while(*pA == ' ') pA++;
				if(pA[0])
				{
					int iZone = -1;
					if(pA[0] >= '0' && pA[0] <= '9')
						iZone = atoi(pA); // kullan˝c˝ belirli zone id'sine gitsin istedi
					else
						iZone = FindZoneByGroupTag(pA);
					if(iZone == -2)
					{
						// TAG CAKISMASI: yanlis gruba saldiri riski - isimle netlestir.
						Log("[ZONE] Tag '%s' birden fazla gruba ait, ?za iptal edildi.", pA);
						g_bridgeSuppressEcho++;
						char szCol[160];
						sprintf_s(szCol, sizeof(szCol),
							LM("!'%s' tag'i birden fazla gruba ait - tam isim yaz (??za <tam isim>).",
								"!'%s' tag belongs to multiple groups - use full name (??za <full name>)."), pA);
						sendChat(szCol);
						return;
					}
					if(iZone >= 0 && iZone < g_zoneCount)
					{
						float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
						float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
						char szCmd[128];
						sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
						Log("[ZONE] %s -> zone #%d, /pos gˆnderildi + /gz.", pA, iZone);
						sendServerCommand(szCmd);
					}
					else
					{
						Log("[ZONE] %s icin zone bulunamadi, yine de /gz deniyor.", pA);
					}
				}
				sendServerCommand((char *)"/gz");
				// Yetki textdraw penceresi: co-leader/leader degilsek sunucu bir textdraw
				// gosterir; ScrShowTextDraw icindeki kontrol onu yakalayip gruba bildirir.
				g_zaPermActive = true;
				g_zaPermUntil = GetTickCount() + 8000;
			}
			// --- ?zsnip <zoneid | gruptag> : zone sniper (YS botu gibi) ---
			// Hedef zonu bul, oraya isinla; o zonede sava˛ SURUYORSA bitisini bekle,
			// bitince /gz ile saldir (zone sniper). Id (sayi) veya isim/tag ile calisir.
			else if(!strncmp(pGroupMsg, "?zsnip", 6) && (pGroupMsg[6] == ' ' || pGroupMsg[6] == '#' || pGroupMsg[6] != '\0'))
			{
				const char *pArg = pGroupMsg + 6;
				while(*pArg == ' ' || *pArg == '#') pArg++;
				if(g_snipPermPending)
				{
					Log("[ZSNIP] Onceki yetki kontrolu hala suruyor, yeni istek yoksayildi.");
					return;
				}
				// ?zsnip off: aktif sniper'i iptal et (uL.Hold: "Aborted by user. ZTK disabled.")
				// off da Co-Leader/Leader yetkisi gerektirir: /gmembers ile dogrulanir.
				if(!_stricmp(pArg, "off"))
				{
					g_snipAction = 1;
					g_snipZone = -1;
					g_snipSenderId = iSenderId;
					g_snipPermPending = true;
					g_waitingForGmembers = true;
					g_gmembersReqTime = GetTickCount();
					GmembersReset();
					Log("[ZSNIP] off icin yetki kontrolu: /gmembers gˆnderiliyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Yetki kontrol ediliyor (/gmembers)...", "!Checking permission (/gmembers)..."));
					sendServerCommand((char *)"/gmembers");
					return;
				}
				int iZone = -1;
				g_snipZoneCount = 0; g_snipZoneIdx = 0; g_snipGroupName[0] = 0;
				if(pArg[0] >= '0' && pArg[0] <= '9')
					iZone = atoi(pArg);
				else
				{
					iZone = FindZoneByGroupTag(pArg);
					// Zincir icin grubu ve TUM zone'larini yakala
					int gi = FindGroupColorByName(pArg);
					if(gi < 0)
					{
						for(int i = 0; i < g_groupColorCount; i++)
							if(g_groupColors[i].szTag[0] && _stricmp(g_groupColors[i].szTag, pArg) == 0) { gi = i; break; }
					}
					if(gi >= 0)
					{
						strncpy_s(g_snipGroupName, sizeof(g_snipGroupName), g_groupColors[gi].szName, sizeof(g_snipGroupName) - 1);
						g_snipZoneCount = CollectGroupZones(g_groupColors[gi].szName, g_snipZones, 64);
						Log("[ZSNIP] Zincir icin %d zone bulundu (grup: %s).", g_snipZoneCount, g_snipGroupName);
					}
				}
				if(iZone == -2)
				{
					// TAG CAKISMASI: yanlis gruba sniper - isimle netlestir.
					Log("[ZSNIP] Tag '%s' birden fazla gruba ait, sniper ba˛latilmadi.", pArg);
					g_bridgeSuppressEcho++;
					char szCol[160];
					sprintf_s(szCol, sizeof(szCol),
						LM("!'%s' tag'i birden fazla gruba ait - tam isim yaz (??zsnip <tam isim>).",
							"!'%s' tag belongs to multiple groups - use full name (??zsnip <full name>)."), pArg);
					sendChat(szCol);
					return;
				}
				if(iZone < 0 || iZone >= g_zoneCount)
				{
					Log("[ZSNIP] %s icin zone bulunamadi, sniper ba˛latilmadi.", pArg);
					char szNF[96];
					sprintf_s(szNF, sizeof(szNF), LM("!Zone bulunamadi: %s", "!Zone not found: %s"), pArg);
					g_bridgeSuppressEcho++;
					sendChat(szNF);
					return;
				}
				// Yetki kapisi: /gmembers ile bot + kullan˝c˝ Co-Leader/Leader mi?
				// Onay gelince (OnGmembersCaptured) sniper ba˛latilir; degilse reddedilir.
				g_snipAction = 0;
				g_snipZone = iZone;
				g_snipSenderId = iSenderId;
				g_snipPermPending = true;
				g_waitingForGmembers = true;
				g_gmembersReqTime = GetTickCount();
				GmembersReset();
				Log("[ZSNIP] Yetki kontrolu: /gmembers gˆnderiliyor (zone #%d, istek: %s).", iZone, pArg);
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Yetki kontrol ediliyor (/gmembers)...", "!Checking permission (/gmembers)..."));
				sendServerCommand((char *)"/gmembers");
			}
			// --- ?yes : zsnip zincirinde siradaki zone'a saldir ---
			else if(!strncmp(pGroupMsg, "?yes", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				if(g_ztkAskNext && g_snipZoneCount > 0 && g_snipZoneIdx < g_snipZoneCount)
				{
					g_ztkAskNext = false;
					int iZone = g_snipZones[g_snipZoneIdx];
					float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
					float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
					char szCmd[128];
					sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
					sendServerCommand(szCmd);
					g_ztkActive = true;
					g_ztkZoneId = iZone;
					g_ztkWaitAnnounced = false;
					Log("[ZSNIP] ?yes: siradaki zone #%d hedeflendi.", iZone);
					g_bridgeSuppressEcho++;
					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg),
						LM("!Zone #%d hedeflendi, savas bitisi bekleniyor...", "!Zone #%d targeted, waiting for the war to end..."), iZone);
					sendChat(szMsg);
				}
			}
			// --- ?no : zsnip zincirini iptal et ---
			else if(!strncmp(pGroupMsg, "?no", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' ') && g_ztkAskNext)
			{
				g_ztkAskNext = false;
				g_snipZoneCount = 0;
				g_snipZoneIdx = 0;
				g_snipGroupName[0] = 0;
				Log("[ZSNIP] ?no: zincir iptal edildi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Sniper zinciri iptal edildi.", "!Sniper chain cancelled."));
			}
			// --- ?zgrp [on|off] / ?tagscan : grup taglarini /groups + /gstats <ID> ile
			// ˆrenir (manuel tag taramasi). "on/off" oto harvest anahtari (DB'de kalici);
			// argsiz = hemen calistir. (?tagscan = manuel tetikleme.) ---
			else if(!strncmp(pGroupMsg, "?zgrp", 5) || !strncmp(pGroupMsg, "?tagscan", 8))
			{
				const char *pArg = pGroupMsg + (!strncmp(pGroupMsg, "?tagscan", 8) ? 8 : 5);
				while(*pArg == ' ') pArg++;
				if(!_stricmp(pArg, "on") || !_stricmp(pArg, "off"))
				{
					g_zgrpAuto = _stricmp(pArg, "on") == 0;
					SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
					Log("[GHP] Oto tag harvest %s.", g_zgrpAuto ? "A«IK (tek seferlik)" : "KAPALI");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM(g_zgrpAuto ? "!Oto tag toplama: A«IK (1 kere, sonra off)." : "!Oto tag toplama: KAPALI.",
						g_zgrpAuto ? "!Auto tag harvest: ON (once, then off)." : "!Auto tag harvest: OFF."));
					if(g_zgrpAuto)
					{
						// Tek seferlik tetik: g_ghpLastRun sifirlanir, ProcessZoneScan
						// spawn'da oldugu gibi bir kez harvest ba˛latir.
						g_ghpLastRun = 0;
						g_ghpLearnedCount = 0;
						// ?zgrp on A«IK secildi: 60 sn spawn korumasi gectiyse harvest i
						// HEMEN ba˛lat (otomatik kapidan beklemek yerine). Zone taramasi
						// aktifse durdur - yoksa tarama /gzinfo dialogu harvest /gstats
						// zinciriyle cakisip ikisi de takilirdi.
						if(g_spawnTick != 0 && (GetTickCount() - g_spawnTick) >= 60000 && !g_ghpActive && !g_inZoneWar && !g_zgoActive)
						{
							if(g_zoneScanIdx >= 0)
							{
								Log("[GHP] ?zgrp on: aktif zone taramasi durduruldu, harvest ˆncelikli.");
								g_zoneScanIdx = -1;
								g_zoneScanQ = 0;
							}
							Log("[GHP] ?zgrp on: harvest hemen ba˛latiliyor (/groups).");
							g_bridgeSuppressEcho++;
							sendChat((char *)LM("!Grup listesi ˆreniliyor...", "!Learning group list..."));
							g_ghpLearnedCount = 0;
							SyncGroupColorsToDb();
							g_waitingForGroups = true;
							g_groupsReqTime = GetTickCount();
							sendServerCommand((char *)"/groups");
						}
						else
							Log("[GHP] ?zgrp on: otomatik kapi bekleniyor (60 sn koruma / sava˛ / aktif i˛lem).");
					}
				}
				else
				{
					Log("[GHP] %s: /groups cagriliyor...", !strncmp(pGroupMsg, "?tagscan", 8) ? "?tagscan" : "?zgrp");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Grup listesi ˆreniliyor...", "!Learning group list..."));
					g_ghpLearnedCount = 0;
					// Mevcut txt'teki (zaten ˆrenilmis) tag'leri hemen DB'ye yaz.
					SyncGroupColorsToDb();
					g_waitingForGroups = true;
					g_groupsReqTime = GetTickCount();
					sendServerCommand((char *)"/groups");
				}
			}
			// --- ?zf <#N> : zone durumu sorgula -√•√Ü /gzinfo + flashing zone dagilimi ---
			else if(!strncmp(pGroupMsg, "?zf", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				Log("[ZONE] ?zf yakalandi, mevcut flashing verisiyle aninda ozet gˆnderiliyor.");
				// Her ?zf'de /gzinfo gˆndermek komut spam'ine (+ mute riski) ve mesaj˝n
				// gec atilmasina yol aciyordu. g_zoneFlash sunucudan periyodik beslenir;
				// mevcut veriyle aninda cevap ver (YS botu gibi).
				BuildFlashZoneSummary();

				// Arkada, rengi BILINMEYEN flashing zone'lari haritadan ˆren (teleport +
				// /gzinfo): renkler 0xHEX ile group_colors'a ve DB'ye dogru yazilir. Tarama
				// tempolu (spam'siz); bitince guncel ozet yeniden gˆnderilir (g_zfScanResend).
				// Boylece eksik/yanlis renkler zamanla dogru toplanir.
				StartFlashScan();

				// Tag'ler /groups+/gstats ile ?zgrp'de ˆrenilir ve DB'de kalicidir
				// (dosya + MySQL). ?zf SADECE mevcut bilgiyle ozet atar; otomatik
				// /gstats spam'i komut mute riskini artiriyordu, onun yerine bilinmeyen
				// renkli flashing zonlar ?zgrp ile (elle) cekilir.
			}
			// --- ?gz : zone war ba˛latma -√•√Ü Discord zonewar kanalina bildirim, 180 sn cooldown (RakGravy ile ayni) ---
			else if(!strncmp(pGroupMsg, "?gz", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kald˝ (?gz)", "!Cooldown: %d secs left (?gz)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kald˝), ?gz yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONEWAR] Grup chat'inde ?gz yakalandi. /gz sunucu komutu gˆnderiliyor (Discord bildirimi yok).");
				sendServerCommand((char *)"/gz");
			}
			// --- ?gz2 : GZ'ye girmeyip gruba belirgin "/gz2 yazin" duyurusu atar (bot sava˛a katilmaz) ---
			else if(!strncmp(pGroupMsg, "?gz2", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				Log("[ZONEWAR] ?gz2 yakalandi, grup chatine belirgin duyuru atiliyor (bot GZ'ye girmiyor).");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!@@@@ /gz2 @@@@ GZ'ye girin! (sava˛ devam ediyor) @@@@ /gz2 @@@@",
					"!@@@@ /gz2 @@@@ Enter GZ! (war is ongoing) @@@@ /gz2 @@@@"));
			}
			// --- ?grole <isim> <rol> : oyun ici rol dei˛iklii (memb -> Member, co -> Co-Leader) ---
			// Diskot/direkt ile ayni otomasyonu kullanir (diyalog + cift tik + inputtext).
			else if(!strncmp(pGroupMsg, "?grole", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				char t[64] = {0}, r[64] = {0};
				if(sscanf(pGroupMsg + 6, "%63s %63s", t, r) == 2 && t[0] && r[0])
				{
					char fullRole[64] = {0};
					if(!_stricmp(r, "mem") || !_stricmp(r, "memb") || !_stricmp(r, "member"))
						strcpy(fullRole, "Member");
					else if(!_stricmp(r, "co") || !_stricmp(r, "coleader") || !_stricmp(r, "co-leader"))
						strcpy(fullRole, "Co-Leader");
					else
						strncpy_s(fullRole, sizeof(fullRole), r, sizeof(fullRole) - 1);
					Log("[GROLE] Oyun ici ?grole: '%s' -> '%s' (isteyen: %d).", t, fullRole, iSenderId);
					StartGrole(t, fullRole);
				}
				else
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullan˝m: ?grole <isim> <memb|co>", "!Usage: ?grole <name> <memb|co>"));
				}
			}
			// --- ?grank <isim> <rank> : oyun ici rank dei˛iklii ---
			else if(!strncmp(pGroupMsg, "?grank", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				char t[64] = {0}, r[64] = {0};
				if(sscanf(pGroupMsg + 6, "%63s %63s", t, r) == 2 && t[0] && r[0])
				{
					Log("[GRANK] Oyun ici ?grank: '%s' -> '%s' (isteyen: %d).", t, r, iSenderId);
					StartGrank(t, r);
				}
				else
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullan˝m: ?grank <isim> <rank>", "!Usage: ?grank <name> <rank>"));
				}
			}
			// --- Grup chat kopusu: ! ile ba˛layan mesajlar Discord'a iletilir.
			// ornek: oyunda "!test" yazilir -> bot sunucudan "!test" (GROUP) alir,
			// basta ! (Discord'a gˆnderim isareti), sonrasi ("test") Discord'a yollanir.
			// (Geri echo: sendChat ile gˆnderilen kendi mesajlarimizda da ! on-eki var;
			//  g_bridgeSuppressEcho ile Discord'a tekrar gitmeleri engellenir.)
			// Discord'dan oyuna iletilen mesajlar "![Discord] ..." on-ekiyle gelir;
			// bunlar tekrar Discord'a gˆnderilmez (sonsuz dongu engeli).
			else if(pGroupMsg[0] == '!')
			{
				if(g_bridgeSuppressEcho > 0)
				{
					g_bridgeSuppressEcho--;
					return;
				}
				// Konsol'dan (cmds.cpp ! yolu) yazilan mesajlar gruba gider ama
				// Discord'a KOPRULENMEZ: iSenderId == bot = bot/console gˆnderimi.
				// Oyuncularin oyun ici !<mesaj> koprusu iSenderId != bot oldugu
				// icin calismaya devam eder.
				if(iSenderId == g_myPlayerID)
				{
					Log("[GROUPCHAT] Bot/konsol mesaj˝ Discord'a iletilmedi: %s", pGroupMsg + 1);
					return;
				}
				if(strncmp(pGroupMsg, "![Discord]", 10) == 0)
				{
					Log("[GROUPCHAT] Discord geri iletimi atlandi (%s).", pGroupMsg);
					return;
				}
				Log("[GROUPCHAT] Grup mesaj˝ Discord'a iletilmek uzere: %s", pGroupMsg + 1);
				SendGroupChatToDiscord(pGroupMsg + 1);
			}
}

void Chat(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	PlayerID sender = rpcParams->sender;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteTextLen;

	unsigned char szText[256];
	memset(szText, 0, 256);

	bsData.Read(playerId);
	bsData.Read(byteTextLen);
	bsData.Read((char*)szText, byteTextLen);
	szText[byteTextLen] = 0;

	if(playerId < 0 || playerId >= MAX_PLAYERS)
		return;

	if(imitateID == playerId || playerId == g_myPlayerID)
		sendChat((char *)szText);

	Log("[CHAT] %s: %s", playerInfo[playerId].szPlayerName, szText);

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr((const char *)szText, settings.findItems[i].szFind))
			{
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
			}
		}
	}
}

void UpdateScoresPingsIPs(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	int iPlayerScore;
	DWORD dwPlayerPing;

	for(PLAYERID i=0; i<(iBitLength/8)/9; i++)
	{
		bsData.Read(playerId);
		bsData.Read(iPlayerScore);
		bsData.Read(dwPlayerPing);

		if(playerId < 0 || playerId >= MAX_PLAYERS)
			continue;

		playerInfo[playerId].iScore = iPlayerScore;
		playerInfo[playerId].dwPing = dwPlayerPing;
	}
}

void SetCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.CurrentCheckpoint.fPosition[0]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[1]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[2]);
	bsData.Read(settings.CurrentCheckpoint.fSize);

	settings.CurrentCheckpoint.bActive = true;

	char SetCheckpointAlert[256];
	sprintf_s(SetCheckpointAlert, 256, "[CP] Checkpoint set to %.2f %.2f %.2f position. (size: %.2f)", settings.CurrentCheckpoint.fPosition[0], settings.CurrentCheckpoint.fPosition[1], settings.CurrentCheckpoint.fPosition[2], settings.CurrentCheckpoint.fSize);
	Log(SetCheckpointAlert);
}

void DisableCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	settings.CurrentCheckpoint.bActive = false;

	Log("[CP] Current checkpoint disabled.");
}

void Pickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;
	PICKUP Pickup;

	bsData.Read(PickupID);
	bsData.Read((PCHAR)&Pickup, sizeof(PICKUP));

	if(settings.uiPickupsLogging != 0)
	{
		char szCreatePickupAlert[256];
		sprintf_s(szCreatePickupAlert, sizeof(szCreatePickupAlert), "[CREATEPICKUP] ID: %d | Model: %d | Type: %d | X: %.2f | Y: %.2f | Z: %.2f", PickupID, Pickup.iModel, Pickup.iType, Pickup.fX, Pickup.fY, Pickup.fZ);
		Log(szCreatePickupAlert);
	}
}

void DestroyPickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;

	bsData.Read(PickupID);

	if(settings.uiPickupsLogging != 0)
	{
		Log("[DESTROYPICKUP] %d", PickupID);
	}
}

void RequestClass(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteRequestOutcome = 0;

	bsData.Read(byteRequestOutcome);

	if(byteRequestOutcome)
	{
		bsData.Read((PCHAR)&SpawnInfo,sizeof(PLAYER_SPAWN_INFO));

		iLocalPlayerSkin = SpawnInfo.iSkin;
	}
}

// RPC_Spawn (sunucu->istemci): sunucu oyuncuyu gercekten spawn ettigi an. Bu onay gelince
// /fr (freeroam) komutunu tam zamaninda gˆnderir. UIF'de oyuncu komutlar˝ RPC_ServerCommand
// uzerinden islenir (RPC_Chat chat mesaj˝ olarak echo edilir), bu yuzden sendServerCommand kullan.
// Not: /fr'yi erken (login/auth oncesi) gˆnderip g_frSentOnce'i yutma; buradaki tek gˆnderim,
// spawn men¸s¸ "Freeroam" secildikten sonraki GERCEK spawn onayina denk gelir.
void SpawnConfirmed(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	// Login bekleniyor: kullan˝c˝ ˛ifre girip hesaba girene kadar /fr ve akislar
	// bekletilir (tmp hesaba atilip gercek hesap kaybedilmesin).
	if(g_loginPending)
	{
		Log("[LOGIN] Spawn onayi geldi ama login bekleniyor, /fr ertelendi (tmp hesap onleniyor).");
		return;
	}

	iSpawned = 1;
	bIsSpectating = 0;
	g_zoneScanWaitStart = GetTickCount(); // oto-tarama sayaci: 30 sn sonra ilk tarama

	// Spawn onayi (RPC_Spawn) alindi: /fr'yi GONDER. Spawn men¸s¸ secimi zaten
	// bir kez /fr gˆnderdi; kullan˝c˝ istegi: spawn olduktan sonra da /fr tekrar
	// gˆnderilsin (virtual map'te kaliyorsa freeroam'a gecmesi icin). Custom pos
	// /pos ile GONDERILMEZ ‚Äî sadece /fr, sonrasinda normal akis devam eder.
	Log("[SPAWN] Sunucu spawn onayi (RPC_Spawn) alindi, /fr (freeroam) tekrar gˆnderiliyor.");
	Log("[SPAWN] Kendi oyuncu adim: '%s' (XML nickname: '%s')", playerInfo[g_myPlayerID].szPlayerName, g_szNickName);
	sendServerCommand((char *)"/fr");

	// Reconnect + spawn: tarama aktifse KALDIGI ZONEDEN DEVAM (o zone'a yeniden
	// teleport + /gzinfo). Scan state'i (queue/idx) hafizada korunur. Mute-bayragi
	// olsun olmasin calisir - genel balant˝ kopup gelince de ayni koruma gerekli,
	// yoksa spawn noktasinda /gzinfo atip YANLIS zone sahibi ˆreniyordu.
	if(g_zoneScanIdx >= 0 && g_zoneScanIdx < g_zoneScanQ)
	{
		bool bWasMute = g_muteReconnectPending;
		g_muteReconnectPending = false;
		Log("[ZF:SCAN] Yeniden spawn: kald˝gi zone #%d'ye devam ediliyor (%s).",
			g_zoneScanQueue[g_zoneScanIdx], bWasMute ? "mute-reconnect" : "yeniden balant˝");
		GangZoneScanGo(g_zoneScanQueue[g_zoneScanIdx]);
	}

	// Yeniden ba˛latma sonrasi restore edilen sava˛ varsa: KISA GZ giri˛ii yap (anlik
	// /gz2) -> guncel timer textdraw'i gelir, CaptureWarTimer stale restore'u tazeler;
	// 4 sn sonra ProcessZoneWarAutoJoin /fr ile geri doner. Timer gelmezse sava˛ bitmis
	// demektir (g_inZoneWar guncellenir). Boylece ?gzt dogru sure gosterir + oto tarama
	// (StartZoneScan) sava˛ yoksa devam edebilir.
	if(g_inZoneWar && g_warRemainingSec >= 0 && !g_zwReturnPending && !g_zwKillListStay)
	{
		g_zwReturnPending = true;
		g_zwReturnAt = GetTickCount() + 4000;
		Log("[WAR:TIMER] Restore sonrasi kisa GZ giri˛ii: timer dogrulaniyor (%d sn restore).", g_warRemainingSec);
		sendServerCommand((char *)"/gz2");
	}
}

void ScrInitMenu(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	memset(&GTAMenu, 0, sizeof(struct stGTAMenu));

	BYTE byteMenuID;
	BOOL bColumns; // 0 = 1, 1 = 2
	CHAR cText[MAX_MENU_LINE];
	float fX;
	float fY;
	float fCol1;
	float fCol2 = 0.0;
	MENU_INT MenuInteraction;

	bsData.Read(byteMenuID);
	bsData.Read(bColumns);
	bsData.Read(cText, MAX_MENU_LINE);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fCol1);
	if (bColumns) bsData.Read(fCol2);
	bsData.Read(MenuInteraction.bMenu);
	for (BYTE i = 0; i < MAX_MENU_ITEMS; i++)
		bsData.Read(MenuInteraction.bRow[i]);

	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szTitle, cText);

	BYTE byteColCount;
	bsData.Read(cText, MAX_MENU_LINE);
	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szSeparator, cText);

	bsData.Read(byteColCount);
	GTAMenu.byteColCount = byteColCount;
	for (BYTE i = 0; i < byteColCount; i++)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		Log("[MENU:%d] %s", i, cText);
		strcpy(GTAMenu.szColumnContent[i], cText);
	}

	if (bColumns)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		//Log("4: %s", cText);

		bsData.Read(byteColCount);
		for (BYTE i = 0; i < byteColCount; i++)
		{
			bsData.Read(cText, MAX_MENU_LINE);
			//Log("5: %d %s", i, cText);
		}
	}
}

LRESULT CALLBACK SAMPDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndEditBox = GetDlgItem(hwnd, IDE_INPUTEDIT);
	HWND hwndListBox = GetDlgItem(hwnd, IDL_LISTBOX);
	WORD wSelection;
	char szResponse[257];

	switch(msg)
	{
	case WM_CREATE:
		{
			HINSTANCE hInst = GetModuleHandle(NULL);
			switch(sampDialog.bDialogStyle)
			{
				case DIALOG_STYLE_MSGBOX:
					if(sampDialog.bButton1Len == 0 && sampDialog.bButton2Len == 0)
					{
						// no butans, no badi cana cross it
					}
					if(sampDialog.bButton1Len != 0 && sampDialog.bButton2Len == 0) // a butan
					{
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							150, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);
					}
					else if(sampDialog.bButton1Len != 0 && sampDialog.bButton2Len != 0) // tu butans
					{
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;

				case DIALOG_STYLE_INPUT:
				case DIALOG_STYLE_PASSWORD:
					{
						CreateWindowEx(NULL, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
							50, 200, 300, 24, hwnd, (HMENU)IDE_INPUTEDIT, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;

				case DIALOG_STYLE_LIST:
					{
						hwndListBox = CreateWindowEx(NULL, "LISTBOX", "",
							WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_HASSTRINGS,
							10, 10, 375, 225, hwnd, (HMENU)IDL_LISTBOX, hInst, NULL);

						char *szInfoTemp = strtok(sampDialog.szInfo, "\n");
						while(szInfoTemp != NULL)
						{
							int id = SendMessage(hwndListBox, LB_ADDSTRING, 0, (LPARAM)szInfoTemp);
							SendMessage(hwndListBox, LB_SETITEMDATA, id, (LPARAM)id);

							szInfoTemp = strtok(NULL, "\n");
						}
						
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;
			}

		}
		break;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDB_BUTTON1:
					if(sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
					{
						wSelection = (WORD)SendMessage(hwndListBox, LB_GETCURSEL, 0, 0);
						if(wSelection != (WORD)-1)
						{
							SendMessage(hwndListBox, LB_GETTEXT, wSelection, (LPARAM)szResponse);
							sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
							PostQuitMessage(0);
						}
						break;
					}

					GetWindowText(hwndEditBox, szResponse, 257);
					// LOGIN DIALOG: kullan˝c˝ Login'e basti ‚Äî ne gˆnderildigini logla
					Log("[DIALOG-SUBMIT] Button1, dialog=%d style=%d, input='%s' (len=%d)",
						sampDialog.wDialogID, sampDialog.bDialogStyle, szResponse, (int)strlen(szResponse));
					if(strstr(sampDialog.szTitle, "Login") || strstr(sampDialog.szTitle, "Giri˛"))
					{
						// Login submit: GTA client ile BIREBIR AYNI format (BYTE uzunluk,
						// listitem 0xFFFF). sendDialogResponse WORD kullaniyor ‚Äî gstats icin
						// sorun degil ama login ˛ifresini sunucu siki parse ediyor.
						BYTE bRespLen = (BYTE)strlen(szResponse);
						RakNet::BitStream bsLogin;
						bsLogin.Write((uint16_t)sampDialog.wDialogID);
						bsLogin.Write((uint8_t)1);
						bsLogin.Write((uint16_t)0xFFFF);
						bsLogin.Write((uint8_t)bRespLen);
						bsLogin.Write(szResponse, bRespLen);
						pRakClient->RPC(&RPC_DialogResponse, &bsLogin, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
						Log("[LOGIN] ﬁifre GTA-formatinda gˆnderildi ('%s', len=%d).", szResponse, (int)bRespLen);
					}
					else
						sendDialogResponse(sampDialog.wDialogID, 1, 0xFFFF, szResponse);
					PostQuitMessage(0);
					break;

				case IDB_BUTTON2:
					GetWindowText(hwndEditBox, szResponse, 257);
					sendDialogResponse(sampDialog.wDialogID, 0, 0xFFFF, szResponse);
					PostQuitMessage(0);
					break;
			}
		}

		break;

	case WM_PAINT:
		{
			if(sampDialog.bDialogStyle != DIALOG_STYLE_LIST)
			{
				RECT rect;
				GetClientRect(hwnd, &rect);
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				HDC hdcMem = CreateCompatibleDC(hdc);
				SelectObject(hdc, hSAMPDlgFont);
				DrawText(hdc, sampDialog.szInfo, strlen(sampDialog.szInfo), &rect, DT_WORDBREAK | DT_EXPANDTABS);
				DeleteDC(hdcMem);
				EndPaint(hwnd, &ps);
			}
			else
			{
				RECT rect;
				GetClientRect(hwnd, &rect);
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				HDC hdcMem = CreateCompatibleDC(hdc);
				DeleteDC(hdcMem);
				EndPaint(hwnd, &ps);
			}
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

DWORD WINAPI DialogBoxThread(PVOID)
{
	WNDCLASSEX wc;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	RECT conRect;
	if(settings.iConsole)
		GetWindowRect(GetConsoleWindow(), &conRect);
	else
		GetWindowRect(hwnd, &conRect);

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = SAMPDlgProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "dlgWndClass";
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc))
		return 0;

	hSAMPDlgFont = CreateFont(18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");

	hwndSAMPDlg = CreateWindowEx(NULL, "dlgWndClass", sampDialog.szTitle, NULL,
		conRect.right, conRect.top, 400, 300, NULL, NULL, hInstance, NULL);

	if(hwndSAMPDlg == NULL)
		return 0;

	ShowWindow(hwndSAMPDlg, 1);
	UpdateWindow(hwndSAMPDlg);
	SetForegroundWindow(hwndSAMPDlg);

	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	sampDialog.iIsActive = 0;
	SendMessage(hwndSAMPDlg, WM_DESTROY, 0, 0);
	DestroyWindow(hwndSAMPDlg);
	UnregisterClass("dlgWndClass", GetModuleHandle(NULL));
	hSAMPDlgFont = NULL;
	TerminateThread(hDlgThread, 0);

	return 0;
}

void ScrDialogBox(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(sampDialog.wDialogID);
	bsData.Read(sampDialog.bDialogStyle);

	bsData.Read(sampDialog.bTitleLength);
	bsData.Read(sampDialog.szTitle, sampDialog.bTitleLength);
	sampDialog.szTitle[sampDialog.bTitleLength] = 0;

	bsData.Read(sampDialog.bButton1Len);
	bsData.Read(sampDialog.szButton1, sampDialog.bButton1Len);
	sampDialog.szButton1[sampDialog.bButton1Len] = 0;

	bsData.Read(sampDialog.bButton2Len);
	bsData.Read(sampDialog.szButton2, sampDialog.bButton2Len);
	sampDialog.szButton2[sampDialog.bButton2Len] = 0;

	char szDlgInfo[2048] = {0};
	stringCompressor->DecodeString(szDlgInfo, sizeof(szDlgInfo), &bsData);

	// --- SPAWN MENUSU: "Where would you like to go?" otomatik Freeroam secimi ---
	// Spawn men¸s¸ (id=30, style 5 TABLIST_HEADERS) acildiginda "Freeroam" satirini
	// secip dialogu gˆndeririz. Bu sayede bot virtual map'e dusmez, gercek freeroam
	// bolgesine spawn olur. Bulunamazsa menu aÁ˝k birakilir (asla kapatilmaz).
	bool bIsSpawnMenu = (strstr(sampDialog.szTitle, "Where would you like to go") != NULL);
	if(bIsSpawnMenu && (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
		sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS))
	{
		int iRow = 0, iItem = 0;
		int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
		char *pLine = szDlgInfo;
		char *pNext = strchr(pLine, '\n');
		bool bSelected = false;
		while(pLine && *pLine)
		{
			int iLen = pNext ? (int)(pNext - pLine) : (int)strlen(pLine);
			if(iLen > 0)
			{
				char szRow[256];
				int iCopy = iLen < 255 ? iLen : 255;
				memcpy(szRow, pLine, iCopy);
				szRow[iCopy] = 0;
				while(iCopy > 0 && szRow[iCopy-1] == '\r') szRow[--iCopy] = 0;
				Log("[SPAWNMENU] row %d: %s", iRow + 1, szRow);
				if(iRow >= iHeaderRows && strstr(szRow, "Freeroam"))
				{
					iItem = iRow - iHeaderRows;
					Log("[SPAWNMENU] 'Freeroam' secildi (ListItem %d).", iItem);
					// SPAWN MENUSU CEVABI: GTA client ile BIREBIR AYNI format (BYTE uzunluk).
					// sendDialogResponse WORD kullaniyor ‚Äî sunucu BYTE bekledigi icin "Freeroam"
					// secimi islenmiyor, bot virtual world'de kaliyordu. (Login'de ayni sorun
					// cozuldu; spawn men¸s¸ de ayni formatla gˆnderilir.)
					{
						const char *szSel = "Freeroam";
						BYTE bSelLen = (BYTE)strlen(szSel);
						RakNet::BitStream bsSel;
						bsSel.Write((uint16_t)sampDialog.wDialogID);
						bsSel.Write((uint8_t)1);
						bsSel.Write((uint16_t)iItem);
						bsSel.Write((uint8_t)bSelLen);
						bsSel.Write(szSel, bSelLen);
						pRakClient->RPC(&RPC_DialogResponse, &bsSel, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
					}
					sampDialog.iIsActive = 0;
					g_dwSpawnMenuHandled = GetTickCount();
					bSelected = true;

					// Freeroam secimi cevaplandigina gˆre /fr'yi gˆnder.
					// VIRTUAL WORLD FIX: BYTE formatli cevap + /fr ‚Äî bot gercek freeroam'da
					// spawn olur (SF kulesi/virtual world'de kalmaz).
					Log("[SPAWN] Spawn men¸s¸ 'Freeroam' cevaplandi, /fr (freeroam) gˆnderiliyor.");
					sendServerCommand((char *)"/fr");
					break;
				}
			}
			iRow++;
			if(!pNext) break;
			pLine = pNext + 1;
			pNext = strchr(pLine, '\n');
		}
		if(bSelected)
			return;
		Log("[SPAWNMENU] Freeroam satiri bulunamadi, menu aÁ˝k birakildi.");
	}

	// /gstats dialogu sonucu: "- Tag: [XX]" satirindan kisa etiketi (tag) ˆren ve
	// group_colors'a yaz. /gzinfo isim/renk baglar; tag ise /gstats'tan gelir (her
	// /gstats acilisinda - manuel ?gstats, Discord komutu, ?zgrp harvest). ?zgrp
	// harvest aktifse (g_ghpActive) zinciri de ilerlet (sonraki /gstats <ID>).
	if(g_waitingForGStats)
	{
		LearnGroupTagFromGStats(szDlgInfo);
		SaveGroupColors();
		if(g_ghpActive) OnGStatsResultForHarvest(szDlgInfo);
	}

	// /pnetstats ve /gstats dialog yakalamasi: yakalandiysa ekrana cizdirme
	if(HandleCapturedDialog(sampDialog.wDialogID, sampDialog.szTitle, szDlgInfo))
	{
		sampDialog.iIsActive = 0;
		return;
	}

	// --- GLOBAL AUTOCLOSE ---
	// AÁ˝k kalmasi zorunlu diyaloglar disindakileri otomatik kapat (ekranda birikmesin).
	// AÁ˝k kalanlar: Login/Giri˛/Authentication, spawn men¸s¸ (freeroam secilmediyse),
	// /astats icin account ID giri˛i (kullan˝c˝ yazacak).
	bool bKeepOpen = false;
	if(strstr(sampDialog.szTitle, "Giri˛") || strstr(sampDialog.szTitle, "Login") ||
		strstr(sampDialog.szTitle, "Authentication"))
		bKeepOpen = true;
	else if(bIsSpawnMenu)
		bKeepOpen = true;
	else if(sampDialog.bDialogStyle == DIALOG_STYLE_INPUT && g_waitingForGStats && !g_gstatsAutoSubmit)
		bKeepOpen = true;

	if(!bKeepOpen)
	{
		if(strstr(sampDialog.szTitle, "Zone Info"))
		{
			Log("[GZINFO-DATA] '%s': %.330s", sampDialog.szTitle, szDlgInfo);
			Log("[GZINFO-BTNS] btn1='%s' btn2='%s'", sampDialog.szButton1, sampDialog.szButton2);
			// Her gzinfo dialogundan ˆren (tarama aktif olmasa da): ?zf'nin kendi
			// /gzinfo'su su anki gercek sahibi getirir, eski/eski cache'i ezer.
			GangZoneScanGzDialog(szDlgInfo);
		}
		Log("[AUTOCLOSE] Dialog id=%d '%s' (style %d) kapat˝ld˝.", sampDialog.wDialogID, sampDialog.szTitle, sampDialog.bDialogStyle);
		sendDialogResponse(sampDialog.wDialogID, 0, 0xFFFF, (char *)"");
		sampDialog.iIsActive = 0;
		return;
	}

	strncpy(sampDialog.szInfo, szDlgInfo, 256);
	sampDialog.szInfo[256] = 0;

	switch(sampDialog.bDialogStyle)
	{
		case DIALOG_STYLE_MSGBOX:
		case DIALOG_STYLE_INPUT:
		case DIALOG_STYLE_LIST:
		case DIALOG_STYLE_PASSWORD:
			if(!sampDialog.iIsActive)
			{
				sampDialog.iIsActive = 1;
				hDlgThread = CreateThread(NULL, 0, DialogBoxThread, NULL, 0, NULL);
			}
		break;

		default:
			if(sampDialog.iIsActive)
			{
				sampDialog.iIsActive = 0;
				SendMessage(hwndSAMPDlg, WM_DESTROY, 0, 0);
				DestroyWindow(hwndSAMPDlg);
				UnregisterClass("dlgWndClass", GetModuleHandle(NULL));
				hSAMPDlgFont = NULL;
				TerminateThread(hDlgThread, 0);
			}
		break;
	}
}

void ScrGameText(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	char szMessage[400];
	int iType, iTime, iLength;

	bsData.Read(iType);
	bsData.Read(iTime);
	bsData.Read(iLength);

	if(iLength > 400) return; // tsk tsk, kye

	bsData.Read(szMessage, iLength);
	szMessage[iLength] = '\0';

	Log("[GAMETEXT] %s", szMessage);

	// "Your group is not in a zone war": bot /gz2 kontrol girdisi gˆnderdi ama
	// sava˛ yok. Sava˛ durumunu sifirla ve kontrol etmeyi BIRAK.
	if(strstr(szMessage, "not in a zone war"))
	{
		if(g_ztkAttackPending)
			g_ztkOutcome = 2; // zsnip /gz saldirisi savas baslatamadi -> kaybedildi say
		if(g_inZoneWar)
			Log("[GAMETEXT] not in a zone war - sava˛ durumu temizleniyor, kontroller durduruluyor.");
		g_inZoneWar = false;
		g_zoneWarStartTick = 0;
		g_warRemainingSec = -1;
		g_warTimerTick = 0;
		g_warTimerViaTdw = false;
		g_gztCountdownAnnounced = 999;
		g_gztLastRem = -1;
		g_zwCheckJoinSent = false;
		g_zwAutoJoinSent = false;
		g_zwReturnPending = false;
		g_zwKillListStay = false;
		g_zwCapturingKillList = false;
		g_zwKillListSent = false;
		g_zwWarZone[0] = 0;
	}

	// "Your group won/lost the zone war": kendi grubumuzun savasi bitti.
	// Chat war-end tespiti ("loses to" vb.) paket kaybinda kacabilir; game text
	// guvenilir ek sinyal. Sonuc zsnip zincirine islenir + war durumu temizlenir.
	if(strstr(szMessage, "won the zone war"))
	{
		g_zwWeWon = true;
		g_ztkOutcome = 1;
		if(g_ztkActive) g_ztkTargetWar = false;
		Log("[GAMETEXT] Grubumuz savasi KAZANDI (zsnip sonuc=%d).", g_ztkOutcome);
		if(g_inZoneWar)
		{
			g_inZoneWar = false;
			g_zoneWarStartTick = 0;
			g_warRemainingSec = -1;
			g_warTimerTick = 0;
			g_warTimerViaTdw = false;
			g_gztCountdownAnnounced = 999;
			g_gztLastRem = -1;
			g_zwWarZone[0] = 0;
		}
	}
	else if(strstr(szMessage, "lost the zone war"))
	{
		g_zwWeWon = false;
		g_ztkOutcome = 2;
		if(g_ztkActive) g_ztkTargetWar = false;
		Log("[GAMETEXT] Grubumuz savasi KAYBETTI (zsnip sonuc=%d).", g_ztkOutcome);
		if(g_inZoneWar)
		{
			g_inZoneWar = false;
			g_zoneWarStartTick = 0;
			g_warRemainingSec = -1;
			g_warTimerTick = 0;
			g_warTimerViaTdw = false;
			g_gztCountdownAnnounced = 999;
			g_gztLastRem = -1;
			g_zwWarZone[0] = 0;
		}
	}
}

void ScrPlayAudioStream(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned char bURLLen;
	char szURL[256];

	bsData.Read(bURLLen);
	bsData.Read(szURL, bURLLen);
	szURL[bURLLen] = 0;

	Log("[AUDIO_STREAM] %s", szURL);
}

void ScrSetDrunkLevel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(iDrunkLevel);
}

void ScrHaveSomeMoney(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iGivenMoney;
	bsData.Read(iGivenMoney);

	iMoney += iGivenMoney;
}

void ScrResetMoney(RPCParameters *rpcParams)
{
	iMoney = 0;
}

void ScrSetPlayerPos(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModePos[0]);
		bsData.Read(settings.fNormalModePos[1]);
		bsData.Read(settings.fNormalModePos[2]);
	}
}

void ScrSetPlayerFacingAngle(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModeRot);
	}
}

// RPC 108 (RPC_ScrAddGangZone): sunucu gang zone olu-stururken minX,minY,maxX,maxY
// float'lar-‚ñí g-nderir; zone ID client taraf-‚ñínda s-‚ñíral-‚ñí atan-‚ñír. UIF zone numaralar-‚ñí
// (#0'dan itibaren, haritan-‚ñín -‚ïùst solundan) bu s-‚ñírayla e-sle-sir -> ?zgo i-gin kaydet.
void ScrAddGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	// Format (cozumlendi): WORD zoneId + 4 float (minX, minY, maxX, maxY) + 4 byte renk.
	// Renk paketten OKUNUR ama SAKLANMAZ/KULLANILMAZ - eslesme isim bazlidir.
	unsigned short usZoneId;
	float fMinX, fMinY, fMaxX, fMaxY;
	unsigned int uiColor = 0;
	if(!bsData.Read(usZoneId) || !bsData.Read(fMinX) || !bsData.Read(fMinY) || !bsData.Read(fMaxX) || !bsData.Read(fMaxY))
		return;
	bsData.Read(uiColor); // atilir

	if(usZoneId < MAX_GANG_ZONES)
	{
		g_zoneMinX[usZoneId] = fMinX;
		g_zoneMinY[usZoneId] = fMinY;
		g_zoneMaxX[usZoneId] = fMaxX;
		g_zoneMaxY[usZoneId] = fMaxY;
		if((int)usZoneId + 1 > g_zoneCount)
			g_zoneCount = (int)usZoneId + 1;
	}
}

// RPC 120 (RPC_ScrRemoveGangZone): zone kald-‚ñír-‚ñíl-‚ñír. Listeyi s-‚ñíf-‚ñírlamay-‚ñíz -g-‚ïùnk-‚ïù
// zone numaralar-‚ñí (#N) sabittir; sadece gerekirse tekrar toplan-‚ñír.
void ScrRemoveGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned int uiZoneID = 0;
	if(bsData.Read(uiZoneID))
		Log("[GANGZONE] zone kald˝rildi: %u", uiZoneID);
}

// RPC 121 (RPC_ScrFlashGangZone): UIF `/gzinfo` ile zone'lar flaslanir. ?zf icin
// hangi zone'larin flaslandigi buradan cikarilir. RENK KULLANILMAZ (eslesme isim+tag).
void ScrFlashGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned short usZoneId = 0;
	bsData.Read(usZoneId);
	if(usZoneId >= MAX_GANG_ZONES) return;
	g_zoneFlash[usZoneId] = true;
	Log("[GANGZONE:FLASH] id=%u", usZoneId);

	// Surekli harita izleme (YS.Hold: "rely on Flash and a monitoring system to
	// constantly track the map"): ismi bilinmeyen bir zone flashlanirsa otomatik takip,
	// StartFlashScan /gzinfo ile sahibini ˆrenir ve ?zf'ye ekler. ?zgrp harvest
	// surerken tetikLENMEZ - o an yalnizca taglar ˆrenilir.
	if(g_zoneOwnerName[usZoneId][0] == 0 && g_zscanAuto && !g_ghpActive)
	{
		Log("[GANGZONE:FLASH] Ismi bilinmeyen zone #%u flashlandi, otomatik tarama tetikleniyor.", usZoneId);
		StartFlashScan();
	}
}

// RPC 85 (RPC_ScrStopFlashGangZone): flaslama durdurulur.
void ScrStopFlashGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned short usZoneId = 0;
	if(bsData.Read(usZoneId))
	{
		g_zoneFlash[usZoneId] = false;
		Log("[GANGZONE:STOPFLASH] id=%u", usZoneId);
		// Kirmizi yanip somne durdu (grup rengine dond-‚ïù): o zone artik o grubun degil -
		// DB'den sil, grubun zone sayisi duser. Sahip bilinmiyorsa StartFlashScan
		// /gzinfo ile ˆrenir (RemoveZoneFromDb g_zoneOwnerName'e bagli degil, id yeter).
		RemoveZoneFromDb((int)usZoneId);
	}
}

void ScrSetSpawnInfo(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYER_SPAWN_INFO SpawnInfo;

	bsData.Read((PCHAR)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO));

	if(settings.iNormalModePosForce == 0)
	{
		settings.fNormalModePos[0] = SpawnInfo.vecPos[0];
		settings.fNormalModePos[1] = SpawnInfo.vecPos[1];
		settings.fNormalModePos[2] = SpawnInfo.vecPos[2];
	}
}

void ScrSetPlayerHealth(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerHealth);
}

void ScrSetPlayerArmour(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerArmour);
}

void ScrSetPlayerSkin(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iPlayerID;
	unsigned int uiSkin;

	bsData.Read(iPlayerID);
	bsData.Read(uiSkin);

	if(iPlayerID < 0 || iPlayerID >= MAX_PLAYERS)
		return;

	if(iGameInited && g_myPlayerID == iPlayerID)
		iLocalPlayerSkin = uiSkin;
}

void ScrCreateObject(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	unsigned short ObjectID;
	bsData.Read(ObjectID);

	unsigned long ModelID;
	bsData.Read(ModelID);

	float vecPos[3];
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);

	float vecRot[3];
	bsData.Read(vecRot[0]);
	bsData.Read(vecRot[1]);
	bsData.Read(vecRot[2]);

	float fDrawDistance;
	bsData.Read(fDrawDistance);

	if(settings.uiObjectsLogging != 0)
	{
		char szCreateObjectAlert[256];
		sprintf_s(szCreateObjectAlert, sizeof(szCreateObjectAlert), "[OBJECT] %d, %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.2f", ObjectID, ModelID, vecPos[0], vecPos[1], vecPos[2], vecRot[0], vecRot[1], vecRot[2], fDrawDistance);
		Log(szCreateObjectAlert);
	}
}

void ScrCreate3DTextLabel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD ID;
	CHAR Text[256];
	DWORD dwColor;
	FLOAT vecPos[3];
	FLOAT DrawDistance;
	BYTE UseLOS;
	WORD PlayerID;
	WORD VehicleID;

	bsData.Read((WORD)ID);
	bsData.Read((DWORD)dwColor);
	bsData.Read((FLOAT)vecPos[0]);
	bsData.Read((FLOAT)vecPos[1]);
	bsData.Read((FLOAT)vecPos[2]);
	bsData.Read((FLOAT)DrawDistance);
	bsData.Read((BYTE)UseLOS);
	bsData.Read((WORD)PlayerID);
	bsData.Read((WORD)VehicleID);

	stringCompressor->DecodeString(Text, 256, &bsData);

	if(settings.uiTextLabelsLogging != 0)
	{
		char szCreate3DTextLabelAlert[256];
		sprintf_s(szCreate3DTextLabelAlert, sizeof(szCreate3DTextLabelAlert), "[TEXTLABEL] %d - %s (%X, %.3f, %.3f, %.3f, %.2f, %i, %d, %d)", ID, Text, dwColor, vecPos[0], vecPos[1], vecPos[2], DrawDistance, UseLOS, PlayerID, VehicleID);
		Log(szCreate3DTextLabelAlert);
	}
}

// UIF sava-s zamanlay-‚ñíc-‚ñí textdraw'-‚ñí: "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> "08:27" (dk:sn).
// Ger-gek kalan s-‚ïùreyi yakalar; ?gzt bunu baz alir (chat duyuru anindan hesaplamak sagliksiz).
static void CaptureWarTimer(const char *pText, int iLen)
{
	if(!pText || !g_inZoneWar) return;
	const char *pColon = NULL;
	for(int i = 0; i < iLen - 1; i++)
	{
		if(pText[i] == ':' && i > 0 && pText[i+1] >= '0' && pText[i+1] <= '9')
		{ pColon = pText + i; break; }
	}
	if(!pColon) return;
	const char *p = pColon - 1;
	while(p >= pText && *p >= '0' && *p <= '9') p--;
	const char *pNum = p + 1;
	if(pNum >= pColon) return;
	int iM = atoi(pNum), iS = atoi(pColon + 1);
	if(iS < 0 || iS > 59 || iM < 0 || iM > 120) return;
	g_warRemainingSec = iM * 60 + iS;
	g_warTimerTick = GetTickCount();
	g_warTimerViaTdw = true; // textdraw kaynagi: KENDI sava˛imizin GZ timer'i
	// UIF sava˛lari 10 dk DEGIL, timer 3 dk gibi kisa surebilir. ZONE_WAR_TOTAL_MS
	// fallback'i (4104) yanlis bitis ani veriyordu -> bot 7 dk gec girip kill list'i
	// kaciriyordu. Timer bilindiginde sava˛ bitis anini geri hesapla: bitis =
	// timer_tick + kalan. Boylece son 00:02 giri˛i dogru zamanda yapilir.
	if(g_warRemainingSec > 0)
	{
		DWORD dwEnd = g_warTimerTick + (DWORD)g_warRemainingSec * 1000;
		DWORD dwStart = dwEnd - ZONE_WAR_TOTAL_MS;
		// Start negatif/gelecege isaret ediyorsa (sava˛ ortasinda yakalandi) simdiki
		// ana yasla: giri˛ guard'i zaten kalan sureyi ˆncelikli kullaniyor.
		if((int)dwStart <= 0 || dwStart > g_warTimerTick)
			dwStart = g_warTimerTick - (DWORD)g_warRemainingSec * 1000;
		g_zoneWarStartTick = dwStart;
		// war_timer.txt KALDIRILDI: sava˛ durumu sadece hafizada, dosyaya yazilmaz.
		Log("[WAR:TIMER] Sava˛ bitis ani hesaplandi: start=%u end=%u", dwStart, dwEnd);
	}
	Log("[WAR:TIMER] Kalan sure yakalandi: %d:%02d (%d sn)", iM, iS, g_warRemainingSec);
}

void ScrShowTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	TEXT_DRAW_TRANSMIT TextDrawTransmit;

	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read((PCHAR)&TextDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT));
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	CaptureWarTimer(cText, (int)cTextLen);

	// ?za yetki kontrolu: /gz sonrasi sunucu co-leader/leader degilsek bir textdraw
	// gosterir (UIF, tam metin surum farkli olabilir). Icinde "leader/yetk" gecen
	// metin yakalanir, gruba dile gˆre bildirilir; pencere 8 sn dolar.
	if(g_zaPermActive && GetTickCount() < g_zaPermUntil)
	{
		if(strstr(cText, "leader") || strstr(cText, "Leader") || strstr(cText, "yetk"))
		{
			Log("[ZA-PERM] Yetki textdraw'i yakalandi: %s", cText);
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Sava˛ ba˛latma yetkim yok (Co-Leader/Leader role gerekiyor).",
				"!I can't start the war (Co-Leader/Leader role required)."));
			g_zaPermActive = false;
		}
		else if(strstr(cText, "Teleported"))
		{
			// Isinlama mesaj˝: yetki uyarisindan once gelir, pencere aÁ˝k kalir.
		}
		else if(g_inZoneWar)
		{
			// Sava˛ ba˛ladi: yetki sorunu yok demektir.
			g_zaPermActive = false;
		}
	}

	if(settings.uiTextDrawsLogging != 0)
		SaveTextDrawData(wTextID, &TextDrawTransmit, cText);

	// Komut mutesi (UIF: "~g~YOU ARE COMMAND MUTED FOR 60 SECONDS"): ?zscan SIRASINDA
	// (/pos /gzinfo) VEYA ?zgrp HARVEST SIRASINDA (/gstats) sunucu komutlar˝ susturursa
	// zincir takilir. Mute'i gˆrunce botu HEMEN yeniden gir (reconnect olunca cmd mute
	// server-side kalkar; 60 sn beklemeye gerek yok). Scan/harvest state korunur;
	// yeniden spawn'da kalinan adimdan devam edilir. Ayni mute penceresindeki tekrar
	// textdraw'lar yeniden tetiklemesin diye bayrak ile bir kez.
	if((g_zoneScanIdx >= 0 || g_ghpActive) && strstr(cText, "MUTED") && !g_muteReconnectPending && iSpawned)
	{
		g_muteReconnectPending = true;
		Log("[MUTE] Komut mutesi yakalandi (%s) - hemen yeniden baglanilip i˛lem surdurulecek.", cText);
		resetPools(1, 0); // aninda reconnect; cmd mute reconnect ile kalkar
	}

	if(TextDrawTransmit.byteSelectable)
		Log("[SELECTABLE-TEXTDRAW] ID: %d, Text: %s.", wTextID, cText);

	// --- Kill list yakalayici: GZ sonucu textdraw'larini X/Y ile topla ---
	// Kill-list textdraw'lari AYRI AYRI gelir (ID 70 "Name", 71 "Kills", ... basliklar
	// Y=80; her oyuncu satiri Y=95+ ve kolonlar X ile hizali). Ayni Y'deki textdraw'lar
	// X'e gˆre siralanip satir olusturur (FinalizeKillList). Timer textdraw'i hari-g
	// GZ icinde gelen her sey toplanir; gereksiz textdraw'lar finalize'da elenir.
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		// Timer textdraw'i (ID 58/65, "0-0 ~n~ 09:59" gibi) toplanmaz
		if(wTextID == 58 || wTextID == 65) { /* timer */ }
		// Dev/b-‚ïùy-‚ïùk UI mesaj textdraw'lari (sava˛ disi uyarilari "YOU ARE OUTSIDE THE
		// WAR AREA", "Teleported", "You are not in a group zone", etkinlik afisleri)
		// skorboard verisi degildir; yakalanirsa sonuca satir olarak karisip takim
		// rengini zehirliyordu (0x106290). Skorboard hucreleri kucuk puntoludur;
		// fLetterHeight>=2 olanlar kacirilir.
		else if(TextDrawTransmit.fLetterHeight >= 2.0f) { /* UI mesaj kutusu: atla */ }
		else if(cTextLen > 0 && cText[0])
		{
			// Temiz metin (renk kodlari atilir) - dedup ve takim rengi icin
			std::string strClean;
			for(int i = 0; i < cTextLen; i++)
			{
				if(cText[i] == '{' && (i + 8) <= cTextLen && cText[i + 7] == '}')
				{
					i += 7;
					continue;
				}
				strClean += cText[i];
			}
			// ~n~ -> satir sonu (tek textdraw'da cok satirli olabilir)
			std::string strLine;
			for(int i = 0; i < cTextLen; i++)
			{
				if(cText[i] == '~' && i + 2 < cTextLen && cText[i + 1] == 'n' && cText[i + 2] == '~')
				{
					strLine += '\n';
					i += 2;
				}
				else
					strLine += cText[i];
			}

			// Takim rengi ˆrenme: kendi adimizin gectigi textdraw'in rengi bizim takim rengi.
			if(g_zwMyColor == 0 && g_inZoneWar && playerInfo[g_myPlayerID].szPlayerName[0])
			{
				const char *pMy = playerInfo[g_myPlayerID].szPlayerName;
				const char *pHit = strstr(cText, pMy);
				if(pHit)
				{
					for(const char *p = pHit; p >= cText; p--)
					{
						if(p[0] == '{' && (p - cText) + 8 <= cTextLen && p[7] == '}')
						{
							unsigned int uiCol = (unsigned int)strtoul(p + 1, NULL, 16);
							if(uiCol != 0)
							{
								g_zwMyColor = uiCol;
								Log("[KILLLIST] Bizim takim rengi ˆrenildi: 0x%06X (ad: %s)", uiCol, pMy);
							}
							break;
						}
					}
				}
			}

			// X/Y + text olarak sakla (ayn-‚ñí ID'nin tekrari olmasin)
			bool bDup = false;
			for(int k = 0; k < g_zwKillTdCount; k++)
			{
				if(g_zwKillTds[k].fX == TextDrawTransmit.fX && g_zwKillTds[k].fY == TextDrawTransmit.fY
					&& strcmp(g_zwKillTds[k].szText, strLine.c_str()) == 0) { bDup = true; break; }
			}
			if(!bDup && g_zwKillTdCount < KILL_TD_MAX)
			{
				g_zwKillTds[g_zwKillTdCount].fX = TextDrawTransmit.fX;
				g_zwKillTds[g_zwKillTdCount].fY = TextDrawTransmit.fY;
				g_zwKillTds[g_zwKillTdCount].dwColor = TextDrawTransmit.dwLetterColor;
				strncpy_s(g_zwKillTds[g_zwKillTdCount].szText, sizeof(g_zwKillTds[0].szText),
					strLine.c_str(), sizeof(g_zwKillTds[0].szText) - 1);
				g_zwKillTdCount++;
				Log("[KILLLIST] TD #%d (ID %d, X=%.0f Y=%.0f): %s", g_zwKillTdCount, wTextID,
					TextDrawTransmit.fX, TextDrawTransmit.fY, strClean.c_str());
			}
		}
	}
}

void ScrHideTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	bsData.Read(wTextID);

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:HIDE] ID: %d.", wTextID);
}

void ScrEditTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	CaptureWarTimer(cText, (int)cTextLen);

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:EDIT] ID: %d, Text: %s.", wTextID, cText);
}

void ScrTogglePlayerSpectating(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BOOL bToggle;

	bsData.Read(bToggle);

	if(bIsSpectating && !bToggle && !iSpawned)
	{
		sampSpawn();
		iSpawned = 1;
	}

	bIsSpectating = bToggle;
}

// Gamemode'dan gelen grup bilgisi: setPlayerWarGroup(playerID, ownGroup, enemyGroup)
void ScrSetWarGroup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteOwnLen = 0;
	char szOwn[64] = {0};
	BYTE byteEnemyLen = 0;
	char szEnemy[64] = {0};

	bsData.Read(byteOwnLen);
	if(byteOwnLen > 63) return;
	bsData.Read(szOwn, byteOwnLen);
	szOwn[byteOwnLen] = '\0';

	bsData.Read(byteEnemyLen);
	if(byteEnemyLen > 63) return;
	bsData.Read(szEnemy, byteEnemyLen);
	szEnemy[byteEnemyLen] = '\0';

	// Oyun RPC'sinden gelen grup adi guvenilmez (kisa kod/ID olabiliyor, ornek: "NM").
	// Kendi grup adimiz chat mesaj˝ndan otomatik ˆrenilir; burada ASLA ezilmez.
	// Dusman grup adini bosken al (welcome back icin); o da tam isim degilse sorun degil.
	if(g_enemyGroupName.empty())
		g_enemyGroupName = szEnemy;
	g_inZoneWar = (szEnemy[0] != '\0');

	Log("[WARGROUP] Kendi Grubu: %s | Dusman Grup: %s", szOwn, szEnemy);
}

// Sonucu "gz sonucl.txt" dosyasina yaz ve Discord'a gˆnder.
// Textdraw'lar X/Y koordinatlariyla toplanir (her kolon/satir ayri textdraw).
// Burada ayni Y'deki textdraw'lar X'e gˆre siralanip satir birlestirilir;
// renklerine gˆre iki takima ayrilir ve "Name Kills Assists Deaths Dmg" baslikli
// box halinde duzenlenir.
void FinalizeKillList()
{
	if(g_zwKillTdCount == 0)
	{
		// Bos finalize KAPTaji oldurmez: gercek skorboard son 2 sn + bitis
		// textdraw'larinda gelir (bot o an disarida olabilir). Pencere yenilenir,
		// KillListSent YOK - sonraki toplama devam eder. Eski davranis bos
		// finalize'de sent=true yapip kaptaji tum sava˛ boyunca kilitliyordu
		// ("gz sonucl.txt yazilmadi" vakasi).
		g_zwCaptureStart = GetTickCount();
		Log("[KILLLIST] Sonuc metni henuz bos, yakalama suruyor (pencere yenilendi).");
		return;
	}

	// X'e gˆre sirala (kolon sirasi: ayni Y icinde soldan saga)
	for(int a = 0; a < g_zwKillTdCount - 1; a++)
	{
		for(int b = a + 1; b < g_zwKillTdCount; b++)
		{
			if(g_zwKillTds[b].fX < g_zwKillTds[a].fX)
			{
				KillTdItem t = g_zwKillTds[a]; g_zwKillTds[a] = g_zwKillTds[b]; g_zwKillTds[b] = t;
			}
		}
	}

	// Y'ye gˆre satir gruplama: satirlar 15 birim arayla (Y=95,110,125...); tolerans
	// 5 - eski 20, komsu satirlari birbirine karistiriyordu ("Name hakixld 0 0 0 ...").
	// Her satir: rank(X=135, ATILIR), Name(X=160), K(X=280), A(X=340), D(X=400), DMG(X=460).
	std::string strMine, strEnemy, strUnknown;
	char szHeader[64] = "";

	// g_zwMyColor bilinmiyorsa: en cok gecen satir rengini kendi takimi varsay
	// (kill list'te kendi takimimizin satirlari cogunluktadir). UIF'te mavi FF0000FF.
	if(g_zwMyColor == 0)
	{
		unsigned int aColCnt[64] = {0};
		DWORD aCols[64] = {0};
		for(int a = 0; a < g_zwKillTdCount; a++)
		{
			DWORD c = g_zwKillTds[a].dwColor & 0xFFFFFF;
			if(c == 0xFFFFFF || c == 0) continue; // beyaz baslik/renksiz
			int k = 0;
			for(; k < 64; k++)
			{
				if(aCols[k] == c) { aColCnt[k]++; break; }
				if(aCols[k] == 0) { aCols[k] = c; aColCnt[k] = 1; break; }
			}
		}
		int iBest = -1;
		for(int k = 0; k < 64; k++)
		{
			if(aCols[k] != 0 && (iBest < 0 || aColCnt[k] > aColCnt[iBest]))
				iBest = k;
		}
		if(iBest >= 0)
		{
			g_zwMyColor = aCols[iBest];
			Log("[KILLLIST] Takim rengi cogunluktan varsayildi: 0x%06X", g_zwMyColor);
		}
	}

	for(int a = 0; a < g_zwKillTdCount; a++)
	{
		if(!g_zwKillTds[a].szText[0]) continue;
		float fRowY = g_zwKillTds[a].fY;
		// Ayni Y bandindaki textdraw'lari topla: (X, renk, metin) ikilileri.
		struct Cell { float fX; DWORD dwColor; char szText[64]; } cells[16];
		int nCells = 0;
		for(int b = a; b < g_zwKillTdCount && nCells < 16; b++)
		{
			if(fabsf(g_zwKillTds[b].fY - fRowY) > 5.0f) continue; // ayni satir (15 birim arayla)
			if(!g_zwKillTds[b].szText[0]) continue;
			cells[nCells].fX = g_zwKillTds[b].fX;
			cells[nCells].dwColor = g_zwKillTds[b].dwColor & 0xFFFFFF;
			strncpy_s(cells[nCells].szText, sizeof(cells[0].szText), g_zwKillTds[b].szText, sizeof(cells[0].szText) - 1);
			nCells++;
			g_zwKillTds[b].szText[0] = 0; // kullanildi, tekrar satir olusmasin
		}
		if(nCells == 0) continue;

		// Renk: isim hucresinin rengi (X~160) takim rengidir; rank hucre (X=135)
		// her takimda farkli olabilir, ona guvenilmez.
		DWORD dwRowColor = 0;
		for(int c = 0; c < nCells; c++)
		{
			if(cells[c].fX >= 145.0f && cells[c].fX < 220.0f)
			{
				dwRowColor = cells[c].dwColor;
				break;
			}
		}
		if(dwRowColor == 0 && nCells > 0)
			dwRowColor = cells[0].dwColor; // isim hucre yoksa ilk hucre rengi

		// Baslik satiri mi? (Name/Kills/Assists/Deaths/Dmg kelimelerinden biri var)
		bool bHeader = false;
		for(int c = 0; c < nCells; c++)
		{
			if(strstr(cells[c].szText, "Name") || strstr(cells[c].szText, "Kills")
				|| strstr(cells[c].szText, "Assists") || strstr(cells[c].szText, "Deaths")
				|| strstr(cells[c].szText, "Dmg"))
			{ bHeader = true; break; }
		}
		if(bHeader)
		{
			// Basligi E1 formatina cevir ve VERIYLE AYNI GENISLIKTE hizala:
			// K 62'nin, A 31'in, D 38'in, DMG 6806'nin ustune gelsin.
			sprintf_s(szHeader, sizeof(szHeader), "%-16s %4s %4s %4s %6s", "Name", "K", "A", "D", "DMG");
			continue;
		}

		// Hucreleri X'e gˆre sirala (kolon sirasi) ve alanlara dagit.
		for(int x = 0; x < nCells - 1; x++)
			for(int y = x + 1; y < nCells; y++)
				if(cells[y].fX < cells[x].fX)
				{
					Cell t = cells[x]; cells[x] = cells[y]; cells[y] = t;
				}
		char szName[64] = "", szK[16] = "", szA[16] = "", szD[16] = "", szDmg[16] = "";
		for(int c = 0; c < nCells; c++)
		{
			float fX = cells[c].fX;
			// Rank s-‚ïùtunu (X=135, "1 2 3...") E1 formatinda yok - atilir.
			if(fX < 145.0f) continue;
			if(fX < 220.0f) strncpy_s(szName, sizeof(szName), cells[c].szText, _TRUNCATE);
			else if(fX < 310.0f) strncpy_s(szK, sizeof(szK), cells[c].szText, _TRUNCATE);
			else if(fX < 370.0f) strncpy_s(szA, sizeof(szA), cells[c].szText, _TRUNCATE);
			else if(fX < 430.0f) strncpy_s(szD, sizeof(szD), cells[c].szText, _TRUNCATE);
			else strncpy_s(szDmg, sizeof(szDmg), cells[c].szText, _TRUNCATE);
		}
		if(!szName[0] && !szK[0]) continue; // bos satir

		// E1 gibi hizali satir: "%-16s %4s %4s %4s %6s"
		char szRow[96];
		sprintf_s(szRow, sizeof(szRow), "%-16s %4s %4s %4s %6s", szName, szK, szA, szD, szDmg);

		// Toplamlar + MVP/feeder istatistikleri (iki takim birlikte)
		int iK = atoi(szK), iA = atoi(szA), iD = atoi(szD), iDmg = atoi(szDmg);
		if(iK > 0 || iD > 0)
		{
			if(iK > g_zwBestK || (iK == g_zwBestK && iDmg > g_zwBestDmg))
			{ g_zwBestK = iK; g_zwBestDmg = iDmg; strncpy_s(g_zwMvpName, sizeof(g_zwMvpName), szName, _TRUNCATE); }
			if(iD > g_zwBestD)
			{ g_zwBestD = iD; strncpy_s(g_zwFeederName, sizeof(g_zwFeederName), szName, _TRUNCATE); }
		}

		if(g_zwMyColor != 0 && dwRowColor == g_zwMyColor)
		{
			if(!strMine.empty()) strMine += "\n";
			strMine += szRow;
			g_zwMineK += iK; g_zwMineA += iA; g_zwMineD += iD; g_zwMineDmg += iDmg;
		}
		else if(g_zwMyColor != 0 && dwRowColor != 0)
		{
			if(!strEnemy.empty()) strEnemy += "\n";
			strEnemy += szRow;
			g_zwEnemyK += iK; g_zwEnemyA += iA; g_zwEnemyD += iD; g_zwEnemyDmg += iDmg;
		}
		else
		{
			if(!strUnknown.empty()) strUnknown += "\n";
			strUnknown += szRow;
		}
	}

	// Takim adlari: once "vs" duyurusu (sol=kirmizi, sag=mavi), yoksa fallback.
	const char *pRedName  = !g_zwWarTeamLeft.empty()  ? g_zwWarTeamLeft.c_str()  : (!g_enemyGroupName.empty() ? g_enemyGroupName.c_str() : "Kirmizi");
	const char *pBlueName = !g_zwWarTeamRight.empty() ? g_zwWarTeamRight.c_str() : (!g_myGroupName.empty()    ? g_myGroupName.c_str()    : "Mavi");
	// Takim yonumuz: "vs" duyurusundaki konum (sol=kirmizi, sag=mavi) en guvenilir;
	// bilinmiyorsa renk fallback'i (mavi 0x0000FF). bWeAreBlue=true ise biz sag grup.
	bool bWeAreBlue = g_zwWeAreBlueKnown ? g_zwWeAreBlue : ((g_zwMyColor & 0xFFFFFF) == 0x0000FF);
	const char *pMineName  = bWeAreBlue ? pBlueName : pRedName;
	const char *pEnemyName = bWeAreBlue ? pRedName  : pBlueName;

	// Kirmizi/mavi kutu + tag + kill toplamlari
	std::string strRedBox, strBlueBox;
	int iRedK = 0, iRedA = 0, iRedD = 0, iRedDmg = 0;
	int iBlueK = 0, iBlueA = 0, iBlueD = 0, iBlueDmg = 0;
	std::string strRedRows, strBlueRows;
	const char *pRedTag = "", *pBlueTag = "";
	int giR = FindGroupColorByName(pRedName);  if(giR >= 0) pRedTag = g_groupColors[giR].szTag;
	int giB = FindGroupColorByName(pBlueName); if(giB >= 0) pBlueTag = g_groupColors[giB].szTag;
	if(!bWeAreBlue)
	{
		strRedRows = strMine;  iRedK = g_zwMineK;  iRedA = g_zwMineA;  iRedD = g_zwMineD;  iRedDmg = g_zwMineDmg;
		strBlueRows = strEnemy; iBlueK = g_zwEnemyK; iBlueA = g_zwEnemyA; iBlueD = g_zwEnemyD; iBlueDmg = g_zwEnemyDmg;
	}
	else
	{
		strRedRows = strEnemy; iRedK = g_zwEnemyK; iRedA = g_zwEnemyA; iRedD = g_zwEnemyD; iRedDmg = g_zwEnemyDmg;
		strBlueRows = strMine; iBlueK = g_zwMineK; iBlueA = g_zwMineA; iBlueD = g_zwMineD; iBlueDmg = g_zwMineDmg;
	}
	{
		char szB[512];
		const char *dash = "--------------------------------------"; // 38 karakter: DMG sutununa kadar
		sprintf_s(szB, sizeof(szB), "%-16s %4s %4s %4s %6s\n%s\n%s\n%s\nTotal     %3d %3d %3d %6d",
			"Name", "K", "A", "D", "DMG", dash,
			strRedRows.empty() ? "(bos)" : strRedRows.c_str(), dash, iRedK, iRedA, iRedD, iRedDmg);
		strRedBox = szB;
		sprintf_s(szB, sizeof(szB), "%-16s %4s %4s %4s %6s\n%s\n%s\n%s\nTotal     %3d %3d %3d %6d",
			"Name", "K", "A", "D", "DMG", dash,
			strBlueRows.empty() ? "(bos)" : strBlueRows.c_str(), dash, iBlueK, iBlueA, iBlueD, iBlueDmg);
		strBlueBox = szB;
	}
	// E1 gibi eski tek-metin ciktisi da korunur (dosya / debug)
	std::string strOut;
	if(!szHeader[0]) sprintf_s(szHeader, sizeof(szHeader), "%-16s %4s %4s %4s %6s", "Name", "K", "A", "D", "DMG");
	if(!strMine.empty())
		strOut += std::string(pMineName) + "\n" + szHeader + "\n" + strMine + "\n";
	if(!strEnemy.empty())
		strOut += "\n" + std::string(pEnemyName) + "\n" + szHeader + "\n" + strEnemy + "\n";
	if(strMine.empty() && strEnemy.empty() && !strUnknown.empty())
		strOut += std::string(szHeader) + "\n" + strUnknown + "\n";

	// Dosyaya yaz
	FILE *fl = fopen("gz sonucl.txt", "w");
	if(fl)
	{
		fprintf(fl, "%s\n", strOut.c_str());
		fclose(fl);
		Log("[KILLLIST] Sonuc 'gz sonucl.txt' dosyasina yazildi (%d textdraw).", g_zwKillTdCount);
	}
	else
	{
		Log("[KILLLIST] 'gz sonucl.txt' dosyasi acilamadi.");
	}

	// Discord'a gˆnder (yapilandirilmis: kazandi mi, kirmizi/mavi takim + tag + kutu, MVP/feeder)
	// --- Top Weapons: RPC 55 kill feed'i iki takimin uyelerinin kill'leriyle
	// filtrele, top 5 silahi Discord'a gˆnder. Uye isimleri scoreboard
	// satirlarindan gelir (ilk alan). GZ pencerelerinde (3sn bas + 00:02 son)
	// gˆrunmeyen nadir oyuncular sayima girmez. (ponytail: oyuncu kacmasi onemli
	// olursa kill feed'ini katil-victim oranina gˆre de filtreleyebiliriz.)
	char szTopWeapons[256] = "";
	{
		char aNames[128][32]; int nNames = 0;
		const char *aLists[] = { strMine.c_str(), strEnemy.c_str() };
		for(int li = 0; li < 2 && nNames < 128; li++)
		{
			const char *p = aLists[li];
			while(*p && nNames < 128)
			{
				while(*p == '\n') p++;
				if(!*p) break;
				const char *pE = p;
				while(*pE && *pE != '\n') pE++;
				char szLine[64]; size_t n = (size_t)(pE - p);
				if(n > 63) n = 63;
				memcpy(szLine, p, n); szLine[n] = 0;
				char *pN = szLine;
				while(*pN == ' ') pN++;
				if(strncmp(pN, "Total", 5) == 0 || strncmp(pN, "(bos)", 5) == 0) { p = pE; continue; }
				char *pSp = pN;
				while(*pSp && *pSp != ' ') pSp++;
				*pSp = 0;
				if(pN[0])
				{
					bool bDup = false;
					for(int i = 0; i < nNames; i++) if(_stricmp(aNames[i], pN) == 0) { bDup = true; break; }
					if(!bDup) strncpy_s(aNames[nNames++], 32, pN, 31);
				}
				p = pE;
			}
		}

		static const char *szWeaponNames[] = {
			"Fist","Brass Knuckles","Golf Club","Nitestick","Knife","Baseball Bat","Hammer",
			"Shovel","Pool Cue","Katana","Chainsaw","Dildo","Dildo","Dildo","Flowers","Cane",
			"Grenade","Teargas","Molotov","9mm","Silenced 9mm","Desert Eagle","Shotgun",
			"Sawn-off Shotgun","Combat Shotgun","Uzi","MP5","AK-47","M4","Tec-9","Rifle",
			"Sniper Rifle","Rocket Launcher","Flame Thrower","Minigun","Remote Explosives",
			"Detonator","Night Vision","Thermal Goggles","Parachute","Armour",
			"Painted Weapon","","","Vehicle Rocket","Vehicle Fire","Vehicle Bullets"
		};
		int aCnt[47] = {0};
		for(int i = 0; i < g_zwWarKillCount; i++)
		{
			BYTE w = g_zwWarKills[i].weapon;
			if(w > 46) continue;
			bool bMember = false;
			for(int k = 0; k < nNames; k++)
				if(_stricmp(aNames[k], g_zwWarKills[i].szKiller) == 0) { bMember = true; break; }
			if(bMember) aCnt[w]++;
		}
		int aOrig[47]; memcpy(aOrig, aCnt, sizeof(aCnt));
		int aTop[5]; int nTop = 0;
		for(int r = 0; r < 5 && nTop < 5; r++)
		{
			int iBest = -1;
			for(int w = 0; w < 47; w++)
				if(aOrig[w] > 0 && (iBest < 0 || aOrig[w] > aOrig[iBest])) iBest = w;
			if(iBest < 0) break;
			aTop[nTop++] = iBest;
			aOrig[iBest] = 0;
		}
		szTopWeapons[0] = 0;
		for(int i = 0; i < nTop; i++)
		{
			const char *pWn = szWeaponNames[aTop[i]];
			if(!pWn || !pWn[0]) pWn = "?";
			char szOne[80];
			sprintf_s(szOne, sizeof(szOne), "%s x%d", pWn, aCnt[aTop[i]]);
			if(i) strcat_s(szTopWeapons, sizeof(szTopWeapons), ", ");
			strcat_s(szTopWeapons, sizeof(szTopWeapons), szOne);
		}
		Log("[KILLLIST] Top Weapons (%d kill, %d uye): %s", g_zwWarKillCount, nNames,
			szTopWeapons[0] ? szTopWeapons : "(yok)");
	}

	SendKillList(strRedBox.c_str(), strBlueBox.c_str(), pRedName, pBlueName, pRedTag, pBlueTag,
		iRedK, iBlueK, g_zwWeWon, g_zwMvpName, g_zwFeederName, g_zwBestDmg, g_zwBestK, g_zwBestD, g_zwFirstBlood, szTopWeapons);

	g_zwCapturingKillList = false;
	g_zwKillListSent = true;
	g_zwKillTdCount = 0;
	g_zwFinalizeAt = 0;
	g_zwBestK = 0; g_zwBestDmg = 0; g_zwBestD = 0;
	g_zwMvpName[0] = 0; g_zwFeederName[0] = 0;
	g_zwMineK = 0; g_zwMineA = 0; g_zwMineD = 0; g_zwMineDmg = 0;
	g_zwEnemyK = 0; g_zwEnemyA = 0; g_zwEnemyD = 0; g_zwEnemyDmg = 0;

	// GZ'de kald˝ysak (kill list toplamak icin) simdi freeroam'a don
	if(g_zwReturnPending)
	{
		g_zwReturnPending = false;
		Log("[KILLLIST] Kill list tamam, /fr ile freeroam'a donuluyor.");
		sendServerCommand((char *)"/fr");
	}
}

// Ana donguden her iterasyonda cagrilir:
// 1. ?pl stats-post zaman asimi cevabi
// 2. Kisa GZ giri˛ donusu (sava˛ basi takim rengi ˆrenimi)
// 3. Sava˛ aktifse son 00:02'de /gz2 (kill-list textdraw'lari icin GZ icinde KALINIR)
// 4. Toplama penceresi dolunca sonucu finalize et (dosya + Discord)
void ProcessZoneWarAutoJoin()
{
	DWORD dwNow = GetTickCount();
	// Stuck war temizleme
	if(g_inZoneWar && g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick > 15*60*1000){
		Log("[ZONEWAR] Sava˛ 15dk'yi asti (stuck), temizleniyor.");
		g_inZoneWar = false;
		g_warRemainingSec = -1;
		g_warTimerTick = 0;
		g_warTimerViaTdw = false;
		g_gztCountdownAnnounced = 999;
		g_gztLastRem = -1;
		g_zoneWarStartTick = 0;
	}

	// ?gzt geri sayimi: uL.Hold/YS tarzi SABIT esikler - 09:10 / 09:03 / 09:01 /
	// 00:10 / 00:03 / 00:01. SADECE TEXTDRAW kaynakli timer gecerli (kendi sava˛imiz
	// GZ'sinde g-r-‚ïùn-‚ïùr); baska sava˛larin "Time left:" chat mesajlari geri sayim
	// ba˛latmaz (bizde sava˛ yokken "War ends in 05:00!" phantom duyurulariyordu).
	if(g_inZoneWar && g_gztOn && g_warTimerViaTdw)
	{
		int iRemainingSec = -1;
		if(g_warTimerTick != 0 && g_warRemainingSec > 0 && dwNow - g_warTimerTick < ZONE_WAR_TOTAL_MS)
			iRemainingSec = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);

		if(iRemainingSec >= 0)
		{
			static const int iT[] = { 9*60+10, 9*60+3, 9*60+1, 10, 3, 1 };
			const int nT = 6;

			// 9:xx esikleri yalnizca GECIS'te duyurulur ‚Äî ?gzt ge√ß acilirsa geriye
			// donuk "9:10" atmaz. Son 10/3/1 esikleri ise kalan sure esigin altina
			// iner inmez duyurulur: textdraw gec yakalansa bile 10 kacirilmaz.
			for(int i = 0; i < nT; i++)
			{
				bool bFire;
				if(iT[i] >= 60)
					bFire = (iRemainingSec <= iT[i] && g_gztLastRem > iT[i]);
				else
					bFire = (iRemainingSec <= iT[i]);
				if(bFire && iT[i] < g_gztCountdownAnnounced)
				{
					g_gztCountdownAnnounced = iT[i];
					char szCnt[128];
					sprintf_s(szCnt, sizeof(szCnt),
						LM("!Sava˛ %02d:%02d sonra bitiyor!", "!War ends in %02d:%02d!"),
						iT[i] / 60, iT[i] % 60);
					Log("[GZT] Geri sayim duyurusu: %02d:%02d kald˝.", iT[i] / 60, iT[i] % 60);
					g_bridgeSuppressEcho++;
					sendChat(szCnt);
				}
			}
			g_gztLastRem = iRemainingSec;
		}
		else
			g_gztLastRem = -1;
	}

	// ?pl: Stats-post gecikmeli cevabi beklerken zaman asimi. Once taze Stats-post
	// dener (genelde ~1 sn icinde gelir); gelmezse cache'ten cevapla (fps yoksa "--").
	if(g_waitingForPNetStats && dwNow - g_pnetstatsReqTime > 3500)
	{
		g_waitingForPNetStats = false;
		if(g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS && g_playerCache[g_plTargetId].dLoss >= 0.0)
		{
			const char *pName = "Player ?";
			char szFallbackName[32];
			if(playerInfo[g_plTargetId].szPlayerName[0]) pName = playerInfo[g_plTargetId].szPlayerName;
			else if(g_playerCache[g_plTargetId].szName[0]) pName = g_playerCache[g_plTargetId].szName;
			else { sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", g_plTargetId); pName = szFallbackName; }
			char szFps[16], szPing[16];
			if(g_playerCache[g_plTargetId].iFps >= 0) sprintf_s(szFps, sizeof(szFps), "%d", g_playerCache[g_plTargetId].iFps); else strcpy(szFps, "--");
			if(g_playerCache[g_plTargetId].iPing >= 0) sprintf_s(szPing, sizeof(szPing), "%d", g_playerCache[g_plTargetId].iPing); else strcpy(szPing, "--");
			char szOut[192];
			sprintf_s(szOut, sizeof(szOut), "!%s's PL: %g%% | FPS: %s | Ping: %s ms",
				pName, g_playerCache[g_plTargetId].dLoss, szFps, szPing);
			Log("[PNETSTATS] Zaman asimi, cache ile cevap: %s", szOut);
			g_bridgeSuppressEcho++;
			sendChat(szOut);
		}
		else
		{
			Log("[PNETSTATS] Stats-post ve cache yok, 'bulunamadi' cevabi.");
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Packetloss bulunamadi.", "!Packetloss not found."));
		}
	}

	// Kisa GZ giri˛ donusu: takim rengi ˆrenildi (ya da sure doldu), /fr ile freeroam'a don.
	// Kill-list giri˛inde (g_zwKillListStay) GZ'de KALINIR - sava-s bitince won/loses
	// textdraw'i gelir, FinalizeKillList /fr'yi kendisi gˆnderir. Kisa giri˛ (sava˛ basi
	// 6 sn / 09:02 kontrol 2 sn) her zaman cikar.
	if(g_zwReturnPending && dwNow >= g_zwReturnAt)
	{
		if(!g_zwKillListStay)
		{
			g_zwReturnPending = false;
			Log("[ZONEWAR] Kisa GZ giri˛ bitti, /fr ile freeroam'a donuluyor.");
			sendServerCommand((char *)"/fr");
		}
	}

	// GZ aktif: son 00:02'de /gz2 gˆnder ve ICERIDE KAL.
	// Kill-list textdraw'lari sadece GZ icindekilere gelir; bitis textdraw'lari
	// g_zwCapturingKillList aÁ˝kken toplanir, pencere dolunca finalize edilir.
	// Iki asamali giri˛ (kullan˝c˝ istegi):
	//  1) 09:02'de kisa kontrol giri˛ii - sava-s aktif mi diye GZ'ye gir, 2 sn kal, /fr ile don.
	//     (Saldiri yapilan gruptan kimse gz2'ye katilmazsa sava˛ ~1 dk icinde biter ve
	//      zone sahibi olunur; kontrol giri˛ii sava-sin hala surdugunu ˆrenir.)
	//  2) 00:02'de kill-list giri˛ii - iceride kal, kill list textdraw'larini topla, bitince Discord'a at.
	if(g_inZoneWar && !g_zwAutoJoinSent)
	{
		int iLeft = -1;
		if(g_warTimerTick != 0 && g_warRemainingSec > 0)
		{
			iLeft = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
			if(iLeft < 0 && g_zoneWarStartTick != 0)
				iLeft = (int)((g_zoneWarStartTick + ZONE_WAR_TOTAL_MS - dwNow) / 1000);
		}
		else if(g_zoneWarStartTick != 0)
			iLeft = (int)((ZONE_WAR_TOTAL_MS - (dwNow - g_zoneWarStartTick)) / 1000);

		// 1) 09:02 kontrol giri˛ii (bir kez): timer 09:02'ye indi-sinde GZ'ye gir,
		//    sava-s aktifse timer'-‚ñí da tazeler; 2 sn sonra /fr ile geri don.
		if(!g_zwCheckJoinSent && !g_zwReturnPending)
		{
			if(iLeft >= 0 && iLeft <= 9 * 60 + 2)
			{
				g_zwCheckJoinSent = true;
				g_zwReturnPending = true;
				g_zwReturnAt = dwNow + 2000; // 2 sn kontrol
				Log("[KILLLIST] 09:02 kontrol giri˛ii: sava-s aktif mi bakiliyor (kalan %d sn).", iLeft);
				sendServerCommand((char *)"/gz2");
			}
		}

		// 2) 00:02 kill-list giri˛ii (bir kez): son 2 saniyede GZ'ye gir, iceride kal.
		bool bEnterNow = false;
		if(iLeft >= 0 && iLeft <= 2)
			bEnterNow = true;

		if(bEnterNow)
		{
			g_zwAutoJoinSent = true;
			g_zwKillListStay = true; // GZ'de kal, sava˛ bitince finalize + /fr
			// Yakalayici sava˛ boyunca zaten aÁ˝k (g_zwCapturingKillList true);
			// GZ'ye giri˛ textdraw'larin toplanmasini saglar. Pencere dolunca finalize.
			g_zwCaptureStart = dwNow;
			g_zwFinalizeAt = 0; // 00:02 giri˛: pencere moduna don
			// Sava˛ bitince /fr ile geri don (pencere sonu finalize + donus)
			g_zwReturnPending = true;
			g_zwReturnAt = dwNow + KILL_LIST_CAPTURE_WINDOW_MS;
			if(g_zwMyColor == 0)
				Log("[KILLLIST] GZ bitimine 00:02 kala giriliyor (takim rengi bilinmiyor, giri˛ sirasinda ˆrenilecek).");
			else
				Log("[KILLLIST] GZ bitimine 00:02 kala giriliyor, kill/death/assist/DMG textdraw'lari toplanacak.");
			sendServerCommand((char *)"/gz2");
		}
	}

	// Toplama: yakalayici aÁ˝kken sadece GERCEK bitis/veri aninda finalize et.
	// 1) Sava˛ bitisi (wins/loses blo-su g_zwFinalizeAt = now+3000 kurar): bitis
	//    textdraw'lari geldikten 3 sn sonra finalize.
	// 2) Pencere modu: veri BIRIKTIYSE (g_zwKillTdCount>0) window dolunca finalize;
	//    bos pencere kaptaji durdurmaz (skorboard son anlarda geliyor).
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		if(g_zwFinalizeAt != 0)
		{
			if(dwNow >= g_zwFinalizeAt)
				FinalizeKillList();
		}
		else if(g_zwKillTdCount > 0 && dwNow - g_zwCaptureStart >= KILL_LIST_CAPTURE_WINDOW_MS)
			FinalizeKillList();
	}
}

// /gmembers SAYFALI: her sayfa buraya beslenir. Donus:
//   GM_NEXT  -> SAMPRPC istenen buton/item ile sonraki sayfayi ister
//   GM_DONE  -> arama bitti; OnGmembersFinalize() sonucu uygular
// Satir kurali: satirda kisi adi + "Co-Leader" veya "Leader" birlikte.
#define GMCOL_RESULT_DONE 0
#define GMCOL_RESULT_NEXT 1
int OnGmembersPage(const char *szInfo, int *outButton, int *outItem)
{
	// Ham icerigi logla (format dogrulamasi)
	Log("[GMEMBERS-DATA] Sayfa %d/%d: %.400s", s_iGmPage, GM_MAX_PAGES, szInfo ? szInfo : "");

	if(szInfo && g_snipPermPending)
	{
		const char *pBot = g_szNickName[0] ? g_szNickName : "";
		const char *pUser = (g_snipSenderId >= 0 && g_snipSenderId < MAX_PLAYERS)
			? playerInfo[g_snipSenderId].szPlayerName : "";
		if(!pUser[0])
			pUser = (g_snipSenderId >= 0 && g_snipSenderId < MAX_PLAYERS && g_playerCache[g_snipSenderId].szName[0])
				? g_playerCache[g_snipSenderId].szName : "";

		const char *pL = szInfo;
		int iRow = 0;
		while(pL && *pL)
		{
			const char *pE = strchr(pL, '\n');
			size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
			if(nLen > 0 && nLen < 255)
			{
				char szLine[256];
				memcpy(szLine, pL, nLen); szLine[nLen] = 0;
				bool bLeaderLine = (strstr(szLine, "Co-Leader") || strstr(szLine, "Leader"));
				if(bLeaderLine)
				{
					if(!s_bGmBotOk && pBot[0] && strstr(szLine, pBot)) { s_bGmBotOk = true; Log("[ZSNIP] Bot bu sayfada bulundu (satir %d).", iRow); }
					if(!s_bGmUserOk && pUser[0] && strstr(szLine, pUser)) { s_bGmUserOk = true; Log("[ZSNIP] Kullan˝c˝ bu sayfada bulundu (satir %d).", iRow); }
				}
				// AÁ˝k "sonraki sayfa" satiri varsa kaydet (tiklanacak ogge)
				if(strstr(szLine, "Sonraki") || strstr(szLine, "sonraki") || strstr(szLine, "Next")
					|| strstr(szLine, "next") || strstr(szLine, ">>"))
					*outItem = iRow;
			}
			iRow++;
			if(!pE) break;
			pL = pE + 1;
		}
	}

	if(s_bGmBotOk && s_bGmUserOk) return GMCOL_RESULT_DONE;      // ikisi de bulundu
	if(s_iGmPage >= GM_MAX_PAGES) return GMCOL_RESULT_DONE;     // sayfa limiti

	// Sonraki sayfa: SAG BUTON '>>' (OK sol, '>>' sag). Dialog metninde
	// ">>/Sonraki" satiri varsa o satira OK ile tikla, yoksa '>>' butonuna
	// (sag buton 0 + secili ilk satir 0; 0+0xFFFF = ESC = iptal).
	s_iGmPage++;
	if(*outItem >= 0) *outButton = 1;
	else { *outButton = 0; *outItem = 0; }
	return GMCOL_RESULT_NEXT;
}

// Arama bitti (bulundu / limit / bos): yetki sonucunu uygula.
void OnGmembersFinalize()
{
	g_waitingForGmembers = false;
	if(!g_snipPermPending) return;
	g_snipPermPending = false;

	const char *pBot = g_szNickName[0] ? g_szNickName : "";
	const char *pUser = (g_snipSenderId >= 0 && g_snipSenderId < MAX_PLAYERS)
		? playerInfo[g_snipSenderId].szPlayerName : "";
	if(!pUser[0])
		pUser = (g_snipSenderId >= 0 && g_snipSenderId < MAX_PLAYERS && g_playerCache[g_snipSenderId].szName[0])
			? g_playerCache[g_snipSenderId].szName : "";

	Log("[ZSNIP] Yetki aramasi bitti (sayfa %d): bot=%s (%s), kullan˝c˝=%s (%s).",
		s_iGmPage, pBot, s_bGmBotOk ? "Co-Leader" : "YOK", pUser, s_bGmUserOk ? "Co-Leader" : "YOK");

	if(!pBot[0] || !pUser[0])
	{
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Isimler tespit edilemedi, sniper iptal.", "!Names could not be resolved, sniper cancelled."));
		return;
	}
	if(!s_bGmBotOk || !s_bGmUserOk)
	{
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Zone sniper icin bot ve kullan˝c˝ Co-Leader olmali.",
			"!Zone sniper requires both bot and user to be Co-Leader."));
		return;
	}

	// Yetki tamam: istege gˆre sniper'i kapat veya ba˛lat.
	if(g_snipAction == 1)
	{
		g_ztkActive = false;
		g_ztkZoneId = -1;
		g_ztkWaitAnnounced = false;
		g_ztkAttackPending = false;
		g_ztkOutcome = 0;
		g_ztkTargetWar = false;
		g_ztkAskNext = false;
		g_snipGroupName[0] = 0;
		g_snipZoneCount = 0;
		g_snipZoneIdx = 0;
		Log("[ZSNIP] Kullan˝c˝ iptal etti (yetki dogrulandi), sniper kapat˝ld˝.");
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Sniper iptal edildi.", "!Sniper aborted by user."));
		return;
	}

	int iZone = g_snipZone;
	if(g_snipZoneCount > 0 && g_snipZoneIdx < g_snipZoneCount)
		iZone = g_snipZones[g_snipZoneIdx]; // zincir baslangici
	if(iZone < 0 || iZone >= g_zoneCount) { Log("[ZSNIP] Hedef zone gecersiz, iptal."); return; }
	DWORD dwNow = GetTickCount();
	int iWaitSec = 0;
	if(g_inZoneWar && g_zwWarZone[0] && atoi(g_zwWarZone) == iZone)
	{
		if(g_warTimerTick != 0 && dwNow - g_warTimerTick < ZONE_WAR_TOTAL_MS)
			iWaitSec = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
		else if(g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick < ZONE_WAR_TOTAL_MS)
			iWaitSec = (int)((ZONE_WAR_TOTAL_MS - (dwNow - g_zoneWarStartTick)) / 1000);
		if(iWaitSec < 0) iWaitSec = 0;
	}
	float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
	float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
	sendServerCommand(szCmd);

	g_ztkActive = true;
	g_ztkZoneId = iZone;
	g_ztkWaitAnnounced = false;
	Log("[ZSNIP] Zone sniper ba˛ladi: zone #%d, bekleyis %d sn.", iZone, iWaitSec);

	char szMsg[160];
	if(iWaitSec > 0)
		sprintf_s(szMsg, sizeof(szMsg),
			LM("!Zone #%d sava˛i bitiyor (%d sn), sniper bekliyor...", "!War in zone #%d ending (%d sec), sniper waiting..."), iZone, iWaitSec);
	else
		sprintf_s(szMsg, sizeof(szMsg),
			LM("!Zone #%d hedeflendi, sava˛ aciliyor...", "!Zone #%d targeted, starting war..."), iZone);
	g_bridgeSuppressEcho++;
	sendChat(szMsg);
}

// ?zsnip (zone sniper): hedef zonede sava˛ SURUYORSA bitisini bekle; bitince
// (ya da sava˛ yoksa) /gz ile saldiri ba˛lat. Sava˛ bitisi g_inZoneWar=false ile anlasilir.
// Grubun TUM zone'larini topla (bellek haritasi + DB fallback). Donus: zone sayisi.
static int CollectGroupZones(const char *szGroupName, int *outZones, int maxZones)
{
	int n = 0;
	if(!szGroupName || !szGroupName[0] || !outZones || maxZones <= 0) return 0;
	char szNeed[96]; strncpy_s(szNeed, sizeof(szNeed), szGroupName, sizeof(szNeed) - 1);
	NormalizeGroupName(szNeed);
	for(int zid = 0; zid < g_zoneCount && n < maxZones; zid++)
	{
		if(!g_zoneOwnerName[zid][0]) continue;
		char szHave[96]; strncpy_s(szHave, sizeof(szHave), g_zoneOwnerName[zid], sizeof(szHave) - 1);
		NormalizeGroupName(szHave);
		if(_stricmp(szHave, szNeed) != 0) continue;
		bool bDup = false;
		for(int i = 0; i < n; i++) if(outZones[i] == zid) { bDup = true; break; }
		if(!bDup) outZones[n++] = zid;
	}
	// DB fallback (bellek restore paket kaybinda eksik olabilir)
	if(n < maxZones)
	{
		try
		{
			std::string url = std::string("/api/groups/zone?name=") + szGroupName;
			for(size_t i = 0; i < url.size(); i++)
				if(url[i] == ' ') url.replace(i, 1, "%20");
			httplib::Client cli(g_szBackendBase);
			cli.set_connection_timeout(4);
			auto res = cli.Get(url.c_str());
			if(res && res->status == 200)
			{
				json j = json::parse(res->body);
				if(j.contains("zones") && j["zones"].is_array())
				{
					for(const auto &z : j["zones"])
					{
						int zid = z.is_number() ? z.get<int>() : atoi(z.get<std::string>().c_str());
						if(zid < 0 || zid >= MAX_GANG_ZONES) continue;
						bool bDup = false;
						for(int i = 0; i < n; i++) if(outZones[i] == zid) { bDup = true; break; }
						if(!bDup && n < maxZones)
						{
							outZones[n++] = zid;
							strncpy_s(g_zoneOwnerName[zid], sizeof(g_zoneOwnerName[0]),
								szGroupName, sizeof(g_zoneOwnerName[0]) - 1);
						}
					}
				}
			}
		}
		catch(...) {}
	}
	return n;
}

// ZSNIP: bu zone hedef grubun zone listesinde mi? (hedef zonede baska grubun
// savasi surerken takip icin - sadece ilk zone degil, grubun tum zone'lari)
static bool IsSniperTargetZone(int zid)
{
	if(!g_ztkActive) return false;
	if(zid == g_ztkZoneId) return true;
	if(g_snipZoneCount > 0)
		for(int i = 0; i < g_snipZoneCount; i++)
			if(g_snipZones[i] == zid) return true;
	return false;
}

void ProcessZtkTick()
{
	if(!g_ztkActive) return;

	// Faz 2: /gz gonderildi, kendi savasimizin sonucu bekleniyor (won/lost game text
	// veya chat war-end). Sonuc gelince zincir kararini ver.
	if(g_ztkAttackPending)
	{
		if(g_inZoneWar) return; // bizim savasimiz hala suruyor
		bool bWon = (g_ztkOutcome == 1) || (g_ztkOutcome == 0 && g_zwWeWon);
		g_ztkAttackPending = false;
		g_ztkOutcome = 0;
		g_ztkActive = false;
		g_ztkZoneId = -1;
		g_ztkWaitAnnounced = false;
		g_zwWeWon = true; // siradaki saldiri icin sifirla
		Log("[ZSNIP] Sonuc: %s (%s).", bWon ? "KAZANILDI" : "KAZANILAMADI", g_snipGroupName);
		g_bridgeSuppressEcho++;
		if(bWon)
			sendChat((char *)LM("!Zone kazanildi!", "!Zone captured!"));
		else
			sendChat((char *)LM("!Zone alinamadi...", "!Zone could not be captured..."));
		// Zincir: daha fazla zone varsa kullaniciya sor (uL.Hold gibi)
		if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
		{
			g_snipZoneIdx++;
			g_ztkAskNext = true;
			Log("[ZSNIP] ?yes ile zone #%d'e devam edilecek.", g_snipZones[g_snipZoneIdx]);
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Diger zone'a da saldirayim mi? (?yes)",
				"!Attack the next zone too? (?yes)"));
		}
		else
		{
			g_snipZoneCount = 0;
			g_snipZoneIdx = 0;
			g_ztkAskNext = false;
			g_snipGroupName[0] = 0;
		}
		return;
	}

	// Faz 1: hedef zonede savas suruyorsa beklemeye devam et (bir kez duyur).
	// Hem kendi savasimiz (g_inZoneWar) hem dusmanin savasi (g_ztkTargetWar -
	// "takes over zone '# N'" mesajiyla biter, zamanlayiciya guvenilmez).
	if(g_ztkTargetWar || (g_inZoneWar && g_zwWarZone[0] && atoi(g_zwWarZone) == g_ztkZoneId))
	{
		if(!g_ztkWaitAnnounced)
		{
			g_ztkWaitAnnounced = true;
			Log("[ZSNIP] Zone #%d savasi suruyor, bitisi bekleniyor.", g_ztkZoneId);
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Zone #%d savasi devam ediyor, sniper bitisini bekliyor.",
				"!War in zone #%d is ongoing, sniper waiting for it to end."));
		}
		return;
	}

	// Faz 1 bitti: savas bitti ya da hic yoktu -> /gz ile saldir, sonucu bekle.
	if(g_ztkZoneId >= 0 && g_ztkZoneId < g_zoneCount)
	{
		Log("[ZSNIP] Zone #%d bosaldi (savas yok), /gz gonderiliyor.", g_ztkZoneId);
		g_ztkAttackPending = true;
		g_ztkOutcome = 0;
		g_zwWeWon = true; // yeni saldiri: sonuc bilinmiyor
		sendServerCommand((char *)"/gz");
		Log("[ZSNIP] /gz gonderildi, won/lost sonucu bekleniyor.");
	}
}
bool LearnGroupTagFromGStats(const char *szInfo)
{
	if(!szInfo || !szInfo[0]) return false;

	// Gecersiz dialoglari ele (grup stats DEGIL): komut listesi "/c - commands",
	// oyuncu stats "hakixld(136)", zone bilgisi "Zone Name: # 0", /groups basligi
	// "ID Name Members Online". DIALOGLAR RENK KODUYLA GELIR ({FFFFFF}...): once
	// ilk satiri renksizlestir, sonra dogrula - yoksa her sey "tagsiz grup" olur.
	{
		char szFirst[96] = {0};
		const char *pL = szInfo;
		int n = 0;
		while(*pL && *pL != '\n' && n < 95) { szFirst[n++] = *pL++; }
		szFirst[n] = 0;
		StripColorCodes(szFirst);
		char *s = szFirst;
		while(*s == ' ' || *s == '\t' || *s == '\r') s++;
		if(s[0] == '/') return false;                       // komut/yardim listesi
		if(strncmp(s, "Zone Name", 9) == 0) return false;   // /gzinfo zone bilgisi
		if(strstr(s, "Welcome")) return false;              // karsilama dialogu
		if(strncmp(s, "ID Name", 7) == 0 && strstr(s, "Members")) return false; // /groups basligi
		const char *pOp = strchr(s, '(');                   // oyuncu formati "isim(pid)"
		if(pOp && pOp[1] >= '0' && pOp[1] <= '9') return false;
	}

	char szName[80] = {0};
	const char *pE = strchr(szInfo, '\n');
	if(pE)
	{
		int n = (int)(pE - szInfo);
		if(n > 79) n = 79;
		memcpy(szName, szInfo, n); szName[n] = 0;
		NormalizeGroupName(szName);
	}
	else
	{
		strncpy_s(szName, sizeof(szName), szInfo, sizeof(szName) - 1);
		NormalizeGroupName(szName);
	}
	if(!szName[0]) return false;

	char szTag[32] = {0};
	const char *pTag = strstr(szInfo, "Tag:");
	if(pTag)
	{
		// UIF /gstats formati: "- Tag:<bosluklar>SG" - tag, Tag: sonrasi ilk
		// satir sonuna kadarki metindir. "[...]" parantezli olsa da ayni kural.
		pTag += 4;
		const char *pS = pTag;
		while(*pS == ' ' || *pS == '\t' || *pS == '\r') pS++;
		const char *pE2 = pS;
		while(*pE2 && *pE2 != '\n' && *pE2 != '\r') pE2++;
		int n = (int)(pE2 - pS);
		if(n > 31) n = 31;
		memcpy(szTag, pS, n); szTag[n] = 0;
		// "[SG]" ise koseli parantezleri temizle
		if(szTag[0] == '[')
		{
			memmove(szTag, szTag + 1, strlen(szTag)); // '[' sil
			for(int i = (int)strlen(szTag) - 1; i >= 0; i--)
			{
				if(szTag[i] == ']') { szTag[i] = 0; break; }
			}
		}
		StripColorCodes(szTag);
		for(int i = (int)strlen(szTag) - 1; i >= 0; i--)
		{
			if(szTag[i] == ' ' || szTag[i] == '\t' || szTag[i] == '\r' || szTag[i] == '\n') szTag[i] = 0;
			else break;
		}
	}

	// Isimle eslesen kaydi bul, tag'ini guncelle (tag yoksa yenisi eklenir)
	// "NULL"/"-" UIF'te tag'siz grup demektir: gercek tag yok. Boyle gruplar
	// "~" sentinel'iyle isaretlenir (harvest bir daha taramasin; ekranda tam isim gˆsterilir).
	if(szTag[0] && (!_stricmp(szTag, "NULL") || !strcmp(szTag, "-"))) szTag[0] = 0;
	int idx = -1;
	for(int i = 0; i < g_groupColorCount; i++)
	{
		char szDbName[80]; strncpy_s(szDbName, sizeof(szDbName), g_groupColors[i].szName, sizeof(szDbName) - 1);
		NormalizeGroupName(szDbName);
		if(_stricmp(szDbName, szName) == 0) { idx = i; break; }
	}
	// Isim esle-smediyse tag eslesmesiyle dene (isimler gzinfo/gstats'ta farkli olabiliyor)
	if(idx < 0 && szTag[0])
	{
		for(int i = 0; i < g_groupColorCount; i++)
		{
			if(_stricmp(g_groupColors[i].szTag, szTag) == 0) { idx = i; break; }
		}
	}
	if(idx >= 0)
	{
		if(szTag[0] && strcmp(g_groupColors[idx].szTag, szTag) != 0)
		{
			Log("[GSTATS-TAG] '%s' etiketi guncellendi: '%s' -> '%s'.", szName, g_groupColors[idx].szTag, szTag);
			strncpy_s(g_groupColors[idx].szTag, sizeof(g_groupColors[0].szTag), szTag, sizeof(g_groupColors[0].szTag) - 1);
			// Isimde renk kodu varsa temizle (DB'ye yalin isim yazilir)
			char szClean[80]; strncpy_s(szClean, sizeof(szClean), szName, sizeof(szClean) - 1);
			NormalizeGroupName(szClean);
			if(strcmp(g_groupColors[idx].szName, szClean) != 0)
			{
				strncpy_s(g_groupColors[idx].szName, sizeof(g_groupColors[0].szName),
					szClean, sizeof(g_groupColors[0].szName) - 1);
				Log("[GSTATS-TAG] '%s' isim renk kodlari temizlendi -> '%s'.", szName, szClean);
			}
			SaveGroupColors();
			return true;
		}
		// Gercek tag'i yok ve hala bos/placeholder: dogrulandi -> "~" isaretle.
		if(!szTag[0] && (!g_groupColors[idx].szTag[0]
			|| !strcmp(g_groupColors[idx].szTag, "~")
			|| !_stricmp(g_groupColors[idx].szTag, "NULL")
			|| !strcmp(g_groupColors[idx].szTag, "-")
			|| _stricmp(g_groupColors[idx].szTag, g_groupColors[idx].szName) == 0))
		{
			strcpy(g_groupColors[idx].szTag, "~");
			SaveGroupColors();
			Log("[GSTATS-TAG] '%s' tag'siz dogrulandi (harvest bir daha taramayacak).", szName);
			return true;
		}
		return false;
	}
	else if(g_groupColorCount < MAX_GROUP_COLORS && szName[0])
	{
		// Isim kaydi yoksa bile ekle (tag'i varsa tag, yoksa "~" dogrulanmis-tagsiz):
		// her /groups taramasinda ayni gruplar yeniden sayilmaz.
		char szClean[80]; strncpy_s(szClean, sizeof(szClean), szName, sizeof(szClean) - 1);
		NormalizeGroupName(szClean);
		strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
			szClean, sizeof(g_groupColors[0].szName) - 1);
		strncpy_s(g_groupColors[g_groupColorCount].szTag, sizeof(g_groupColors[0].szTag),
			szTag[0] ? szTag : "~", sizeof(g_groupColors[0].szTag) - 1);
		g_groupColorCount++;
		SaveGroupColors();
		Log("[GSTATS-TAG] '%s' -> '%s' kaydedildi.", szClean, szTag[0] ? szTag : "(tagsiz)");
		return true;
	}
	return false;
}
// /groups sonucundan tum grup ID'lerini alir; her ID icin sirasiyla /gstats <ID>
// gˆndererek grubun kisa TAG'ini ˆrenir. Zincir bir sonraki adimi her gstats
// sonucunda SIGILIR (SAMPRPC -> OnGStatsResultForHarvest).
// ponytail: /groups ve /gstats ciktilari canli sunucuya gˆre; format degisirse
// parse kurali (OnGroupsListCaptured) guncellenir.
#define GHP_MAX_GROUPS 256
static int   g_ghpIds[GHP_MAX_GROUPS];
static char  g_ghpNames[GHP_MAX_GROUPS][80];

// /groups sonuc dialogunu bekleyen flag (SAMPRPC yakalar, OnGroupsListCaptured'a verir)
bool g_waitingForGroups = false;
unsigned long g_groupsReqTime = 0;

// /groups sonuc dialogunu parse et + harvest'i ba˛lat. SAMPRPC cagirir.
void OnGroupsListCaptured(const char *szInfo)
{
	Log("[GHP] OnGroupsListCaptured: ghpActive=%d len=%d", g_ghpActive, szInfo ? (int)strlen(szInfo) : -1);
	if(g_ghpActive) return;
	Log("[GROUPS-DATA] %.500s", szInfo ? szInfo : "");

	g_ghpCount = 0; g_ghpIdx = 0; g_ghpActive = true; g_ghpWaitingGstats = false;
	if(!szInfo) { Log("[GHP] /groups ciktisi bos, harvest iptal."); g_ghpActive = false; return; }

	// Satir bazli parse: "N. isim" / "N: isim" / "N- isim" (ilk token sayi = grup ID)
	char szLine[200];
	int nSkipped = 0; // tag'i zaten olan gruplar (/gstats gˆnderilmez)
	const char *pL = szInfo;
	while(pL && *pL && g_ghpCount < GHP_MAX_GROUPS)
	{
		const char *pE = strchr(pL, '\n');
		size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
		if(nLen == 0 || nLen >= sizeof(szLine)) { pL = pE ? pE + 1 : NULL; continue; }
		memcpy(szLine, pL, nLen); szLine[nLen] = 0;
		pL = pE ? pE + 1 : NULL;

		char *pCur = szLine;
		while(*pCur == ' ' || *pCur == '\t' || *pCur == '\r') pCur++;
		if(*pCur < '0' || *pCur > '9') continue; // ID'siz satir
		int iId = atoi(pCur);
		while(*pCur >= '0' && *pCur <= '9') pCur++;
		// "N. isim" | "N: isim" | "N- isim" | "N isim"
		while(*pCur == ' ' || *pCur == '.' || *pCur == ':' || *pCur == '-' || *pCur == '\t') pCur++;

		char szName[80];
		strncpy_s(szName, sizeof(szName), pCur, sizeof(szName) - 1);
		NormalizeGroupName(szName);
		// Tablist formati "ID\tisim\t..." olabilir: ismi ilk tab'da kes
		for(char *pT = szName; *pT; pT++)
		{
			if(*pT == '\t') { *pT = 0; break; }
		}
		// Satir sonu bosluk / \r temizle
		for(int i = (int)strlen(szName) - 1; i >= 0; i--)
		{
			if(szName[i] == ' ' || szName[i] == '\r' || szName[i] == '\t') szName[i] = 0;
			else break;
		}
		// /groups satiri "N. Isim <online-sayisi>" biciminde: sondaki sayi uyelik
		// adedi, ismin parcasi DEGIL. Kesmezsek DB'deki temiz isimle eslesmez ve
		// hicbir grup ATLANDI sayilmazdi (her periyotta hepsi yeniden taran-‚ñíyordu).
		{
			int nLen = (int)strlen(szName);
			int nEnd = nLen;
			while(nEnd > 0 && szName[nEnd-1] >= '0' && szName[nEnd-1] <= '9') nEnd--;
			if(nEnd > 0 && nEnd < nLen && szName[nEnd-1] == ' ')
				szName[nEnd-1] = 0;
		}
		if(iId <= 0 || !szName[0]) continue;

		// DB'de GERCEK kisa tag'i olan grup atlanir: /gstats sadece BILINMEYENLER
		// icin gˆnderilir (komut mute riski + gereksiz tekrar).
		// Isim eslesmesi TEK BASINA yetmez: zone taramasinda ˆrenilen gruplara once
		// tag=isim yazilir (placeholder) - bunlar BILINMEYEN sayilip /gstats ile kisa
		// taglari cekilir (?zf tam isim yerine kisa tag gostersin diye).
		bool bKnown = false;
		char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), szName, sizeof(szNeed) - 1);
		NormalizeGroupName(szNeed);
		for(int i = 0; i < g_groupColorCount; i++)
		{
			char szDbClean[80]; strncpy_s(szDbClean, sizeof(szDbClean), g_groupColors[i].szName, sizeof(szDbClean) - 1);
			NormalizeGroupName(szDbClean);
			if(_stricmp(szDbClean, szNeed) == 0 && g_groupColors[i].szName[0]
				&& g_groupColors[i].szTag[0]
				&& _stricmp(g_groupColors[i].szTag, g_groupColors[i].szName) != 0)
			{
				bKnown = true;
				break;
			}
			// Tag gˆrduk: bir isimle iliskilendirilmis kisa etiket.
			if(g_groupColors[i].szTag[0] && _stricmp(g_groupColors[i].szTag, "NULL") != 0
				&& _stricmp(g_groupColors[i].szTag, szNeed) == 0)
			{
				bKnown = true;
				break;
			}
		}
		if(bKnown)
		{
			Log("[GHP] '%s' (id=%d) ATLANDI - grup datasi zaten var, eklenmedi.", szName, iId);
			nSkipped++;
			continue;
		}

		g_ghpIds[g_ghpCount] = iId;
		strncpy_s(g_ghpNames[g_ghpCount], 80, szName, 79);
		g_ghpCount++;
	}

	Log("[GHP] /groups: %d grup ATLANDI (datasi zaten var), %d yeni grup tag icin taranacak.",
		nSkipped, g_ghpCount);
	if(g_ghpCount == 0) { g_ghpActive = false; return; }
	g_ghpNextStepAt = GetTickCount() + GHP_STEP_DELAY_MS; // ilk /gstats oncesi de mute korumasi
	ProcessGhpHarvestStep();
}

// Sonraki harvester adimi: bir sonraki ID icin /gstats gˆnder (veya bitir).
void ProcessGhpHarvestStep()
{
	if(!g_ghpActive || g_ghpWaitingGstats) return;

	// Komut mute korumasi: deadline dolmadiysa beklet. Zaman asimina dusmus
	// bekleme (g_waitingForGStats false) deadline'i ASAR - zincir takilmasin.
	if(g_ghpNextStepAt != 0 && GetTickCount() < g_ghpNextStepAt && g_waitingForGStats) return;

	if(g_ghpIdx >= g_ghpCount)
	{
		Log("[GHP] Harvest tamamlandi: %d grup islendi.", g_ghpCount);
		g_ghpActive = false;
		g_ghpNextStepAt = 0;
		g_forceZscanAfterHarvest = true; // zone scan harvesti tetikle (yeni veri varsa, zscan off olsa bile)
		// ?zgrp on TEK SEferliktir: harvest bitti, otomatik off'a dus (kullan˝c˝
		// istedigi an tekrar ?zgrp on diyebilir; periyodik tekrar YOK).
		// g_zgrpAuto surekli acik - bitince standby (uyku) moduna gecer, kapanmaz
		if(false)
		{
			g_zgrpAuto = false;
			SyncBotStatusToDb(); // gruba gˆre DB'ye kaydet
			Log("[GHP] Oto tag harvest tamamlandi, standby (uyku) moduna gecildi.");
		}
		SyncGroupColorsToDb();
		// Gruba mesaj ATILMAZ (harvest arka plan isidir; spam olmasin).
		if(g_ghpLearnedCount > 0)
			Log("[GHP] %d yeni grup tagi DB'ye kaydedildi.", g_ghpLearnedCount);
		// Tag'ler ˆrenildi; ?zscan on ise SIMDI flashing zone taramasini ba˛lat
		// (kullan˝c˝ sirasi: once taglar, sonra zone ˆrenme). Restore sonrasi.
		if(g_zscanAuto && s_bGroupDbRestored && iSpawned && g_zoneScanIdx < 0 && !g_inZoneWar && !g_zgoActive)
		{
			Log("[GHP] Harvest bitti - otomatik zone taramasi ba˛latiliyor.");
			StartZoneScan(true);
		}
		// Tag'ler ˆrenildi; ?zf ozeti tag'lerle tekrar atilsin (kullan˝c˝ istemedigi
		// surece bir daha harvest yapilmaz - tag'ler db'de kalici).
		if(g_zfFlashPendingFlag || g_zfFlashAt != 0)
		{
			g_zfFlashAt = GetTickCount() + 1000;
			Log("[GHP] Harvest sonrasi ?zf ozeti tag'lerle yeniden gˆnderilecek.");
		}
		return;
	}

	int iId = g_ghpIds[g_ghpIdx];
	Log("[GHP] (%d/%d) /gstats %d gˆnderiliyor (grup: %s).", g_ghpIdx + 1, g_ghpCount, iId, g_ghpNames[g_ghpIdx]);

	// cmds.cpp /gstats blokuyla ayni flag kurulumu -> sonuc dialogu SAMPRPC 4. blokta yakalanir
	g_waitingForGStats = true;
	g_gstatsReqTime = GetTickCount();
	g_gstatsAutoSubmit = true;
	g_gstatsInput = "";
	g_gstatsRequesterId = "";
	g_gstatsSentFirst = false;
	char szCmd[32];
	sprintf_s(szCmd, sizeof(szCmd), "/gstats %d", iId);
	g_gstatsInput = szCmd + 8; // "123" -> INPUT dialoguna yazilip submit edilir
	sendServerCommand(szCmd);
	g_ghpWaitingGstats = true;
}

// /gstats <ID> sonucu: harvest modundaysa TAG'i ˆren + siradaki ID'ye gec.
// true doner => SAMPRPC bu sonucu Discord'a gˆndermez (harvest icin kullanildi).
// NOT: /gstats ciktisindan zone->sahip baglamasi YAPILMAZ. Diyalogdaki sayilar
// guvenilmezdi (uyelerin sayisi/kill istatistigi zone sanilip yanlis zone
// baglaniyordu: #26 -> dEV, #81 -> SG hatalari). Zone sahibi YALNIZCA /gzinfo
// taramasindan ve "ZONE WAR: X takes/keeps" mesajlarindan ˆrenilir.
bool OnGStatsResultForHarvest(const char *szInfo)
{
	if(!g_ghpActive || !g_ghpWaitingGstats) return false;

	Log("[GSTATS-DATA] %.300s", szInfo ? szInfo : "");

	// Gecersiz dialog: "/c - commands" gibi KOMUT/YARDIM listesi grup stats
	// degildir. (/gstats <id> gecersiz/olmayan ID'de sunucu yardim dialogu
	// acabilir.) Boyle bir cevap grupta tag var/yok demek DEGILDIR - o grubu
	// atla, "tag'siz" olarak isaretleme, zinciri ilerlet.
	const char *pFirst = szInfo ? szInfo : "";
	while(*pFirst == ' ' || *pFirst == '\t' || *pFirst == '\r') pFirst++;
	if(pFirst[0] == '/')
	{
		Log("[GHP] Gecersiz /gstats dialogu (komut listesi), grup atlaniyor: %.60s", pFirst);
		g_ghpIdx++;
		g_ghpWaitingGstats = false;
		g_waitingForGStats = false;
		g_ghpStaleRetries = 0;
		g_muteReconnectPending = false;
		g_ghpNextStepAt = GetTickCount() + GHP_STEP_DELAY_MS;
		return true;
	}

	// "- Tag: [nM]" satirindan kisa etiketi ˆren (?zgrp ile toplu tag toplama)
	if(LearnGroupTagFromGStats(szInfo)) g_ghpLearnedCount++;

	g_ghpIdx++;
	g_ghpWaitingGstats = false;
	g_waitingForGStats = false;
	g_ghpStaleRetries = 0; // adim basariyla ilerledi: yeni adim icin retry sayisi sifir
	g_muteReconnectPending = false; // adim cevap verdi: balant˝ saglikli, sonraki mute icin sifirla
	// Komut mute korumasi: siradaki /gstats'i aninda degil, GHP_STEP_DELAY_MS
	// sonra gˆnder (ProcessZoneScan deadline'i gˆrunce ProcessGhpHarvestStep cagirir).
	g_ghpNextStepAt = GetTickCount() + GHP_STEP_DELAY_MS;
	return true;
}

// ?zgo: bekleme suresi doldu mu? -> eski (spawn) pozisyonuna don + gruba bildir.
void ProcessZgoReturn()
{
	if(!g_zgoActive) return;

	DWORD dwNow = GetTickCount();
	if(dwNow < g_zgoReturnTick) return;

	g_zgoActive = false;

	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", g_zgoReturnPos[0], g_zgoReturnPos[1], g_zgoReturnPos[2]);
	Log("[ZGO] Bekleme suresi doldu, eski pozisyona donuluyor (%s).", szCmd);
	sendServerCommand(szCmd);

	char szMsg[128];
	if(g_zgoZoneId >= 0)
		sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d beklemesi bitti, geri donuldu.", "!Zone #%d wait finished, returned back."), g_zgoZoneId);
	else
		sprintf_s(szMsg, sizeof(szMsg), LM("!Zone beklemesi bitti, geri donuldu.", "!Zone wait finished, returned back."));
	g_bridgeSuppressEcho++;
	sendChat(szMsg);
}

void RegisterRPCs(RakClientInterface *pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, ServerJoin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, ServerQuit);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_InitGame, InitGame);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, WorldPlayerAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath, WorldPlayerDeath);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDeathMessage, ScrDeathMessage);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, WorldPlayerRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd, WorldVehicleAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, WorldVehicleRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, ConnectionRejected);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, ClientMessage);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Chat, Chat);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs, UpdateScoresPingsIPs);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, SetCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, DisableCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Pickup, Pickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DestroyPickup, DestroyPickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RequestClass);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Spawn, SpawnConfirmed);

		// Scripting RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInitMenu, ScrInitMenu);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDialogBox, ScrDialogBox);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrGameText);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_PlayAudioStream, ScrPlayAudioStream);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel, ScrSetDrunkLevel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrHaveSomeMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel, ScrCreate3DTextLabel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw, ScrEditTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWarGroup, ScrSetWarGroup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAddGangZone, ScrAddGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone, ScrRemoveGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone, ScrFlashGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone, ScrStopFlashGangZone);
	}
}

void UnRegisterRPCs(RakClientInterface * pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerJoin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerQuit);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_InitGame);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDeathMessage);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ConnectionRejected);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ClientMessage);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Chat);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DisableCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Pickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DestroyPickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestClass);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Spawn);

		// Scripting RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInitMenu);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDialogBox);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_PlayAudioStream);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrResetMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetWarGroup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrAddGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone);
	}
}

