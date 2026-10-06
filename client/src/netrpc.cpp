/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"

#include <cmath>   // json.hpp'nin std::isfinite gereksinimi
#include <math.h>
#undef isfinite    // MSVC math.h makrosu std::isfinite'i kýrar (Security.cpp'de sorun yok; burada main.h getiriyor)
#include <stdio.h>
#undef snprintf    // MSVC stdio.h makrosu std::snprintf'i kýrar
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

// Zone war süresi: sabit 10 dk (sunucu kuralý). ?gzt kalan süreyi bundan hesaplar;
// "Type /gz2 to join" bildirimi sava-sýn ba-slangý-g anýdýr (g_zoneWarStartTick).
#define ZONE_WAR_TOTAL_MS 600000 // Oasis: savas ~6-10 dk (logda 6dk18sn olculdu)
// GENEL savas takibi (?gzt time tum savaslar icin): grubumuz olmasa da aktif
// savas gorunur. Geri sayim duyurulari (10/5/3/1) sadece BIZIM savasimizda atilir.
static bool  g_anyWarActive = false;
static DWORD g_anyWarStartTick = 0;
static char  g_anyWarDesc[128] = {0};
static DWORD g_lastWarEndTick = 0;
// Chat savas sonucu (yedek): dialog verisi gelmezse 15sn sonra gonderilir.
// Dialog/textdraw sonucu oncelikli (kullanici chat ozeti istemiyor).
static char  g_wrWin[64] = {0}, g_wrLose[64] = {0}, g_wrScore[64] = {0}, g_wrZone[64] = {0};
static bool  g_wrPosted = false;
static DWORD g_wrArmedAt = 0;
 // son savas bitisi (sonuc dialogu penceresi)
static DWORD g_zwLastGzAt = 0;   // son /gz gonderimi (yetki reddi korelasyonu)
static bool  g_zwNoGzEntry = false; // /gz reddedildi: bu savas giris deneme
#define KILL_LIST_CAPTURE_WINDOW_MS 90000 // sonu-g textdraw'larý i-gin toplama penceresi (son 60 sn giri? + bitis textdraw'lari)
static DWORD g_zoneWarStartTick = 0;

// Kill list yakalayýcý durumu (GZ sonucu textdraw'larý -> dosya + Discord).
// Kill-list textdraw'lari sadece GZ -â–‘-c-â–‘NDEK-â–‘LERE gelir; bot son 00:02'de /gz2 ile girer ve kalir.
static bool g_zwCapturingKillList = false;  // textdraw toplama aktif mi
static bool g_zwKillListSent = false;       // sonu-g zaten g-nderildi mi
static DWORD g_zwCaptureStart = 0;          // toplama ba-slangýcý
static DWORD g_zwFinalizeAt = 0;
static DWORD g_zwEmptyFinalizeSince = 0; // bos finalize ilk gorulme zamani (retry/give-up)            // savas bitisi finalize zamani (wins/loses blo-su kurar)
static std::string g_zwKillListText;        // birikmi-s sonu-g metni (renk kodlarý korunarak toplanýr, finalize'da ayrý-stýrýlýr)
static char g_zwWarZone[16] = {0};          // aktif sava-sýn zone no'su ("146"); 60 sn'lik tekrar duyurusu bununla elenir
static std::string g_zwWarTeamLeft;         // "X vs Y" duyurusundan SOL grup (kirmizi takim)
static std::string g_zwWarTeamRight;        // "X vs Y" duyurusundan SAG grup (mavi takim)
static bool g_zwWelcomed[MAX_PLAYERS];      // bu sava-sta oyuncu ba-sýna bir kez welcome (sava-s ba-sýnda sýfýrlanýr)

// Kill list textdraw'lari AYRI AYRI gelir: her kolon/satir ayri textdraw (ID 70 "Name",
// 71 "Kills", 72 "Assists", 73 "Deaths", 74 "Dmg" basliklari Y=80; veri satirlari Y=95+,
// X kolon hizasi). Ayni Y'deki textdraw'lar X'e gore yan yana dizilir -> satir olusturur.
#define KILL_TD_MAX 768
struct KillTdItem { float fX, fY; char szText[64]; DWORD dwColor; };
static KillTdItem g_zwKillTds[KILL_TD_MAX];
static int g_zwKillTdCount = 0;
void FinalizeKillList(); // ileri bildirim (savas-basi kalintisini kapatmak icin)
// Kill list istatistikleri (Discord embed icin)
static int g_zwBestK = 0, g_zwBestDmg = 0, g_zwBestD = 0;
static char g_zwMvpName[64] = {0}, g_zwFeederName[64] = {0};
static int g_zwMineK = 0, g_zwMineA = 0, g_zwMineD = 0, g_zwMineDmg = 0;
static int g_zwEnemyK = 0, g_zwEnemyA = 0, g_zwEnemyD = 0, g_zwEnemyDmg = 0;
static bool g_zwWeWon = true; // savas sonucu: biz kazandik mi (war-end blogu kurar)
static char g_zwFirstBlood[160] = {0}; // GZ ilk kill: "isim (Weapon: X) (Distance: Ym)"

// Top Weapons: RPC 55 (DeathMessage - sagdaki kill feed) savas boyunca toplanir.
// "ZONE WAR: X vs Y" duyurusundaki konum takimimizi belirler (sol=kirmizi, sag=mavi);
// finalize'da scoreboard isimleriyle filtrelenir, top 5 silah Discord'a gider.
static bool g_zwWeAreBlue = false;      // isim eslesmesi: grubumuz "vs" de sagda mi (mavi)
static bool g_zwWeAreBlueKnown = false; // vs duyurusundan belirlendi mi (renk fallback'i icin)
#define WAR_KILL_MAX 1024
struct WarKill { char szKiller[32]; BYTE weapon; };
static WarKill g_zwWarKills[WAR_KILL_MAX];
static int g_zwWarKillCount = 0;

// GZ giri-sleri: sava-s ba-sýnda takým rengi --srenmek i-gin kýsa giri-s; son 00:02'de kill-list giri-si.
static bool   g_zwReturnPending = false;   // kýsa giri-s aktif ([gz2] -> [fr] ile geri d-nülecek)
static DWORD  g_zwReturnAt = 0;
static bool   g_zwKillListStay = false;    // kill-list giri-sinde GZ'de kalinir (savas bitince finalize + /fr)
static bool   g_zwAutoJoinSent = false;    // son 00:02'de /gz2 g-nderildi mi (kill-list i-gin i-geride kalýnýr)
static bool   g_zwCheckJoinSent = false;   // 09:02 kontrol giri-si yapýldý mý (sava-s aktif mi diye bakar)
static unsigned int g_zwMyColor = 0;       // GZ'de bizim takým rengi ({RRGGBB}, 0 = bilinmiyor)
// Esir-alma ile grup adi ogrenimi: "ZONE WAR: X takes over zone '# N'" -> zone N'nin
// sahipligi (RPC 108 ile) guncellenir; o guncellemede X -> zone N esle (pending).
// (renk teyit pending mekanizmasi kaldirildi - sahiplik ZONE WAR mesajindan
//  direkt baglanir, renk kullanilmaz)

// Sava-s zamanlayýcý (UIF textdraw ID 60): "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> skor + kalan süre.
// Bot GZ i-ginde kýsa giri-slerde (sava-s ba-sý renk --srenme) timer'i de yakalayabilir.
static DWORD g_warTimerTick = 0;            // son yakalama zamaný
static int  g_warRemainingSec = -1;         // yakalanan kalan süre (sn)

// Gang zone yakalayýcý: UIF zone numaralarý haritanýn üst solundan (Tierra Robada)
// ba-slayarak #0'dan itibaren sýralý olu-sturulur -> gang zone ID == zone # kabul edilir.
// RPC_ScrAddGangZone (108) geldik-ge koordinatlar buraya kaydedilir (?zgo i-gin).
#define MAX_GANG_ZONES 2048
static float g_zoneMinX[MAX_GANG_ZONES];
static char g_zoneNames[MAX_GANG_ZONES][64] = {0}; // /gzinfo "Zone:" satirindan (seized eslemesi icin)
static float g_zoneMinY[MAX_GANG_ZONES];
static float g_zoneMaxX[MAX_GANG_ZONES];
static float g_zoneMaxY[MAX_GANG_ZONES];
// (g_zoneColor dizisi kaldirildi - eslesme tamamen isim bazli, renk saklanmaz/kullanilmaz)
static bool g_zoneFlash[MAX_GANG_ZONES];
// zone -> sahip grup adi (/gzinfo "owned by group X" ile ogrenilir). Renk bagimsiz
// eslesme: YS.Hold "bot renk bagimli olmasin, sadece flash bagimli olsun" - renk geri
// donusumunde ayni renk farkli gruplara baglaniyordu; isim eslesmesi bunu cozer.
static char g_zoneOwnerName[MAX_GANG_ZONES][80];
static int g_zoneCount = 0;

// ?zgo: bot zone'a isinlanip 40 sn bekler, sonra eski pozisyonuna doner.
#define ZGO_STAY_MS 40000
// Sunucu zone id'si (ekranda gorunen, /gzinfo) bizim dizinimizden 1 KUCUK:
// create/flash paketleri 1'den basliyor, /gzinfo 0'dan. 57 ornekte de fark
// tam -1 idi ("?zgo 4" -> /gzinfo "#3"). Bu iki makro donusumu tek yerde tutar.
#define ZONE_IDX_FROM_SHOWN(n) ((n) + 1)  // ekranda gorunen numara -> dizin
#define ZONE_SHOWN_FROM_IDX(i) ((i) - 1)  // dizin -> ekranda gorunen numara
#define ZGO_TELEPORT_Z 100.0f // yuksekten dus, zemin Z'sine otur (sabit 25 bazi bolgelerde yerin dibinde kaliyor)
static bool g_zgoActive = false;            // isinlama + bekleme aktif mi
static DWORD g_zgoReturnTick = 0;           // geri donus zamani
static float g_zgoReturnPos[3] = {0, 0, 0}; // donulecek (spawn) pozisyon

// ?gzt: geri sayim duyurulari (YS botu gibi). Esikler SABIT DOSYADAN degil,
// sava??n gercek suresinden otomatik hesaplanir (kullan?c?: "ilk zamaný -gekip
// ona g-re ayarla"): yarim nokta + 60/30/10 sn. Sunucu sureyi zaten surekli
// veriyor (timer textdraw + Time left mesajlari) - CaptureWarTimer yakalar.
static int   g_gztCountdownAnnounced = 999;
static int   g_gztLastRem = -1;          // onceki kontrol anindaki kalan saniye (esik gecisi tespiti)
static bool  g_warTimerViaTdw = false;    // timer TEXTDRAW'dan mi geldi? (kendi savasimiz
                                          // GZ'sinde g-rünür; uL.Hold'un baska sava?lar icin
                                          // attigi "Time left:" chat mesajlari sayilmaz -
                                          // yoksa bizde savas yokken geri sayim basliyordu)
// ?gzt on/off: geri sayim duyurulari acik mi? (varsayilan ON = mevcut davranis)
static bool g_gztOn = true;

// ?za: /gz sonrasi yetki kontrolu - co-leader/leader degilse UIF bir textdraw
// gosterir. Bu pencere icinde gelen textdraw'larda "leader" gecen metin yakalanir
// ve gruba dile gore "yetkim yok" mesaji atilir.
static bool  g_zaPermActive = false;
static DWORD g_zaPermUntil = 0;

// ?zsnip (zone sniper): hedef zone'a isinla, o zonede savas bitene kadar bekle,
// bitince /gz gonder (oraya saldiri). Isim (grup tag'i) veya sayisal zone id alir.
static bool  g_ztkActive = false;
static bool  g_ztkVerifyPending = false; // /gzinfo ile sahip dogrulama bekleniyor mu
static int   g_ztkVerifyZone = -1;        // dogrulanan zone
static DWORD  g_ztkTeleportAt = 0;        // son sniper teleportu (?zgo mantigi varis payi)
static int   g_ztkVerifyTries = 0;         // cevapsiz /gzinfo sayisi (3 denemede siradakine gec)
static DWORD  g_ztkVerifyAt = 0;           // /gzinfo gonderim ani (12 sn timeout)
static bool   g_ztkWarSeen = false;       // /gz sonrasi savasimiz basladi mi (sahte-win kilidi)
static DWORD  g_ztkAttackAt = 0;           // /gz gonderim ani (20 sn savas-bekleme)
static int   g_ztkZoneId = -1;
static bool  g_ztkWaitAnnounced = false;
// Zincir saldiri (?zsnip + ?yes): grubun diger zone'larina sirasiyla saldir.
static char  g_snipGroupName[96] = {0}; // hedef grubun adi
static int   g_snipZones[64];           // grubun saldiri sirasi zone listesi
static int   g_snipZoneCount = 0;
static int   g_snipZoneIdx = 0;         // siradaki zone (son saldirilan = idx-1)
static bool  g_ztkAskNext = false;
static bool  g_ztkPreAsked = false;
static bool  g_ztkYesPending = false;      // saldiri bitti, ?yes bekleniyor
static bool  g_znipHeldForWar = false;
static DWORD g_znipHoldAt = 0;
static bool  g_ztkAttackPending = false; // /gz gonderildi, won/lost sonucu bekleniyor
static int   g_ztkOutcome = 0;            // 0=bilinmiyor, 1=kazanildi, 2=kaybedildi
static bool  g_ztkTargetWar = false;      // hedef zonede dusman savasi suruyor (mesaj bazli takip)
static bool IsSniperTargetZone(int zid); // ileri bildirim (zsnip zone eslesmesi)
static int CollectGroupZones(const char *szGroupName, int *outZones, int maxZones); // ileri bildirim

// Kesilen zone taramasi + bekleyen ?zsnip istegi: ?yes = saldir, ?no = taramaya devam
static bool  g_snipInterrupt = false;
// ?zsnip yetki kontrolu: /gmembers dialogundan bot + komutu atan kullanici
// Co-Leader/Leader rolune sahip mi? Degilse sniper (baslatma ve off) CALISTIRILMAZ.
// NOT: /gmembers SAYFALI dialogdur; bu surum ilk sayfayi okur (isimler sonraki
// sayfalardaysa yetki dogrulanamaz, reddedilir - guvenli varsayilan).
static bool  g_snipPermPending = false;  // /gmembers cevabi bekleniyor
static int   g_snipAction = 0;           // 0 = sniper baslat, 1 = sniper kapat
static int   g_snipZone = -1;            // bekleyen hedef zone
static int   g_snipSenderId = -1;        // komutu atan kullanici (playerInfo'dan ismi alinir)
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
int g_gmPhase = 0;            // 0=kapali 1=uyeler 2=astats 3=gvips 4=gonderim (SAMPRPC extern)
int g_gmPage = 0;
unsigned long g_gmPhaseAt = 0;
int g_gmAstatsIdx = 0;
bool g_gmAstatsWaiting = false;
char g_gmAstatsName[64] = {0};
bool g_gmGvipsWaiting = false;
static bool g_gmSendDone = false;
static std::map<std::string, std::string> g_gmKnownAcc;
static bool g_gmPrefilled = false;
static std::string g_gmRequestId;
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

// Query icin URL encode (ASCII disi baytlar %XX, bosluk +)
static void GmUrlEncode(const std::string &in, std::string &out)
{
	static const char *hexd = "0123456789ABCDEF";
	for(size_t i = 0; i < in.size(); i++)
	{
		unsigned char c = (unsigned char)in[i];
		if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
		else if(c == ' ') out += '+';
		else { out += '%'; out += hexd[c >> 4]; out += hexd[c & 15]; }
	}
}

void StartGmembersCollect(const char *requestId)
{
	g_gmRequestId = (requestId && requestId[0]) ? requestId : "";
	if(g_gmPhase != 0)
	{
		Log("[GM] Koleksiyon zaten calisiyor (faz %d).", g_gmPhase);
		return;
	}
	g_gmPhase = 1;
	g_gmCount = 0; g_gmPage = 0;
	g_gmAstatsIdx = 0; g_gmAstatsWaiting = false;
	g_gmGvipsWaiting = false; g_gmSendDone = false;
	g_gmKnownAcc.clear(); g_gmPrefilled = false;
	// Tabloda account ID'si olanlari cek: bunlara /astats GONDERILMEZ (faz 3'te atlanir)
	try {
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(4);
		std::string grpEnc; GmUrlEncode(g_myGroupName, grpEnc);
		std::string strUrl = std::string("/api/gmembers/known?hwid=") + g_myHWID + "&group=" + grpEnc;
		auto res = cli.Get(strUrl.c_str());
		if(res && res->status == 200){
			json jk = json::parse(res->body);
			if(jk.value("success", false) && jk.contains("known") && jk["known"].is_object()){
				for(auto &el : jk["known"].items()){
					if(!el.value().is_string()) continue;
					std::string k = el.key(), v = el.value().get<std::string>();
					for(size_t i = 0; i < k.size(); i++) if(k[i] >= 'A' && k[i] <= 'Z') k[i] += 32;
					if(k[0] && v[0]) g_gmKnownAcc[k] = v;
				}
				Log("[GM] Tablodan %d kayitli account ID alindi (astats atlanacak).", (int)g_gmKnownAcc.size());
			}
		}
	} catch(...) {}
	g_gmPhaseAt = GetTickCount();
	Log("[GM] ?gmembers koleksiyonu basladi (faz 1: uye listesi).");
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
			for(int i = 0; szClean[i] && i < 255; i++) szLow[i] = (char)((szClean[i]>='A'&&szClean[i]<='Z')?szClean[i]+32:szClean[i]);

			// Sayfalama isareti: ">>" / next / sonraki
			if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki") || strstr(szLow, "\xBB") || strstr(szLow, "\xAB"))
			{
				*outItem = iRow;
				iRow++; if(!pE) break; pL = pE + 1; continue;
			}
			// Baslik satiri: "#/ID/Name/Rank" gibi
			if((strstr(szLow, "id") && strstr(szLow, "name")) || strstr(szLow, "rank") && strstr(szLow, "role"))
			{ iRow++; if(!pE) break; pL = pE + 1; continue; }

			// Isim: baslangictaki sira numarasini ("1. ", "1: ", "1 ") atla
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
		// Oasis: "Name[accid]" - account ID'yi ayikla (/astats atlanir), ismi temizle.
		char szAccPre[24] = {0};
		{
			char *pAccO = strchr(szName, '[');
			char *pAccC = pAccO ? strchr(pAccO, ']') : NULL;
			if(pAccO && pAccC && pAccC > pAccO + 1)
			{
				size_t nA = (size_t)(pAccC - pAccO - 1);
				if(nA < sizeof(szAccPre)) { memcpy(szAccPre, pAccO + 1, nA); szAccPre[nA] = 0; }
				*pAccO = 0;
			}
		}

			// Rank/Role: gmembers formati - 2.kolon Rank, 3.kolon Role (ne ise o), bilinmeyen rank da alinir
			const char *szRanksLow[] = { "co-leader", "co leader", "leader", "member", "elder", "veteran", "recruit", "non gz", "non-gz", "nongz", "gz" };
			const char *szRanksOrig[] = { "Co-Leader", "Co-Leader", "Leader", "Member", "Elder", "Veteran", "Recruit", "Non GZ", "Non GZ", "Non GZ", "GZ" };
			char szRank[32] = {0}, szRole[32] = {0};
			{
				char szCopy[256]; strncpy_s(szCopy, sizeof(szCopy), szClean, sizeof(szCopy)-1);
				char *ctx = NULL;
				char *tok = strtok_s(szCopy, "\t", &ctx);
				char *cols[8] = {0}; int nCol = 0;
				while(tok && nCol < 8) { cols[nCol++] = tok; tok = strtok_s(NULL, "\t", &ctx); }
				if(nCol >= 2){
					char tmpRank[32]={0}, tmpRole[32]={0};
					strncpy_s(tmpRank, sizeof(tmpRank), cols[1], sizeof(tmpRank)-1);
					char *s = tmpRank; while(*s==' '||*s=='\t'||*s=='\r') s++; char *e = s+strlen(s)-1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0; if(s!=tmpRank) memmove(tmpRank, s, strlen(s)+1);
					strncpy_s(szRank, sizeof(szRank), tmpRank, sizeof(szRank)-1);
					if(nCol >= 3){
						strncpy_s(tmpRole, sizeof(tmpRole), cols[2], sizeof(tmpRole)-1);
						s=tmpRole; while(*s==' '||*s=='\t'||*s=='\r') s++; e=s+strlen(s)-1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0; if(s!=tmpRole) memmove(tmpRole, s, strlen(s)+1);
						strncpy_s(szRole, sizeof(szRole), tmpRole, sizeof(szRole)-1);
					} else {
						strncpy_s(szRole, sizeof(szRole), szRank, sizeof(szRole)-1);
					}
					char rLow[32]={0}; for(int i=0; tmpRank[i]&&i<31;i++) rLow[i]=(tmpRank[i]>='A'&&tmpRank[i]<='Z')?tmpRank[i]+32:tmpRank[i];
					for(int r=0;r<11;r++) if(strstr(rLow, szRanksLow[r])){ strncpy_s(szRank, sizeof(szRank), szRanksOrig[r], sizeof(szRank)-1); break; }
					char roleLow[32]={0}; for(int i=0; tmpRole[i]&&i<31;i++) roleLow[i]=(tmpRole[i]>='A'&&tmpRole[i]<='Z')?tmpRole[i]+32:tmpRole[i];
					for(int r=0;r<11;r++) if(strstr(roleLow, szRanksLow[r])){ strncpy_s(szRole, sizeof(szRole), szRanksOrig[r], sizeof(szRole)-1); break; }
					if(!szRole[0] && tmpRole[0]) strncpy_s(szRole, sizeof(szRole), tmpRole, sizeof(szRole)-1);
				} else {
					char *p = szClean;
					while(*p && (*p==' '||*p=='\t')) p++;
					while(*p && *p!=' ' && *p!='\t') p++;
					while(*p==' '||*p=='\t') p++;
					char tok2[32]={0}; int k=0; while(p[k] && p[k]!=' ' && p[k]!='\t' && k<31){ tok2[k]=p[k]; k++; } tok2[k]=0;
					if(tok2[0]){ strncpy_s(szRank, sizeof(szRank), tok2, sizeof(szRank)-1); strncpy_s(szRole, sizeof(szRole), tok2, sizeof(szRole)-1); }
					char rLow[32]={0}; for(int i=0; tok2[i]&&i<31;i++) rLow[i]=(tok2[i]>='A'&&tok2[i]<='Z')?tok2[i]+32:tok2[i];
					for(int r=0;r<11;r++) if(strstr(rLow, szRanksLow[r])){ strncpy_s(szRank, sizeof(szRank), szRanksOrig[r], sizeof(szRank)-1); strncpy_s(szRole, sizeof(szRole), szRanksOrig[r], sizeof(szRole)-1); break; }
				}
			}

			// Son aktif: son kolon - "2 days ago", "Online", "5 hours ago" gibi ne ise o
			char szLast[32] = {0};
			{
				char szCopy3[256]; strncpy_s(szCopy3, sizeof(szCopy3), szClean, sizeof(szCopy3)-1);
				char *ctx3=NULL; char *tok3=strtok_s(szCopy3, "\t", &ctx3);
				char *cols3[8]={0}; int n3=0; while(tok3 && n3<8){ cols3[n3++]=tok3; tok3=strtok_s(NULL, "\t", &ctx3); }
				if(n3 >= 2){
					const char *pLast = cols3[n3-1];
					if(pLast && pLast[0]){
						strncpy_s(szLast, sizeof(szLast), pLast, sizeof(szLast)-1);
						char *s=szLast; while(*s==' '||*s=='\t'||*s=='\r') s++; char *e=s+strlen(s)-1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0; if(s!=szLast) memmove(szLast,s,strlen(s)+1);
					}
				}
				if(!szLast[0]){
					const char *pT = strrchr(szClean, ' ');
					const char *pT2 = strrchr(szClean, '\t');
					const char *pW = pT && (!pT2 || pT > pT2) ? pT : pT2;
					if(pW && pW[1]) strncpy_s(szLast, sizeof(szLast), pW + 1, sizeof(szLast) - 1);
				}
			}

			// Tekrar kontrolu
			bool bDup = false;
			for(int i = 0; i < g_gmCount; i++)
				if(_stricmp(g_gmRows[i].szName, szName) == 0) { bDup = true; break; }
			if(!bDup)
			{
				strncpy_s(g_gmRows[g_gmCount].szName, sizeof(g_gmRows[0].szName), szName, sizeof(g_gmRows[0].szName) - 1);
				strncpy_s(g_gmRows[g_gmCount].szRank, sizeof(g_gmRows[0].szRank), szRank, sizeof(g_gmRows[0].szRank) - 1);
				strncpy_s(g_gmRows[g_gmCount].szRole, sizeof(g_gmRows[0].szRole), szRole, sizeof(g_gmRows[0].szRole) - 1);
				strncpy_s(g_gmRows[g_gmCount].szLast, sizeof(g_gmRows[0].szLast), szLast, sizeof(g_gmRows[0].szLast) - 1);
				if(szAccPre[0]) strncpy_s(g_gmRows[g_gmCount].szAccount, sizeof(g_gmRows[0].szAccount), szAccPre, sizeof(g_gmRows[0].szAccount) - 1);
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
	char szLow600[600]={0}; for(int i=0; szClean[i] && i<599; i++) szLow600[i]=(szClean[i]>='A'&&szClean[i]<='Z')?szClean[i]+32:szClean[i];
	char szAcc[24] = {0};

	// gmembers astats formati: "Account ID: 12345", "ID: 12345", "Acc ID : 123" - case-insensitive
	const char *pLowId = strstr(szLow600, "account id");
	if(!pLowId) pLowId = strstr(szLow600, "account");
	if(!pLowId) pLowId = strstr(szLow600, "acc id");
	if(!pLowId) pLowId = strstr(szLow600, "id");
	if(pLowId)
	{
		size_t off = pLowId - szLow600;
		const char *pN = szClean + off;
		while(*pN && *pN != ':' && (*pN < '0' || *pN > '9')) pN++;
		if(*pN == ':') pN++;
		while(*pN == ' ' || *pN == '\t' || *pN == ':') pN++;
		int n = 0;
		while(pN[n] >= '0' && pN[n] <= '9' && n < 23) { szAcc[n] = pN[n]; n++; }
		szAcc[n]=0;
		if(pLowId && strstr(pLowId, "account")==NULL && strlen(szAcc) < 4) szAcc[0]=0;
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

// Faz 2: /gvips dialogundan isim -> VIP tipi parse et
void SendGmembersPartial();
void OnGmVipsResult(const char *szInfo)
{
	Log("[GM-VIPS-DATA] %.800s", szInfo ? szInfo : "");
	if(!szInfo) { g_gmGvipsWaiting = false; g_gmPhase = 3; g_gmPhaseAt = GetTickCount(); g_gmAstatsIdx = 0; g_gmAstatsWaiting = false; SendGmembersPartial(); return; }

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
			// Oasis: "1. 1337Alex [359] - Standard VIP (+1 points)"
			{
				const char *pDot = strchr(szClean, '.');
				const char *pDash = strstr(szClean, " - ");
				bool bN = pDot && pDot - szClean >= 1 && pDot - szClean <= 3;
				if(bN) { for(const char *qo = szClean; qo < pDot; qo++) if(*qo < '0' || *qo > '9') { bN = false; break; } }
				if(bN && pDash && pDash > pDot)
				{
					char szNm[64] = {0}, szVp[64] = {0};
					const char *pSo = pDot + 1;
					while(*pSo == ' ' || *pSo == '\t') pSo++;
					const char *pEo = pSo;
					while(*pEo && *pEo != ' ' && *pEo != '\t' && *pEo != '[') pEo++;
					size_t nNo = (size_t)(pEo - pSo);
					if(nNo > 0 && nNo < 63) { memcpy(szNm, pSo, nNo); szNm[nNo] = 0; }
					const char *pVo = pDash + 3;
					while(*pVo == ' ') pVo++;
					strncpy_s(szVp, sizeof(szVp), pVo, _TRUNCATE);
					for(int tio = (int)strlen(szVp) - 1; tio >= 0; tio--) { if(szVp[tio] == ' ' || szVp[tio] == '\r') szVp[tio] = 0; else break; }
					if(szNm[0] && szVp[0])
					{
						for(int io = 0; io < g_gmCount; io++)
							if(_stricmp(g_gmRows[io].szName, szNm) == 0)
							{
								strncpy_s(g_gmRows[io].szVip, sizeof(g_gmRows[io].szVip), szVp, sizeof(g_gmRows[io].szVip) - 1);
								Log("[GM] VIP (Oasis): '%s' -> %s", szNm, szVp);
								break;
							}
					}
					if(!pE) break; pL = pE + 1; continue;
				}
			}

			char szLow[256] = {0};
			for(int i = 0; szClean[i] && i < 255; i++) szLow[i] = (char)((szClean[i]>='A'&&szClean[i]<='Z')?szClean[i]+32:szClean[i]);
			if(strstr(szLow, ">>") || strstr(szLow, "next") || strstr(szLow, "sonraki") || strstr(szLow, "\xBB") || strstr(szLow, "\xAB"))
			{ if(!pE) break; pL = pE + 1; continue; }
			if(strstr(szLow, "expire") || (strstr(szLow, "vip") && (strstr(szLow, "name") || strstr(szLow, "isim"))))
			{ if(!pE) break; pL = pE + 1; continue; }

			// Isim + VIP: format "N  Name<TAB>VIP Level<TAB>Expire" (sadece VIP'liler listelenir)
			// - isim: satir numarasi + bas nokta uye listesiyle ayni normalize edilir
			// - vip: "Diamond (14 days)" -> sheet renklendirmesi ilk kelimeye bakar
			char szName[64] = {0};
			char szVip[40] = {0};
			{
				char szCopy2[256]; strncpy_s(szCopy2, sizeof(szCopy2), szClean, sizeof(szCopy2)-1);
				char *ctx2=NULL; char *tok2=strtok_s(szCopy2, "\t", &ctx2);
				char *cols2[8]={0}; int n2=0; while(tok2 && n2<8){ cols2[n2++]=tok2; tok2=strtok_s(NULL, "\t", &ctx2); }
				if(n2 >= 2){
					bool found=false;
					for(int c=0;c<n2 && !found;c++){
						char *pC = cols2[c];
						while(*pC==' '||*pC=='\t'||*pC=='\r') pC++;
						while(*pC>='0' && *pC<='9') pC++;
						while(*pC=='.'||*pC==':'||*pC=='-'||*pC==' '||*pC=='\t') pC++;
						char cand[64]={0}; int ci=0;
						while(pC[ci] && pC[ci]!=' ' && pC[ci]!='\t' && pC[ci]!='\r' && ci<63){ cand[ci]=pC[ci]; ci++; } cand[ci]=0;
						if(!cand[0]) continue;
						for(int gi=0; gi<g_gmCount; gi++){
							if(_stricmp(g_gmRows[gi].szName, cand)==0){ strncpy_s(szName, sizeof(szName), g_gmRows[gi].szName, sizeof(szName)-1); found=true; break; }
						}
					}
					if(!found){
						char *pC = cols2[0];
						while(*pC==' '||*pC=='\t'||*pC=='\r') pC++;
						while(*pC>='0' && *pC<='9') pC++;
						while(*pC=='.'||*pC==':'||*pC=='-'||*pC==' '||*pC=='\t') pC++;
						int kn=0; while(pC[kn] && pC[kn]!=' ' && pC[kn]!='\t' && pC[kn]!='\r' && kn<63){ szName[kn]=pC[kn]; kn++; } szName[kn]=0;
					}
					char szLevel[32]={0}, szExp[32]={0};
					if(n2 >= 3){ strncpy_s(szLevel, sizeof(szLevel), cols2[n2-2], sizeof(szLevel)-1); strncpy_s(szExp, sizeof(szExp), cols2[n2-1], sizeof(szExp)-1); }
					else { strncpy_s(szLevel, sizeof(szLevel), cols2[n2-1], sizeof(szLevel)-1); }
					{ char *s=szLevel; while(*s==' '||*s=='\t'||*s=='\r') s++; char *e=s+strlen(s)-1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0; if(s!=szLevel) memmove(szLevel,s,strlen(s)+1); }
					{ char *s=szExp; while(*s==' '||*s=='\t'||*s=='\r') s++; char *e=s+strlen(s)-1; while(e>=s && (*e==' '||*e=='\t'||*e=='\r')) *e--=0; if(s!=szExp) memmove(szExp,s,strlen(s)+1); }
					{ char lLow[32]={0}; for(int i=0; szLevel[i]&&i<31;i++) lLow[i]=(szLevel[i]>='A'&&szLevel[i]<='Z')?szLevel[i]+32:szLevel[i];
						if(strncmp(lLow, "vip ", 4)==0){ memmove(szLevel, szLevel+4, strlen(szLevel+4)+1); for(int i=0; szLevel[i]&&i<31;i++) lLow[i]=(szLevel[i]>='A'&&szLevel[i]<='Z')?szLevel[i]+32:szLevel[i]; }
						const char *known[]={"diamond","gold","silver","platinum","bronze"}; const char *knownOrig[]={"Diamond","Gold","Silver","Platinum","Bronze"};
						for(int v=0;v<5;v++) if(strstr(lLow, known[v])){ strncpy_s(szLevel, sizeof(szLevel), knownOrig[v], sizeof(szLevel)-1); break; } }
					if(szLevel[0] && szExp[0]) sprintf_s(szVip, sizeof(szVip), "%s (%s)", szLevel, szExp);
					else if(szLevel[0]) strncpy_s(szVip, sizeof(szVip), szLevel, sizeof(szVip)-1);
				} else {
					int k = 0;
					while(szClean[k] && szClean[k] != ' ' && szClean[k] != '\t' && szClean[k] != '-' && k < 63) { szName[k] = szClean[k]; k++; }
					szName[k] = 0;
					const char *szVips[] = { "diamond", "gold", "silver", "platinum", "bronze" };
					for(int v = 0; v < 5; v++)
					{
						const char *pV = strstr(szLow, szVips[v]);
						if(pV) { szVip[0] = (szVips[v][0]>='a'&&szVips[v][0]<='z')?szVips[v][0]-32:szVips[v][0]; strncpy_s(szVip + 1, sizeof(szVip) - 1, szVips[v] + 1, sizeof(szVip) - 2); break; }
					}
				}
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
	g_gmPhase = 3;
	g_gmPhaseAt = GetTickCount();
	g_gmAstatsIdx = 0;
	g_gmAstatsWaiting = false;
	SendGmembersPartial();
	Log("[GM] /gvips tamam, faz 3: /astats basliyor.");
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
	j["requestId"] = g_gmRequestId;
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
			Log("[GM] Google Sheets gonderimi: %s (%d uye).", bOk ? "BASARILI" : "HATA", g_gmCount);
			if(!bOk) Log("[GM] Backend cevabi: %.200s", strResp.c_str());
		}
		else
			Log("[GM] Backend'e ulasilamadi (status %d).", res ? res->status : -1);
	}
	catch(...) { Log("[GM] Gonderim hatasi."); }
	g_gmPhase = 0;
	Log("[GM] Koleksiyon tamam (%d uye).", g_gmCount);
}

// Ana donguden (main.cpp) her iterasyonda cagrilir.
void SendGmembersPartial()
{
	try
	{
		json j;
		j["hwid"] = g_myHWID;
		j["playerName"] = g_szNickName ? g_szNickName : "";
		j["group"] = g_myGroupName;
	j["requestId"] = g_gmRequestId;
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
		cli.Post("/api/gmembers/sheet", j.dump(), "application/json");
	}
	catch(...){}
}

void ProcessGmembers()
{
	if(g_gmPhase == 0) return;
	DWORD dwNow = GetTickCount();

	if(g_gmPhase == 1)
	{
		if(dwNow - g_gmPhaseAt > 90000)
		{
			Log("[GM] Uye listesi zaman asimi (sayfa %d, %d uye) - devam ediliyor.", g_gmPage, g_gmCount);
			g_gmPhase = 2; g_gmGvipsWaiting = true; g_gmPhaseAt = dwNow;
			SendGmembersPartial();
			sendServerCommand((char *)"/gvips");
		}
		return; // sayfalar SAMPRPC'de islenir
	}

	if(g_gmPhase == 2)
	{
		// gvips sonrasi tabloya yaz, sonra astats
		if(dwNow - g_gmPhaseAt > 30000)
		{
			Log("[GM] /gvips zaman asimi.");
			g_gmPhase = 3; g_gmAstatsIdx = 0; g_gmAstatsWaiting = false; g_gmPhaseAt = dwNow;
			SendGmembersPartial();
			return;
		}
		return; // dialog SAMPRPC'de islenir (OnGmVipsResult -> faz 3)
	}

	if(g_gmPhase == 3)
	{
		if(!g_gmPrefilled){
			g_gmPrefilled = true;
			int nPre = 0;
			for(int i = 0; i < g_gmCount; i++){
				if(g_gmRows[i].szAccount[0]) continue;
				char nLow[64]={0}; for(int k=0; g_gmRows[i].szName[k]&&k<63;k++){ char cc=g_gmRows[i].szName[k]; nLow[k]=(cc>='A'&&cc<='Z')?cc+32:cc; }
				std::map<std::string,std::string>::iterator it = g_gmKnownAcc.find(nLow);
				if(it != g_gmKnownAcc.end()){ strncpy_s(g_gmRows[i].szAccount, sizeof(g_gmRows[i].szAccount), it->second.c_str(), sizeof(g_gmRows[i].szAccount)-1); nPre++; }
			}
			if(nPre){ Log("[GM] %d uye tabloda vardi, astats atlandi.", nPre); SendGmembersPartial(); }
		}
		while(g_gmAstatsIdx < g_gmCount && g_gmRows[g_gmAstatsIdx].szAccount[0]){
			Log("[GM] astats atlandi (tabloda var): %s", g_gmRows[g_gmAstatsIdx].szName);
			g_gmAstatsIdx++;
		}
		// tum uyeler bitti mi? -> gonderim
		if(g_gmAstatsIdx >= g_gmCount && !g_gmAstatsWaiting)
		{
			Log("[GM] /astats tamam (%d uye), faz 4: gonderim.", g_gmCount);
			SendGmembersPartial();
			g_gmPhase = 4; g_gmPhaseAt = dwNow;
			OnGmSend();
			return;
		}
		if(!g_gmAstatsWaiting && g_gmAstatsIdx < g_gmCount && dwNow - g_gmPhaseAt >= 3000)
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

	if(g_gmPhase == 4)
	{
		if(!g_gmSendDone) OnGmSend(); // gvips'ten faz 4'e gecildiyse gonderimi baslat
		if(dwNow - g_gmPhaseAt > 30000)
		{
			Log("[GM] Gonderim zaman asimi.");
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

// ?zgrp harvest durumu: ?zf (ve ?zgo) tag eksikse otomatik /groups harvest baslatir;
bool  g_ghpActive = false;                     // harvest devam ediyor mu (SAMPRPC extern)
static bool g_forceZscanAfterHarvest = false; // harvest bitti -> yeni veri varsa zone scan tetikle (zscan off olsa bile)
static bool  g_ghpWaitingGstats = false;     // /gstats sonucu bekleniyor mu
// /grole / /grank paginated otomasyonu (Discord -> /grole listesinde ara + rol sec)
bool g_groleActive = false;
std::string g_groleTarget;
std::string g_groleRole;
int  g_grolePhase = 0;            // 1=uye listesi, 2=rol secimi
static bool  g_ginvPending = false;   // /ginvite sunucu cevabi bekleniyor
static DWORD g_ginvAt = 0;
unsigned long g_groleStartTime = 0;
int  g_grolePage = 0;
static bool  g_groleDirectPending = false; // once direkt "?grole <isim> <rol>" denendi, onay bekleniyor
static DWORD g_groleDirectAt = 0;          // direkt onay icin son tarih (4 sn sonra dialog otomasyonuna gec)
// Ortak uye-tiklama adaylari (grole + grank): uL bot yapimcisinin ipucu â€” liste
// formatina gore sunucunun hangi buton/item kabul ettigi bilinmiyor. Adaylar
// 1.5 sn arayla sirali denenir; faz-2 diyalog basliginda hedef adi dogrulanir.
int   g_grTargetItem = -1;
int   g_grRawRow = -1;        // hedefin ham satir indeksi (baslik+bos satirlar dahil)
char  g_grRowText[128] = {0}; // hedef satirin ILK SUTUNU (renksiz) â€” TABLIST inputtext olarak gonderilir
unsigned short g_grDlgId = 0;
static int   g_grClickAttempt = -1;
int   g_grLastSentItem = -1; // faz-2 submit icin son tiklanan item
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
bool g_gkickActive = false;
std::string g_gkickTarget;
std::string g_gkickReason;
int  g_gkickPhase = 0;            // 1=uye listesi, 2=sebep gonderimi
unsigned long g_gkickStartTime = 0;
int  g_gkickPage = 0;
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
// /gkick list fetch (uye listesi; sebep secimi ayri modal)
bool g_gkickListActive = false;
std::string g_gkickListRequesterId;
std::string g_gkickListGuildId;
std::string g_gkickListHwid;
std::vector<std::string> g_gkickListMembers;
int g_gkickListPage = 0;
unsigned long g_gkickListStartTime = 0;
 // UIF komut mute riski: /gstats art arda hizli giderse sunucu komutlari susturur.
// Adimlar arasina 3 sn tempolu bekleme koyulur; sonuc aninda degil deadline sonrasi
// siradaki komut gonderilir. Kilit cozum de (ProcessZoneScan) ayni deadline'i bekler.
#define GHP_STEP_DELAY_MS 5000
static int   g_ghpCount = 0;
static int   g_ghpIdx = 0;              // siradaki /gstats cagrisi
static DWORD g_ghpNextStepAt = 0;            // siradaki /gstats ne zaman gonderilebilir
static int   g_ghpStaleRetries = 0;          // ayni adimin tekrar deneme sayisi
static void ProcessGhpHarvestStep();         // ?zgrp harvest zincirini surdur (ProcessZoneScan kilidinden)
#define GHP_AUTO_PERIOD_MS 900000    // oto ?zgrp periyodu: 15 dk'da bir eksik tag toplama
static DWORD g_ghpLastRun = 0;            // son oto harvest baslangici
static bool g_ghpForce = false; // ?zgrp on: tam tarama zorla (tek seferlik)
static int   g_ghpLearnedCount = 0;       // bu zincirde kac tag ogrenildi (spam'siz bildirim icin)
static bool  g_zgrpAuto = false;          // ?zgrp on/off: oto harvest â€” DEFAULT KAPALI (elle ?zgrp/?tagscan ile calisir)

void ProcessZfFlashSummary()
{
	if(!g_zfFlashPendingFlag) return;
	if(GetTickCount() < g_zfFlashAt) return;
	g_zfFlashPendingFlag = false;
	g_zfFlashAt = 0;
	Log("[ZF] /gzinfo sonrasi 1 sn gecti, flashing zone dagilimi gonderiliyor.");
	BuildFlashZoneSummary();
}

// ?zf: flashing zone'lari grup tag'larina gore sayip gruba bildirir.

// /gmembers SAYFALI dialog: uye sayisina gore sayfa sayisi degisir. Bot+kullanici
// bulunana kadar sayfalar taranir (max 8); "Sonraki/Next" satiri varsa ona tiklanir,
// yoksa ikincil buton ile sayfa cevrilir.
#define GM_MAX_PAGES 8
static int  s_iGmPage = 1;
static bool s_bGmBotOk = false;
static bool s_bGmUserOk = false;
static char s_sGmBotRole[32] = {0};
static char s_sGmUserRole[32] = {0};
void GmembersReset() { s_iGmPage = 1; s_bGmBotOk = false; s_bGmUserOk = false; s_sGmBotRole[0] = 0; s_sGmUserRole[0] = 0; }

// Oto zone taramasi: flashing zone'lara isinlanip /gzinfo'dan grup adini ogrenir
// (zgo mantigi). Sonuc group_colors.txt'ye yazilir; ?zf etiketleri ve
// "serbest zone" tespiti bu haritadan gelir.
#define ZONE_SCAN_PER_SAMPLE_MS 7000 // Oasis cevabi yavas: 4sn yetmiyordu  // /gzinfo cevabi icin bekleme (mute korumali)
#define ZONE_SCAN_TELEPORT_SYNC_MS 3500 // /pos sonrasi teleport tamamlanana kadar bekle (stale /gzinfo + yanlis isim onlenir)
#define ZONE_SCAN_PERIOD_MS      900000 // oto tarama periyodu: yeni grup adlari icin 15 dk'da bir
static int   g_zoneScanQueue[MAX_GANG_ZONES] = {0};
static int   g_zoneScanQ = 0;          // kuyruktaki ornek zone sayisi
static int   g_zoneScanIdx = -1;       // -1: tarama yok; 0..: siradaki ornek
static bool  g_zscanPausedForSnip = false; // sniper suresince tarama duraklatildi mi
// Kullanici istegi: ?zsnip sonrasi tarama OTOMATIK devam etmesin.
// ?zscan on/sn ile bilerek baslatilir; ?zsnip off / ?no bu bayragi temizler.
static bool  g_zsnipeHoldScan = false;
static int   g_zscanPausedIdx = -1;        // duraklatilan ornek (devam icin)
static bool  g_forceFullVerifyOnce = false; // ?zscan sn: tek seferlik tam kontrol istegi
// Sniper suresince zone taramasi DURAKLATILIR: taramanin /pos isinlamalari
// sniperin /pos + /gz akisiyla cakisip yanlis zonea saldirtiyordu (GeMs vakasi).
// Bitince kalinan ornekten devam eder.
static void PauseZoneScanForSnip()
{
	// Kullanici istegi: sniper aktifken harvest de dursun (sadece sniper odak).
	if(g_ghpActive)
	{
		g_ghpActive = false;
		Log("[ZSNIP] Harvest durduruldu (sadece sniper odak; oto ?zgrp acik kalir).");
	}
	if(g_gmPhase != 0)
	{
		g_gmPhase = 0;
		g_gmAstatsWaiting = false;
		g_gmGvipsWaiting = false;
		Log("[ZSNIP] Gmembers harvest durduruldu (sadece sniper odak).");
	}
	// Sniper bitince tarama otomatik baslamasin (kullanici ?zscan on/sn yazana kadar).
	g_zsnipeHoldScan = true;
	if(g_zscanPausedForSnip) return;
	g_zscanPausedForSnip = true;
	if(g_zoneScanIdx < 0)
	{
		Log("[ZSNIP] Zone taramasi sniper suresince duraklatildi (aktif tarama yok).");
		return;
	}
	g_zscanPausedIdx = g_zoneScanIdx;
	g_zoneScanIdx = -1;
	g_zscanPausedForSnip = true;
	Log("[ZSNIP] Aktif zone taramasi duraklatildi (sniper bitince devam etmeyecek).");
}
// Dialoglu komutlar (?grole/?grank/?gkick): harvestleri durdur + acik dialogu
// kapat; bitince harvestler kalinan yerden OTOMATIK devam eder.
static void CloseOpenListDlg(); // tanim asagida
static bool g_harvestPausedForDlg = false;
static void PauseHarvestsForDialog(const char *szWho)
{
	if(g_ghpActive)
	{
		g_ghpActive = false;
		Log("[DLGCMD] Harvest durduruldu (%s).", szWho);
	}
	if(g_gmPhase != 0)
	{
		g_gmPhase = 0;
		g_gmAstatsWaiting = false;
		g_gmGvipsWaiting = false;
		Log("[DLGCMD] Gmembers harvest durduruldu (%s).", szWho);
	}
	if(!g_harvestPausedForDlg && g_zoneScanIdx >= 0 && !g_zscanPausedForSnip)
	{
		g_harvestPausedForDlg = true;
		g_zscanPausedIdx = g_zoneScanIdx;
		g_zoneScanIdx = -1;
		Log("[DLGCMD] Zone taramasi duraklatildi (%s).", szWho);
	}
	CloseOpenListDlg();
}
static void ResumeHarvestsAfterDialog(const char *szWho)
{
	if(!g_harvestPausedForDlg) return;
	g_harvestPausedForDlg = false;
	if(g_zscanPausedForSnip || g_zsnipeHoldScan || g_ztkActive)
		{ Log("[DLGCMD] Tarama snip nedeniyle duraklatilmis, devam edilmiyor (%s).", szWho); return; }
	if(g_zoneScanIdx < 0 && g_zscanPausedIdx >= 0 && g_zscanPausedIdx < MAX_GANG_ZONES)
	{
		g_zoneScanIdx = g_zscanPausedIdx;
		Log("[DLGCMD] Zone taramasi kalinan yerden devam ediyor (%s).", szWho);
	}
	g_zscanPausedIdx = -1;
}
static void ResumeZoneScanAfterSnip()
{
	if(!g_zscanPausedForSnip) return;
	if(g_ztkActive) return;
	// Kullanici istegi: ?zsnip sonrasi tarama OTOMATIK devam ETMEZ; ?zscan on/sn gerekir.
	if(g_zsnipeHoldScan)
	{
		Log("[ZSNIP] Tarama duraklatilmis birakildi (devam icin ?zscan on).");
		return;
	}
	g_zscanPausedForSnip = false;
	if(g_zoneScanIdx < 0 && g_zscanPausedIdx >= 0 && g_zscanPausedIdx < MAX_GANG_ZONES)
	{
		g_zoneScanIdx = g_zscanPausedIdx;
		Log("[ZSNIP] Zone taramasi kalinan yerden devam ediyor.");
	}
	g_zscanPausedIdx = -1;
}

static DWORD g_zoneScanDeadline = 0;   // ornek icin zaman asimi
static bool  g_zoneScanWaitingTeleport = false; // /pos gonderildi, /gzinfo henuz gonderilmedi
static float g_zoneScanReturnPos[3] = {0, 0, 0};
// Dialog bekcisi: 30 sn yanitsiz kalan dialogu stile gore kapatir. Acik dialog
// /pos ve /gzinfo komutlarini reddettirir ("dialog open"). Otomasyonlar bazen
// dialogu kapatmadan birakiyor (timeout/iptal); bekci hepsini toplar.
static int   g_watchDlgId = -1;
static DWORD g_watchDlgAt = 0;
static int   g_watchDlgStyle = 0;
static bool  g_watchDlgKeep = false;
static DWORD g_lastPosOkTick = 0;
static DWORD g_firstRejectTick = 0; // ilk "dialog open" red zamani (surekli red = kilit)
static void SendListClose(int iDlgId); // tanim asagida
// Acik liste dialogunu HEMEN kapat (grole sonu/timeout/ret). Bekci 30sn bekler,
// o arada /pos reddedilir. Sadece liste stilleri + son 15sn icinde gorulmusse.
static void CloseOpenListDlg()
{
	if(g_watchDlgId < 0 || g_watchDlgKeep) return;
	if(GetTickCount() - g_watchDlgAt > 15000) return;
	if(g_watchDlgStyle == DIALOG_STYLE_MSGBOX)
	{
		sendDialogResponse((WORD)g_watchDlgId, 1, 0xFFFF, (char *)"");
		Log("[DLG] Acik dialog kapatildi (id=%d).", g_watchDlgId);
		g_watchDlgId = -1;
		return;
	}
	if(g_watchDlgStyle != DIALOG_STYLE_LIST && g_watchDlgStyle != DIALOG_STYLE_TABLIST && g_watchDlgStyle != DIALOG_STYLE_TABLIST_HEADERS) return;
	SendListClose(g_watchDlgId);
	Log("[DLG] Acik liste kapatildi (id=%d).", g_watchDlgId);
	g_watchDlgId = -1;
}
static int   g_watchCloseMode = 0;
static char  g_watchDlgRow[256] = {0};
static void SendListSelectRaw(int iDlgId, int iItem, const char *szRow)
{
	RakNet::BitStream bsE;
	bsE.Write((uint16_t)iDlgId);
	bsE.Write((uint8_t)1);
	bsE.Write((uint16_t)iItem);
	size_t nL = szRow ? strlen(szRow) : 0;
	if(nL > 200) nL = 200;
	bsE.Write((uint8_t)nL);
	if(nL) bsE.Write(szRow, nL);
	pRakClient->RPC(&RPC_DialogResponse, &bsE, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
}
// Liste kapatma: Cancel + satir 0 + BYTE (DENEY2, calisan yol).
// Liste kapatma: Cancel tiklamasi formati (buton 0 + satir 0 + WORD len).
// ESC (secimsiz) yutuluyor; Cancel (satirli) kapaniyor.
static void SendListClose(int iDlgId)
{
	sendDialogResponse((WORD)iDlgId, 0, 0, (char *)"");
}
// ?follow (whisper): hedefi /pos ile takip. Hedef streamden duserse
// (freeroamdan cikmis/gamemode/offline) 5 tur sonra otomatik biter.
static const char *LM(const char *tr, const char *en); // tanim asagida (dil secimi)
static bool  g_followActive = false;
static char  g_followName[24] = {0};
static int   g_followSlot = -1;
static int   g_followMiss = 0;
static DWORD g_followLastTick = 0;
static float g_followLastX = 0.0f, g_followLastY = 0.0f, g_followLastZ = 0.0f;
#define FOLLOW_TICK_MS 3000
#define FOLLOW_MAX_MISS 5
static void FollowWhisperReply(const char *szMsg)
{
	char szW[160];
	sprintf_s(szW, sizeof(szW), "~ %s", szMsg);
	g_bridgeSuppressEcho++;
	sendChat(szW);
	Log("[FOLLOW] whisper cevabi: %s", szMsg);
}
void ProcessFollowTick()
{
	if(!g_followActive) return;
	DWORD dwNow = GetTickCount();
	if(dwNow - g_followLastTick < FOLLOW_TICK_MS) return;
	g_followLastTick = dwNow;
	int iSlot = g_followSlot;
	if(iSlot < 0 || iSlot >= MAX_PLAYERS || !playerInfo[iSlot].szPlayerName[0]
		|| _stricmp(playerInfo[iSlot].szPlayerName, g_followName) != 0)
	{
		iSlot = -1;
		for(int s = 0; s < MAX_PLAYERS; s++)
			if(playerInfo[s].szPlayerName[0] && _stricmp(playerInfo[s].szPlayerName, g_followName) == 0) { iSlot = s; break; }
		if(iSlot >= 0) { g_followSlot = iSlot; Log("[FOLLOW] slot guncellendi: %s -> %d.", g_followName, iSlot); }
	}
	if(iSlot < 0 || !playerInfo[iSlot].iIsStreamedIn)
	{
		if(++g_followMiss >= FOLLOW_MAX_MISS)
		{
			g_followActive = false;
			FollowWhisperReply(LM("hedef freeroamda degil, takip bitti.", "Target left freeroam, stopped following."));
			Log("[FOLLOW] %s streamde yok (%d tur), takip bitti.", g_followName, g_followMiss);
		}
		return;
	}
	g_followMiss = 0;
	float *pP = playerInfo[iSlot].onfootData.vecPos;
	if(playerInfo[iSlot].iAreWeInAVehicle && (playerInfo[iSlot].incarData.vecPos[0] != 0.0f || playerInfo[iSlot].incarData.vecPos[1] != 0.0f))
		pP = playerInfo[iSlot].incarData.vecPos;
	if(pP[0] == 0.0f && pP[1] == 0.0f) { g_followMiss++; return; } // konum henuz yok
	float fDX = pP[0] - g_followLastX, fDY = pP[1] - g_followLastY, fDZ = pP[2] - g_followLastZ;
	if((g_followLastX != 0.0f || g_followLastY != 0.0f) && (fDX*fDX + fDY*fDY + fDZ*fDZ) < 4.0f) return; // hedef oynamadi, yerinde dur
	g_followLastX = pP[0]; g_followLastY = pP[1]; g_followLastZ = pP[2];
	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", pP[0] + 2.5f, pP[1], pP[2]); // yaninda, yerde (havada degil)
	sendServerCommand(szCmd);
}
static bool  g_zoneScanDone = false;   // oturumda bir kez
static DWORD g_zoneScanWaitStart = 0;  // oto-baslatma sayaci (spawn'dan 30 sn sonra)
static DWORD g_zoneScanLastRun = 0;    // son oto tarama zamani (periyod kontrolu)
bool g_loginPending = false;
static bool g_bLoggedIn = false; // oto giris mesaji goruldu (harvest/scan bunu bekler)           // UIF Login dialogu acik (SAMPRPC extern); kullanici sifre girene
                                       // kadar spawn/akislar bekler, tmp hesaba atilmaz.
unsigned long g_loginPendingUntil = 0; // login bekleme timeout (10 sn) - dialog hic gelmezse devam et
static bool  g_zscanAuto = false;      // ?zscan on/off: otomatik aralikli tarama acik mi
// Komut mutesi (".?zscan sirasinda sunucu /pos//gzinfo mute'lar"): mute textdraw'i
// g?rununce bir kez reconnect tetikle (ayný mute penceresinde tekrar etmemesi icin bayrak).
static bool  g_muteReconnectPending = false;
// Oyuncu imzasi (id): "Isim(12): mesaj" kalibi - griefer korumasi icin
static bool HasPlayerTag(const char *s){
	for(const char *p = s; p && *p; p++){
		if(*p == '('){
			const char *q = p + 1; int nd = 0;
			while(*q >= '0' && *q <= '9'){ q++; nd++; }
			if(nd > 0 && nd <= 4 && *q == ')'){
				const char *r = q + 1;
				while(*r == ' ') r++;
				if(*r == ':') return true;
			}
		}
	}
	return false;
}
// Mute gorunce (cmd + chat) aninda reconnect: mute server-side kalkar, is devam eder.
// SADECE aktif is varken (scan/harvest/gmembers) + pencerede bir kez ateslenir.
static void TriggerMuteReconnect(const char *szText, const char *src)
{
	if(!szText || !szText[0] || !src) return;
	if(g_muteReconnectPending) return;
	if(!iSpawned) return;
	if(g_zoneScanIdx < 0 && !g_ghpActive && g_gmPhase == 0) return;
	if(!strstr(szText, "MUTED")) return;
	g_muteReconnectPending = true;
	if(g_gmPhase != 0){
		Log("[GM] Mute - gmembers harvest yeniden baslatilacak (reconnect sonrasi).");
		g_gmPhase = 1; g_gmCount = 0; g_gmPage = 0; g_gmAstatsIdx = 0; g_gmAstatsWaiting = false; g_gmGvipsWaiting = false; g_gmSendDone = false;
	}
	Log("[MUTE] Mute yakalandi [%s] (%.80s) - hemen yeniden baglaniliyor.", src, szText);
	resetPools(1, 0);
}

// Freeroam'a gecis: /fr'yi oturum basina EN FAZLA bir kez gonder (cmd mute riski yok).
// Birincil tetik: spawn menusu "Freeroam" cevabi (ScrDialogBox). Yedek tetik: spawn'dan
// 10 sn sonra tek seferlik zaman tabanli gonderim (menu hic gelmezse / RPC_Spawn gelmezse).
// SpawnConfirmed (RPC_Spawn) de /fr gonderebilir; hepsi g_frSentOnce ile korunur.
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
		Log("[SPAWN] Spawn menusu gelmedi/zaman asimi, tek seferlik /fr gonderimi.");
		sendServerCommand((char *)"/fr");
	}
}

// Spawn menusu ("Where would you like to go?") son otomatik secilme zamani.
static DWORD g_dwSpawnMenuHandled = 0;

// -------------------------------------------------------------
// ?zf GRUP TAG HARITASI (isim -> grup tag'i)
// Eslesme YALNIZCA ISIM + TAG ile: zone sahibi /gzinfo'dan ogrenilen ISIM,
// tag /groups+/gstats harvest'iyla ogrenilir; group_colors.txt'ye kalici yazilir,
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
	// 2) Tag bulunamazsa TAM ISIM ile dene (kullanici tag yerine isim yazabilir):
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
// zone'u doner. Grubun hic zonu yoksa ya da hepsi savastaysa -1.
// "Free" -> sahipsiz (sahibi bilinmeyen) ilk zon.
// TAG CAKISMASI: ayni kisa tag'i birden fazla grup kullaniyorsa (sunucu gercegi,
// orn. 'dEV' -> The Devils Organizations + The Devilz Dead Industries) rastgele
// secim yanlis zon gonderir. Bu yuzden cakisma varsa -2 doner; cagiran yer
// isimle netlestirme ister (??zgo <tam isim> kullan).
static int FindZoneByGroupTag(const char *szTag)
{
	if(!szTag || !szTag[0]) return -1;
	if(!_stricmp(szTag, "Free"))
	{
		// SADECE taramada gercekten sahipsiz ogrenilmis zone'lar ("Free" isaretli).
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
		if(g_zoneFlash[zid]) return zid;   // flashlanan zone oncelikli
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
	// Kullanici: text dosyasina kayit YOK - DB tek kaynak. (LoadGroupsFromDb
	// ProcessDbSync uzerinden acilista DB'den geri yukler; bu fonksiyon bos.)
	g_groupColorCount = 0;
	Log("[ZF] Text dosyasi yuklemesi kaldirildi (DB tek kaynak).");
}

// Bugunun tarihi YYYYMMDD sayisi (gunluk zone verisi sifirlama icin).
static int CurrentDayStamp()
{
	time_t t = time(nullptr);
	struct tm lt;
	localtime_s(&lt, &t);
	return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

// baglanti basinda cagrilir (misc_funcs.sampConnect): yeniden ogrenme mute riski
void InitGroupColorsFromDisk()
{
	LoadGroupColors();
	// Kendi grup adi kalici kayit DOSYADA DEGIL - DB'de (bots.group_name).
	// SyncZscanFromDb acilista DB'den okur, yoksa /stats ile ogrenip yazar.
	// Zone->sahip haritasi ARTIK DB'de (gangs.zones JSON): acilista LoadGroupsFromDb
	// geri yükler (ProcessDbSync tetikler). zones.txt KALDIRILDI - DB tek dogru kaynak.
	// Yeniden baslatma sonrasi savas/timer restore: savas bitis ani (sistem tick)
	// dosyada. GetTickCount() sistem uptime'i oldugu icin restart sonrasi da gecerlidir;
	// bitis ani gecmediyse savas hala suruyor demektir -> kalan sure restore edilir
	// (geri sayim duyurusu + kill list girisii bu sureye baglidir).
	// war_timer.txt KALDIRILDI (kullanici: dosya yok). Savas durumu sadece hafizada;
	// yeniden baslatmada savas bilgisi restore edilmez, taze oturum baslar.
}

static void SaveGroupColors()
{
	// Kullanici: text dosyasina kayit YOK - DB tek kaynak. Veri ProcessDbSync'in
	// /api/groups push'uyla (60 sn'de bir) DB'ye yazilir.
	Log("[ZF] Text dosyasi kaydi kaldirildi (DB tek kaynak, %d grup).", g_groupColorCount);
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
static int g_lastGlogType = 0; // 1=rol degisikligi, 2=rank degisikligi, 3=kick
// ?post 30 sn gerekce penceresi: ?post yazinca beklenir, grup mesaji gerekce olur.
static bool  g_pushPending = false;
static DWORD g_pushUntil = 0;
static char  g_pushReason[256] = {0};
static int   g_pushRequester = -1;
// /pm: Discord'dan hedef oyuncuya mesaj + yanit takibi (yanit gelince Discord'a iletilir).
static bool  g_pmPending = false;
static char  g_pmTarget[64] = {0};    // hedef (oyuncu adi veya ID - kullan?c? yazdigi gibi)
static char  g_pmName[64] = {0};      // cozulen oyuncu ADI (yanit isimle eslesir)
static int   g_pmId = -1;             // cozulen oyuncu ID (yanit (id) ile eslesir)
static char  g_pmRequesterId[64] = {0};
static DWORD g_pmUntil = 0;
// GUVENLIK (/changepass / /email): PM'den deger alinir, /changepass gonderilir,
// UIF input diyalogu acar; SAMPRPC diyaloga degeri yazip submit eder.
bool  g_secCmdPending = false;
bool  g_secCmdIsEmail = false;
char  g_secCmdValue[128] = {0};
char  g_secCmdValue2[128] = {0}; // Oasis changepass D2 (yeni sifre)
DWORD g_secCmdAt = 0;
int   g_secCmdCount = 0; // kac diyalog dolduruldu (max 2: yeni + onay/tekrar)

// ASCII token'i kopyala (src'ten baslar, tokenden once durur).
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

// Admin eylem mesajini DB'ye POST et: "ADMIN: gandalf banned Bowtac(163) from CNR Mode..."
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

// Grup rol/rank/kick/invite degisikligi mesajini DB'ye POST et + son kaydi sakla (?post icin).
// Otomatik sebep penceresi: botun kendisi yapmadiysa gruba sebep sorulur (30 sn),
// ?post yazmaya gerek kalmaz. Bot aksiyonlarinda (Discord otomasyonu) sessiz kalinir.
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
	// ponytail: botun kendi otomasyonu (actor==bot nick) sebepli/sebepsiz bilerek yapildi, sorma
	if(g_szNickName[0] && !_stricmp(actor, g_szNickName)) return;
	g_pushPending = true;
	g_pushUntil = GetTickCount() + 30000;
	g_pushReason[0] = 0;
	g_pushRequester = -1;
	for(int s = 0; s < MAX_PLAYERS; s++)
		if(playerInfo[s].szPlayerName[0] && !_stricmp(playerInfo[s].szPlayerName, actor)) { g_pushRequester = s; break; }
	g_bridgeSuppressEcho++;
	// ponytail: LM() asagida tanimli (satir ~2626), burada g_langTurkish dogrudan
	sendChat((char *)(g_langTurkish ? "!Gerekce yazabilirsin (30 sn)." : "!You can type a reason (30 secs)."));
	Log("[POST] Otomatik sebep penceresi acildi: %s -> %s (tip %d).", actor, target, type);
}

// /pm: Discord'dan hedef oyuncuya mesaj gonder (grup chat'inde gorunur).
// Format: "[Discord][kullanici]: mesaj". Yanit gelince Discord'a iletilir.
void StartPm(const char *target, const char *message, const char *discordUser, const char *requesterId)
{
	if(!target || !target[0] || !message || !message[0]) return;

	// Mesaj icerigi: "[Discord][kullan?c?]: mesaj" - ozel mesaj olarak gider.
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
	Log("[PM] Discord'dan ozel mesaj g?nderildi: hedef '%s' (id %d): %s", target, iTargetId, szOut);

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

// ?post: son yakalanan grup aksiyonunu (rol/rank/kick) gerekceyle Discord'a embed gonder.
static void PushLastGroupLog(const char *reason)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!g_lastGlogTarget[0]) return;
	g_pushPending = false; // tek embed: gonderince pencere kapanir (yoksa 30sn sonunda kopya embed gider)
	g_pushRequester = -1;
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
		{
			Log("[POST] Discord'a gonderildi: %s -> %s (tip %d, gerekce: %s).", g_lastGlogActor, g_lastGlogTarget, g_lastGlogType, (reason && reason[0]) ? reason : "-");
			if(reason && reason[0])
			{
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a gonderildi.", "!Sent to Discord."));
			}
		}
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
	char qOld = '\'';
	if(!pOld) { pOld = strstr(pFrom, "from \""); qOld = '"'; }
	if(!pOld) return;
	pOld += 6;
	const char *pOldEnd = strchr(pOld, qOld);
	if(!pOldEnd) return;
	CopyToken(oldRank, oldLen, pOld, pOldEnd);
	const char *pTo = strstr(pOldEnd, "to '");
	char qTo = '\'';
	if(!pTo) { pTo = strstr(pOldEnd, "to \""); qTo = '"'; }
	if(!pTo) return;
	pTo += 4;
	const char *pToEnd = strchr(pTo, qTo);
	if(pToEnd) CopyToken(newRank, newLen, pTo, pToEnd);
	if(!oldRank[0])
	{
		const char *pFU = strstr(pFrom, "from ");
		if(pFU)
		{
			pFU += 5;
			const char *pTU = strstr(pFU, " to ");
			if(pTU)
			{
				CopyToken(oldRank, oldLen, pFU, pTU);
				const char *pNE = pTU + 4;
				while(*pNE == ' ') pNE++;
				const char *pEE = pNE + strlen(pNE);
				while(pEE > pNE && (pEE[-1] == ' ' || pEE[-1] == '\r' || pEE[-1] == '\n' || pEE[-1] == '.')) pEE--;
				CopyToken(newRank, newLen, pNE, pEE);
			}
		}
	}
}

// "GROUP: (Harm)(3716972) changed the rank of [YS]FamilyRoots(3752380) from 'F' to 'F'"
static void CheckGroupLogMessage(const char *msg)
{
	// Sunucu rol/rank degisikligini reddetti: otomasyonu DURDUR (donguye girme),
	// gruba bildir. ("SYSTEM: You cannot change the Role of this member.")
	if(strstr(msg, "cannot change the Role") || strstr(msg, "cannot change the role")
		|| strstr(msg, "cannot change the Rank") || strstr(msg, "cannot change the rank"))
	{
		if(g_groleActive || g_grankActive)
		{
			const char *pWho = g_groleActive ? g_groleTarget.c_str() : g_grankTarget.c_str();
			Log("[GROLE] Sunucu reddetti (yetki/rutbe), otomasyon durduruldu: %s.", pWho ? pWho : "?");
			g_bridgeSuppressEcho++;
			char szDeny[160];
			sprintf_s(szDeny, sizeof(szDeny), LM("!Rol degisikligi reddedildi (yetkim yetmiyor): %s.", "!Role change denied (insufficient rank): %s."), pWho ? pWho : "?");
			sendChat(szDeny);
			g_groleActive = false; g_grolePhase = 0; g_groleDirectPending = false;
			g_grankActive = false; g_grankPhase = 0;
			g_grRoleDblPending = false;
			CloseOpenListDlg();
		}
		return;
	}
	const char *pChg = strstr(msg, "changed the rank of");
	int iType = 2;
	if(!pChg){ pChg = strstr(msg, "changed the Rank of"); }
	if(!pChg){ pChg = strstr(msg, "changed the role of"); iType = 1; }
	if(!pChg){ pChg = strstr(msg, "changed the Role of"); iType = 1; }
	if(!pChg)
	{
		// invite: "GROUP: X invited/invite Y ..." - kick ile ayni Name(acc) formu
		// ponytail: kick parse'inin kopyasi, mesaj formati netlesince birlesir
		// Sahte eslesme: oyuncu "invite me" yazinca sistem daveti saniliyordu.
		// Sistem mesaji GROUP: on-eklidir; oyuncu sohbeti elenir.
		const char *pI = NULL;
		if(strstr(msg, "GROUP:"))
		{
			pI = strstr(msg, " invited ");
			if(!pI) pI = strstr(msg, " invite ");
			if(!pI) pI = strstr(msg, " invites ");
		}
		if(pI)
		{
			{ const char *pA = msg; while(*pA == ' ' || *pA == '\t') pA++; if(!strncmp(pA, "ADMIN:", 6)) pI = NULL; }
		}
		if(pI)
		{
			char actor[64] = {0}, target[64] = {0};
			long long actorAcc = 0, targetAcc = 0;
			{
				const char *q = strstr(msg, "GROUP:") ? strstr(msg, "GROUP:") + 6 : msg;
				while(*q == ' ' || *q == '\t') q++;
				const char *pAO = strchr(q, '(');
				if(pAO && pAO < pI)
				{
					const char *pAE = pAO;
					while(pAE > q && (pAE[-1] == ' ' || pAE[-1] == '\t')) pAE--;
					CopyToken(actor, sizeof(actor), q, pAE);
					const char *pAC = strchr(pAO + 1, ')');
					if(pAC) actorAcc = _strtoi64(pAO + 1, NULL, 10);
				}
				else
				{
					int n = 0; while(q[n] && q[n] != ' ' && q[n] != '\t' && n < 63){ actor[n] = q[n]; n++; } actor[n] = 0;
				}
			}
			const char *pT = strchr(pI + 1, ' ');
			pT = pT ? pT + 1 : NULL;
			if(pT){ while(*pT == ' ') pT++; if(!strncmp(pT, "the ", 4)) { pT += 4; while(*pT == ' ') pT++; } }
			if(pT && *pT)
			{
				const char *pTO = strchr(pT, '(');
				int tn = 0;
				for(const char *q = pT; *q && *q != ' ' && *q != '\t' && q != pTO && tn < 63; q++) target[tn++] = *q;
				target[tn] = 0;
				const char *pTClose = pTO ? strchr(pTO + 1, ')') : NULL;
				if(pTClose) targetAcc = _strtoi64(pTO + 1, NULL, 10);
			}
			if(actor[0] && target[0])
			{
				Log("[GLOG] Invite yakalandi: %s(%lld) -> %s(%lld)", actor, actorAcc, target, targetAcc);
				SendGroupLog(actor, actorAcc, target, targetAcc, "", "Invited", 4);
			}
			return;
		}
		// kick: "GROUP: (Ad)(acc) kicked Hedef(acc) ..." â€” rol/rank degil, kick yakala
		const char *pK = strstr(msg, "GROUP:") ? strstr(msg, " kicked ") : NULL;
		if(!pK) return;
		{ const char *pA = msg; while(*pA == ' ' || *pA == '\t') pA++; if(!strncmp(pA, "ADMIN:", 6)) return; } // ADMIN kick admin logundur
		char actor[64] = {0}, target[64] = {0};
		long long actorAcc = 0, targetAcc = 0;
		// actor formati "Name(acc)": isim parantez oncesi, acc parantez ici
		{
			const char *q = strstr(msg, "GROUP:") ? strstr(msg, "GROUP:") + 6 : msg;
			while(*q == ' ' || *q == '\t') q++;
			const char *pAO = strchr(q, '(');
			if(pAO && pAO < pK)
			{
				const char *pAE = pAO;
				while(pAE > q && (pAE[-1] == ' ' || pAE[-1] == '\t')) pAE--;
				CopyToken(actor, sizeof(actor), q, pAE);
				const char *pAC = strchr(pAO + 1, ')');
				if(pAC) actorAcc = _strtoi64(pAO + 1, NULL, 10);
			}
			else
			{
				int n = 0; while(q[n] && q[n] != ' ' && q[n] != '\t' && n < 63){ actor[n] = q[n]; n++; } actor[n] = 0;
			}
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
	// yapan: "Name(acc)" tek form veya "(Ad)(acc)" cift form - ikisini de kapsa
	char actor[64] = {0}, target[64] = {0}, oldRank[64] = {0}, newRank[64] = {0};
	long long actorAcc = 0, targetAcc = 0;
	{
		const char *q = msg;
		const char *pGr = strstr(msg, "GROUP:");
		const char *pAd = strstr(msg, "ADMIN:");
		if(pGr && (!pAd || pGr < pAd)) q = pGr + 6;
		else if(pAd) q = pAd + 6;
		while(*q == ' ' || *q == '\t') q++;
		const char *pAO = strchr(q, '(');
		if(pAO && pAO == q)
		{
			const char *pAC = strchr(pAO + 1, ')');
			const char *pA2O = pAC ? strchr(pAC + 1, '(') : NULL;
			const char *pA2C = pA2O ? strchr(pA2O + 1, ')') : NULL;
			if(pAC && pA2O && pA2C){ CopyToken(actor, sizeof(actor), pAO + 1, pAC); actorAcc = _strtoi64(pA2O + 1, NULL, 10); }
		}
		else if(pAO)
		{
			const char *pAE = pAO;
			while(pAE > q && (pAE[-1] == ' ' || pAE[-1] == '\t')) pAE--;
			CopyToken(actor, sizeof(actor), q, pAE);
			const char *pAC = strchr(pAO + 1, ')');
			if(pAC) actorAcc = _strtoi64(pAO + 1, NULL, 10);
		}
	}
	// hedef: "changed the rank of " sonrasi "Ad(accid)" - ad paranteze kadar
	const char *pTgt = pChg + 19; // "changed the rank/role of" dort formda da 19 harf
	while(*pTgt == ' ') pTgt++;
	const char *pTOpen = strchr(pTgt, '(');
	const char *pTBr = strchr(pTgt, '[');
	if(pTBr && (!pTOpen || pTBr < pTOpen)) pTOpen = pTBr; // Oasis "Ad[id]" formu
	if(pTOpen)
	{
		CopyToken(target, sizeof(target), pTgt, pTOpen);
		const char *pTClose = strchr(pTOpen + 1, (*pTOpen == '[') ? ']' : ')');
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
	Log("[GLOG] Rol degisikligi yakalandi: %s(%lld) -> %s(%lld)", actor, actorAcc, target, targetAcc);
	SendGroupLog(actor, actorAcc, target, targetAcc, oldRank, newRank, iType);

	// --- /grole DIREKT onayi: bot "?grole <isim> <rol>" gonderdiyse ve hedef eslesiyorsa ---
	if(g_groleDirectPending && g_szNickName[0] && !_stricmp(actor, g_szNickName) && !_stricmp(target, g_groleTarget.c_str()))
	{
		Log("[GROLE] DIREKT onay alindi: %s rol degisti ('%s' -> '%s'), otomasyon tamam.", target, oldRank, newRank);
		g_groleActive = false;
		g_grolePhase = 0;
		g_groleDirectPending = false;
		g_grTargetItem = -1; g_grDlgId = 0;
	}
	// Menu yolu (dialog otomasyonu) yayin onayi: hedef tutuyorsa basari, gruba bildir.
	if(g_groleActive && target[0] && !_stricmp(target, g_groleTarget.c_str()))
	{
		Log("[GROLE] Onay alindi: %s rol degisti ('%s' -> '%s').", target, oldRank, newRank);
		g_bridgeSuppressEcho++;
		char szOk[192];
		sprintf_s(szOk, sizeof(szOk), LM("!Rol degisti: %s ('%s' -> '%s').", "!Role changed: %s ('%s' -> '%s')."), target, oldRank, newRank);
		sendChat(szOk);
		g_groleActive = false; g_grolePhase = 0; g_grRoleDblPending = false;
		CloseOpenListDlg();
	}
	if(g_grankActive && target[0] && !_stricmp(target, g_grankTarget.c_str()))
	{
		Log("[GRANK] Onay alindi: %s rank degisti ('%s' -> '%s').", target, oldRank, newRank);
		g_bridgeSuppressEcho++;
		char szOk2[192];
		sprintf_s(szOk2, sizeof(szOk2), LM("!Rank degisti: %s ('%s' -> '%s').", "!Rank changed: %s ('%s' -> '%s')."), target, oldRank, newRank);
		sendChat(szOk2);
		g_grankActive = false; g_grankPhase = 0; g_grRoleDblPending = false;
	}
}

// Gecici hesaplar "_tmp + rakam" ile biter (Lucas_Perez_tmp0); bunlarin
// eylemleri DBye yazilmaz (IDleri geri donusumlu, sorgulari kirletir).
static bool IsTempAccount(const char *name)
{
	if(!name || !name[0]) return false;
	size_t n = strlen(name);
	if(n < 5) return false;
	size_t i = n;
	while(i > 0 && name[i-1] >= '0' && name[i-1] <= '9') i--;
	if(i == n || i < 4) return false;
	if(name[i-4] != '_') return false;
	char c1 = name[i-3], c2 = name[i-2], c3 = name[i-1];
	if(c1 != 't' && c1 != 'T') return false;
	if(c2 != 'm' && c2 != 'M') return false;
	if(c3 != 'p' && c3 != 'P') return false;
	return true;
}

// --- account ID ile baslayan admin eylemi: /astats ile isim cozulup
// isim + account ID birlikte DBye yazilir. Cozum yoksa/zaman asiminda
// "account ID N" kaydina dusulur.
struct AlogAccItem
{
	bool bUsed;
	char szAdmin[64];
	long long llAccId;
	char szAction[16];
	char szMode[64];
	int iDur;
	char szReason[128];
};
static AlogAccItem g_alogAccQ[8];
bool g_alogAstatsActive = false;
static int g_alogAstatsIdx = -1;
static DWORD g_alogAstatsReqTime = 0;
static char g_alogAstatsId[24] = {0};
static bool EnqueueAlogAccItem(const char *admin, long long accId, const char *action, const char *mode, int dur, const char *reason)
{
	for(int i = 0; i < 8; i++)
	{
		if(g_alogAccQ[i].bUsed) continue;
		g_alogAccQ[i].bUsed = true;
		strncpy_s(g_alogAccQ[i].szAdmin, sizeof(g_alogAccQ[i].szAdmin), admin ? admin : "", _TRUNCATE);
		g_alogAccQ[i].llAccId = accId;
		strncpy_s(g_alogAccQ[i].szAction, sizeof(g_alogAccQ[i].szAction), action ? action : "", _TRUNCATE);
		strncpy_s(g_alogAccQ[i].szMode, sizeof(g_alogAccQ[i].szMode), mode ? mode : "", _TRUNCATE);
		g_alogAccQ[i].iDur = dur;
		strncpy_s(g_alogAccQ[i].szReason, sizeof(g_alogAccQ[i].szReason), reason ? reason : "", _TRUNCATE);
		Log("[ALOG] account ID %lld cozumu kuyruga alindi (%s %s).", accId, admin ? admin : "?", action ? action : "?");
		return true;
	}
	return false;
}
static void AlogAccFallback(int idx)
{
	if(idx < 0 || idx >= 8 || !g_alogAccQ[idx].bUsed) return;
	char szName[64];
	sprintf_s(szName, sizeof(szName), "account ID %lld", g_alogAccQ[idx].llAccId);
	SendAdminLog(g_alogAccQ[idx].szAdmin, szName, g_alogAccQ[idx].llAccId,
		g_alogAccQ[idx].szAction, g_alogAccQ[idx].szMode, g_alogAccQ[idx].iDur, g_alogAccQ[idx].szReason);
	g_alogAccQ[idx].bUsed = false;
}
static bool AlogContainsCI(const char *str, const char *sub)
{
	if(!str || !sub || !sub[0]) return false;
	size_t nsub = strlen(sub);
	for(const char *p = str; *p; p++)
	{
		size_t k = 0;
		while(k < nsub && p[k])
		{
			char a = p[k], b = sub[k];
			if(a >= 'A' && a <= 'Z') a += 32;
			if(b >= 'A' && b <= 'Z') b += 32;
			if(a != b) break;
			k++;
		}
		if(k == nsub) return true;
	}
	return false;
}
// /astats sonucundan oyuncu adi: "Name:"/"Player:"/"Nick:"/"Isim:" etiketi
// aranir; etiket yoksa false doner (caller "account ID N" ile kaydeder).
static bool ParseAstatsName(const char *szInfo, const char *szTitle, char *outName, size_t outLen)
{
	if(!outName || outLen == 0) return false;
	outName[0] = 0;
	const char *parts[2] = { szInfo, szTitle };
	for(int pi = 0; pi < 2; pi++)
	{
		const char *src = parts[pi];
		if(!src || !src[0]) continue;
		const char *pL = src;
		while(pL && *pL)
		{
			const char *pE = strchr(pL, '\n');
			size_t nL = pE ? (size_t)(pE - pL) : strlen(pL);
			if(nL > 0 && nL < 200)
			{
				char szLine[200], szLow[200];
				memcpy(szLine, pL, nL); szLine[nL] = 0;
				char tmp[200]; int w = 0;
				for(size_t r = 0; szLine[r] && w < 199; )
				{
					if(szLine[r] == '{' && szLine[r+1] && szLine[r+2] && szLine[r+3] && szLine[r+4] && szLine[r+5] && szLine[r+6] && szLine[r+7] == '}') { r += 8; continue; }
					tmp[w++] = szLine[r++];
				}
				tmp[w] = 0;
				memcpy(szLine, tmp, w + 1);
				for(size_t k = 0; szLine[k] && k < 199; k++) szLow[k] = (szLine[k] >= 'A' && szLine[k] <= 'Z') ? szLine[k] + 32 : szLine[k];
				szLow[nL] = 0;
				const char *labels[7] = { "name:", "player:", "nick:", "isim:", "oyuncu:", "account name:", "kullanici:" };
				for(int li = 0; li < 7; li++)
				{
					const char *pF = strstr(szLow, labels[li]);
					if(!pF) continue;
					const char *pV = szLine + (pF - szLow) + strlen(labels[li]);
					while(*pV == ' ' || *pV == '\t') pV++;
					char cand[64]; int ci = 0;
					while(*pV && *pV != ' ' && *pV != '\t' && *pV != '\r' && *pV != '(' && *pV != '[' && ci < 63) { cand[ci++] = *pV++; }
					cand[ci] = 0;
					if(ci >= 2 && !AlogContainsCI(cand, "stats") && !AlogContainsCI(cand, "account") && !AlogContainsCI(cand, "uif"))
					{
						strncpy_s(outName, outLen, cand, outLen - 1);
						return true;
					}
				}
			}
			if(!pE) break;
			pL = pE + 1;
		}
	}
	return false;
}
// /astats sonucu geldi: kuyruktaki account ID eylemini isimle tamamla.
// true = dialog tuketildi (kapatilabilir).
bool OnAlogAstatsResult(const char *szTitle, const char *szInfo)
{
	if(!g_alogAstatsActive || g_alogAstatsIdx < 0 || g_alogAstatsIdx >= 8) return false;
	AlogAccItem *it = &g_alogAccQ[g_alogAstatsIdx];
	if(!it->bUsed) { g_alogAstatsActive = false; return false; }
	char szId[24];
	sprintf_s(szId, sizeof(szId), "%lld", it->llAccId);
	Log("[ALOG-ASTATS] id=%s sonuc: %.300s", szId, szInfo ? szInfo : "");
	if(!szInfo || (!AlogContainsCI(szInfo, "account") && !strstr(szInfo, szId) && !(szTitle && (AlogContainsCI(szTitle, "account") || strstr(szTitle, szId)))))
		return false; // baska akisin dialogu: dokunma
	char szName[64] = {0};
	if(ParseAstatsName(szInfo, szTitle, szName, sizeof(szName)))
	{
		Log("[ALOG] account ID %s -> '%s' ismiyle kaydediliyor.", szId, szName);
		SendAdminLog(it->szAdmin, szName, it->llAccId, it->szAction, it->szMode, it->iDur, it->szReason);
	}
	else
	{
		Log("[ALOG] account ID %s ismi bulunamadi, ID ile kaydediliyor.", szId);
		AlogAccFallback(g_alogAstatsIdx);
	}
	it->bUsed = false;
	g_alogAstatsActive = false;
	g_alogAstatsIdx = -1;
	g_waitingForGStats = false;
	g_gstatsInput = "";
	g_gstatsSentFirst = false;
	return true;
}
// Her tick: siradaki account ID eylemi icin /astats gonder; zaman asiminda ID ile kaydet.
static void ProcessAlogAstatsQueue()
{
	DWORD dwNow = GetTickCount();
	if(g_alogAstatsActive)
	{
		if(dwNow - g_alogAstatsReqTime > 15000)
		{
			Log("[ALOG] /astats cozumu zaman asimi, ID ile kaydediliyor.");
			AlogAccFallback(g_alogAstatsIdx);
			g_alogAstatsActive = false;
			g_alogAstatsIdx = -1;
		}
	return;
	}
	int iNext = -1;
	for(int i = 0; i < 8; i++) if(g_alogAccQ[i].bUsed) { iNext = i; break; }
	if(iNext < 0) return;
	if(!iSpawned || g_waitingForGStats || g_ghpActive || g_gmPhase != 0 || g_waitingForGroupName || g_waitingForGmembers) return;
	AlogAccItem *it = &g_alogAccQ[iNext];
	sprintf_s(g_alogAstatsId, sizeof(g_alogAstatsId), "%lld", it->llAccId);
	g_waitingForGStats = true;
	g_gstatsReqTime = dwNow;
	g_gstatsAutoSubmit = true;
	g_gstatsInput = g_alogAstatsId;
	g_gstatsRequesterId = "";
	g_gstatsSentFirst = false;
	g_alogAstatsActive = true;
	g_alogAstatsIdx = iNext;
	g_alogAstatsReqTime = dwNow;
	char szCmd[64];
	sprintf_s(szCmd, sizeof(szCmd), "/astats %s", g_alogAstatsId);
	Log("[ALOG] /astats %s gonderiliyor (isim cozumu).", g_alogAstatsId);
	sendServerCommand(szCmd);
}
// "ADMIN: gandalf banned Bowtac(163) from CNR Mode for 180 minutes (REASON: ...)"
static void CheckAdminLogMessage(const char *msg)
{
	{ const char *pG = msg; while(*pG == ' ' || *pG == '\t') pG++; if(!strncmp(pG, "GROUP:", 6)) return; } // GROUP aksiyonu grup logundur
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
		const char *pOpen = NULL;
		// Hedef "account ID 3292655" ise: ad sabit formatta, ID sayisal
		if((pT[0]=='a'||pT[0]=='A')&&(pT[1]=='c'||pT[1]=='C')&&(pT[2]=='c'||pT[2]=='C')&&(pT[3]=='o'||pT[3]=='O')&&(pT[4]=='u'||pT[4]=='U')&&(pT[5]=='n'||pT[5]=='N')&&(pT[6]=='t'||pT[6]=='T'))
		{
			const char *pN = pT + 7;
			while(*pN==' '||*pN=='\t') pN++;
			if(*pN=='I'||*pN=='i') { pN++; if(*pN=='D'||*pN=='d') pN++; }
			while(*pN==' '||*pN=='\t') pN++;
			if(*pN>='0'&&*pN<='9')
			{
				targetId = _strtoi64(pN, NULL, 10);
				sprintf_s(target, sizeof(target), "account ID %lld", targetId);
			}
		}
		if(!target[0]) pOpen = strchr(pT, '(');
		const char *pEnd = pOpen ? pOpen : pT;
		while(pEnd > pT && pEnd[-1] == ' ') pEnd--;
		if(!target[0]) CopyToken(target, sizeof(target), pT, pEnd);
		if(pOpen)
		{
			const char *pClose = strchr(pOpen + 1, ')');
			if(pClose) targetId = _strtoi64(pOpen + 1, NULL, 10);
		}
	}
	if(!target[0]) return;
	char action[16] = {0};
	CopyToken(action, sizeof(action), verb + 1, verb + strlen(verb));
	// Gecici hesap eylemleri DB kirletmesin
	if(IsTempAccount(target))
	{
		Log("[ALOG] Temp hesap, DBye yazilmiyor: %s %s", action, target);
	return;
	}
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
	if(!strncmp(target, "account ID ", 11) && EnqueueAlogAccItem(admin, targetId, action, mode, durationMin, reason))
	return; // /astats isim cozumu bitince kaydedilecek (yoksa ID ile)
	SendAdminLog(admin, target, targetId, action, mode, durationMin, reason);
}

// oyun ici "?alog <id|accid|isim>" sorgusu: DB'den sayi ozeti iste ve oyuna yaz
// "?alogs 14" girilirse slot ID o anki isme cevrilir: slot IDler geri
// donusumludur, DBdeki target_id ile birebir karsilastirilamaz.
// Slot cevrimdisi/bilinmiyorsa NULL doner (yanlis kisi gosterilmez);
// buyuk sayilar account ID sayilip aynen gecer.
static bool QueryBackendPlayerRow(int iPid, char *szName, size_t nNameLen, double *pLoss, int *pFps, int *pPing, int *pAgeSec); // tanim asagida (?pl bolumu)
static bool QueryBackendPlayerRowByAccount(int iAcc, char *szName, size_t nNameLen, int *pAgeSec); // hesap ID -> isim

// ---- HESAP ID HARITASI (isim -> hesap ID) ---------------------------------
// Slot ID her reconnect'te degisir ve baska oyunculara devredilir; hesap ID
// (/stats) kalicidir. Admin logu hedefini once ISIMLE, sonra hesap ID ile
// baglamak icin bu harita kullanilir. Bilinmeyen isim icin sunucuya bir kez
// /stats <isim> sorulur; cevap dialogu gelince harita doldurulur.
static std::map<std::string,int> g_accMap;      // kucuk isim -> hesap id
static std::string g_accPendingName;           // /stats sorusunda bekleyen isim
static DWORD g_accPendingAt = 0;                // soru zamani (timeout icin)
#define ACC_PROBE_TIMEOUT_MS 20000

static int FindAccountIdByName(const char *szName)
{
	if(!szName || !szName[0]) return 0;
	char nLow[64] = {0};
	for(int k = 0; szName[k] && k < 63; k++) { char cc = szName[k]; nLow[k] = (cc >= 'A' && cc <= 'Z') ? (char)(cc + 32) : cc; }
	std::map<std::string,int>::iterator it = g_accMap.find(nLow);
	return it != g_accMap.end() ? it->second : 0;
}

// Ismin hesap ID'si bilinmiyorsa sunucuya /stats <isim> sor (tek seferlik).
// Cevap dialogu HandleAccStatsDialog'da islenir.
static void EnsureAccountId(const char *szName)
{
	if(!szName || !szName[0] || !g_isEngineActive) return;
	if(FindAccountIdByName(szName) > 0) return;
	DWORD dw = GetTickCount();
	if(!g_accPendingName.empty() && dw - g_accPendingAt < ACC_PROBE_TIMEOUT_MS) return; // zaten bir tanesi soruluyor
	char szCmd[96];
	sprintf_s(szCmd, sizeof(szCmd), "/stats %s", szName);
	Log("[ACC] Hesap ID bilinmiyor, sunucuya soruluyor: %s", szCmd);
	g_accPendingName = szName;
	g_accPendingAt = dw;
	sendServerCommand(szCmd);
}
static const char *ResolveAlogQuery(const char *q)
{
	static char s_buf[64];
	const char *p = q ? q : "";
	while(*p == ' ') p++;
	size_t n = 0;
	while(p[n] && p[n] != ' ' && p[n] != '\t' && n < sizeof(s_buf) - 1) { s_buf[n] = p[n]; n++; }
	s_buf[n] = 0;
	if(!s_buf[0]) return s_buf;
	// Iki numara alani var ve ust uste biniyor: oyun ici id (tab menusu, 0-999)
	// ve /stats hesap id (kalici). Belirsizligi SOZDIZIMI ile cozuyoruz:
	//   ?alogs 12   -> OYUN ICI id (ekranda gordugu sey)
	//   ?alogs a:12 -> HESAP id (/stats)
	//   ?alogs o:12 -> oyun ici id (acik yazim, istege bagli)
	bool bAcc = false, bGame = false;
	if(s_buf[1] == ':')
	{
		if(s_buf[0] == 'a' || s_buf[0] == 'A') bAcc = true;
		else if(s_buf[0] == 'o' || s_buf[0] == 'O') bGame = true;
		if(bAcc || bGame)
		{
			memmove(s_buf, s_buf + 2, strlen(s_buf + 2) + 1);
			while(*s_buf == ' ') memmove(s_buf, s_buf + 1, strlen(s_buf + 1));
			Log("[ALOG] Sorgu oneki: %s", bAcc ? "hesap ID (a:)" : "oyun ici id (o:)");
		}
	}
	bool bNum = true;
	for(size_t k = 0; s_buf[k]; k++) if(s_buf[k] < '0' || s_buf[k] > '9') { bNum = false; break; }
	if(!bNum)
	{
		for(int s = 0; s < MAX_PLAYERS; s++)
			if(playerInfo[s].szPlayerName[0] && _stricmp(playerInfo[s].szPlayerName, s_buf) == 0)
			{
					Log("[ALOG] '%s' CANLI (slot %d).", s_buf, s);
				EnsureAccountId(s_buf);
				break;
			}
		return s_buf;
	}
	int nId = atoi(s_buf);
	// Hesap ID oneki: oyun ici id adimi ATLANIR, dogrudan hesap tablosuna bak.
	if(bAcc)
	{
		char szAccName[64] = {0}; int iAccAge = 999999;
		if(QueryBackendPlayerRowByAccount(nId, szAccName, sizeof(szAccName), &iAccAge)
			&& szAccName[0] && iAccAge >= 0 && iAccAge < 120)
		{
			Log("[ALOG] Hesap ID %d -> '%s' (DB %ds).", nId, szAccName, iAccAge);
			g_accMap[std::string(szAccName)] = nId;
			strncpy_s(s_buf, sizeof(s_buf), szAccName, sizeof(s_buf) - 1);
			return s_buf;
		}
		Log("[ALOG] Hesap ID %d bulunamadi (cevrimdisi olabilir).", nId);
		return NULL;
	}
	if(nId < 0 || nId >= MAX_PLAYERS) return s_buf;
	if(playerInfo[nId].szPlayerName[0])
	{
		strncpy_s(s_buf, sizeof(s_buf), playerInfo[nId].szPlayerName, sizeof(s_buf) - 1);
		// Oyun ici id (tab menusu) birincil: admin ekranda gordugu sayi bu.
		// Hesap ID farkliysa karisiklik olmasin diye ikisini de yaziyoruz.
		int iKnown = FindAccountIdByName(s_buf);
		if(iKnown > 0 && iKnown != nId)
			Log("[ALOG] UYARI: oyun ici id %d = '%s' (hesap ID %d). Sayi oyun ici id olarak cozuldu; hesap icin ?alogs a:%d yaz.", nId, s_buf, iKnown, nId);
		Log("[ALOG] Slot %d -> '%s' ismiyle sorgulaniyor.", nId, s_buf);
		EnsureAccountId(s_buf);
		return s_buf;
	}
	if(g_playerCache[nId].szName[0] && g_playerCache[nId].dwTime > 1)
	{
		strncpy_s(s_buf, sizeof(s_buf), g_playerCache[nId].szName, sizeof(s_buf) - 1);
		Log("[ALOG] Slot %d -> '%s' ismiyle sorgulaniyor (cache).", nId, s_buf);
		return s_buf;
	}
	// Son care: DB (60 sn push). Yasli satir kabul edilmez (bayat slot).
	char szDbName[64] = {0}; int iDbAge = 999999;
	if(QueryBackendPlayerRow(nId, szDbName, sizeof(szDbName), NULL, NULL, NULL, &iDbAge)
		&& szDbName[0] && iDbAge >= 0 && iDbAge < 120)
	{
		strncpy_s(s_buf, sizeof(s_buf), szDbName, sizeof(s_buf) - 1);
		Log("[ALOG] Slot %d -> '%s' ismiyle sorgulaniyor (DB, %ds).", nId, s_buf, iDbAge);
		return s_buf;
	}
	// Sayi slot olarak cozulemedi: simdi HESAP ID olabilir (/stats bunu gosterir,
	// players.id ise baglanti slot ID'si). DB'deki account_id -> isim, sonra
	// CANLI listeden teyit - "slot offline" hatasi boylece duzelir.
	char szAccName[64] = {0}; int iAccAge = 999999;
	if(QueryBackendPlayerRowByAccount(nId, szAccName, sizeof(szAccName), &iAccAge)
		&& szAccName[0] && iAccAge >= 0 && iAccAge < 120)
	{
		bool bLive = false;
		for(int s = 0; s < MAX_PLAYERS; s++)
			if(playerInfo[s].szPlayerName[0] && _stricmp(playerInfo[s].szPlayerName, szAccName) == 0) { bLive = true; break; }
		Log("[ALOG] Hesap ID %d -> '%s' (%s, DB %ds).", nId, szAccName, bLive ? "CANLI" : "kayitli", iAccAge);
		// Cakisma uyarisi: ayni sayi hem hesap ID hem baska birinin slotu ise
		// sessizce yanlis kisi donmemek icin ikisini de yaziyoruz.
		if(playerInfo[nId].szPlayerName[0] && _stricmp(playerInfo[nId].szPlayerName, szAccName) != 0)
			Log("[ALOG] UYARI: hesap ID %d = '%s' | oyun ici id %d = '%s'. Sorgu hesap ID olarak cozuldu; oyun ici id icin ?alogs o:%d yaz.", nId, szAccName, nId, playerInfo[nId].szPlayerName, nId);
		g_accMap[std::string(szAccName)] = nId;
		strncpy_s(s_buf, sizeof(s_buf), szAccName, sizeof(s_buf) - 1);
		return s_buf;
	}
	return NULL; // slot cevrimdisi/bilinmiyor: yanlis kisi gosterilmesin
}
static void QueryAdminLogSummary(const char *q)
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(!q)
	{
		g_bridgeSuppressEcho++;
		sendChat((char *)(g_langTurkish ? "!Slot cevrimdisi veya bilinmiyor, isimle dene." : "!Slot offline or unknown, try the name."));
		return;
	}
	if(!q[0])
	{
		g_bridgeSuppressEcho++;
		sendChat((char *)(g_langTurkish ? "!Kullanim: ?alogs <id|isim>" : "!Usage: ?alogs <id|name>"));
		return;
	}
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(5);
		json body = {{"hwid", g_myHWID}, {"q", q}};
		auto res = cli.Post("/api/alog", body.dump(), "application/json");
		if(res && res->status == 200)
		{
			// Backend {"success":bool,"summary"|"message"} doner; ham JSON degil
			// ozet/mesaj metni gruba iletilir ("!" prefix zorunlu).
			std::string txt;
			try
			{
				json j = json::parse(res->body);
				if(j.value("success", false))
					txt = j.value("summary", "");
				else
					txt = j.value("message", "");
			}
			catch(...) { txt.clear(); }
			if(txt.empty()) txt = res->body;
			std::string out = "!" + txt;
			g_bridgeSuppressEcho++;
			sendChat((char *)out.c_str());
			Log("[ALOG] ?alog yaniti: %s", txt.c_str());
		}
		else
			Log("[ALOG] ?alog sorgusu basarisiz (HTTP %d)", res ? res->status : -1);
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
		json j = {
			{"id", i},
			{"name", g_playerCache[i].szName[0] ? g_playerCache[i].szName : "?"},
			{"packetloss", g_playerCache[i].dLoss},
			{"fps", g_playerCache[i].iFps},
			{"ping", g_playerCache[i].iPing}
		};
		// /stats'in gosterdigi hesap ID'yi de yolla: 'id' her reconnect'te
		// degisen BAGLANTI slot ID'si, hesap ID ise kalicidir.
		if(g_playerCache[i].szName[0])
		{
			char nLow[64] = {0};
			for(int k = 0; g_playerCache[i].szName[k] && k < 63; k++) { char cc = g_playerCache[i].szName[k]; nLow[k] = (cc >= 'A' && cc <= 'Z') ? (char)(cc + 32) : cc; }
			std::map<std::string,std::string>::iterator it = g_gmKnownAcc.find(nLow);
			if(it != g_gmKnownAcc.end() && it->second.size() && it->second.size() < 8) j["accountId"] = atoi(it->second.c_str());
		}
		arr.push_back(j);
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

// Acilista DB'den BUGUNKU grup+tag+zone haritasini geri yükler.
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
				// Kalici zone->sahip haritasini geri yükle
				if(g.contains("zones") && g["zones"].is_array())
				{
					for(const auto &zv : g["zones"])
					{
						int zid = zv.is_number() ? zv.get<int>() : atoi(zv.get<std::string>().c_str());
						zid = ZONE_IDX_FROM_SHOWN(zid);   // DB ekranda gorunen numarayi tutar
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
		Log("[ZF] DB'den geri yüklendi: %d grup, %d zone sahipligi.", g_groupColorCount, nZones);
		return true;
	}
	catch(...) { return false; }
}

static void SyncGroupColorsToDb()
{
	// Ogrenilen grup adi + kisa tag + KALICI sahip olunan TUM zone id'ler tek
	// satir (group_name upsert). Flashing filtresi VAR - DB canli savas haritasi (sonuk zoneler yazilmaz). zones.txt
	// YOK: DB acilista GET /api/groups ile geri yüklenir (LoadGroupsFromDb).
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	// KILIT: DB'den ilk restore tamamlanmadan YAZMA. Bos hafiza ile yazim,
	// DB'deki zone haritasini silip botu her acilista sifirdan tarama zorluyordu.
	if(!s_bGroupDbRestored) return;

	json arr = json::array();
	for(int i = 0; i < g_groupColorCount; i++)
	{
		if(!g_groupColors[i].szName[0]) continue;
		json zids = json::array();
		// DB canli savas haritasidir: YALNIZCA su an flashing zone'ler yazilir.
		for(int z = 0; z < MAX_GANG_ZONES; z++)
			if(g_zoneFlash[z] && g_zoneOwnerName[z][0] && _stricmp(g_zoneOwnerName[z], g_groupColors[i].szName) == 0)
			{
				int nShown = ZONE_SHOWN_FROM_IDX(z);
				if(nShown >= 0) zids.push_back(nShown);   // DB/Discord ayni numarayi gorsun (/gzinfo gibi)
			}
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

// Flash'tan dusen zone: kirmizi yanip somneyi birakti (grup rengine dondü). Zone
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
		// groupName YALNIZCA doluyken g?nder: bos g?nderilirse DB deki kayitli
		// grup adi her acilista silinir, bot yeniden ??renmek zorunda kalir.
		if(!g_myGroupName.empty())
			body["groupName"] = g_myGroupName;
		// zscan/gzt/zgrp tercihlerini SADECE DB'den okunduktan SONRA gonder.
		// (Yoksa acilis aninda varsayilan degerler DB'deki kayitli tercihi ezdi:
		// ?zscan on sonraki restart'ta "kapali" olarak goruluyordu.)
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

// ?zscan on/off tercihini gruba gore DB'den oku (acilis/ilgili zamanlarda). Grup
void StartZoneScan(bool bAuto); // tanim asagida (netrpc.cpp); SyncZscanFromDb'den cagrilir
// adi biliniyorsa GET /api/bot/toggles ile cekilip g_zscanAuto/g_gztOn uygulanir.
bool g_waitingForGroupName = false;   // /stats sonucundan kendi grup adi okunuyor (SAMPRPC extern)
static int s_gnameMode = 0; // 0: once /gstats dene, 1: /stats dongusu
int  g_groupNamePhase = 0;            // 0=idle, 1=ilk diyalog, 2=ikinci diyalog (grup adi)
DWORD g_groupNameReqTime = 0;         // zaman asimi (SAMPRPC extern)

// /stats sonucundan ogrenilen KENDI GRUP ADI. (SAMPRPC cagirir - diyalog ilk satiri.)
void LearnOwnGroupName(const char *szName)
{
	if(!szName || !szName[0]) return;
	// Mevcut ad ayniysa is yok; farkliysa guncelle + DB'ye yaz.
	if(!g_myGroupName.empty() && _stricmp(g_myGroupName.c_str(), szName) == 0) return;
	bool bChanged = !g_myGroupName.empty();
	g_myGroupName = szName;
	Log("[GSTATS] Kendi grup adi ogrenildi: %s%s", szName,
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
	PauseHarvestsForDialog("?grole");
	g_grolePage = 0;
	g_groleStartTime = GetTickCount();
	g_grTargetItem = -1; g_grDlgId = 0; g_grClickAttempt = -1; g_grClickAt = 0;
	// Direkt "?grole <isim> <rol>" chat komutu UIF'de calismiyor (test edildi);
	// /grole dialog otomasyonu kullanilir.
	g_groleDirectPending = false;
	g_groleDirectAt = 0;
	Log("[GROLE] Otomasyon baslatildi: '%s' -> '%s' (/grole gonderiliyor)...", target, role);
	sendServerCommand((char *)"/grole");
}
void StartGrank(const char *target, const char *rank)
{
	if(!target || !target[0] || !rank || !rank[0]) return;
	g_grankActive = true;
	g_grankTarget = target;
	g_grankRank = rank;
	g_grankPhase = 1;
	PauseHarvestsForDialog("?grank");
	g_grankPage = 0;
	g_grankStartTime = GetTickCount();
	g_grTargetItem = -1; g_grDlgId = 0; g_grClickAttempt = -1; g_grClickAt = 0; g_grDblPhase = 0;
	Log("[GRANK] Otomasyon baslatildi: '%s' -> '%s' (/grank gonderiliyor)...", target, rank);
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
		Log("[GROLE-LIST] Liste bos, gonderilmiyor.");
		g_groleListActive = false;
		return;
	}
	Log("[GROLE-LIST] Toplam %d uye toplandi, Discord'a gonderiliyor...", (int)g_groleListMembers.size());
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
		if(res && res->status==200) Log("[GROLE-LIST] Discord'a gonderildi.");
		else Log("[GROLE-LIST] Gonderilemedi (HTTP %d)", res?res->status:-1);
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
		Log("[GRANK-LIST] Liste bos, gonderilmiyor.");
		g_grankListActive = false;
		return;
	}
	Log("[GRANK-LIST] Toplam %d uye toplandi, Discord'a gonderiliyor...", (int)g_grankListMembers.size());
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
		if(res && res->status==200) Log("[GRANK-LIST] Discord'a gonderildi.");
		else Log("[GRANK-LIST] Gonderilemedi (HTTP %d)", res?res->status:-1);
	}catch(...){}
	g_grankListActive = false;
	g_grankListMembers.clear();
	g_grankListPage = 0;
}
void StartGkick(const char *target, const char *reason)
{
	if(!target || !target[0]) return;
	if(g_gkickActive) Log("[GKICK] Onceki otomasyon iptal edilip yeniden baslatiliyor.");
	g_gkickActive = true;
	g_gkickTarget = target;
	g_gkickReason = reason ? reason : "";
	g_gkickPhase = 1;
	PauseHarvestsForDialog("?gkick");
	g_gkickPage = 0;
	g_gkickStartTime = GetTickCount();
	g_grTargetItem = -1; g_grDlgId = 0; g_grClickAttempt = -1; g_grClickAt = 0; g_grDblPhase = 0;
	Log("[GKICK] Otomasyon baslatildi: '%s' (sebep: '%s') (/gkick gonderiliyor)...", target, g_gkickReason.c_str()[0] ? g_gkickReason.c_str() : "-");
	sendServerCommand((char *)"/gkick");
}
void StartGkickList(const char *requesterId, const char *guildId, const char *hwid)
{
	if(!requesterId || !requesterId[0]) return;
	g_gkickListActive = true;
	g_gkickListRequesterId = requesterId;
	g_gkickListGuildId = guildId ? guildId : "";
	g_gkickListHwid = hwid ? hwid : "";
	g_gkickListMembers.clear();
	g_gkickListPage = 0;
	g_gkickListStartTime = GetTickCount();
	Log("[GKICK-LIST] Uye listesi cekiliyor (req=%s)...", requesterId);
	sendServerCommand((char *)"/gkick");
}
void FinishGkickListAndPost()
{
	if(g_gkickListMembers.empty()){
		Log("[GKICK-LIST] Liste bos, gonderilmiyor.");
		g_gkickListActive = false;
		return;
	}
	Log("[GKICK-LIST] Toplam %d uye toplandi, Discord'a gonderiliyor...", (int)g_gkickListMembers.size());
	try{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {
			{"requesterId", g_gkickListRequesterId},
			{"guildId", g_gkickListGuildId},
			{"hwid", g_gkickListHwid},
			{"members", g_gkickListMembers}
		};
		auto res = cli.Post("/api/gkick/list", body.dump(), "application/json");
		if(res && res->status==200) Log("[GKICK-LIST] Discord'a gonderildi.");
		else Log("[GKICK-LIST] Gonderilemedi (HTTP %d)", res?res->status:-1);
	}catch(...){}
	g_gkickListActive = false;
	g_gkickListMembers.clear();
	g_gkickListPage = 0;
}
static void SyncZscanFromDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	if(g_zscanDbLoaded) return;
	// Grup adi bilinmiyorsa: /stats (kendi ID'miz) ile ogren. /stats once oyuncu
	// stats diyalogu acar, "Group Stats" butonuna (button 1) basinca grup adini
	// gosteren ikinci diyalog gelir. (NOT: "logged in - Ominous" ranktir, grup
	// adi degil - ona guvenme.)
	if(g_myGroupName.empty())
	{
		// Zaman asimi: 15 sn icinde ogrenilemezse beklemeyi sifirla; sonraki tick'te
		// yeniden /stats dener. SAMPRPC gstats timeout'undan (20sn) BAGIMSIZ - yoksa
		// gstats kapaninca bu da sonsuza dek kilitlenirdi (wait=1 takilmasi).
		if(g_waitingForGroupName && (GetTickCount() - g_groupNameReqTime > 15000))
		{
			g_waitingForGroupName = false;
			g_groupNamePhase = 0;
						if(s_gnameMode == 0)
			{
				s_gnameMode = 1;
				Log("[GSTATS] /gstats ile grup adi alinamadi, /stats dongusune dusuluyor.");
			}
			else
			{
				Log("[GSTATS] Grup adi beklemasi zaman asimina ugradi, yeniden deneniyor.");
			}
		}
		// SPAWN GATE: /stats yalnizca spawn sonrasi gonderilir. Spawn menusu/
		// gecisi sirasinda gonderilirse sunucu grup stats diyalogunu bastiriyor.
		if(!g_waitingForGroupName && iSpawned && g_zoneScanIdx < 0 && !g_ghpActive && !g_inZoneWar)
		{
			// ONCE DB'DEN OKU (bots.group_name - dosya yok, DB tek kaynak). Kayitli
			// grup adi varsa /stats ogrenmesi ATLANIR; yoksa asagida /stats ile ogrenilir.
			static DWORD s_lastDbNameTry = 0;
			bool bLoadedFromDb = false;
			// DB kaydi yokken her tick HTTP+log spam oluyordu: 30 sn de bir dene.
			if(s_lastDbNameTry == 0 || GetTickCount() - s_lastDbNameTry >= 30000)
			{
				s_lastDbNameTry = GetTickCount();
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
				catch(...) { Log("[GSTATS] DB yukleme: baglanti hatasi."); }
			}
			else
				Log("[GSTATS] DB yukleme atlandi: g_szNickName bos.");
			}
			if(!bLoadedFromDb && false) // otomatik ogrenme KAPALI: ad DB'de kayitli varsayilir, ?gname ile elle
			{
				if(s_gnameMode == 0)
				{
					// ONCE /gstats dene: parametresiz grup stats (bos submit -> kendi grubu).
					// 15 sn timeout /stats dongusune duser. Faz 2 sonucu isler.
					g_waitingForGroupName = true;
					g_groupNamePhase = 2;
					g_groupNameReqTime = GetTickCount();
					g_waitingForGStats = true;
					g_gstatsReqTime = GetTickCount();
					g_gstatsAutoSubmit = true;
					g_gstatsInput = "";
					g_gstatsRequesterId = "";
					g_gstatsSentFirst = false;
					Log("[GSTATS] Grup adi icin /gstats gonderiliyor (tek diyalog denemesi)...");
					sendServerCommand((char *)"/gstats");
				}
				else
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
				Log("[GSTATS] Grup adi icin /stats %d gonderiliyor (Group Stats butonu bekleniyor)...", g_myPlayerID);
				sendServerCommand(szCmd);
			}
			}
		}
		else if(!g_waitingForGroupName)
		{
			// Spawn beklenirken log spam'i olmasin: 2 sn'de bir bildir.
			static DWORD s_lastGroupNameBlkLog = 0;
			if(s_lastGroupNameBlkLog == 0 || GetTickCount() - s_lastGroupNameBlkLog >= 2000)
			{
				s_lastGroupNameBlkLog = GetTickCount();
				Log("[GSTATS] Grup adi yok - ?gname ile ogret (spawn=%d scan=%d ghp=%d war=%d)...",
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
				g_myGroupName.c_str(), bOn ? "ACIK" : "KAPALI", bGzt ? "ACIK" : "KAPALI", bZgrp ? "ACIK" : "KAPALI");
			if(bOn && s_bGroupDbRestored && g_zoneScanIdx < 0 && iSpawned
				&& !g_ghpActive && !g_waitingForGroups
				&& g_spawnTick != 0 && (GetTickCount() - g_spawnTick) >= 60000 && g_bLoggedIn
				&& (g_zoneScanLastRun == 0 || GetTickCount() - g_zoneScanLastRun >= ZONE_SCAN_PERIOD_MS))
				StartZoneScan(true); // restore sonrasi ilk tarama: spawn korumasi + harvest oncelikli
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
				"RakSAMP kapatiliyor. Backend'i yeniden baslatin ve tekrar deneyin.",
				"RakSAMP - Baglanti Kesildi",
				MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}
	}
	// Login beklentisi main.cpp ana dongusunde yonetilir (g_loginPending iken bu
	// fonksiyon cagrilmaz). Acilis: BUGUNKU grup+tag+zone haritasini DB'den geri yükle (bir kez, 5 sn
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
	// Gun degisiminde eski zone verisini temizle (kullanici: 22 agustos verisi
	// 23 agustosta silinmeli). GET /api/groups zaten bugunku kayitlari verir;
	// burada yerel temizlik + push yapilir.
	static int s_dbLastDay = CurrentDayStamp();
	int iToday = CurrentDayStamp();
	if(iToday != s_dbLastDay)
	{
		s_dbLastDay = iToday;
		Log("[ZF] Gun degisti (%d): eski zone sahiplik verisi temizleniyor.", iToday);
		for(int z = 0; z < MAX_GANG_ZONES; z++)
			{ g_zoneOwnerName[z][0] = 0; g_zoneFlash[z] = false; }
		SyncGroupColorsToDb(); // bos zone listeleriyle bugunun kayitlari yazilir
	}
	// zscan/gzt/zgrp toggle'ini HER TICK'te dene (60 sn throttle YOK): grup adi
	// sonradan ogrenilirse (ornek: bot acilip gruba girdikten sonra) bayrak kurulsun.
	// g_zscanDbLoaded true olunca iceride tekrar denenmez (bir kez).
	SyncZscanFromDb();
	DWORD dwNow = GetTickCount();
	if(dwNow - g_lastDbSyncTick < 60000) return;
	g_lastDbSyncTick = dwNow;
	SyncPlayerCacheToDb();
	SyncGroupColorsToDb();
	SyncBotStatusToDb();

	// DB TEK KAYNAK: tag'ler sonradan da gelisebilir (baska bot/gec calisan harvest
	// ve/veya server'daki DATE(updated_at)=CURDATE() filtreli GET yuzunden ilk
	// restore'da gelmemis olabilir). LoadGroupsFromDb sadece EKLEME yapar ve DB'deki
	// BOS tag'i hafizadaki dolu tag'e yazmaz -> periyodik yeniden cekmek guvenlidir.
	// Boylece DB'de var olan ama hafizaya hic yuklenmemis tag'ler de gelir (yoksa
	// ?zf o gruplarin ismini gosterirdi; tag'i yuklenmisler -2 seklinde etiketlenir).
	LoadGroupsFromDb();
}

// Grup dili: ?tr / ?en ile degistirilir; varsayilan INGILIZCE. Tek dogru kaynak
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
	// Sahibi /gzinfo taramasiyla zaten dogru ogrenildiyse (doluysa) EZME; bos ise fallback yaz.
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
		Log("[ZFU:LEARN] '%s' ogrenildi (zone #%d)", szBuf, iZone);
	}
}

// -------------------------------------------------------------
// -------------------------------------------------------------
// ?pl ISTATISTIK CACHE'I (bellek ici; dosya YOK)
// UIF'in /pnetstats cevabi FPS icermez (FPS: --); FPS dahil tam istatistik YS.Hold'un
// "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w" grup mesajlarinda gelir. Bu cache
// ?pl cevabina FPS'i (ve eksik loss/ping'i) koyar. Kalicilik DB'dedir (players tablosu,
// SyncPlayerCacheToDb her 60 sn push eder; bos bellek DB'den tamamlanir).
// Isimler de buraya yazilir: grup basligindan ogrenilen ad, ?pl cevabinda "hakixld"
// yerine gercek kullanici adinin gorunmesini saglar.
// -------------------------------------------------------------
void ResetPlayerCache()
{
	memset(g_playerCache, 0, sizeof(g_playerCache));
	Log("[BUILD] Oasis beta 2026-10-04 23:14 (liste kapatma orijinal + aninda).");
	Log("[PL-CACHE] bellek sifirlandi (kaynak: canli + DB).");
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

// ?zf: flashing zone'lari grup tag'larina gore sayip gruba bildirir (YS bot gibi).
// Cikti TEK mesajdir (SA-MP 128 karakter siniri): en buyuk gruplar once, tasarsa
// kesilir. Bilinmeyen grup rengi goruldugunde kendi flash taramamiz (/gzinfo)
// baslatilir - YS botu gibi yeni gruplari kendi kendine ogrenir.

// ?zf sonrasi: bilinmeyen renkli flashing zone'lar icin tarama baslatildiysa
// tarama bittiginde guncel ozet TEKRAR gonderilir (artik renkler biliniyor).
static bool g_zfScanResend = false;
static int  g_zfFollowUps = 0;         // tarama sonu takip turu sayisi (sonsuz dongu kilidi)
static int  g_zfPendingZones[64];
static int  g_zfPendingCount = 0; // mesgulken flashlanip dusen bilinmeyen zoneler
// Mesgulken flashlanip dusen bilinmeyen zone: savas bitip flash sonse bile
// takip turunda /gzinfo ile ogrenilsin diye listeye alinir.
static void StashUnknownZone(int zid)
{
	if(zid < 0 || zid >= MAX_GANG_ZONES) return;
	if(g_zoneOwnerName[zid][0] != 0) return;
	for(int k = 0; k < g_zfPendingCount; k++) if(g_zfPendingZones[k] == zid) return;
	if(g_zfPendingCount >= 64) return;
	g_zfPendingZones[g_zfPendingCount++] = zid;
	Log("[GANGZONE:FLASH] Zone #%d bekleme listesinde (%d).", zid, g_zfPendingCount);
}
static void StartFlashScan();          // ?zf: sahibi bilinmeyen flashing zone'lari ogren

// ?zf cikti parcasi: "!Flashing Zones: Tag(n) | ..." (Tek veya [1/2] [2/2]).
// UIF'te "!" on eki = gruba mesaj (hakixld: "!..." -> (GROUP) kanali).
struct FlashZoneItem { char szLabel[64]; int n; };
static void SendFlashSlice(const FlashZoneItem items[], int iFrom, int iTo, int iNum, int iOf)
{
	char szOut[140]; int nOut = 0;
	// Tarama/kill-list capture suruyorsa "capturing" belirt (kullanici: "capture ediyorsa
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
		// RENK BAGIMSIZ eslesme: flashing zone'un sahibi /gzinfo'dan ogrenilen ISIM
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

	// count'a gore buyukten kucuge
	for(int i = 0; i < aggCount; i++)
		for(int j = i + 1; j < aggCount; j++)
			if(agg[j].n > agg[i].n)
			{
				int ti = agg[i].gi; int tn = agg[i].n;
				agg[i].gi = agg[j].gi; agg[i].n = agg[j].n;
				agg[j].gi = ti; agg[j].n = tn;
			}

	// Bilinmeyen (sahipli ama haritada olmayan) renk var mi? Varsa bu renklerin
	// flashing zone'larina tek tek teleport olup /gzinfo ile ogren (YS botu gibi
	// kendi taramamiz). Ozet ONCE mevcut bilgiyle gonderilir, tarama bitince tekrar.
	bool bUnknown = false;
	for(int a = 0; a < aggCount && !bUnknown; a++)
	{
		if(agg[a].gi == -2) continue; // renk 0 = sahipsiz/free, ogrenmeye gerek yok
		if(agg[a].gi < 0) bUnknown = true; // -1: hicbir gruba eslesmeyen renk
	}

	// YS botu gibi: "Flashing Zones: Tag(n) | ..." - etiketler once toplanir;
	// uzunluga gore Tek veya [1/2]+[2/2] iki mesaj.
	FlashZoneItem items[256];
	int nItems = 0;
	for(int a = 0; a < aggCount && nItems < 256; a++)
	{
		int idx = agg[a].gi; // -2 free, -1 bilinmeyen, >=0 grup indeksi
		const char *pszLabel = "Free";
		// "NULL"/"-"/"~" = UIF'te tag'siz grup (gercek tag yok), isimle gosterilir.
		bool bNoTag = (idx >= 0 && !g_groupColors[idx].szTag[0])
			|| (idx >= 0 && (!_stricmp(g_groupColors[idx].szTag, "NULL") || !strcmp(g_groupColors[idx].szTag, "-")
				|| !strcmp(g_groupColors[idx].szTag, "~")));
		if(agg[a].gi == -2 || (idx >= 0 && !g_groupColors[idx].szTag[0] && !g_groupColors[idx].szName[0]))
			pszLabel = "Free";
		else if(idx >= 0 && !bNoTag)
		pszLabel = g_groupColors[idx].szTag; // tag varsa hep tag (sayisal da dahil: "737")
		else if(idx >= 0)
			pszLabel = g_groupColors[idx].szName;  // tag yoksa tam ad
		else
			pszLabel = "capturing";              // bilinmeyen: tarama sonrasi etiket ogrenilir
		char szLabelClean[64] = {0};
		strncpy_s(szLabelClean, sizeof(szLabelClean), pszLabel, sizeof(szLabelClean)-1);
		StripColorCodes(szLabelClean); // {RRGGBB} / <RRGGBB> renk kodlarini temizle
		strncpy_s(items[nItems].szLabel, sizeof(items[0].szLabel), szLabelClean, sizeof(items[0].szLabel) - 1);
		items[nItems].n = agg[a].n;
		nItems++;
	}

	// nItems==0: gorunur bolgede bilinen grup yok - BO-S mesaj atma (bo-s "Flashing
	// Zones: " gruba gidip "mesaj atmýyor" hissi verir). Bilinmeyen renk varsa
	// tarama baslat (yalnizca flashing zonelere gider); ogrenilince tekrar ozet atilir.
	if(nItems == 0)
	{
		Log("[ZF] Bilinen flashing grup yok (bolge bos veya hepsi bilinmeyen). Toplam %d zone.", total);
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Flashing zone yok.", "!No flashing zones."));
		// Tag ogrenme yalnizca ?zgrp ile (elle): otomatik /gstats spam'i mute riski.
		return;
	}

	// YS botu gibi dinamik parcalama: 1 mesaja ~5 grup / ~105 char sigar. Grup sayisi
	// artarsa otomatik [1/N] [2/N]... bolesin (her zaman [1/1] degil). Parcalari once
	// char hesabiyla planla, sonra sirayla gonder.
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
	Log("[ZF] Toplam %d flashing zone (gorunen bolge), %d parca.", total, nParts);
	// Bilinmeyen renk ogrenme yalnizca ?zgrp (elle): otomatik /gstats spam'i mute riski.
}

// --- Oto renk taramasi: renk -> grup adi (gzinfo), zgo mantigiyla ---

static void GangZoneScanNext();
static void GangZoneScanGo(int iZone);

// Sniper saldiri-onayi: /gzinfo sonucu beklenen zoneyse sahibi zincir grubuyla
// karsilastir; eslesirse /gz, eslesmezse sonrakine gec/kapat (bayat zincir korumasi).
// "/gzinfo" metnindeki "can be attacked in X hours and Y minutes" suresini
// dakika olarak dondurur; yoksa -1. (Renk kodlari once temizlenir.)
static int ParseAttackCooldownMin(const char *szInfo)
{
	if(!szInfo) return -1;
	const char *p = strstr(szInfo, "can be attacked in");
	if(!p) return -1;
	char szClean[256]; strncpy_s(szClean, sizeof(szClean), p, sizeof(szClean) - 1);
	StripColorCodes(szClean);
	p = szClean;
	while(*p && (*p < '0' || *p > '9')) p++;
	if(!*p) return -1;
	int h = atoi(p);
	const char *pA = strstr(p, "and");
	int m = 0;
	if(pA) { pA += 3; while(*pA && (*pA < '0' || *pA > '9')) pA++; m = atoi(pA); }
	return h * 60 + m;
}
// Sniper teleport: ?zgo ile ayni (/pos + varis payi + durum sifirlama).
// Tum sniper isinlanmalari buradan gecer: ayni zone'e cift /pos ve dusus
// sirasinda atilan /gzinfo (eski zone okuma) engellenir.
static void ZtkChainAdvance();
static void ZtkGotoZone(int zid)
{
	if(g_snipZoneCount > 0 && zid >= 0 && zid < MAX_GANG_ZONES)
	{
		char szHave[80]; strncpy_s(szHave, sizeof(szHave), g_zoneOwnerName[zid], sizeof(szHave) - 1);
		NormalizeGroupName(szHave);
		char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), g_snipGroupName, sizeof(szNeed) - 1);
		NormalizeGroupName(szNeed);
		if(szNeed[0] && szHave[0] && _stricmp(szHave, szNeed) != 0
			&& _stricmp(szHave, "Free") != 0 && _stricmp(szHave, "-") != 0)
		{
			Log("[ZSNIP] Zone #%d taze cachede zaten %s (beklenen %s) - isinlanmadan atlaniyor.", zid, szHave, szNeed);
			ZtkChainAdvance();
			return;
		}
	}
	g_ztkZoneId = zid;
	g_ztkWaitAnnounced = false;
	g_ztkTargetWar = false;
	g_ztkVerifyPending = false;
	g_ztkVerifyZone = -1;
	g_ztkVerifyTries = 0;
	g_ztkTeleportAt = GetTickCount();
	float fCX = (g_zoneMinX[zid] + g_zoneMaxX[zid]) / 2.0f;
	float fCY = (g_zoneMinY[zid] + g_zoneMaxY[zid]) / 2.0f;
	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCX, fCY, ZGO_TELEPORT_Z);
	sendServerCommand(szCmd);
}

// Zincirde sonrakine gec ya da bitir (dogrulama uyusmazligi / cooldown / basarisiz saldiri sonrasi).
static void ZtkChainAdvance()
{
	g_ztkTargetWar = false;
	g_ztkWaitAnnounced = false;
	g_ztkOutcome = 0;
	g_ztkVerifyTries = 0;
	if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
	{
		g_snipZoneIdx++;
		int nNext = g_snipZones[g_snipZoneIdx];
		ZtkGotoZone(nNext);
		Log("[ZSNIP] Siradaki zone #%d hedeflendi.", nNext);
	}
	else
	{
		g_ztkActive = false;
		g_ztkZoneId = -1;
		g_snipZoneCount = 0;
		g_snipZoneIdx = 0;
		g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
		g_snipGroupName[0] = 0;
		ResumeZoneScanAfterSnip();
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Sniper bitti.", "!Sniper finished."));
	}
}
// /gz etkisiz kaldi: savas hic baslamadi (cooldown/mute/konum). Zincirden devam.
static void ZtkAttackFailed()
{
	Log("[ZSNIP] Saldiri basarisiz: savas baslamadi.");
	g_bridgeSuppressEcho++;
	sendChat((char *)LM("!Saldiri basarisiz: savas baslamadi.", "!Attack failed: no war started."));
	g_ztkAttackPending = false;
	g_ztkOutcome = 0;
	ZtkChainAdvance();
}
static void ZtkVerifyComplete(const char *szInfo)
{
	int zid = g_ztkVerifyZone;
	g_ztkVerifyPending = false;
	g_ztkVerifyZone = -1;
	g_ztkVerifyTries = 0;
	if(!g_ztkActive || zid < 0 || zid >= MAX_GANG_ZONES) return;
	int iCd = ParseAttackCooldownMin(szInfo);
	// Flashing zone her zaman vurulabilir: cooldown gosterilse bile yoksayilir.
	if(iCd > 5 && g_zoneFlash[zid])
		Log("[ZSNIP] Zone #%d flashing (%d dk cooldown yoksayildi).", zid, iCd);
	if(iCd > 5 && !g_zoneFlash[zid])
	{
		Log("[ZSNIP] Zone #%d saldiri korumali (cooldown %d dk), atlaniyor.", zid, iCd);
		g_bridgeSuppressEcho++;
		char szMsg2[128];
		sprintf_s(szMsg2, sizeof(szMsg2), LM("!Zone #%d saldiri korumali (%d dk), atlaniyor.", "!Zone #%d is protected (%d min), skipping."), ZONE_SHOWN_FROM_IDX(zid), iCd);
		sendChat(szMsg2);
		ZtkChainAdvance();
	return;
	}
	char szHave[80]; strncpy_s(szHave, sizeof(szHave), g_zoneOwnerName[zid], sizeof(szHave) - 1);
	NormalizeGroupName(szHave);
	char szNeed[80]; strncpy_s(szNeed, sizeof(szNeed), g_snipGroupName, sizeof(szNeed) - 1);
	NormalizeGroupName(szNeed);
	// Sayisal ?zsnip (?zgo mantigi): grup adi yoksa zone dogrudan vurulur.
	if(!szNeed[0] || (szHave[0] && !_stricmp(szHave, szNeed)))
	{
		Log("[ZSNIP] Zone #%d sahibi dogrulandi (%s), /gz gonderiliyor.", zid, g_zoneOwnerName[zid]);
		g_ztkYesPending = false;
		g_ztkPreAsked = false;
		g_ztkAttackPending = true;
		g_ztkWarSeen = false;
		g_ztkAttackAt = GetTickCount();
		g_ztkOutcome = 0;
		g_zwWeWon = true;
		sendServerCommand((char *)"/gz");
		Log("[ZSNIP] /gz gonderildi, won/lost sonucu bekleniyor.");
	return;
	}
	Log("[ZSNIP] Zone #%d sahibi uyusmadi (beklenen %s, gercek %s) - saldiri iptal.", zid, g_snipGroupName, szHave[0] ? szHave : "?");
	g_bridgeSuppressEcho++;
	char szMsg[128];
	sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d artik %s degil (%s), atlaniyor.", "!Zone #%d is no longer %s (%s), skipping."), ZONE_SHOWN_FROM_IDX(zid), g_snipGroupName, szHave[0] ? szHave : "?");
	sendChat(szMsg);
	if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
	{
		g_snipZoneIdx++;
		int nNext = g_snipZones[g_snipZoneIdx];
		ZtkGotoZone(nNext);
		Log("[ZSNIP] Dogrulama sonrasi siradaki zone #%d hedeflendi.", nNext);
	}
	else
	{
		g_ztkActive = false;
		g_ztkZoneId = -1;
		g_snipZoneCount = 0;
		g_snipZoneIdx = 0;
		g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
		g_snipGroupName[0] = 0;
		ResumeZoneScanAfterSnip();
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Sniper bitti.", "!Sniper finished."));
	}
}
// /gzinfo dialogu geldi: "Zone Name: # N" -> zone id, "owned by group "X"" -> ad.
// Sahipsiz cikarsa renk "Free" (bos tag) olarak isaretlenir; tekrar kontrol edilmez.
static int FindZoneByName(const char *szName)
{
	if(!szName || !szName[0]) return -1;
	char szNeed[64];
	strncpy_s(szNeed, sizeof(szNeed), szName, _TRUNCATE);
	NormalizeGroupName(szNeed);
	for(int z = 0; z < g_zoneCount && z < MAX_GANG_ZONES; z++)
	{
		if(!g_zoneNames[z][0]) continue;
		char szHave[64];
		strncpy_s(szHave, sizeof(szHave), g_zoneNames[z], _TRUNCATE);
		NormalizeGroupName(szHave);
		if(!_stricmp(szHave, szNeed)) return z;
	}
	return -1;
}
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
	// Oasis: "Zone: <ad>" ayri satir, ID "Zone ID: {FFE599}22" satirinda.
	// Sayidan once renk kodu geliyor -> atoi hep 0 veriyordu. Once temizle.
	if(iZone < 0)
	{
		const char *pId = strstr(szInfo, "Zone ID:");
		if(pId)
		{
			char szNum[32] = {0};
			size_t nN = strcspn(pId + 8, "\r\n");
			if(nN > 31) nN = 31;
			memcpy(szNum, pId + 8, nN);
			szNum[nN] = 0;
			StripColorCodes(szNum);
			iZone = atoi(szNum);
		}
	}

	// Kimlik = TELESTRANSPORT ETTIGIMIZ zone'un dizini (kuyruk sirasi), gelen
	// ID degil: sunucu /gzinfo'da bir geriden sayiyor (bizim #59 -> sunucu #58,
	// 57 ornekte de fark tam -1). Gelen ID bir onceki rect'e ait oldugu icin
	// onu yaziyoruzsa sahipler bir slot kaymis oluyor. Flash ID'leri, ?zf ve
	// ?zgo hep bizim dizinimizi kullandigi icin sahibi ornek dizinine yaziyoruz.
	// ponytail: duzgun ornek varsa sunucu ID'si ne derse dese yazilir; kayma
	// varsayimi sadece logda gorunur, sonuc degistirmez.
	int iDlgZone = iZone;
	if(g_ztkVerifyPending && g_ztkVerifyZone >= 0 && g_ztkVerifyZone < MAX_GANG_ZONES)
	{
		if(iDlgZone != g_ztkVerifyZone)
			Log("[ZF:SCAN] Dogrulama ornegi #%d kimligiyle ogreniliyor (gelen #%d).", g_ztkVerifyZone, iDlgZone);
		iZone = g_ztkVerifyZone;
	}
	else if(g_zoneScanIdx >= 0 && g_zoneScanIdx < g_zoneScanQ)
	{
		if(iDlgZone != g_zoneScanQueue[g_zoneScanIdx])
			Log("[ZF:SCAN] Sunucu ID #%d dondu, ornegin rect'i #%d (kayma).", iDlgZone, g_zoneScanQueue[g_zoneScanIdx]);
		iZone = g_zoneScanQueue[g_zoneScanIdx];
	}
	else if(iDlgZone < 0 || iDlgZone >= MAX_GANG_ZONES)
	{
		Log("[ZF:SCAN] Dialog kimliksiz ve ornek yok; atlandi.");
		return;
	}

	if(iZone >= 0 && iZone < MAX_GANG_ZONES)
	{
		const char *pZn = strstr(szInfo, "Zone:");
		while(pZn && !strncmp(pZn + 5, " ID:", 4)) pZn = strstr(pZn + 5, "Zone:");
		if(pZn)
		{
			const char *pZS = pZn + 5;
			while(*pZS == ' ') pZS++;
			char szZN[64] = {0};
			size_t nZN = strcspn(pZS, "\r\n");
			if(nZN > 63) nZN = 63;
			memcpy(szZN, pZS, nZN); szZN[nZN] = 0;
			StripColorCodes(szZN);
			char *pZT = szZN; while(*pZT == ' ') pZT++;
			for(int k = (int)strlen(szZN) - 1; k >= 0 && (szZN[k] == ' ' || szZN[k] == '\t'); k--) szZN[k] = 0;
			if(pZT[0]) { strncpy_s(g_zoneNames[iZone], sizeof(g_zoneNames[0]), pZT, _TRUNCATE); Log("[ZF:SCAN] Zone #%d adi: '%s'.", iZone, g_zoneNames[iZone]); }
		}
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
	// Oasis: "Owned by: Nightmare Empire [nM]" - isim + [TAG].
	if(!szOwner[0])
	{
		const char *pOb = strstr(szInfo, "Owned by:");
		if(pOb)
		{
			pOb += 9;
			while(*pOb == ' ' || *pOb == '\t') pOb++;
			const char *pEol = pOb;
			while(*pEol && *pEol != '\n' && *pEol != '\r') pEol++;
			char szLine[96] = {0};
			size_t nL = (size_t)(pEol - pOb);
			if(nL > 95) nL = 95;
			memcpy(szLine, pOb, nL); szLine[nL] = 0;
			char szTag[16] = {0};
			char *pBrO = strrchr(szLine, '[');
			char *pBrC = pBrO ? strchr(pBrO, ']') : NULL;
			if(pBrO && pBrC && pBrC > pBrO + 1 && pBrC - pBrO < 12 && pBrO > szLine)
			{
				size_t nT = (size_t)(pBrC - pBrO - 1);
				if(nT > 15) nT = 15;
				memcpy(szTag, pBrO + 1, nT); szTag[nT] = 0;
				char *pTrim = pBrO;
				while(pTrim > szLine && *(pTrim-1) == ' ') pTrim--;
				*pTrim = 0;
			}
			NormalizeGroupName(szLine);
			if(szLine[0])
			{
				strncpy_s(szOwner, sizeof(szOwner), szLine, _TRUNCATE);
				if(szTag[0])
				{
					int idx = FindGroupColorByName(szOwner);
					if(idx >= 0 && (!g_groupColors[idx].szTag[0] || !_stricmp(g_groupColors[idx].szTag, g_groupColors[idx].szName)))
					{
						strncpy_s(g_groupColors[idx].szTag, sizeof(g_groupColors[0].szTag), szTag, sizeof(g_groupColors[0].szTag) - 1);
						SaveGroupColors();
						Log("[ZF:SCAN] Zone #%d tag ogrenildi: '%s' -> '%s'.", iZone, szOwner, szTag);
					}
				}
			}
		}
	}
	// Oasis: "Attack cooldown: Ready to attack" (savasilabilir) ya da kalan sure.
	{
		const char *pCd = strstr(szInfo, "Attack cooldown:");
		if(pCd)
		{
			char szCd[64] = {0};
			const char *pS = pCd + 17;
			while(*pS == ' ' || *pS == '\t') pS++;
			size_t nC = strcspn(pS, "\r\n");
			if(nC > 63) nC = 63;
			memcpy(szCd, pS, nC); szCd[nC] = 0;
			StripColorCodes(szCd);
			Log("[ZF:SCAN] Zone #%d cooldown: %s", iZone, szCd);
		}
	}
	// ISIM BAZLI ogrenme: /gzinfo'nun "owned by group X" ismi -> DB isim->tag eslemesi.
	// Sahipsiz zone -> "Free" sahte grubu: ?zgo free bu zone'lara gider, DB'ye de
	// Free grubunun zone listesi olarak kaydedilir.
	// Uyeleri cevrimdisi grubun zoneu: "owned by group" yok. Bilinen sahibi
	// EZME (Dev vakasi: offline olunca Free dusup sniper yanlis saldirdi);
	// bilinmiyorsa Free isaretle ki sonraki tur gercek isim ogrenilsin.
	if(!szOwner[0] && strstr(szInfo, "does not have members online")
		&& iZone >= 0 && iZone < MAX_GANG_ZONES && g_zoneOwnerName[iZone][0] != 0
		&& _stricmp(g_zoneOwnerName[iZone], "Free") != 0)
	{
		strncpy_s(szOwner, sizeof(szOwner), g_zoneOwnerName[iZone], sizeof(szOwner) - 1);
		Log("[ZF:SCAN] Ornek zone #%d: sahibi cevrimdisi, bilinen isim korundu (%s).", iZone, szOwner);
	}
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
			Log("[ZF:SCAN] Yeni grup ogrenildi: '%s'", szOwner);
		}
	}
	Log("[ZF:SCAN] Ornek zone #%d: %s", iZone, szOwner);

	// zone -> sahip grup adi: ?zf flashing zone'lari bu isimle DB'deki tag'e esler.
	if(iZone >= 0 && iZone < MAX_GANG_ZONES)
		strncpy_s(g_zoneOwnerName[iZone], sizeof(g_zoneOwnerName[0]), szOwner, sizeof(g_zoneOwnerName[0]) - 1);
	// Sniper dogrulama bekliyorsa tamamla (tarama akisi aynen devam eder).
	// Sunucu bir geriden sayiyor (ornek #20 -> cevap #19): beklenen-1 de kabul.
	if(g_ztkVerifyPending && (iZone == g_ztkVerifyZone || iZone == g_ztkVerifyZone - 1))
	{
		if(iZone != g_ztkVerifyZone) Log("[ZF:SCAN] Dogrulama kayma ile eslesti (beklenen #%d, gelen #%d).", g_ztkVerifyZone, iZone);
		ZtkVerifyComplete(szInfo);
	}

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
		if(nF == 0)
		{
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Serbest zone yok.", "!No free zones."));
		}

		// ?zf bilinmeyen renkler icin tarama baslatmissa: guncel ozeti tekrar gonder
		// (artik renkler ogrenildi, "?" yerine isimler gorunecek).
		// SPAM ONLEME: ?zf komutu disinda otomatik ozet ATILMAZ (her tag ogreniminde
		// gruba mesaj gidiyordu). Yalnizca isaret sifirlanir; kullanici ?zf atinca gorur.
		g_zfScanResend = false;

		// Tarama bitti: ogrenilen zone->grup+tag haritasini DB'ye kaydet (kalici harita).
		// (bot yeniden baslayinca zoneler "sifirlanmasin").
		SyncGroupColorsToDb();

		char szCmd[128];
		sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f",
			g_zoneScanReturnPos[0], g_zoneScanReturnPos[1], g_zoneScanReturnPos[2]);
		sendServerCommand(szCmd);
		// Tag'ler /groups + /gstats ile YALNIZCA ?zgrp / ?tagscan (elle) ile ogrenilir;
		// tarama bitince otomatik /groups GONDERILMEZ (her seferinde tag bulmaya calisip
		// /gstats spam'i + mute riski yaratiyordu).
		// Tarama surerken flashlanip atlanan bilinmeyen zone kaldiysa eve donusten
		// SONRA bir tur daha (#147 vakasi: "Atlanildi (scan aktif)" -> 15 dk'ya kaliyordu).
		// Sinirsiz dongu olmasin diye oturum basina 5 takip turu.
		// Bekleyen bilinmeyen zoneler (mesgulken dusenler): flash sonmus olsa bile
		// /gzinfo sahibi soyler - dogrudan kuyruga alinir, ogrenilince listeden duser.
		bool bScanContinues = false;
		{
			int nP = 0;
			for(int k = 0; k < g_zfPendingCount; k++)
			{
				int zid = g_zfPendingZones[k];
				if(zid < 0 || zid >= g_zoneCount) continue;
				if(g_zoneOwnerName[zid][0] != 0) continue;
				bool bDup = false;
				for(int q = 0; q < nP; q++) if(g_zoneScanQueue[q] == zid) { bDup = true; break; }
				if(!bDup && nP < MAX_GANG_ZONES) g_zoneScanQueue[nP++] = zid;
			}
			g_zfPendingCount = 0;
			if(nP > 0)
			{
				g_zoneScanQ = nP;
				g_zoneScanIdx = 0;
				Log("[ZF:SCAN] Bekleyen %d bilinmeyen zone takip turu.", nP);
				GangZoneScanGo(g_zoneScanQueue[0]);
				bScanContinues = true;
			}
		}
		if(!bScanContinues && g_zfFollowUps < 5)
		{
			g_zfFollowUps++;
			StartFlashScan();
			if(g_zoneScanIdx >= 0) bScanContinues = true;
		}
		if(!bScanContinues)
		{
			Sleep(2000);
			sendServerCommand((char *)"/fr");
			Log("[ZF:SCAN] Tarama bitti, /fr.");
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
	// 1. faz: teleport. Sunucu konumu i-sleyene kadar /gzinfo G-Ã»NDER-â–‘LMEZ -
	// ayni anda giderse cevap eski konumun zone'unu dondurur (stale zinciri).
	g_zoneScanWaitingTeleport = true;
	g_zoneScanDeadline = GetTickCount() + ZONE_SCAN_TELEPORT_SYNC_MS;
	Log("[ZF:SCAN] Ornek zone #%d taraniyor -> /pos (%s)", iZone, szCmd);
	sendServerCommand(szCmd);
}

void StartZoneScan(bool bAuto)
{
	if(g_zoneScanIdx >= 0 || !iSpawned || g_inZoneWar || g_zscanPausedForSnip)
	{
		Log("[ZF:SCAN] StartZoneScan atlandi (scanAktif=%d spawned=%d war=%d).",
			g_zoneScanIdx >= 0, iSpawned, g_inZoneWar);
		return;
	}

	// Tarama dengesi (kullanici): bilinen zone'lari HER turda yeniden taramak
	// bosu bosuna vakit kaybi; hic taramamak da veriyi bozar. Cozum:
	// Normal tur  -> YALNIZCA sahibi bilinmeyen flashing zone'lar.
	// 5 periyot: 0-1. turlar TAM KONTROL (bayat veri), 2-4. turlar bilinmeyen-only.
	// ILK GIRIS: s_iScanPass=1'den baslar -> ilk tur bilinmeyen-only (DB'de var
	// olanlar atlanir, sadece eksikler ogrenilir). Yarinki giris gun degisince
	// LoadGroupsFromDb bos donecek, tum zone'lar "bilinmeyen" sayilip taraniacak.
	// ?zscan sn tek seferlik TAM kontrol. Acilis bilinmeyen-only kosar:
	// DB'de kayitli zoneler atlanir (hizli acilis).
	if(g_forceFullVerifyOnce)
	{
		g_forceFullVerifyOnce = false;
		g_zoneScanQ = 0;
		for(int zid = 0; zid < g_zoneCount && g_zoneScanQ < MAX_GANG_ZONES; zid++)
			g_zoneScanQueue[g_zoneScanQ++] = zid;
		if(g_zoneScanQ == 0)
		{
			g_zoneScanLastRun = GetTickCount();
			Log("[ZF:SCAN] Acilis kontrolu: taranacak zone yok.");
			return;
		}
		Log("[ZF:SCAN] ?zscan sn TAM KONTROL: %d zone tek tek dogrulanacak.", g_zoneScanQ);
	}
	else
	{
	static int s_iScanPass = 0; // ilk tur tam kontrol olsun
	bool bFullVerify = (s_iScanPass % 5 < 2); // 5 periyot: 0-1. turlar tam kontrol, 2-4. bilinmeyen-only
	s_iScanPass++;

	g_zoneScanQ = 0;
	for(int zid = 0; zid < g_zoneCount && g_zoneScanQ < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue; // YALNIZCA flashing zone'lar
		if(!bFullVerify && g_zoneOwnerName[zid][0] != 0 && _stricmp(g_zoneOwnerName[zid], "Free") != 0) continue; // bilinen atla (Free her tur dogrulanir)
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
	}

	float fPos[3] = {0, 0, 0};
	if(!getPlayerPos(g_myPlayerID, fPos))
	{
		g_zoneScanWaitStart = 0; // oto-baslatma 30 sn sonra tekrar deneyecek
		Log("[ZF:SCAN] Pozisyon alinamadi, tarama baslatilmadi (oto-retry).");
		return;
	}
	g_zoneScanReturnPos[0] = fPos[0];
	g_zoneScanReturnPos[1] = fPos[1];
	g_zoneScanReturnPos[2] = fPos[2];

	g_zoneScanIdx = 0;
	g_zoneScanLastRun = GetTickCount(); // oto aralikli tarama icin son baslatma anii
	Log("[ZF:SCAN] %d zone taramasý basliyor (%s).", g_zoneScanQ, bAuto ? "oto" : "manuel ?zscan");
	GangZoneScanGo(g_zoneScanQueue[0]);
}

// ?zf: mevcut flashing zone'lardan sahibi BILINMEYEN olanlari tek tek ornek se-gip
// teleport + /gzinfo ile ogrenir (YS botu yerine kendi taramamiz). Tarama bitince
// GangZoneScanNext guncel ozeti tekrar gonderir.
static void StartFlashScan()
{
	if(g_zoneScanIdx >= 0 || !iSpawned || g_zscanPausedForSnip)
	{
		static DWORD s_lastSkipLog = 0;
		if(s_lastSkipLog == 0 || GetTickCount() - s_lastSkipLog >= 60000)
		{
			s_lastSkipLog = GetTickCount();
			Log("[ZF:FLASHSCAN] Atlanildi (scan aktif=%d spawned=%d).", g_zoneScanIdx >= 0, iSpawned);
		}
		return;
	}

	g_zoneScanQ = 0;
	for(int zid = 0; zid < g_zoneCount && g_zoneScanQ < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue;      // yalnizca flashing zone'lar
		if(g_zoneOwnerName[zid][0] != 0) continue; // isim biliniyor, ogrenmeye gerek yok
		bool bDup = false;
		for(int q = 0; q < g_zoneScanQ; q++)
			if(g_zoneScanQueue[q] == zid) { bDup = true; break; }
		if(!bDup)
			g_zoneScanQueue[g_zoneScanQ++] = zid;
	}
	if(g_zoneScanQ == 0) return; // bilinmeyen flashing zone yok

	float fPos[3] = {0, 0, 0};
	if(!getPlayerPos(g_myPlayerID, fPos)) { Log("[ZF:FLASHSCAN] Pozisyon alinamadi, tarama baslatilmadi."); return; }
	g_zoneScanReturnPos[0] = fPos[0];
	g_zoneScanReturnPos[1] = fPos[1];
	g_zoneScanReturnPos[2] = fPos[2];

	g_zoneScanIdx = 0;
	g_zfScanResend = true; // tarama bitince guncel ozet yeniden gonderilir (artik tag'ler biliniyor)
	Log("[ZF:FLASHSCAN] %d sahibi bilinmeyen flashing zone ogreniliyor.", g_zoneScanQ);
	GangZoneScanGo(g_zoneScanQueue[0]);
}

// Her ana dongu tick'inde: oto-tarama (spawn'dan 30 sn sonra, sonra her 15 dk'da bir) +
// stale renk temizligi. Zone war sirasinda tarama bekletilir (teleport yok).
void StartGrCandidateClicks(int rawRow, int item, unsigned short dlgId, int page, const char *firstCol)
{
	g_grLastSentItem = -1;
	g_grRawRow = rawRow;
	g_grTargetItem = item;
	g_grDlgId = dlgId;
	g_grPage = page;
	g_grClickAttempt = -1;
	g_grClickAt = 0;
	g_grDblPhase = 0;
	g_grRowText[0] = 0;
	if(firstCol && firstCol[0]) strncpy_s(g_grRowText, sizeof(g_grRowText), firstCol, sizeof(g_grRowText) - 1);
	Log("[GROLE] Uye tiklama adaylari baslatildi (ham satir=%d, item=%d, sayfa=%d, sutun='%s').", rawRow, item, page, g_grRowText);
}

static void SendGrClick(int item, const char *szInput)
{
	g_grLastSentItem = item;
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
// (renksiz) â€” sunucu uyeyi bundan taniyor olabilir. Adaylar: (item, input) kombinasyonlari.
void ProcessGrCandidateClicks()
{
	if(g_grTargetItem < 0 || g_grDlgId == 0) return;
	if(!g_groleActive && !g_grankActive && !g_gkickActive) return;
	if(g_groleActive && g_grolePhase != 1) return;
	if(g_grankActive && g_grankPhase != 1) return;
	if(g_gkickActive && g_gkickPhase != 1) return;
	if(g_grClickAttempt < 0){ g_grClickAttempt = 0; g_grDblPhase = 0; g_grClickAt = GetTickCount() + 100; return; }
	if(GetTickCount() < g_grClickAt) return;
	if(g_grClickAttempt >= 4)
	{
		Log("[GROLE] Hedef item tiklanamadi, iptal ediliyor.");
		RakNet::BitStream bsC;
		bsC.Write((uint16_t)g_grDlgId);
		bsC.Write((uint8_t)0);
		bsC.Write((uint16_t)0);
		bsC.Write((uint8_t)0);
		pRakClient->RPC(&RPC_DialogResponse, &bsC, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Uye secilemedi, islem iptal.", "!Member could not be selected, cancelled."));
		g_groleActive = false; g_grolePhase = 0;
		g_grankActive = false; g_grankPhase = 0;
		g_gkickActive = false; g_gkickPhase = 0;
		g_grTargetItem = -1; g_grDlgId = 0;
		return;
	}
	int item = g_grTargetItem;
	const char *szInput = (g_grClickAttempt == 0) ? g_grRowText : "";
	if(item < 0) item = 0;
	if(g_grDblPhase == 0){
		Log("[GROLE] Hedef item=%d cift tik (1/2)...", item);
		SendGrClick(item, szInput);
		g_grDblPhase = 1;
		g_grClickAt = GetTickCount() + 400;
	}
	else{
		Log("[GROLE] Hedef item=%d cift tik (2/2)...", item);
		SendGrClick(item, szInput);
		g_grDblPhase = 0;
		g_grClickAttempt++;
		g_grClickAt = GetTickCount() + 2500;
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
	if(!g_groleActive && !g_grankActive) return; // zaman asimi/iptal sonrasi bayat tiklama: gonderme
	g_grRoleDblPending = false;
	if(g_grRoleIsGrank){
		Log("[GRANK] Secim gonderildi, yayin onayi bekleniyor: %s -> %s", g_grankTarget.c_str(), g_grankRank.c_str());
	}
	else{
		Log("[GROLE] Secim gonderildi, yayin onayi bekleniyor: %s -> %s", g_groleTarget.c_str(), g_groleRole.c_str());
	}
}

void ProcessZoneScan()
{
	if(g_harvestPausedForDlg && !g_groleActive && !g_grankActive && !g_gkickActive)
		ResumeHarvestsAfterDialog("oto");
	if(g_wrArmedAt != 0 && !g_wrPosted && !g_zwKillListSent && GetTickCount() - g_wrArmedAt > 15000)
	{
		g_wrPosted = true;
		char szBox[256];
		sprintf_s(szBox, sizeof(szBox), "%s defeated %s\n%s%s%s (chat sonucu)", g_wrWin, g_wrLose[0] ? g_wrLose : "?", g_wrScore, g_wrZone[0] ? " - " : "", g_wrZone);
		Log("[WAR] Dialog verisi gelmedi, chat sonucu Discord'a: %s", szBox);
		SendKillList(szBox, "", g_wrWin, g_wrLose, "", "", 0, 0, !g_myGroupName.empty() && _stricmp(g_wrWin, g_myGroupName.c_str()) == 0, "", "", 0, 0, 0, "", "");
	}
	if(g_firstRejectTick != 0 && GetTickCount() - g_firstRejectTick > 2*60*1000)
	{
		Log("[DLG-WATCH] 2dkdir surekli red, yeniden baglaniliyor.");
		g_firstRejectTick = 0;
		g_lastPosOkTick = 0;
		g_watchDlgId = -1;
		g_ghpActive = false;
		g_waitingForGroups = false;
		g_waitingForGStats = false;
		g_ghpLastRun = GetTickCount() + 45*60*1000; // travma molasi: oto harvest 60dk durur (acilmayan listeyi tekrar acmasin)
		resetPools(1, 0);
		return;
	}
	if(!g_bLoggedIn && iSpawned && g_spawnTick != 0 && GetTickCount() - g_spawnTick > 180000)
	{
		g_bLoggedIn = true;
		Log("[LOGIN] Giris mesaji gorulmedi (180sn), varsayimla devam ediliyor.");
	}
	if(g_watchDlgId >= 0 && !g_watchDlgKeep && GetTickCount() - g_watchDlgAt > 30000)
	{
		int iWdId = g_watchDlgId, iWdStyle = g_watchDlgStyle;
		g_watchDlgAt = GetTickCount();
		if(iWdStyle == DIALOG_STYLE_LIST || iWdStyle == DIALOG_STYLE_TABLIST || iWdStyle == DIALOG_STYLE_TABLIST_HEADERS)
		{
			SendListSelectRaw(iWdId, 0, g_watchDlgRow[0] ? g_watchDlgRow : NULL);
			Log("[DLG-WATCH] satir secimi gonderildi (id=%d).", iWdId);
		}
		else
			sendDialogResponse((WORD)iWdId, 1, 0xFFFF, (char *)"");
		Log("[DLG-WATCH] 30 sn acik kalan dialog kapatildi (id=%d style=%d).", iWdId, iWdStyle);
	}


	// ?post gerekce penceresi doldu mu? (30 sn) â€” gerekce yazilmadiysa bos gonderilir,
	// Discord tarafi "gerekce verilmedi" yazar.
	if(g_pushPending && GetTickCount() > g_pushUntil)
	{
		g_pushPending = false;
		Log("[POST] 30 sn doldu, gonderiliyor (gerekce: '%s').", g_pushReason[0] ? g_pushReason : "-");
		PushLastGroupLog(g_pushReason[0] ? g_pushReason : "");
	}
	// Faz-2 rol/rank cift tik ikinci tiklamasi.
	if(g_grRoleDblPending && GetTickCount() >= g_grRoleDblAt)
		SendGrRoleSecondClick();
	// /grole DIREKT komut ("?grole <isim> <rol>") 4 sn icinde "changed the role of"
	// onayi uretmediyse: /grole dialog otomasyonuna gec (hala phase=1 page=0'sa).
	// (Direkt komut diyalog acmissa phase 1 zaten ilerliyordur; tekrar /grole gonderilmez.)
	if(g_groleActive && g_groleDirectPending && GetTickCount() > g_groleDirectAt && g_grolePhase == 1 && g_grolePage == 0)
	{
		g_groleDirectPending = false;
		Log("[GROLE] Direkt komut onay vermedi, /grole dialog otomasyonuna gecilir (hedef '%s').", g_groleTarget.c_str());
		sendServerCommand((char *)"/grole");
	}
	// Hedef bulunduysa uye tiklama adaylarini dene (buton/item bilinmiyor).
	ProcessGrCandidateClicks();

	// ?zgrp harvest kilidi cozum: /gstats sonucu gelmezse SAMPRPC g_waitingForGStats'i
	// sifirlar (yalnizca dialog geldiginde kontrol eder - hic dialog gelmezse flag ACIK
	// kalir) ama g_ghpWaitingGstats true kalir -> zincir takilir. Mutlak zaman asimi:
	// istekten 50 sn gectiyse beklemeyi birak. Dialog cevabi sunucuda gecikebiliyor;
	// ayni komutu tekrar gonderince spam/mute riski artar, SADECE bir kez tekrar dener.
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
	// dolunca gonderilir (OnGStatsResultForHarvest deadline'i kurar).
	if(g_ghpActive && !g_ghpWaitingGstats && g_ghpNextStepAt != 0 && GetTickCount() >= g_ghpNextStepAt)
	{
		ProcessGhpHarvestStep();
	}

	if(g_zoneScanIdx < 0)
	{
		// Otomatik ?zgrp: g_zgrpAuto acikken TEK SEferlik calisir (spawn sonrasi
		// ilk tur). Periyodik DEGIL: harvest bitince g_zgrpAuto otomatik kapanir.
		// SADECE eksik tag olan grup varsa baslar: restore sonrasi g_groupColors
		// dolu ama tag'i bos/placeholder (tag==isim) olan grup varsa harvest ile
		// kisa tag cekilir. Tum tagler tamamsa GEREKSIZ /groups+ /gstats spam'i
		// atilmaz (kullanici istegi).
		DWORD dwNow3 = GetTickCount();
		bool bNeedHarvest = false;
		// GRUP ADI GATE: harvest yalnizca kendi grup adi ogrenildikten SONRA baslar
		// (ogrenme 1 kereliktir, /stats -> Group butonu; harvest /gstats diyaloglari
		// ogrenme faziyla ayni basligi tasir, cakisirlardi).
		// Acilis: ?zgrp on kayitliysa (DB) her baslatmada bir kez tam tarama.
		static bool s_zgrpStartupDone = false;
		if(g_zgrpAuto && !s_zgrpStartupDone && s_bGroupDbRestored && iSpawned && !g_myGroupName.empty())
		{
			s_zgrpStartupDone = true;
			g_ghpForce = true;
			Log("[GHP] Acilis taramasi kuruldu (kayitli ?zgrp on): ilk sessizlikte baslayacak.");
		}
		if(g_zgrpAuto && s_bGroupDbRestored && iSpawned && !g_myGroupName.empty() && (g_ghpLastRun == 0 || GetTickCount() - g_ghpLastRun >= GHP_AUTO_PERIOD_MS)
			&& g_spawnTick != 0 && (GetTickCount() - g_spawnTick) >= 30000 && g_bLoggedIn) // spawn+login korumasi
		{
			// Eksik tag tespiti: tag bos veya placeholder (tag == isim) olan grup
			// Oto harvest periyodu doldu: her zaman harvest baslat. Bilinen gruplar
			// zincirde atlanir (nSkipped logu), sadece YENI gruplar icin /gstats gider.
			// Tek seferlik degil: ?zgrp on kaldigi surece her 15 dk tekrarlar.
			bNeedHarvest = true;
			// Sifirdan baslangic: hic grup yoksa kesif icin /groups cek (bos DB/dosya sonrasi bootstrap)
			if(g_ghpForce) bNeedHarvest = true; // elle tetik: periyot disinda da calisir
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
		// bu yuzden periyod genis tutulur; savas/zgo/harvest sirasinda bekler.
		// DB RESTORE ONCE: restore tamamlanmadan tarama baslamaz (yoksa var olan
		// zone verisi "bilinmiyor" sanilip yeniden taraniyordu).
		// GRUP ADI GATE: tarama da kendi grup adi ogrenilmeden baslamaz (ogrenme
		// onceliklidir, sonra harvest/zscan devam eder).
		// HARVEST SONRASI OTOMATIK: harvest bitti, yeni veri varsa zone scan tetikle (zscan off olsa bile).
		else if(s_bGroupDbRestored && !g_myGroupName.empty() && iSpawned && !g_ghpActive && !g_waitingForGroups && !g_inZoneWar && !g_zgoActive && g_zscanAuto && !g_zscanPausedForSnip
			&& g_spawnTick != 0 && (dwNow3 - g_spawnTick) >= 30000 && g_bLoggedIn) // harvest ile ayni spawn korumasi + login: once harvest, sonra scan
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
	if(g_inZoneWar || g_zgoActive) return; // savas/zgo surecinde tarama bekler
	DWORD dwNow = GetTickCount();
	if(dwNow <= g_zoneScanDeadline) return;

	// 1. faz bitti (teleport sync): simdi /gzinfo gonder, cevap icin yeni deadline.
	if(g_zoneScanWaitingTeleport)
	{
		g_zoneScanWaitingTeleport = false;
		float fDbgPos[3] = {0, 0, 0};
		getPlayerPos(g_myPlayerID, fDbgPos);
		Log("[ZF:SCAN] Ornek zone #%d icin /gzinfo gonderiliyor (bot %.0f, %.0f).", g_zoneScanQueue[g_zoneScanIdx], fDbgPos[0], fDbgPos[1]);
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
	// (g_knownGroupMembers: kendi grubumuzun sohbetinde gorulen uyeler - botun grubu.
	//  Oyunun TUM oyuncularina degil, sadece kendi grubuna bildirilir.)
	if(g_inZoneWar && playerId != g_myPlayerID && g_knownGroupMembers[playerId] && !g_zwWelcomed[playerId])
	{
		g_zwWelcomed[playerId] = true;
		Log("[WELCOME] Grup uyesi (%d) %s baglandi, savas aktif (%s).", playerId, szPlayerName, g_enemyGroupName.c_str());
		static char szWarNotice[192];
		sprintf_s(szWarNotice, sizeof(szWarNotice),
			LM("!%s oyuna katildi. Zone war devam ediyor! /gz yazarak savasa katil.",
			   "!%s joined. Zone war is ongoing! Type /gz to join the fight."),
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
	if(g_followActive && playerId == g_followSlot) { g_followActive = false; Log("[FOLLOW] hedef cikti, takip bitti."); }
	memset(&g_playerCache[playerId], 0, sizeof(g_playerCache[0])); // cikis yapan slotun cachei temizlenir (bayat isim kalmasin)
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
	// UIF Login: baglanti sonrasi spawn istegi GONDERILMEZ (tmp hesap onlemi).
	// Login dialogu gelir (g_loginPending=true) -> kullanici sifreyle girer ->
	// "You logged in" mesaji gelince false olur, spawn/akis baslar.
	// Login dialogu HIC gelmezse (sunucu tmp'e zorluyorsa) 10 sn sonra otomatik
	// devam et (takilip sonsuza dek bekleme).
	g_loginPending = true;
	g_loginPendingUntil = GetTickCount() + 10000;
	Log("[LOGIN] Baglanti tamam, login bekleniyor (tmp hesap onleniyor)...");
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
// oyunculara gonderir (FR'deyken bile) - format: BYTE silah, WORD kurban, WORD katil.
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

// Sava-s ba-slangýcýnda kýsa GZ giri-si: kendi takým rengimizi --srenmek i-gin /gz2,
// birka-g saniye i-geride kal (skorboard textdraw'larýnda kendi adýmýzýn rengi = takým rengi),
// sonra /fr ile freeroam'a geri d-n. /pos bu modda -galý-smaz; -gýký-s /fr ile yapýlýr.
static void ZwJoinCaptureReturn()
{
	g_zwReturnPending = true;
	g_zwKillListStay = false;
	// 2 sn: takim rengi + war timer textdraw'inin ilk edit'i gelene kadar kal,
	// sonra /fr ile geri don. (Uzun kalma GZ'de takilma/ban riski tasir.)
	g_zwReturnAt = GetTickCount() + 3000; // ilk 3 sn kurali: Oasis
	Log("[ZONEWAR] Kisa GZ giris yapiliyor, takim rengi ogrenilip /fr ile geri donulecek.");
	if(g_zwNoGzEntry) { Log("[ZONEWAR] Kisa giris atlandi (/gz once reddedildi)."); return; }
	g_zwLastGzAt = GetTickCount();
	sendServerCommand((char *)"/gz");
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
	// Oasis: "Ad(id)" gecen her satirdan oyuncu ogren (chat, kill feed, TELE/MTELE,
	// GROUP giris/cikis/invite). ?alogs slot->isim cozumu buna bagli.
	// KENDI YANKIMIZ HARIC: botun gonderdigi grup mesaji ("! [G - 30] hakixld(17): ...")
	// sunucudan geri gelir; icindeki "zaza(1)" gibi Ad(sayi)lar zone SAYISIDIR,
	// slot degil. Gonderen bizsek bu satirdan hic oyuncu ogrenme.
	bool bOwnEcho = false;
	{
		char szMy1[32] = {0}, szMy2[32] = {0};
		strncpy_s(szMy1, sizeof(szMy1), g_szNickName, _TRUNCATE);
		if(playerInfo[g_myPlayerID].szPlayerName[0]) strncpy_s(szMy2, sizeof(szMy2), playerInfo[g_myPlayerID].szPlayerName, _TRUNCATE);
		const char *pPre = szNonColorEmbeddedMsg;
		while(*pPre == ' ' || *pPre == '!') pPre++;
		if(*pPre == '[') { const char *pCl = strchr(pPre, ']'); if(pCl) pPre = pCl + 1; }
		while(*pPre == ' ') pPre++;
		for(int k = 0; k < 2 && !bOwnEcho; k++)
		{
			const char *pMy = (k == 0) ? szMy1 : szMy2;
			if(!pMy[0]) continue;
			size_t nMy = strlen(pMy);
			if(_strnicmp(pPre, pMy, nMy) == 0 && pPre[nMy] == '(') bOwnEcho = true;
		}
		// (sessiz: her kendi mesajimizda log basmak gurultuydu)
	}
	if(!bOwnEcho)
	{
		const char *p = szNonColorEmbeddedMsg;
		while((p = strchr(p, '(')) != NULL)
		{
			const char *pD = p + 1;
			while(*pD >= '0' && *pD <= '9') pD++;
			if(pD > p + 1 && *pD == ')')
			{
				int iPid = atoi(p + 1);
				if(iPid >= 0 && iPid < MAX_PLAYERS)
				{
					const char *pE = p;
					while(pE > szNonColorEmbeddedMsg)
					{
						char c = *(pE - 1);
						if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
							|| c == '_' || c == '.' || c == '[' || c == ']' || c == '-') pE--;
						else break;
					}
					size_t nN = (size_t)(p - pE);
					if(nN > 0 && nN <= 24)
					{
						char szNm[32] = {0};
						memcpy(szNm, pE, nN); szNm[nN] = 0;
						playerInfo[iPid].iIsConnected = 1;
						strncpy_s(playerInfo[iPid].szPlayerName, sizeof(playerInfo[iPid].szPlayerName), szNm, _TRUNCATE);
						strncpy_s(g_playerCache[iPid].szName, sizeof(g_playerCache[0].szName), szNm, _TRUNCATE);
						g_playerCache[iPid].dwTime = GetTickCount();
					}
				}
			}
			p++;
		}
	}

	// --- Admin + grup rol degisikligi loglari (DB'ye kayit) ---
	// "(GROUP)" prefix'siz gelen sistem mesajlari: "ADMIN: X banned Y(id) ..."
	// ve "GROUP: (...) changed the rank of ..." buradan yakalanir.
	if(!g_bLoggedIn && strstr(szNonColorEmbeddedMsg, "logged in") && !strstr(szNonColorEmbeddedMsg, "logged out"))
	{
		char szMy[32] = {0};
		strncpy_s(szMy, sizeof(szMy), g_szNickName, _TRUNCATE);
		if(strstr(szNonColorEmbeddedMsg, "You have been logged in") || strstr(szNonColorEmbeddedMsg, "You logged in")
			|| (szMy[0] && !_strnicmp(szNonColorEmbeddedMsg, "GROUP:", 6) && strstr(szNonColorEmbeddedMsg, szMy)))
		{
			g_bLoggedIn = true;
			Log("[LOGIN] Giris tamamlandi, oto harvest/scan serbest.");
		}
	}
	CheckAdminLogMessage(szNonColorEmbeddedMsg);
	CheckGroupLogMessage(szNonColorEmbeddedMsg);
	// Chat mutesi: oyuncu imzali satirlar haric (biri chatte MUTED yazarsa atlanir)
	if(strstr(szNonColorEmbeddedMsg, "MUTED") && !HasPlayerTag(szNonColorEmbeddedMsg))
		TriggerMuteReconnect(szNonColorEmbeddedMsg, "chat");

	// First Blood: GZ kill feedi "X(pid) has killed Y(pid) (Weapon: Z) (Distance: Nm)"
	// (Most Wanted formati "* X killed Y" â€” * Ã¶nekli olanlar degil). Savas basinda
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
	// Oasis kill feed: "* Rainy(22) killed lesonit(23) with Sawn-Off Shotgun (Distance: 11.12m / HP: 167.00)"
	if(g_inZoneWar && !g_zwFirstBlood[0] && szNonColorEmbeddedMsg[0] == '*' && strstr(szNonColorEmbeddedMsg, " killed ") && strstr(szNonColorEmbeddedMsg, " with ") && strstr(szNonColorEmbeddedMsg, "(Distance:"))
	{
		const char *pK = strstr(szNonColorEmbeddedMsg, " killed ");
		char szKiller[64] = {0};
		const char *pOp = strchr(szNonColorEmbeddedMsg + 2, '(');
		if(pOp && pOp < pK){ size_t n = (size_t)(pOp - (szNonColorEmbeddedMsg + 2)); if(n > 63) n = 63; memcpy(szKiller, szNonColorEmbeddedMsg + 2, n); szKiller[n] = 0; }
		char szWpn[48] = {0};
		const char *pW = strstr(pK, " with ");
		const char *pD = pW ? strstr(pW, "(Distance:") : NULL;
		if(pW && pD){ const char *pS = pW + 6; while(*pS == ' ') pS++; size_t n = (size_t)(pD - pS); while(n > 0 && pS[n-1] == ' ') n--; if(n > 47) n = 47; memcpy(szWpn, pS, n); szWpn[n] = 0; }
		char szDist[16] = {0};
		if(pD){ const char *pS = pD + 10; while(*pS == ' ') pS++; size_t n = 0; while(n < 15 && pS[n] >= '0' && pS[n] <= '9') { szDist[n] = pS[n]; n++; } szDist[n] = 0; }
		if(szKiller[0])
		{
			sprintf_s(g_zwFirstBlood, sizeof(g_zwFirstBlood), "%s (Weapon: %s) (Distance: %sm)", szKiller, szWpn, szDist);
			Log("[KILLLIST] First Blood: %s", g_zwFirstBlood);
		}
	}


	// --- Zone War chat dedektoru (RakGravy desenleri) ---
	// "keeps zone" / "takes over" / "wins against" / "loses to" iceren mesajlar
	// Discord zonewar kanalina bildirilir. 60 sn cooldown, cift tetiklenmeyi onler.
	// Yalnizca botun kendi grubunun mesajlari yakalanir.
	// Kendi grup adi /gz2 sonrasi gelen ilk "ZONE WAR: X" mesajindan otomatik ogrenilir
	// (oyun RPC'sinden gelen isim guvenilmez; bot baskalari tarafindan da kullanilacak).
	{
		// YS.Hold'un periyodik "Time left: MM:SS" duyurusu: timer textdraw'ina
		// bagimli olmadan kalan sureyi verir (GZ'de olmaya gerek yok - grup chat'i).
		// Textdraw gelmedigi zamanlarda (restart sonrasi) savas suresini bilmek icin
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
				// Chat kaynagi (uL.Hold tum savaslarin suresini yazar): ?gzt time
				// gostergesi icin tutulur ama GERI SAYIM BUNA GORE ATILMAZ -
				// bizim savasimiz degilse phantom duyurular cikiyordu.
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

		// "Type /gz2 to join" bildirimi: botun grubunun savasi basladi.
		// Duyuru bazen "ZONE WAR: A$AP vs uL ... Type /gz2 to join" gibi GRUP
		// ADLARINI icerir, bazen sadece "Zone: '# N'.  Type /gz2 to join." olur.
		// Kural: "X vs Y" varsa grubumuz icinde degilse baska grubun savasi, yoksay.
		// "vs" yoksa mesaj grubumuza ozel gonderilmistir -> bizim savasimiz.
		// "Type /gz2 to join" bildirimi: sava? duyurusu. KRITIK: sunucu HER grubun
		// sava? duyurusunu chate yazar. Sadece "ZONE WAR: X vs Y" icinde grubumuz
		// geciyorsa BIZIM sava?imizdir. Ciplak "Zone: '# N'.  Type /gz2 to join."
		// tek basina sava? BASLATMAZ (baska grubun sava?i olabilir); yalnizca
		// bizim sava?imiz AKTIFKEN zone numarasini verir.
		bool bJoinNotice = (strstr(pZoneMsg, "Type /gz2 to join") != NULL);
		// Oasis: "SYSTEM: Your group is under attack! Use /GZ or /CZ to join the war."
		// Grup uyelerine ozel gider -> dogrudan bizim savasimizdir.
		bool bOasisUnderAttack = (strstr(pZoneMsg, "Your group is under attack") != NULL);
		bool bOasisAttacking = (strstr(pZoneMsg, "Your group is attacking a zone") != NULL);
		// Herhangi bir savas basladi/bitiyor: grubumuz olmasa da ?gzt time icin izle.
		if(strstr(pZoneMsg, "Started at ") != NULL && strstr(pZoneMsg, " vs. ") != NULL)
		{
			const char *pSt = strstr(pZoneMsg, "Started at ");
			char szNew[128] = {0};
			strncpy_s(szNew, sizeof(szNew), pSt, _TRUNCATE);
			{ char *pE = szNew; while(*pE && *pE != '.' && (pE - szNew) < 120) pE++; *pE = 0; }
			if(!g_anyWarActive || strcmp(g_anyWarDesc, szNew) != 0)
			{
				strncpy_s(g_anyWarDesc, sizeof(g_anyWarDesc), szNew, _TRUNCATE);
				g_anyWarStartTick = GetTickCount();
				Log("[WAR] Genel savas basladi: %s", g_anyWarDesc);
			}
			g_anyWarActive = true;
		}
		if(strstr(pZoneMsg, "Your position has been set") != NULL) { g_lastPosOkTick = GetTickCount(); g_firstRejectTick = 0; }
		if(strstr(pZoneMsg, "dialog open") != NULL && (g_zoneScanIdx >= 0 || g_followActive || g_zgoActive || g_ztkActive || g_ghpActive || g_waitingForGroups || g_waitingForGStats) && g_firstRejectTick == 0)
		{
			g_firstRejectTick = GetTickCount();
			Log("[DLG-WATCH] Surekli red basladi, 5dk surerse reconnect.");
		}
		if(strstr(pZoneMsg, " defeated ") != NULL || strstr(pZoneMsg, "Results [") != NULL)
		{
			g_lastWarEndTick = GetTickCount();
			if(g_anyWarActive) Log("[WAR] Genel savas bitti.");
			g_anyWarActive = false;
			g_anyWarStartTick = 0;
			g_anyWarDesc[0] = 0;
		}
		{ // Savas sonucu chatten: doldur + kur, 15sn icinde dialog verisi gelmezse gonderilir
			if(g_wrArmedAt != 0 && !g_wrPosted && !g_zwKillListSent && g_wrWin[0] && g_wrScore[0])
			{
				g_wrPosted = true;
				char szOld[256];
				sprintf_s(szOld, sizeof(szOld), "%s defeated %s\n%s%s%s (chat sonucu)", g_wrWin, g_wrLose[0] ? g_wrLose : "?", g_wrScore, g_wrZone[0] ? " - " : "", g_wrZone);
				Log("[WAR] Yeni savas basladi, onceki sonuc hemen gonderiliyor: %s", szOld);
				SendKillList(szOld, "", g_wrWin, g_wrLose, "", "", 0, 0, !g_myGroupName.empty() && _stricmp(g_wrWin, g_myGroupName.c_str()) == 0, "", "", 0, 0, 0, "", "");
			}
			if(!strncmp(pZoneMsg, "ZONE WAR:", 9) && strstr(pZoneMsg, " defeated ") != NULL)
			{
				const char *pD = strstr(pZoneMsg, " defeated ");
				const char *pW = pZoneMsg + 9;
				while(*pW == ' ') pW++;
				size_t nW = (size_t)(pD - pW);
				if(nW > 0 && nW < sizeof(g_wrWin)) { memcpy(g_wrWin, pW, nW); g_wrWin[nW] = 0; }
				const char *pL = pD + 11;
				while(*pL == ' ') pL++;
				size_t nL = strcspn(pL, ".\r\n");
				if(nL > 0 && nL < sizeof(g_wrLose)) { memcpy(g_wrLose, pL, nL); g_wrLose[nL] = 0; }
				g_wrScore[0] = 0; g_wrZone[0] = 0; g_wrPosted = false; g_wrArmedAt = 0;
			}
			if(!strncmp(pZoneMsg, "ZONE WAR:", 9) && strstr(pZoneMsg, "Results [") != NULL)
			{
				const char *pR = strstr(pZoneMsg, "Results [") + 9;
				const char *pE = strchr(pR, ']');
				if(pE && (size_t)(pE - pR) < sizeof(g_wrScore)) { memcpy(g_wrScore, pR, (size_t)(pE - pR)); g_wrScore[pE - pR] = 0; }
				const char *pDash = pE ? strstr(pE, " - ") : NULL;
				if(pDash) { strncpy_s(g_wrZone, sizeof(g_wrZone), pDash + 3, _TRUNCATE); char *pZ = g_wrZone + strlen(g_wrZone); while(pZ > g_wrZone && (pZ[-1] == ' ' || pZ[-1] == '\r' || pZ[-1] == '\n' || pZ[-1] == '. ')) pZ--; *pZ = 0; }
				bool bOurs = !g_myGroupName.empty() && (strstr(pZoneMsg, g_myGroupName.c_str()) != NULL || (g_wrWin[0] && _stricmp(g_wrWin, g_myGroupName.c_str()) == 0) || (g_wrLose[0] && _stricmp(g_wrLose, g_myGroupName.c_str()) == 0));
				if(g_wrWin[0] && g_wrScore[0] && !g_wrPosted && bOurs)
				{
					g_wrArmedAt = GetTickCount();
					Log("[WAR] Chat sonucu kuruldu (15sn dialog verisi bekleniyor): %s vs %s.", g_wrWin, g_wrLose);
				}
			}
		}
		if(strstr(szNonColorEmbeddedMsg, "sufficient permissions") != NULL && GetTickCount() - g_zwLastGzAt < 5000)
		{
			if(!g_zwNoGzEntry)
			{
				g_zwNoGzEntry = true;
				Log("[ZONEWAR] /gz yetki reddi: bu savas GZsiz izlenecek (rank yetersiz olabilir).");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!GZye giremiyorum (yetki yok), savas disaridan izlenecek.", "!Can't enter GZ (no permission), watching from outside."));
			}
		}
		bool bOasisStarted = (strstr(pZoneMsg, "Started at ") != NULL && strstr(pZoneMsg, " vs. ") != NULL
			&& !g_myGroupName.empty() && strstr(pZoneMsg, g_myGroupName.c_str()) != NULL);
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
			Log("[ZONEWAR] Duyuru: '%s' vs '%s' - bizim sava?: %s", szLeft, szRight,
				bNamedOurWar ? "EVET" : "HAYIR");
			if(!bNamedOurWar)
				Log("[ZONEWAR] Baska grubun sava? duyurusu yoksayildi (%s).", pZoneMsg);
		}
		// Ciplak zone prompt u: sava?imiz AKTIFSE zone no sunu guncelle (yoksa yoksay)
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
							ZtkGotoZone(nNext);
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
				Log("[ZONEWAR] Aktif sava? zone #%s duyurusu alindi.", g_zwWarZone);
			}
		}
		if(bNamedOurWar || bOasisUnderAttack || bOasisAttacking || bOasisStarted)
		{
			// Duyurunun zone'sunu cikar: "Zone: '# 146'." -> "146"
			// Canli format "Zone: '# 157'" - týrnak sonrasi bo-sluk olabilir, atla.
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
			// Ayni savasin tekrar eden duyurusu: yoksay. (Yoksa g_zoneWarStartTick her
			// tekrarda sifirlanir ve ?gzt kalan sureyi yanlis hesaplar.) Savas zaten
			// aktifse gelen her yeni join duyurusu tekrar sayilir; zone tag'i uyu-smuyorsa
			// (bos tag dahil) da ayni savasin devami kabul edilir.
			bool bGeneralRepeat = (g_anyWarActive && g_anyWarDesc[0] && strstr(pZoneMsg, g_anyWarDesc) != NULL
				&& GetTickCount() - g_anyWarStartTick > 5000 && g_warRemainingSec >= 0);
			if((g_inZoneWar && g_zoneWarStartTick != 0
				&& (!szZoneTag[0] || !strcmp(g_zwWarZone, szZoneTag)))
				|| bGeneralRepeat)
			{
				// Tekrar duyurusu. Bot yeniden basladiysa / oturum ortasinda savasa
				// denk geldiyse timer hic cekilmemis olabilir (g_warRemainingSec<0):
				// o zaman kisa GZ giri? yapip timer'ý + takim rengini yeniden ??ren.
				if(g_warRemainingSec < 0 && !g_zwReturnPending)
				{
					g_warTimerTick = 0;
					Log("[ZONEWAR] Savas ortasinda timer bilinmiyor, kisa giris ile yeniden ogreniliyor.");
					ZwJoinCaptureReturn();
				}
				else
					Log("[ZONEWAR] Savas duyurusu tekrari yoksayildi (zone #%s aktif).", g_zwWarZone);
			}
			else
			{
				pAction = "join";
				g_zwNoGzEntry = false; // yeni savas: giris yasagi sifirla
				strncpy_s(g_zwWarZone, sizeof(g_zwWarZone), szZoneTag, sizeof(g_zwWarZone) - 1);
				// Savas baslangic anini kaydet: kalan sure = ZONE_WAR_TOTAL_MS - gecen sure.
				g_zoneWarStartTick = GetTickCount();
				g_inZoneWar = true;
				g_gztCountdownAnnounced = 999; // yeni savas: geri sayim esikleri sifirdan
				g_gztLastRem = -1;
				// Taze sayac: eski savastan kalan tick/kalan sure yeni savasi zehirlememeli.
				// Zehirsiz eski degerle geri sayim sifirdan biler "00:01" duyurup zinciri
				// tum savas boyunca susturuyordu (#gzt duyuru atmiyor vakasi).
				g_warRemainingSec = ZONE_WAR_TOTAL_MS / 1000;
				g_warTimerTick = g_zoneWarStartTick;
				g_warTimerViaTdw = false; // yeni savas: textdraw yakalanana kadar duyuru YOK
				memset(g_zwWelcomed, 0, sizeof(g_zwWelcomed)); // yeni savas: welcome tekrari olmasin
				// Yeni savas: onceki yakalayici/kill list durumunu sifirla
				// ONCE bekleyen kill list varsa (onceki savas sonucu) finalize et â€”
				// yoksa yeni savasin reset'i veriyi siler, sonuc Discord'a hic gitmez.
				if(g_zwCapturingKillList && g_zwKillTdCount > 0 && !g_zwKillListSent)
				{
					Log("[KILLLIST] Yeni savas basladi, onceki savasin kill listi finalize ediliyor...");
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
				g_zwWarKillCount = 0; // Top Weapons: yeni savas, yeni kill feed
				// Kill list textdraw'lari (Name/Kills/Assists/Deaths/Dmg) GZ'de
				// olmaya gerek kalmadan da gelebiliyor (SaveTextDrawData logladi).
				// Savas boyunca yakalayiciyi ACIK tut; bitince finalize et.
				// Oasis repo: kill list dialogdan gelir - textdraw yakalama cop toplayip Discord'a spam atiyordu.
			g_zwCapturingKillList = false;
				g_zwCaptureStart = GetTickCount();
				g_zwFinalizeAt = 0; // yeni savas: bitis finalize'i yeniden kurulacak
					g_zwEmptyFinalizeSince = 0; // yeni savas: bos-finalize sayaci sifirlanir
				Log("[KILLLIST] Yakalayici savas boyunca acik (kill list textdraw'lari toplanacak).");
				// Kisa GZ giris: takim rengimizi ogren (skorboard textdraw'inda kendi adimizin
				// rengi bizim takim rengidir). 4 sn iceride kal, sonra /pos ile geri don.
				g_warRemainingSec = -1;
				g_warTimerTick = 0;
				ZwJoinCaptureReturn();
				Log("[ZONEWAR] Savas basladi (zone #%s), sure sayaci baslatildi.", g_zwWarZone);
				// Kill list takim basliklari icin "X vs Y" duyurusundaki iki grubu sakla:
				// SOL grup = kirmizi, SAG grup = mavi (kullanici kurali). UIF duyurusu
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
				// Grubumuzun adini henuz bilmiyorsak ogrenme moduna gir.
				// Sonraki ilk "ZONE WAR: X keeps/takes..." mesajindaki X bizim grubumuzdur.
				if(g_myGroupName.empty() && !g_zwLearningGroup)
				{
					g_zwLearningGroup = true;
					g_zwLearnTick = GetTickCount();
					Log("[ZONEWAR] /gz2 bildirimi alindi, kendi grup adi ogrenilecek...");
				}
			}
		}

		// --- Zone sahipligi ogrenimi (SADECE isim bazli; RENK KULLANILMAZ) ---
		// "ZONE WAR: X takes over zone '# N'"  -> X o zone'un YENI sahibi: direkt bagla.
		// "ZONE WAR: X keeps zone '# N'"       -> X mevcut sahibi: direkt bagla.
		// (Eski akis takes-over'da RPC 108 RENK guncellemesi bekliyordu - renkle
		// isimiz yok; sunucu mesajinin kendisi yeterli ve aninda ogrenir.)
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
							g_ztkAttackPending = false; // bayat 20sn timeri zinciri oldurmesin
							// O zone bitti (kim alirsa alsin - grubun datasindan duser):
							// SONRAKI zone'a aninda gec + saldir (Faz1 /gz atar).
							Log("[ZSNIP] Hedef zone #%d savasi bitti (takes over), sonraki zone'a geciliyor.", zid);
							if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
							{
								g_snipZoneIdx++;
								int nNext = g_snipZones[g_snipZoneIdx];
								ZtkGotoZone(nNext);
								g_bridgeSuppressEcho++;
								char szMsg[128];
								sprintf_s(szMsg, sizeof(szMsg),
									LM("!Zone #%d bitti, sonraki zone #%d saldiriliyor...",
										"!Zone #%d ended, attacking next zone #%d..."), ZONE_SHOWN_FROM_IDX(zid), ZONE_SHOWN_FROM_IDX(nNext));
								sendChat(szMsg);
							}
							else
							{
								g_ztkActive = false;
								g_ztkZoneId = -1;
								g_snipZoneCount = 0;
								g_snipZoneIdx = 0;
								g_snipGroupName[0] = 0;
								g_bridgeSuppressEcho++;
																ResumeZoneScanAfterSnip();
								sendChat((char *)LM("!Sniper bitti.", "!Sniper finished."));
								Log("[ZSNIP] Zincir tamamlandi (baska zone yok).");
							}
						}
					}
				}
			}
		}

		if(strncmp(pZoneMsg, "ZONE WAR:", 9) == 0)
		{
			const char *pSz = strstr(pZoneMsg, " seized ");
			if(pSz)
			{
				char szWin[80] = {0}, szZName[80] = {0}, szLose[80] = {0};
				const char *pW = pZoneMsg + 9;
				while(*pW == ' ') pW++;
				size_t nW = (size_t)(pSz - pW);
				if(nW > 0 && nW < sizeof(szWin)) { memcpy(szWin, pW, nW); szWin[nW] = 0; }
				const char *pR2 = pSz + 8;
				const char *pFr = strstr(pR2, " from ");
				if(pFr)
				{
					size_t nZ = (size_t)(pFr - pR2);
					if(nZ > 0 && nZ < sizeof(szZName)) { memcpy(szZName, pR2, nZ); szZName[nZ] = 0; }
					const char *pL2 = pFr + 6;
					while(*pL2 == ' ') pL2++;
					size_t nLg = strcspn(pL2, ".\r\n");
					if(nLg > 0 && nLg < sizeof(szLose)) { memcpy(szLose, pL2, nLg); szLose[nLg] = 0; }
				}
				if(szWin[0] && szZName[0])
				{
					int zid = FindZoneByName(szZName);
					Log("[ZONEWAR] Seized: %s <- %s (%s), zone id=%d.", szZName, szWin, szLose[0] ? szLose : "?", zid);
					if(zid >= 0) BindZoneGroup(zid, szWin);
					bool bSnipTarget = (g_ztkActive && g_ztkZoneId >= 0 && g_ztkZoneId < MAX_GANG_ZONES
						&& (zid == g_ztkZoneId || (g_zoneNames[g_ztkZoneId][0] && !_stricmp(szZName, g_zoneNames[g_ztkZoneId]))));
					if(bSnipTarget)
					{
						g_ztkTargetWar = false;
						g_ztkAttackPending = false; // eski saldirinin 20sn timeri zinciri oldurmesin
						Log("[ZSNIP] Hedef zone savassiz el degistirdi (seized), sonraki zone'a geciliyor.");
						if(g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
						{
							g_snipZoneIdx++;
							int nNext = g_snipZones[g_snipZoneIdx];
							ZtkGotoZone(nNext);
							g_bridgeSuppressEcho++;
							char szMsg[128];
							sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d bitti, sonraki zone #%d saldiriliyor...", "!Zone #%d ended, attacking next zone #%d..."), ZONE_SHOWN_FROM_IDX(zid), ZONE_SHOWN_FROM_IDX(nNext));
							sendChat(szMsg);
						}
						else
						{
							g_ztkActive = false;
							g_ztkZoneId = -1;
							g_snipZoneCount = 0;
							g_snipZoneIdx = 0;
							ResumeZoneScanAfterSnip();
							sendChat((char *)LM("!Sniper bitti.", "!Sniper finished."));
							Log("[ZSNIP] Zincir tamamlandi (baska zone yok).");
						}
					}
				}
			}
		}
		if((pTail = strstr(pZoneMsg, "keeps zone")) != NULL) { pAction = "keeps"; }
		else if((pTail = strstr(pZoneMsg, "takes over")) != NULL) { pAction = "takes"; }
		else if((pTail = strstr(pZoneMsg, "wins against")) != NULL) { pAction = "wins"; }
		else if((pTail = strstr(pZoneMsg, "loses to")) != NULL) { pAction = "loses"; }
		else if((pTail = strstr(pZoneMsg, "defeated")) != NULL) { pAction = "defeated"; }
		else if((pTail = strstr(pZoneMsg, "Results [")) != NULL) { pAction = "results"; }

		// Ogrenme modundaysak ve "ZONE WAR: <Grup> ..." mesaji geldiyse, grup adini cek.
		// /gz2'den sonra gelen ilk zone mesaji bizim grubumuzun mesajidir.
		if(pAction && g_zwLearningGroup && (strncmp(pZoneMsg, "ZONE WAR:", 9) == 0) && strcmp(pAction, "defeated") != 0 && strcmp(pAction, "results") != 0)
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
					Log("[ZONEWAR] Kendi grup adi otomatik ogrenildi: %s", g_myGroupName.c_str());
				}
			}
			else
			{
				g_zwLearningGroup = false;
			}
		}

		// Kendi grubumuzun adini biliyorsak, mesajda o ad gecmiyorsa yoksay.
		// Grup adi henuz ogrenilmediyse (bos) hicbir zone mesaji yakalanmaz;
		// Kullanici: wins/loses her grup icin tetiklenirse botun savasi karisir.
		// Sadece botun grubunun adi mesaja dahilse isle (takes over dahil).
		bool bOasisEnd = (pAction && (!strcmp(pAction, "defeated") || !strcmp(pAction, "results")));
		if(pAction && !bJoinNotice)
		{
			if((!g_myGroupName.empty() && strstr(pZoneMsg, g_myGroupName.c_str()) == NULL) || (!bOasisEnd && g_myGroupName.empty()))
				pAction = NULL;
		}

		if(pAction)
		{
			DWORD dwNow = GetTickCount();
			// 60 sn anti-spam: YALNIZCA zone-war mesajlari bu zamanlayiciyi gunceller.
			// (?za/?gz kendi g_lastZoneWarTime'i kullanir â€” paylasilirsa "loses to"
			// war-end mesaji sessizce yutulur, savas hic bitmezdi.)
			if(g_lastZoneWarMsgTime == 0 || (dwNow - g_lastZoneWarMsgTime) >= 30000)
			{
				g_lastZoneWarMsgTime = dwNow;

// Savas bitti (wins/loses/takes over): sure sayacini sifirla.
				if(!strcmp(pAction, "wins") || !strcmp(pAction, "loses") || !strcmp(pAction, "takes over") || !strcmp(pAction, "defeated") || !strcmp(pAction, "results"))
				{
					// Sabit 10 dk kurali gecerli (kullanici onayi); sureyi olcmeye gerek yok.
					g_zoneWarStartTick = 0;
					g_inZoneWar = false;
					g_gztCountdownAnnounced = 999; // savas bitti: sonraki savas icin sifir
					g_gztLastRem = -1;
					g_zwWarZone[0] = 0; // sonraki savas yeni duyuru sayilir
					g_zwWarTeamLeft.clear();  // sonraki savas takimlari yeniden parse edilir
					g_zwWarTeamRight.clear();
					g_warRemainingSec = -1;
					g_warTimerTick = 0;
					g_warTimerViaTdw = false;

					// Kill list toplanacaksa GZ'de KAL (textdraw'lar bitis duyurusundan
					// sonra gelir); finalize edilince /fr ile donulur. Kill list yoksa
					// kisa giris donusu hemen yapilir.
if(g_zwCapturingKillList && !g_zwKillListSent)
				{
					// Bitis textdraw'larini bekle; g_zwReturnPending true kalir,
					// finalize sonrasi d-nüs (ProcessZoneWarAutoJoin) /fr g?nderir.
					g_zwFinalizeAt = GetTickCount() + 3000;
					Log("[KILLLIST] Savas bitti (%s), GZ'de kalinip sonuc textdraw'lari bekleniyor...", pAction);
				}
				else if(g_zwReturnPending)
				{
					g_zwReturnPending = false;
					Log("[ZONEWAR] Savas bitti, /fr ile freeroam'a donuluyor.");
					sendServerCommand((char *)"/fr");
				}

				// NOT: g_zwKillListSent burada AYARLANMAZ â€” g_zwFinalizeAt=+3sn
				// finalize'i tetikler; KillListSent'i erken true yapmak finalize'i
				// engelliyordu (sonuc Discord'a hic gitmiyordu). FinalizeKillList
				// isini bitirince kendisi true yapar.
					Log("[ZONEWAR] Savas bitti (%s), sure sayaci sifirlandi.", pAction);

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
							// "X loses" mesajinda bizim ad yoksa (rakip kaybetti) degil;
							// "loses" sonrasi bizim ad varsa kaybettik. YS format: "X loses to Y".
							const char *pLoses = strstr(pZoneMsg, "loses");
							if(pLoses && strstr(pLoses + 6, g_myGroupName.c_str()))
								bWeLost = true;
						}
						if(!bWeLost && strstr(pZoneMsg, "defeated"))
						{
							// Oasis: "ZONE WAR: X defeated Y" - Y bizsek kaybettik.
							const char *pDef = strstr(pZoneMsg, "defeated");
							if(pDef && strstr(pDef + 8, g_myGroupName.c_str()))
								bWeLost = true;
						}
						if(bWeLost)
						{
							g_zwWeWon = false;
							Log("[ZONEWAR] Grubumuz savasi KAYBETTI (%s).", pZoneMsg);
							g_bridgeSuppressEcho++;
							sendChat((char *)LM("!Your group lost the zone war!", "!Your group lost the zone war!"));
						}
					}
				}

				Log("[ZONEWAR] Chat mesaji yakalandi (%s): %s", pAction, pZoneMsg);
				// Discord zonewar kanalina embed gonderilmez (kullanici istegi: gereksiz spam).
				// Zone sahipligi ogrenimi (ZFU: takes over/keeps -> BindZoneGroup) calismaya devam eder.
			}
		}
	}

	// --- Welcome back dedektoru: "GROUP: X logged in" ---
	// Oyun ici gruplarda "X logged in" mesaji gelince Discord welcome kanalina bildir.
	{
		// Sistemsel ">> You logged in to your account": sunucu oturumu hatirliyor
		// (kullanici daha once girmis). Login beklemeyi KALDIRMAZ â€” sunucuya login
		// islemini bitirmesi icin 3 sn verilir; ancak ondan SONRA spawn akisi serbest.
		// (Not: bu mesaj tmp hesapta da geliyor; tek basina gercek login degildir.)
		if(strstr(szNonColorEmbeddedMsg, "You logged in to your account") && g_loginPending)
		{
			// Sunucu login'i islesin: 3 sn sonra akis devam etsin.
			if(g_loginPendingUntil == 0)
				g_loginPendingUntil = GetTickCount() + 3000;
			Log("[LOGIN] 'You logged in' mesaji alindi, 3 sn sonra akis devam edecek.");
		}
		const char *pLogged = strstr(szNonColorEmbeddedMsg, "logged in");
		// Sistemsel ">> You logged in to your account" mesaji greet TETIKLEMEZ:
		// yalnizca "GROUP: X(id) logged in" formatli grup uyesi girisleri.
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

				// OYUN ICINI otomatik greet (uL.Hold tarzi) - yalnizca ?greet ACIK ise.
				if(!g_welcomeEnabled)
				{
					Log("[WELCOME] Greet KAPALI, oyun ici mesaj atlanmadi (Discord da atlar).");
				}
				else
				{
					// - Savas varsa: " Welcome <isim>! We are in a ZONE WAR against <rakip>! Type /gz for backup!"
					// - Ozel mesaj ayarliysa (?welcomemsg): o mesaj gonderilir
					// - Bot dili TR ise: " Tekrar hos geldin <isim>!" / EN: " Welcome back <isim>!"
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
						sprintf_s(szGreet, sizeof(szGreet), LM("!Welcome %s! Grup savasindayiz! Yedek icin /gz yaz!", "!Welcome %s! We are in a ZONE WAR! Type /gz for backup!"), szName);
					else
						sprintf_s(szGreet, sizeof(szGreet), "!Welcome %s! We are in a ZONE WAR against %s! Type /gz for backup!", szName, strEnemy.c_str());
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
						sprintf_s(szGreet, sizeof(szGreet), LM("!Tekrar hos geldin %s!", "!Welcome back %s!"), szName);
					}
				}
g_bridgeSuppressEcho++;
				sendChat(szGreet);
				}
			}
		}
	}

	// WHISPER ?follow: "(Whisper) Isim(id): mesaj". SADECE whisperdan calisir.
	// ?follow = gondereni takip et, ?follow stop = dur. Hedef freeroamdan
	// cikarsa (stream duser) takip otomatik biter. Cevaplar "~" ile gider.
	if(!strncmp(szNonColorEmbeddedMsg, "(Whisper)", 9))
	{
		const char *pW = szNonColorEmbeddedMsg + 9;
		while(*pW == ' ') pW++;
		const char *pOp = strchr(pW, '(');
		const char *pCl = pOp ? strchr(pOp, ')') : NULL;
		const char *pColon = pCl ? strchr(pCl, ':') : NULL;
		char szWName[24] = {0}; int iWId = -1;
		if(pOp && pCl)
		{
			size_t nN = (size_t)(pOp - pW); if(nN > 23) nN = 23;
			memcpy(szWName, pW, nN); szWName[nN] = 0;
			iWId = atoi(pOp + 1);
		}
		const char *pWMsg = pColon ? pColon + 1 : "";
		while(*pWMsg == ' ') pWMsg++;
		char szWClean[128] = {0};
		strncpy_s(szWClean, sizeof(szWClean), pWMsg, _TRUNCATE);
		StripColorCodes(szWClean);
		bool bSelfW = false;
		if(szWName[0] && g_szNickName[0] && !_stricmp(szWName, g_szNickName)) bSelfW = true;
		if(!bSelfW && playerInfo[g_myPlayerID].szPlayerName[0] && !_stricmp(szWName, playerInfo[g_myPlayerID].szPlayerName)) bSelfW = true;
		bool bWAuth = !bSelfW && szWName[0]
			&& (!_stricmp(szWName, "Oreyoo") || !_stricmp(szWName, "fSc.Oreyoo") || !_stricmp(szWName, "hakixld") || !_stricmp(szWName, "cb650r"));
		if(bWAuth && !strncmp(szWClean, "?follow", 7) && (szWClean[7] == ' ' || szWClean[7] == '\0'))
		{
			const char *pFArg = szWClean + 7;
			while(*pFArg == ' ') pFArg++;
			if(!pFArg[0])
			{
				if(g_zoneScanIdx >= 0 || g_zgoActive || g_ztkActive || g_inZoneWar)
				{
					FollowWhisperReply(LM("mesgulum (tarama/savas), sonra dene.", "Busy (scan/war), try later."));
					Log("[FOLLOW] %s istedi ama mesgul, baslatilmadi.", szWName);
					return;
				}
				strncpy_s(g_followName, sizeof(g_followName), szWName, _TRUNCATE);
				g_followSlot = iWId;
				g_followMiss = 0;
				g_followLastTick = 0;
				g_followLastX = 0.0f; g_followLastY = 0.0f; g_followLastZ = 0.0f;
				g_followActive = true;
				FollowWhisperReply(LM("takip basladi.", "Following you."));
				Log("[FOLLOW] %s (%d) takip ediliyor.", szWName, iWId);
				return;
			}
			if(!_stricmp(pFArg, "stop"))
			{
				if(g_followActive) { g_followActive = false; FollowWhisperReply(LM("takip durdu.", "Stopped following.")); Log("[FOLLOW] %s istedi: durdu.", szWName); }
				else FollowWhisperReply(LM("takip etmiyorum.", "Not following anyone."));
				return;
			}
			FollowWhisperReply(LM("Kullanim: ?follow | ?follow stop", "Usage: ?follow | ?follow stop"));
			Log("[FOLLOW] %s bilinmeyen arguman, kullanim gonderildi.", szWName);
			return;
		}
	}
	// GUVENLIK KOMUTLARI: ?changepass / ?email â€” yalnizca PM ile ve yetkili isimlerden.
	// Format: ">> PM from <isim>(<id>) (tag): mesaj". ?help'te gosterilmez.
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
			&& (!_stricmp(szPmName, "Oreyoo") || !_stricmp(szPmName, "fSc.Oreyoo") || !_stricmp(szPmName, "hakixld") || !_stricmp(szPmName, "cb650r"));

		if(bAuth && !strncmp(pMsg, "?changepass", 11) && (pMsg[11] == ' ' || pMsg[11] == '\0'))
		{
			// Oasis: D1 "current password" + D2 "new password". Kullanim: ?changepass <mevcut> <yeni>
			const char *pVal = pMsg + 11;
			while(*pVal == ' ') pVal++;
			char szCur[64] = {0}, szNew[64] = {0};
			{
				const char *pSp = strchr(pVal, ' ');
				if(pSp)
				{
					size_t nC = (size_t)(pSp - pVal);
					if(nC > 63) nC = 63;
					memcpy(szCur, pVal, nC); szCur[nC] = 0;
					pSp++;
					while(*pSp == ' ') pSp++;
					strncpy_s(szNew, sizeof(szNew), pSp, _TRUNCATE);
				}
			}
			if(!szCur[0] || !szNew[0])
			{
				sendServerCommand((char *)"/r Kullanim: ?changepass <mevcut> <yeni>");
				return;
			}
			strncpy_s(g_secCmdValue, sizeof(g_secCmdValue), szCur, sizeof(g_secCmdValue) - 1);
			strncpy_s(g_secCmdValue2, sizeof(g_secCmdValue2), szNew, sizeof(g_secCmdValue2) - 1);
			g_secCmdIsEmail = false;
			g_secCmdPending = true;
			g_secCmdAt = GetTickCount() + 15000;
			g_secCmdCount = 0;
			Log("[SEC] ?changepass PM'de (yetkili: %s), deger: '%s'", szPmName, g_secCmdValue);
			sendServerCommand((char *)"/changepass");
			// Onay GRUBA degil PM yaniti (/r) olarak gider.
			sendServerCommand((char *)"/r Sifre degistirme menusu bekleniyor...");
			return;
		}
		if(bAuth && !strncmp(pMsg, "?email", 6) && (pMsg[6] == ' ' || pMsg[6] == '\0'))
		{
			// Oasis: D1 password + D2 email. Kullanim: ?email <sifre> <eposta>
			const char *pVal = pMsg + 6;
			while(*pVal == ' ') pVal++;
			bool bMailOk = false;
			{
				const char *pSp2 = strchr(pVal, ' ');
				if(pSp2)
				{
					size_t nP = (size_t)(pSp2 - pVal);
					char szPw[64] = {0}, szMail[128] = {0};
					if(nP > 63) nP = 63;
					memcpy(szPw, pVal, nP); szPw[nP] = 0;
					pSp2++;
					while(*pSp2 == ' ') pSp2++;
					strncpy_s(szMail, sizeof(szMail), pSp2, _TRUNCATE);
					if(szPw[0] && szMail[0])
					{
						strncpy_s(g_secCmdValue, sizeof(g_secCmdValue), szPw, sizeof(g_secCmdValue) - 1);
						strncpy_s(g_secCmdValue2, sizeof(g_secCmdValue2), szMail, sizeof(g_secCmdValue2) - 1);
						bMailOk = true;
					}
				}
			}
			if(!bMailOk)
			{
				sendServerCommand((char *)"/r Kullanim: ?email <sifre> <eposta>");
				return;
			}
			g_secCmdIsEmail = true;
			g_secCmdPending = true;
			g_secCmdAt = GetTickCount() + 15000;
			g_secCmdCount = 0;
			Log("[SEC] ?email PM'de (yetkili: %s), deger: '%s'", szPmName, g_secCmdValue);
			sendServerCommand((char *)"/email");
			sendServerCommand((char *)"/r E-posta menusu bekleniyor...");
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

	// ?ginv sonucu: sunucu SYSTEM cevabini gruba ilet (10 sn pencere).
	if(g_ginvPending)
	{
		if(GetTickCount() - g_ginvAt >= 10000) g_ginvPending = false;
		else if(!strncmp(szNonColorEmbeddedMsg, "SYSTEM:", 7))
		{
			const char *pR = szNonColorEmbeddedMsg + 7;
			while(*pR == ' ') pR++;
			if(strstr(pR, "Group already") || strstr(pR, "nvited") || strstr(pR, "not found")
				|| strstr(pR, "cannot") || strstr(pR, "Can't") || strstr(pR, "already"))
			{
				g_ginvPending = false;
				char szOut[192];
				sprintf_s(szOut, sizeof(szOut), "!Davet: %s", pR);
				g_bridgeSuppressEcho++;
				sendChat(szOut);
			}
		}
	}
	// Grup chat tespiti: "(GROUP) [20] isim(oyuncuID): mesaj"
	// KENDI oyuncunun ?w / ?pl / ?gstats komutlarini yakalar (cooldown ile).
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

			// "(GROUP) [20] isim(oyuncuID): mesaj" basligindan gonderici adini ogren.
			// playerInfo[yeniKatilan].szPlayerName sadece ServerJoin ile dolar; botun
			// baglanmasindan ONCE oyunda olanlar bos kalir. Bu yuzden ?pl cevabinda
			// bakilan kullanicinin adi g_szNickName'e (bot adi) dusuyordu. Grup
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
							g_playerCache[iSenderId].dwTime = GetTickCount();
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

				// Kendi grubumuzu ilk gordugumuzde kaydet
				if(iSenderId == g_myPlayerID && iGrp > 0)
					g_myGroupId = szGrpId;

				// Ayni grupta gordugumuz oyuncular -> grup uyesi
				if(g_myGroupId != "" && szGrpId == g_myGroupId)
					g_knownGroupMembers[iSenderId] = 1;
			}

			// Bot kendi sendChat ile gonderdigi mesaji (GROUP) olarak echo
			// edilirse tekrar islenmesin (cevap/otomatik mesajlar).
			// NOT: Oyun ici (GTA SA) sohbet kutusundan yazilan mesajlar sunucu
			// tarafindan gonderan oyuncunun ID'siyle yayinlanir. Kullanici
			// bot hesabi (hakixld) degil, oyun hesabiyla (orn. washe) yazdigi
			// icin iSenderId != g_myPlayerID olur. Bu yuzden sadece bot'un
			// KENDI echo'larini engelliyoruz, diger grup uyelerinin mesajlarini
			// HandleGroupCommand'e veriyoruz.
			if(iSenderId == g_myPlayerID) return;

			// Sunucu kendi mesajimizi (GROUP) olarak geri echo etmedigi icin komut
			// mantigi HandleGroupCommand'e tasindi (konsol inputu RunCommand'den de cagrilir).
			HandleGroupCommand(pGroupMsg, iSenderId);
		}
	}
	// Oasis grup chat: '! [G - 30] Name(id): mesaj'. UIF (GROUP) blogunun karsiligi.
	if(!strncmp(szNonColorEmbeddedMsg, "! [G - ", 7))
	{
		int iGrp = atoi(szNonColorEmbeddedMsg + 7);
		const char *pBrk = strchr(szNonColorEmbeddedMsg + 7, ']');
		const char *pOp = pBrk ? strchr(pBrk, '(') : NULL;
		const char *pCl = pOp ? strchr(pOp, ')') : NULL;
		const char *pColon = pCl ? strchr(pCl, ':') : NULL;
		if(iGrp > 0 && pBrk && pOp && pCl && pColon)
		{
			int iSenderId = atoi(pOp + 1);
			if(iSenderId >= 0 && iSenderId < MAX_PLAYERS)
			{
				const char *pGroupMsg = pColon + 1;
				while(*pGroupMsg == ' ') pGroupMsg++;
				if(pBrk + 1 < pOp)
				{
					const char *pNameStart = pBrk + 1;
					while(*pNameStart == ' ') pNameStart++;
					size_t nLen = (size_t)(pOp - pNameStart);
					if(nLen > 0 && nLen <= 24)
					{
						char szGrpName[64] = {0};
						memcpy(szGrpName, pNameStart, nLen);
						szGrpName[nLen] = 0;
						StripColorCodes(szGrpName);
						if(szGrpName[0])
						{
							playerInfo[iSenderId].iIsConnected = 1;
							strncpy_s(playerInfo[iSenderId].szPlayerName,
								sizeof(playerInfo[iSenderId].szPlayerName), szGrpName, _TRUNCATE);
							if(g_playerCache[iSenderId].dwTime == 0
								|| strcmp(g_playerCache[iSenderId].szName, szGrpName) != 0)
							{
								strncpy_s(g_playerCache[iSenderId].szName,
									sizeof(g_playerCache[0].szName), szGrpName, _TRUNCATE);
								g_playerCache[iSenderId].dwTime = GetTickCount();
							}
						}
					}
				}
				char szGrpId[16] = {0};
				sprintf_s(szGrpId, sizeof(szGrpId), "%d", iGrp);
				if(g_myGroupId.empty()) g_myGroupId = szGrpId;
				if(iSenderId == g_myPlayerID) return;
				if(szGrpId == g_myGroupId) g_knownGroupMembers[iSenderId] = 1;
				HandleGroupCommand(pGroupMsg, iSenderId);
			}
		}
	}
}

// Grup chat komutlarini lokal isler (?help, ?w, ?ping, ?pl, ?gst, ?gstats,
// ?greet, ?welcomemsg, ?gzt, ?discordchat, ?1414, ?gz ve !<mesaj>).
// Sunucu kendi mesajini (GROUP) olarak geri echo etmedigi icin bu mantik hem
// ClientMessage yakalamasindan hem de RunCommand (konsol inputu) icin cagrilir.
// iSenderId < 0 ise konsol/gelen sender yok anlaminda g_myPlayerID varsayilir.
void HandleGroupCommand(const char *pGroupMsg, int iSenderId)
{
			// ?post gerekce penceresi: ?post sonrasi 30 sn icinde yazilan normal grup
			// mesaji gerekce olarak yakalanir (komutlar haric).
			if(g_pushPending && g_pushRequester >= 0 && iSenderId == g_pushRequester && pGroupMsg[0] != '?' && pGroupMsg[0] != '!' && pGroupMsg[0] != '/' && pGroupMsg[0] != '\0')
			{
				strncpy_s(g_pushReason, sizeof(g_pushReason), pGroupMsg, sizeof(g_pushReason) - 1);
				Log("[POST] Gerekce alindi: '%s'", g_pushReason);
			}
			// /pm yaniti: hedef oyuncu grup chat'inde normal mesaj atarsa Discord'a ilet
			// (Discord'da /pm'i kullanan kisiye etiketli + ephemeral gosterilir).
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
			// mesaji: FPS dahil tam istatistiktir. /pnetstats FPS icermediginden bunu
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
					if(bNew)
						Log("[PL-CACHE] %d (%s) stats cache: loss %.2f%% fps %d ping %d",
							iPid, g_playerCache[iPid].szName[0] ? g_playerCache[iPid].szName : "?",
							g_playerCache[iPid].dLoss, g_playerCache[iPid].iFps, g_playerCache[iPid].iPing);

					// ?pl bekliyorsa ve hedef bu oyuncuysa cevabi simdi gonder:
					// /pnetstats dialogu "Network Active: 1" gibi BOS doner; asil
					// loss/fps/ping bu "Stats ->" mesajindadir (SAMPRPC dialogda loss
					// bulamazsa cevabi buraya birakir, flag acik kalir).
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
				Log("[HELP] ?help yakalandi, komut listesi gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Komutlar: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo ?grole <isim> <memb|co> ?grank <isim> <rank> ?ginv <id>", "!Commands: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo ?grole <name> <memb|co> ?grank <name> <rank> ?ginv <id>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Ayarlar: ?greet ?welcomemsg ?gm <mesaj> ?discordchat", "!Settings: ?greet ?welcomemsg ?gm <message> ?discordchat"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bilgi: ?info ?disc ?test ?m ?astats <id> ?alogs <id>", "!Info: ?info ?disc ?test ?m ?astats <id> ?alogs <id>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a mesaj: !<mesaj> yaz", "!Send Discord message: !<message>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Nasil: ?help", "!How: ?help"));
			}
			// --- ?test : baglanti kontrolu, bot cevap verir ---
			else if(!strncmp(pGroupMsg, "?test", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[TEST] ?test yakalandi, cevap gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bot calisiyor. ?help ile komutlari gor.", "!Bot is running. Use ?help to see commands."));
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
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?w)", "!Cooldown: %d secs left (?w)"), iLeft);
					Log("[WARHELP] Cooldown aktif (%d sn kaldi), ?w yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastWarHelpTime = dwNow;
				Log("[WARHELP] Grup chat'inde ?w yakalandi. %s Discord'a gonderiliyor...", pCmdSenderName);
				TriggerWarHelp(pCmdSenderName);
				// uL.Hold gibi gonderim onayi: "The call was sent to Discord."
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a gonderildi.", "!The call was sent to Discord."));
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
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?ping)", "!Cooldown: %d secs left (?ping)"), iLeft);
					Log("[PING] Cooldown aktif (%d sn kaldi), ?ping yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastPingTime = dwNow;
				Log("[PING] ?ping yakalandi, Pong cevabi gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Pong!");
			}
			// --- ?pl [id] : /stats Session (FPS/Ping/Packet Loss), 30 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?pl", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPLTime != 0 && (dwNow - g_lastPLTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastPLTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?pl)", "!Cooldown: %d secs left (?pl)"), iLeft);
					Log("[PNETSTATS] Cooldown aktif (%d sn kaldi), ?pl yoksayildi.", iLeft);
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
					// Baglanti kontrolu YOK: istenen id her zaman sorgulanir (UIF tum
					// oyuncularin istatistiklerini bilir). Bagliyiz sandigimiz kisi yanlis
					// olabilir (ServerJoin'i kacirmisiz); sunucu cozer.
					int i = atoi(pArg);
					if(i >= 0 && i < MAX_PLAYERS)
						iTarget = i;
				}

				g_plTargetId = iTarget;
				g_waitingForPNetStats = true;
				g_pnetstatsReqTime = GetTickCount();



				Log("[PNETSTATS] /stats gonderiliyor (hedef: %d)...", iTarget);
				char szStatsCmd[64];
				if(iTarget == g_myPlayerID)
					sprintf_s(szStatsCmd, sizeof(szStatsCmd), "/stats");
				else
					sprintf_s(szStatsCmd, sizeof(szStatsCmd), "/stats %d", iTarget);
				sendServerCommand(szStatsCmd);
				// /pnetstats AKSIYONU YOK (Oasis /stats Session hepsini verir).
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
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gst)", "!Cooldown: %d secs left (?gst)"), iLeft);
					Log("[GST] Cooldown aktif (%d sn kaldi), ?gst yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGst = true;
				g_gstReqTime = GetTickCount();
				Log("[GST] /gstats gonderiliyor (zone + aktif oyuncu sayisi icin)...");
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
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gstats)", "!Cooldown: %d secs left (?gstats)"), iLeft);
					Log("[GSTATS] Cooldown aktif (%d sn kaldi), ?gstats yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
				g_gstatsSentFirst = false;
				g_gstatsRequesterId = ""; // oyun ici istek: Discord'a gonderilmez
				Log("[GSTATS] /gstats gonderiliyor...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?greet [on|off] : welcome back mesajlarini ac/kapat (DB'de saklanir).
			// Argsiz = toggle. Sonuc GRUPTA bildirilir (acik/kapali).
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
					// Per-user self: gonderen kisi kendi adina mesaj ayarlar
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
					SyncBotStatusToDb(); // gruba gore DB'ye kaydet
					Log("[GZT] Timer ON.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: ACIK. Son 10/5/3/1 saniyede duyuru yapilacak.", "!War timer: ON. Announcements at last 10/5/3/1 secs."));
					return;
				}
				if(!_stricmp(pArg, "off"))
				{
					g_gztOn = false;
					SyncBotStatusToDb(); // gruba gore DB'ye kaydet
					Log("[GZT] Timer OFF.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: KAPALI. Duyuru yapilmayacak.", "!War timer: OFF. No announcements."));
					return;
				}

				// ?gzt time: su anki savas suresini goster - cooldown yok.
				if(_stricmp(pArg, "time") != 0 && pArg[0])
				{
					Log("[GZT] Bilinmeyen arguman: '%s' (on/off/time).", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?gzt | ?gzt time | ?gzt off", "!Usage: ?gzt | ?gzt time | ?gzt off"));
					return;
				}

				// Argumansiz ?gzt: durumu bildir (KAPALI ise "kapali" uyarisi). "?gzt time"
				// acik olsa da olmasa da su anki sureyi gosterir (kullanici istegi).
				bool bExplicitTime = !_stricmp(pArg, "time");
				if(!g_gztOn && !bExplicitTime)
				{
					Log("[GZT] Argumansiz ama timer KAPALI, 'kapali' uyarisi gonderildi.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War timer: KAPALI. Acmak icin ?gzt on | sure: ?gzt time",
						"!War timer: OFF. Enable with ?gzt on | time: ?gzt time"));
					return;
				}
				DWORD dwNow = GetTickCount();
				// Kalan sure: oncelikle yakalanan GZ timer textdraw'ina dayanir
				// (gercek kalan sure; chat duyuru ani ile savas basi ayni degil).
				// Yoksa baslangic tick'inden hesaplanan deger kullanilir.
				int iRemainingSec = -1;
				if(!g_inZoneWar)
				{
					if(g_anyWarActive && g_anyWarStartTick != 0 && dwNow - g_anyWarStartTick < ZONE_WAR_TOTAL_MS)
					{
						int iElapsed = (int)((dwNow - g_anyWarStartTick) / 1000);
						int iRem = (int)(ZONE_WAR_TOTAL_MS / 1000) - iElapsed;
						if(iRem < 0) iRem = 0;
						char szAny[256];
						sprintf_s(szAny, sizeof(szAny), LM("!War time: %d sn kaldi (%s)", "!War time: %d sec left (%s)"), iRem, g_anyWarDesc[0] ? g_anyWarDesc : "?");
						Log("[GZT] Baska grubun savasi: %s", szAny);
						g_bridgeSuppressEcho++;
						sendChat(szAny);
						return;
					}
					Log("[GZT] Savas aktif degil, War time bilgisi gonderilemiyor.");
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
					// Savas baslangici bilinmiyor ya da coktan bitti.
					Log("[GZT] Savas baslangici yok, War time bilgisi gonderilemiyor.");
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
					Log("[GUILDID] Arguman yok, kullanim: ?guildid <discord sunucu id>");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?guildid <discord sunucu id>",
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
					Log("[GUILDID] Baglanti basarisiz: %s", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Baglanti basarisiz: sunucu botu eklememis olabilir.",
						"!Link failed: the guild may not have added the bot."));
				}
			}
			// --- ?discordchat : Discord grup chat kopusunu ac/kapat (MongoDB'de saklanir) ---
			else if(!strncmp(pGroupMsg, "?discordchat", 12) && (pGroupMsg[12] == '\0' || pGroupMsg[12] == ' '))
			{
				Log("[GROUPCHAT] ?discordchat komutu yakalandi, durum degistiriliyor...");
				ToggleDiscordChat();
			}
			// --- ?m : game-modes listesi sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?m", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				Log("[MODES] ?m yakalandi, /gamemodes gonderiliyor.");
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
				QueryAdminLogSummary(ResolveAlogQuery(pGroupMsg + 6));
			}
			// --- ?alog <id|accid|isim> : DB'deki admin loglarindan sayi ozeti ---
			else if(!strncmp(pGroupMsg, "?alog", 5) && strncmp(pGroupMsg, "?alogs", 6) != 0 && (pGroupMsg[5] == ' ' || pGroupMsg[5] != '\0'))
			{
				Log("[ALOG] ?alog yakalandi, DB sorgusu baslatiliyor.");
				QueryAdminLogSummary(ResolveAlogQuery(pGroupMsg + 5));
			}
			// --- ?gname : kendi grup adini YENIDEN ogren (1 kerelik /stats -> Group
			// butonu akisi; ogrenileni DB'ye yazar). Grup degistiginde kullanilir. ---
			else if(!strncmp(pGroupMsg, "?gname", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				Log("[GNAME] ?gname yakalandi, grup adi yeniden ogreniliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Grup adi yeniden ogreniliyor...", "!Re-learning group name..."));
				// Mevcut adi sifirla -> SyncZscanFromDb yeniden ogrenir (DB'de yoksa
				// asagida tek seferlik /gstats ile ogrenilir).
				g_myGroupName.clear();
				g_zscanDbLoaded = false;
				g_waitingForGroupName = false;
				g_groupNamePhase = 0;
				s_gnameMode = 0; // grup degisti: once /gstats dene
				// DB'deki eski adi da temizle ki ogrenilen taze ad yazilsin
				try
				{
					httplib::Client cli(g_szBackendBase);
					cli.set_connection_timeout(3);
					json body = {{"name", g_szNickName}, {"data", json::object()}, {"groupName", ""}};
					cli.Post("/api/bot/status", body.dump(), "application/json");
				}
				catch(...) {}
				// Tek seferlik ogrenme: /gstats (bos submit -> kendi grubu), faz 2 isler.
				g_waitingForGroupName = true;
				g_groupNamePhase = 2;
				g_groupNameReqTime = GetTickCount();
				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
				g_gstatsAutoSubmit = true;
				g_gstatsInput = "";
				g_gstatsRequesterId = "";
				g_gstatsSentFirst = false;
				Log("[GSTATS] ?gname: grup adi icin /gstats gonderiliyor...");
				sendServerCommand((char *)"/gstats");
				g_bridgeSuppressEcho++;
			}
			// --- ?post [gerekce] : son grup aksiyonunu (rol/rank/kick) Discord log kanalina embed gonder ---
			// Gerekce inline yazilmazsa 30 sn beklenir; bu surede yazilan grup mesaji gerekce olur.
			else if(!strncmp(pGroupMsg, "?post", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				if(!g_lastGlogTarget[0])
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Log yok: once bir rol/rank/kick degisikligi yakalanmali.", "!No log: a role/rank/kick change must be detected first."));
				}
				else
				{
					const char *pReason = pGroupMsg + 5;
					while(*pReason == ' ') pReason++;
					if(*pReason != '\0')
					{
						Log("[POST] ?post gerekceli: %s", pReason);
						g_pushRequester = iSenderId;
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
								for(int k = 0; szName[k] && nl < 79; k++){ if(szName[k] == '\'') continue; szNeedL[nl++] = ((szName[k]>='A'&&szName[k]<='Z')?szName[k]+32:szName[k]); }
								szNeedL[nl] = 0;
								int iFound = -1;
								for(int i = 0; i < g_groupColorCount; i++)
								{
									if(!g_groupColors[i].szName[0]) continue;
									char szDbL[80]; int dl = 0;
									for(int k = 0; g_groupColors[i].szName[k] && dl < 79; k++){ if(g_groupColors[i].szName[k] == '\'') continue; szDbL[dl++] = ((g_groupColors[i].szName[k]>='A'&&g_groupColors[i].szName[k]<='Z')?g_groupColors[i].szName[k]+32:g_groupColors[i].szName[k]); }
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
			// --- ?zlist / ?zwho : bulundugu zone bilgisi -Ã¥Ã† /gzinfo (hangi grup, ne zaman bosalir) ---
			else if((!strncmp(pGroupMsg, "?zlist", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			     || (!strncmp(pGroupMsg, "?zwho", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' ')))
			{
				Log("[ZONE] %s yakalandi, /gzinfo gonderiliyor.", pGroupMsg);
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?zscan on/off: otomatik aralikli tarama. ?zscan on -> taramayi baslat +
			// aralikli tekrarla; ?zscan off -> durdur. Argumansiz -> durumu bildir. ---
			else if(!strncmp(pGroupMsg, "?zscan", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				const char *pArg = pGroupMsg + 7;
				while(*pArg == ' ') pArg++;
				if(!_stricmp(pArg, "on"))
				{
					g_zscanAuto = true;
					// ?zsnip sonrasi birakilan duraklamayi kaldir (kullanici bilerek aciyor).
					if(g_zscanPausedForSnip)
					{
						g_zscanPausedForSnip = false;
						g_zscanPausedIdx = -1;
						g_zsnipeHoldScan = false;
						Log("[ZF:SCAN] ?zscan on: sniper duraklamasi kaldirildi.");
					}
					SyncBotStatusToDb(); // gruba gore DB'ye kaydet
					g_zoneScanLastRun = 0; // ?zscan on = hemen tara
					// g_zoneScanLastRun sifirlandi: periyodik kapi bir sonraki tickte
					// ateslenir (savas/harvest/spawn korumasi gecerliyse). Ac/kapa spaminda
					// her seferinde bastan baslamamak icin kapi sartlari aynen gecerli.
					g_bridgeSuppressEcho++;
					if(g_inZoneWar)
						// Savas sirasinda tarama bilin-gli bekletilir (teleport = savas bugu);
						// kullaniciya S-Ã»YLE, sessiz no-op olmasin.
						sendChat((char *)LM("!Zone war aktif - tarama savas bitince otomatik baslayacak.",
							"!Zone war active - scan will start automatically after the war."));
					else
						sendChat((char *)LM("!Zone tarama: ACIK (periyodik).", "!Zone scan: ON (periodic)."));
				}
				else if(!_stricmp(pArg, "off"))
				{
					g_zscanAuto = false;
					g_zoneScanIdx = -1;   // aktif taramayi da durdur (kuyruk temizlenir)
					g_zoneScanQ = 0;
					SyncBotStatusToDb(); // gruba gore DB'ye kaydet
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Zone tarama: KAPALI.", "!Zone scan: OFF."));
				}
				else if(!_stricmp(pArg, "sn"))
				{
					// Kullanici bilerek tam tarama istedi: aktif sniper yoksa duraklamayi kaldir.
					if(g_zscanPausedForSnip && !g_ztkActive)
					{
						g_zscanPausedForSnip = false;
						g_zscanPausedIdx = -1;
						g_zsnipeHoldScan = false;
						Log("[ZF:SCAN] ?zscan sn: sniper duraklamasi kaldirildi.");
					}
					// Tek seferlik TAM tarama: tum zonalar dogrulanir, bitince DB
					// senkronu tek-sahipligi kurar (yeni gruba yaz, eskiden sil).
					if(g_zscanPausedForSnip)
					{
						g_forceFullVerifyOnce = true;
						Log("[ZF:SCAN] ?zscan sn kuyruga alindi (sniper bitince tam kontrol).");
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Sniper bitince tam kontrol yapilacak.", "!Full verification runs after the sniper."));
					}
					else if(g_zoneScanIdx >= 0 || !iSpawned || g_inZoneWar)
					{
						Log("[ZF:SCAN] ?zscan sn atlandi (scanAktif=%d spawned=%d war=%d).", g_zoneScanIdx >= 0, iSpawned, g_inZoneWar);
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Simdi tarama yapilamiyor (mesgul).", "!Cannot scan right now (busy)."));
					}
					else
					{
						g_forceFullVerifyOnce = true;
						Log("[ZF:SCAN] ?zscan sn: tek seferlik tam kontrol baslatiliyor.");
						StartZoneScan(true);
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Tam kontrol basliyor (?zscan sn).", "!Full verification starting (?zscan sn)."));
					}
				}
				else
				{
					// Argumansiz/bilinmeyen: durumu bildir + kullanim.
					Log("[ZONE] ?zscan yakalandi (arg='%s'), durum bildiriliyor.", pArg);
					g_bridgeSuppressEcho++;
					if(pArg[0] && _stricmp(pArg, "on") && _stricmp(pArg, "off"))
						sendChat((char *)LM("!Kullanim: ?zscan on | ?zscan off | ?zscan sn", "!Usage: ?zscan on | ?zscan off | ?zscan sn"));
					else
						sendChat((char *)LM(g_zscanAuto ? "!Zone tarama: ACIK." : "!Zone tarama: KAPALI.",
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
				{
					int zTry = ZONE_IDX_FROM_SHOWN(atoi(pArg));
					if(zTry >= 0 && zTry < g_zoneCount) iZone = zTry;
					// else: sayi gecerli zone degilse tag/isim olarak dene ("737" tagi gibi)
				}
				if(iZone < 0)
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
					// secim yanlis zone gonderir; tam ISIM ile netlestir.
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
					PauseZoneScanForSnip(); // tarama /pos kapismamasi icin duraklat (donuste devam)
					g_zgoZoneId = iZone;
					g_zgoReturnTick = GetTickCount() + ZGO_STAY_MS;
					sendServerCommand(szCmd);

					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d de. /go yazarak yanima isinlanabilirsin, 40 sn sonra donuyorum.",
				"!At zone #%d. Type /go to teleport to me, returning in 40 sec."), ZONE_SHOWN_FROM_IDX(iZone));
					g_bridgeSuppressEcho++;
					sendChat(szMsg);
				}
				else
				{
					// Tag/isim bilinmiyorsa: kendi flash taramamizla (teleport + /gzinfo)
					// ogrenmesini baslat; ?zgo tekrar deneyince bulur.
					Log("[ZONE] ?zgo %s icin zone bulunamadi (grup bilinmiyor), flash tarama baslatiliyor.", pArg);
					StartFlashScan();
					g_bridgeSuppressEcho++;
					char szMsg[160];
					sprintf_s(szMsg, sizeof(szMsg),
						LM("!%s icin zon taraniyor, ~30 sn sonra ?zgo %s tekrar dene.",
						   "!Scanning zones for %s, retry ?zgo %s in ~30 sec."), pArg, pArg);
					sendChat(szMsg);
				}
			}
			// --- ?zgo (argumansiz) : zone listesi bilgisi ---
			else if(!strncmp(pGroupMsg, "?zgo", 4) && (pGroupMsg[4] == '\0'))
			{
				Log("[ZONE] ?zgo (argumansiz), /gzinfo gonderiliyor.");
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?za / ?zattack [<zoneid | gruptag>] : zone saldirisi - id veya tag ile
// hedef zonu bulup gider, sonra /gz gonderir (bot co-leader/leader olmali; yetki sunucuda). ---
			else if((!strncmp(pGroupMsg, "?za", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			     || (!strncmp(pGroupMsg, "?zattack", 8) && (pGroupMsg[8] == '\0' || pGroupMsg[8] == ' ')))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?za)", "!Cooldown: %d secs left (?za)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kaldi), ?za yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONE] %s yakalandi. /gz sunucu komutu gonderiliyor...", pGroupMsg);
				// <arg> verildiyse hedefi bul: sayisal = zone id, harf = grup tag'i.
				const char *pA = pGroupMsg + (strncmp(pGroupMsg, "?zattack", 8) == 0 ? 8 : 3);
				while(*pA == ' ') pA++;
				if(pA[0])
				{
					int iZone = -1;
					if(pA[0] >= '0' && pA[0] <= '9')
						iZone = atoi(pA); // kullanici belirli zone id'sine gitsin istedi
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
						Log("[ZONE] %s -> zone #%d, /pos gonderildi + /gz.", pA, iZone);
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
			// Hedef zonu bul, oraya isinla; o zonede savas SURUYORSA bitisini bekle,
			// bitince /gz ile saldir (zone sniper). Id (sayi) veya isim/tag ile calisir.
			else if(!strncmp(pGroupMsg, "?zsnip", 6) && (pGroupMsg[6] == ' ' || pGroupMsg[6] == '#' || pGroupMsg[6] != '\0'))
			{
				const char *pArg = pGroupMsg + 6;
				while(*pArg == ' ' || *pArg == '#') pArg++;
				// off: bekleyen istegi/bekleyen yetki kontrolunu iptal edebilmeli, gecir.
				if(g_snipPermPending && _stricmp(pArg, "off") != 0)
				{
					Log("[ZSNIP] Onceki yetki kontrolu hala suruyor, yeni istek yoksayildi.");
					return;
				}
				// ?zsnip off: aktif sniper'i iptal et (uL.Hold: "Aborted by user. ZTK disabled.")
				// off da Co-Leader/Leader yetkisi gerektirir: /gmembers ile dogrulanir.
				if(!_stricmp(pArg, "off"))
				{
					g_zsnipeHoldScan = false; // off: tarama normale doner
					// Bekleyen (?yes/?no bekleyen) ?zsnip istegi varsa: off yetkisiz iptal eder.
					if(g_snipInterrupt)
					{
						g_snipInterrupt = false;
						g_snipPermPending = false;
						g_waitingForGmembers = false;
						g_snipZone = -1;
						ResumeZoneScanAfterSnip();
						Log("[ZSNIP] off: bekleyen istek iptal, zone taramasi devam ediyor.");
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Sniper istegi iptal, zone taramasi devam ediyor.",
							"!Pending sniper cancelled, zone scan continues."));
						return;
					}
					g_snipAction = 1;
					g_snipZone = -1;
					g_snipSenderId = iSenderId;
					g_snipPermPending = true;
					g_waitingForGmembers = true;
					g_gmembersReqTime = GetTickCount();
					GmembersReset();
					Log("[ZSNIP] off icin yetki kontrolu: /gmembers gonderiliyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Yetki kontrol ediliyor (/gmembers)...", "!Checking permission (/gmembers)..."));
					sendServerCommand((char *)"/gmembers");
					return;
				}
				if(g_ztkActive || g_snipInterrupt || g_snipPermPending)
				{
					Log("[ZSNIP] Zaten aktif sniper var, yeni istek yoksayildi.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Sniper zaten aktif, durdurmak icin ?zsnip off.", "!Sniper already active, use ?zsnip off to stop."));
					return;
				}
				int iZone = -1;
				g_snipZoneCount = 0; g_snipZoneIdx = 0; g_snipGroupName[0] = 0;
				if(pArg[0] >= '0' && pArg[0] <= '9')
				{
					int zTry = ZONE_IDX_FROM_SHOWN(atoi(pArg));
					if(zTry >= 0 && zTry < g_zoneCount) iZone = zTry;
					// else: sayi gecerli zone degilse tag/isim olarak dene ("737" tagi gibi)
				}
				if(iZone < 0)
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
					Log("[ZSNIP] Tag '%s' birden fazla gruba ait, sniper baslatilmadi.", pArg);
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
					Log("[ZSNIP] %s icin zone bulunamadi, sniper baslatilmadi.", pArg);
					char szNF[96];
					sprintf_s(szNF, sizeof(szNF), LM("!Zone bulunamadi: %s", "!Zone not found: %s"), pArg);
					g_bridgeSuppressEcho++;
					sendChat(szNF);
					return;
				}
				// Yetki kapisi: /gmembers ile bot + kullanici Co-Leader/Leader mi?
				// Onay gelince (OnGmembersCaptured) sniper baslatilir; degilse reddedilir.
				// Kullanici istegi: ?zsnip, aktif zone taramasini HEMEN durdurur
				// (taramanin /pos teleportlari sniperin /pos + /gz akisiyla cakisiyor).
				// Onay gelirse sniper baslar; ?no gelirse tarama kaldigi yerden devam eder.
				PauseZoneScanForSnip();
				CloseOpenListDlg();
				g_snipInterrupt = true; // istek beklemede: ?yes saldirir, ?no taramaya doner
				g_snipAction = 0;
				g_snipZone = iZone;
				g_snipSenderId = iSenderId;
				g_snipPermPending = true;
				g_waitingForGmembers = true;
				g_gmembersReqTime = GetTickCount();
				GmembersReset();
				Log("[ZSNIP] Yetki kontrolu: /gmembers gonderiliyor (zone #%d, istek: %s).", iZone, pArg);
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Yetki kontrol ediliyor (/gmembers)...", "!Checking permission (/gmembers)..."));
				sendServerCommand((char *)"/gmembers");
			}
			// --- ?yes : zsnip zincirinde siradaki zone'a saldir ---
			else if(!strncmp(pGroupMsg, "?yes", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				// Kesilmis tarama uzerine gelen ?yes: bekleyen ?zsnip hedefine saldir.
				if(g_snipInterrupt && g_snipZone >= 0 && g_snipZone < g_zoneCount)
				{
					g_snipInterrupt = false;
					g_snipPermPending = false;
					g_waitingForGmembers = false;
					int iZoneY = g_snipZone;
					g_snipZone = -1;
					ZtkGotoZone(iZoneY);
					g_ztkActive = true;
					Log("[ZSNIP] ?yes: bekleyen zone #%d hedeflendi, sniper basladi.", iZoneY);
					g_bridgeSuppressEcho++;
					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg),
						LM("!Zone #%d hedeflendi, savas aciliyor...", "!Zone #%d targeted, starting war..."), ZONE_SHOWN_FROM_IDX(iZoneY));
					sendChat(szMsg);
				}
				else if(g_ztkAskNext && g_snipZoneCount > 0 && g_snipZoneIdx < g_snipZoneCount)
				{
					if(g_inZoneWar)
					{
						g_ztkYesPending = true;
						Log("[ZSNIP] ?yes savas bitimine ertelendi.");
						g_bridgeSuppressEcho++;
						sendChat((char *)LM("!Tamam, savas bitince gecilecek.", "!OK, moving after the war."));
						return;
					}
					g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
					g_ztkYesPending = false;
					g_ztkPreAsked = false;
					int iZone = g_snipZones[g_snipZoneIdx];
					ZtkGotoZone(iZone);
					PauseZoneScanForSnip();
					g_ztkActive = true;
					Log("[ZSNIP] ?yes: siradaki zone #%d hedeflendi.", iZone);
					g_bridgeSuppressEcho++;
					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg),
						LM("!Zone #%d hedeflendi, savas bitisi bekleniyor...", "!Zone #%d targeted, waiting for the war to end..."), ZONE_SHOWN_FROM_IDX(iZone));
					sendChat(szMsg);
				}
				else
				{
					Log("[ZSNIP] ?yes: bekleyen zincir sorusu yok.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Bekleyen zincir sorusu yok (?zsnip ile baslat).", "!No pending chain question (start with ?zsnip)."));
				}
			}
			// --- ?no : zsnip zincirini iptal et ---
			else if(!strncmp(pGroupMsg, "?no", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				g_zsnipeHoldScan = false; // ?no: tarama normale doner
				// Kesilmis tarama uzerine gelen ?no: istegi iptal et, taramayi devam ettir.
				if(g_snipInterrupt)
				{
					g_snipInterrupt = false;
					g_snipPermPending = false;
					g_waitingForGmembers = false;
					g_snipZone = -1;
					ResumeZoneScanAfterSnip();
					Log("[ZSNIP] ?no: istek iptal edildi, zone taramasi devam ediyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Sniper iptal, zone taramasi devam ediyor.",
						"!Sniper cancelled, zone scan continues."));
				}
				else if(!g_ztkAskNext)
				{
					Log("[ZSNIP] ?no: bekleyen zincir sorusu yok.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Bekleyen zincir sorusu yok.", "!No pending chain question."));
				}
				else
				{
					g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
					g_snipZoneCount = 0;
					g_snipZoneIdx = 0;
					g_snipGroupName[0] = 0;
					ResumeZoneScanAfterSnip();
					Log("[ZSNIP] ?no: zincir iptal edildi.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Sniper zinciri iptal edildi.", "!Sniper chain cancelled."));
				}
			}
			// --- ?zgrp [on|off] / ?tagscan : grup taglarini /groups + /gstats <ID> ile
			// ogrenir (manuel tag taramasi). "on/off" oto harvest anahtari (DB'de kalici);
			// argsiz = hemen calistir. (?tagscan = manuel tetikleme.) ---
			else if(!strncmp(pGroupMsg, "?zgrp", 5) || !strncmp(pGroupMsg, "?tagscan", 8))
			{
				const char *pArg = pGroupMsg + (!strncmp(pGroupMsg, "?tagscan", 8) ? 8 : 5);
				while(*pArg == ' ') pArg++;
				if(!_stricmp(pArg, "on") || !_stricmp(pArg, "off"))
				{
					g_zgrpAuto = _stricmp(pArg, "on") == 0;
					if(g_zgrpAuto) g_ghpForce = true;
					SyncBotStatusToDb(); // gruba gore DB'ye kaydet
					Log("[GHP] Oto tag harvest %s.", g_zgrpAuto ? "ACIK (15 dk periyodik)" : "KAPALI");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM(g_zgrpAuto ? "!Oto tag toplama: ACIK (15 dk periyodik)." : "!Oto tag toplama: KAPALI.",
						g_zgrpAuto ? "!Auto tag harvest: ON (15 min periodic)." : "!Auto tag harvest: OFF."));
					if(g_zgrpAuto)
					{
						// Tek seferlik tetik: g_ghpLastRun sifirlanir, ProcessZoneScan
						// spawn'da oldugu gibi bir kez harvest baslatir.
						g_ghpLastRun = 0;
						g_ghpLearnedCount = 0;
						// ?zgrp on A?IK secildi: 60 sn spawn korumasi gectiyse harvest i
						// HEMEN ba?lat (otomatik kapidan beklemek yerine). Zone taramasi
						// aktifse durdur - yoksa tarama /gzinfo dialogu harvest /gstats
						// zinciriyle cakisip ikisi de takilirdi.
						if(g_spawnTick != 0 && (GetTickCount() - g_spawnTick) >= 60000 && !g_ghpActive && !g_inZoneWar && !g_zgoActive)
						{
							if(g_zoneScanIdx >= 0)
							{
								Log("[GHP] ?zgrp on: aktif zone taramasi durduruldu, harvest ?ncelikli.");
								g_zoneScanIdx = -1;
								g_zoneScanQ = 0;
							}
							Log("[GHP] ?zgrp on: harvest hemen ba?latiliyor (/groups).");
							g_bridgeSuppressEcho++;
							sendChat((char *)LM("!Grup listesi ??reniliyor...", "!Learning group list..."));
							g_ghpLearnedCount = 0;
							SyncGroupColorsToDb();
							g_waitingForGroups = true;
							g_groupsReqTime = GetTickCount();
							sendServerCommand((char *)"/groups");
						}
						else
							Log("[GHP] ?zgrp on: otomatik kapi bekleniyor (60 sn koruma / sava? / aktif i?lem).");
					}
				}
				else
				{
					Log("[GHP] %s: /groups cagriliyor...", !strncmp(pGroupMsg, "?tagscan", 8) ? "?tagscan" : "?zgrp");
					g_ghpForce = true; // elle tarama: tam liste
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Grup listesi ogreniliyor...", "!Learning group list..."));
					g_ghpLearnedCount = 0;
					// Mevcut txt'teki (zaten ogrenilmis) tag'leri hemen DB'ye yaz.
					SyncGroupColorsToDb();
					g_waitingForGroups = true;
					g_groupsReqTime = GetTickCount();
					sendServerCommand((char *)"/groups");
				}
			}
			// --- ?zf <#N> : zone durumu sorgula -Ã¥Ã† /gzinfo + flashing zone dagilimi ---
			else if(!strncmp(pGroupMsg, "?zf", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				Log("[ZONE] ?zf yakalandi, mevcut flashing verisiyle aninda ozet gonderiliyor.");
				// Ozet gonderilmeden once DB'deki guncel tag'leri cek: tag'ler sonradan
				// gelisebilir (baska bot/gec harvest). LoadGroupsFromDb sadece ekleme
				// yapar ve bos tag'i dolu tag'e yazmaz; burada senkron cekim, ?zf'nin
				// isim yerine tag gostermesini saglar. (DB tek kaynak; periyodik
				// ProcessDbSync yenilemesine ek olarak istek aninda tazelenir.)
				LoadGroupsFromDb();
				// Her ?zf'de /gzinfo gondermek komut spam'ine (+ mute riski) ve mesajin
				// gec atilmasina yol aciyordu. g_zoneFlash sunucudan periyodik beslenir;
				// mevcut veriyle aninda cevap ver (YS botu gibi).
				BuildFlashZoneSummary();

				// Arkada, rengi BILINMEYEN flashing zone'lari haritadan ogren (teleport +
				// /gzinfo): renkler 0xHEX ile group_colors'a ve DB'ye dogru yazilir. Tarama
				// tempolu (spam'siz); bitince guncel ozet yeniden gonderilir (g_zfScanResend).
				// Boylece eksik/yanlis renkler zamanla dogru toplanir.
				StartFlashScan();

				// Tag'ler /groups+/gstats ile ?zgrp'de ogrenilir ve DB'de kalicidir
				// (dosya + MySQL). ?zf SADECE mevcut bilgiyle ozet atar; otomatik
				// /gstats spam'i komut mute riskini artiriyordu, onun yerine bilinmeyen
				// renkli flashing zonlar ?zgrp ile (elle) cekilir.
			}
			// --- ?gz : zone war baslatma -Ã¥Ã† Discord zonewar kanalina bildirim, 180 sn cooldown (RakGravy ile ayni) ---
			else if(!strncmp(pGroupMsg, "?gz", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gz)", "!Cooldown: %d secs left (?gz)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kaldi), ?gz yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONEWAR] Grup chat'inde ?gz yakalandi. /gz sunucu komutu gonderiliyor (Discord bildirimi yok).");
				sendServerCommand((char *)"/gz");
			}
			// --- ?gz2 : GZ'ye girmeyip gruba belirgin "/gz2 yazin" duyurusu atar (bot savasa katilmaz) ---
			else if(!strncmp(pGroupMsg, "?gz2", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				Log("[ZONEWAR] ?gz2 yakalandi, grup chatine belirgin duyuru atiliyor (bot GZ'ye girmiyor).");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!@@@@ /gz @@@@ GZ'ye girin! (savas devam ediyor) @@@@ /gz @@@@",
					"!@@@@ /gz @@@@ Enter GZ! (war is ongoing) @@@@ /gz @@@@"));
			}
			// --- ?grole <isim> <rol> : oyun ici rol degisikligi (memb -> Member, co -> Co-Leader) ---
			// Diskot/direkt ile ayni otomasyonu kullanir (diyalog + cift tik + inputtext).
			else if(!strncmp(pGroupMsg, "?grole", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				char t[64] = {0}, r[64] = {0};
				if(sscanf(pGroupMsg + 6, "%63s %63s", t, r) == 2 && t[0] && r[0])
				{
					char fullRole[64] = {0};
					if(!_stricmp(r, "dep") || !_stricmp(r, "deputy") || !_stricmp(r, "downer"))
						strcpy(fullRole, "Deputy Owner");
					else if(!_stricmp(r, "leader") || !_stricmp(r, "lead"))
						strcpy(fullRole, "Leader");
					else if(!_stricmp(r, "co") || !_stricmp(r, "coleader") || !_stricmp(r, "co-leader"))
						strcpy(fullRole, "Co-Leader");
					else if(!_stricmp(r, "mem") || !_stricmp(r, "memb") || !_stricmp(r, "member"))
						strcpy(fullRole, "Member");
					else if(!_stricmp(r, "trial"))
						strcpy(fullRole, "Trial Member");
					else if(!_stricmp(r, "passive"))
						strcpy(fullRole, "Passive Member");
					else if(!_stricmp(r, "guest"))
						strcpy(fullRole, "Guest");
					else
						strncpy_s(fullRole, sizeof(fullRole), r, sizeof(fullRole) - 1);
					Log("[GROLE] Oyun ici ?grole: '%s' -> '%s' (isteyen: %d).", t, fullRole, iSenderId);
					StartGrole(t, fullRole);
				}
				else
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?grole <isim> <dep|leader|co|memb|trial|passive|guest>", "!Usage: ?grole <name> <dep|leader|co|memb|trial|passive|guest>"));
				}
			}
			// --- ?grank <isim> <rank> : oyun ici rank degisikligi ---
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
					sendChat((char *)LM("!Kullanim: ?grank <isim> <rank>", "!Usage: ?grank <name> <rank>"));
				}
			}
			// --- ?ginv[ite] <id> : /ginvite ile oyuncu davet ---
			int nGinvCmd = 0;
			if(!strncmp(pGroupMsg, "?ginvite", 8) && (pGroupMsg[8] == '\0' || pGroupMsg[8] == ' ')) nGinvCmd = 8;
			else if(!strncmp(pGroupMsg, "?ginv", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' ')) nGinvCmd = 5;
			if(nGinvCmd)
			{
				const char *pArg = pGroupMsg + nGinvCmd;
				while(*pArg == ' ') pArg++;
				if(*pArg >= '0' && *pArg <= '9' && atoi(pArg) >= 0 && atoi(pArg) < MAX_PLAYERS)
				{
					char szCmd[32]; sprintf_s(szCmd, sizeof(szCmd), "/ginvite %d", atoi(pArg));
					Log("[GINV] Oyun ici ?ginv: id %d (isteyen: %d).", atoi(pArg), iSenderId);
					sendServerCommand(szCmd);
					g_ginvPending = true;
					g_ginvAt = GetTickCount();
				}
				else
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?ginv <id>", "!Usage: ?ginv <id>"));
				}
			}
			// --- ?gkick <isim> [sebep] : uye listesinde ara + kick + sebep ---
			else if(!strncmp(pGroupMsg, "?gkick", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				char t[64] = {0};
				if(sscanf(pGroupMsg + 6, "%63s", t) == 1 && t[0])
				{
					const char *pR = pGroupMsg + 6;
					while(*pR == ' ') pR++;
					while(*pR && *pR != ' ') pR++;
					while(*pR == ' ') pR++;
					char reason[128] = {0};
					strncpy_s(reason, sizeof(reason), pR, sizeof(reason) - 1);
					Log("[GKICK] Oyun ici ?gkick: '%s' (sebep: '%s', isteyen: %d).", t, reason[0] ? reason : "-", iSenderId);
					StartGkick(t, reason);
				}
				else
				{
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?gkick <isim> [sebep]", "!Usage: ?gkick <name> [reason]"));
				}
			}
			// --- Grup chat kopusu: ! ile baslayan mesajlar Discord'a iletilir.
			// ornek: oyunda "!test" yazilir -> bot sunucudan "!test" (GROUP) alir,
			// basta ! (Discord'a gonderim isareti), sonrasi ("test") Discord'a yollanir.
			// (Geri echo: sendChat ile gonderilen kendi mesajlarimizda da ! on-eki var;
			//  g_bridgeSuppressEcho ile Discord'a tekrar gitmeleri engellenir.)
			// Discord'dan oyuna iletilen mesajlar "![Discord] ..." on-ekiyle gelir;
			// bunlar tekrar Discord'a gonderilmez (sonsuz dongu engeli).
			else if(pGroupMsg[0] == '!')
			{
				if(g_bridgeSuppressEcho > 0)
				{
					g_bridgeSuppressEcho--;
					return;
				}
				// Konsol'dan (cmds.cpp ! yolu) yazilan mesajlar gruba gider ama
				// Discord'a KOPRULENMEZ: iSenderId == bot = bot/console gonderimi.
				// Oyuncularin oyun ici !<mesaj> koprusu iSenderId != bot oldugu
				// icin calismaya devam eder.
				if(iSenderId == g_myPlayerID)
				{
					Log("[GROUPCHAT] Bot/konsol mesaji Discord'a iletilmedi: %s", pGroupMsg + 1);
					return;
				}
				if(strncmp(pGroupMsg, "![Discord]", 10) == 0)
				{
					Log("[GROUPCHAT] Discord geri iletimi atlandi (%s).", pGroupMsg);
					return;
				}
				Log("[GROUPCHAT] Grup mesaji Discord'a iletilmek uzere: %s", pGroupMsg + 1);
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
// /fr (freeroam) komutunu tam zamaninda gonderir. UIF'de oyuncu komutlari RPC_ServerCommand
// uzerinden islenir (RPC_Chat chat mesaji olarak echo edilir), bu yuzden sendServerCommand kullan.
// Not: /fr'yi erken (login/auth oncesi) gonderip g_frSentOnce'i yutma; buradaki tek gonderim,
// spawn menusu "Freeroam" secildikten sonraki GERCEK spawn onayina denk gelir.
void SpawnConfirmed(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	// Login bekleniyor: kullanici sifre girip hesaba girene kadar /fr ve akislar
	// bekletilir (tmp hesaba atilip gercek hesap kaybedilmesin).
	if(g_loginPending)
	{
		Log("[LOGIN] Spawn onayi geldi ama login bekleniyor, /fr ertelendi (tmp hesap onleniyor).");
		return;
	}

	iSpawned = 1;
	bIsSpectating = 0;
	g_zoneScanWaitStart = GetTickCount(); // oto-tarama sayaci: 30 sn sonra ilk tarama

	// Spawn onayi (RPC_Spawn) alindi: /fr'yi GONDER. Spawn menusu secimi zaten
	// bir kez /fr gonderdi; kullanici istegi: spawn olduktan sonra da /fr tekrar
	// gonderilsin (virtual map'te kaliyorsa freeroam'a gecmesi icin). Custom pos
	// /pos ile GONDERILMEZ â€” sadece /fr, sonrasinda normal akis devam eder.
	Log("[SPAWN] Sunucu spawn onayi (RPC_Spawn) alindi, /fr (freeroam) tekrar gonderiliyor.");
	Log("[SPAWN] Kendi oyuncu adim: '%s' (XML nickname: '%s')", playerInfo[g_myPlayerID].szPlayerName, g_szNickName);
	sendServerCommand((char *)"/fr");

	// Reconnect + spawn: tarama aktifse KALDIGI ZONEDEN DEVAM (o zone'a yeniden
	// teleport + /gzinfo). Scan state'i (queue/idx) hafizada korunur. Mute-bayragi
	// olsun olmasin calisir - genel baglanti kopup gelince de ayni koruma gerekli,
	// yoksa spawn noktasinda /gzinfo atip YANLIS zone sahibi ogreniyordu.
	if(g_zoneScanIdx >= 0 && g_zoneScanIdx < g_zoneScanQ)
	{
		bool bWasMute = g_muteReconnectPending;
		g_muteReconnectPending = false;
		Log("[ZF:SCAN] Yeniden spawn: kaldigi zone #%d'ye devam ediliyor (%s).",
			g_zoneScanQueue[g_zoneScanIdx], bWasMute ? "mute-reconnect" : "yeniden baglanti");
		GangZoneScanGo(g_zoneScanQueue[g_zoneScanIdx]);
	}

	// Yeniden baslatma sonrasi restore edilen savas varsa: KISA GZ girisii yap (anlik
	// /gz2) -> guncel timer textdraw'i gelir, CaptureWarTimer stale restore'u tazeler;
	// 4 sn sonra ProcessZoneWarAutoJoin /fr ile geri doner. Timer gelmezse savas bitmis
	// demektir (g_inZoneWar guncellenir). Boylece ?gzt dogru sure gosterir + oto tarama
	// (StartZoneScan) savas yoksa devam edebilir.
	if(g_inZoneWar && g_warRemainingSec >= 0 && !g_zwReturnPending && !g_zwKillListStay)
	{
		g_zwReturnPending = true;
		g_zwReturnAt = GetTickCount() + 4000;
		Log("[WAR:TIMER] Restore sonrasi kisa GZ girisii: timer dogrulaniyor (%d sn restore).", g_warRemainingSec);
		sendServerCommand((char *)"/gz");
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
					// LOGIN DIALOG: kullanici Login'e basti â€” ne gonderildigini logla
					Log("[DIALOG-SUBMIT] Button1, dialog=%d style=%d, input='%s' (len=%d)",
						sampDialog.wDialogID, sampDialog.bDialogStyle, szResponse, (int)strlen(szResponse));
					if(strstr(sampDialog.szTitle, "Login") || strstr(sampDialog.szTitle, "Giris"))
					{
						// Login submit: GTA client ile BIREBIR AYNI format (BYTE uzunluk,
						// listitem 0xFFFF). sendDialogResponse WORD kullaniyor â€” gstats icin
						// sorun degil ama login sifresini sunucu siki parse ediyor.
						BYTE bRespLen = (BYTE)strlen(szResponse);
						RakNet::BitStream bsLogin;
						bsLogin.Write((uint16_t)sampDialog.wDialogID);
						bsLogin.Write((uint8_t)1);
						bsLogin.Write((uint16_t)0xFFFF);
						bsLogin.Write((uint8_t)bRespLen);
						bsLogin.Write(szResponse, bRespLen);
						pRakClient->RPC(&RPC_DialogResponse, &bsLogin, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
						Log("[LOGIN] Sifre GTA-formatinda gonderildi ('%s', len=%d).", szResponse, (int)bRespLen);
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

// Oasis savas-sonu kill list dialogu: baslik ornegi Oasis Group Zone War [tRc (0) vs 9 (3)],
// satirlar ornegi 1. Rainy(22)  2 / 0  2.00  301.00 (Player / K-D / Ratio / Damage).
// UIF tarafindaki gibi textdraw yok: tablo dogrudan dialogdan alinir, ayni SendKillList
// hatti ile Discord tarafina gider. Takim dagilimi dialogda yok: tum satirlar tek kutuda,
// takim adlari + skor basliktan.
// Savas sonucu select-menu dialogu: savas biteli <90sn ve baslik savasla ilgiliyse
// dokumle, bilinen formatta parse et, her halukarda Cancel satir 0 ile kapat
// (secimsiz birakilirsa dialog acik kalir, /pos reddedilir).
static bool OasisKillListDialog(const char *szTitle, const char *szInfo);
static bool WarResultDialogTry(const char *szTitle, const char *szInfo, int iDlgId, int iStyle)
{
	if(GetTickCount() - g_lastWarEndTick > 90000) return false;
	if(iStyle != DIALOG_STYLE_LIST && iStyle != DIALOG_STYLE_TABLIST && iStyle != DIALOG_STYLE_TABLIST_HEADERS) return false;
	if(!(strstr(szTitle, "War") || strstr(szTitle, "Result") || strstr(szTitle, "Score") || strstr(szTitle, "Kill") || strstr(szTitle, "Winner") || strstr(szTitle, "MVP") || strstr(szTitle, "GZ"))) return false;
	Log("[WARRESULT-DATA] '%s': %.800s", szTitle, szInfo ? szInfo : "");
	if(OasisKillListDialog(szTitle, szInfo)) return true;
	RakNet::BitStream bsR;
	bsR.Write((uint16_t)iDlgId);
	bsR.Write((uint8_t)0);
	bsR.Write((uint16_t)0);
	bsR.Write((uint8_t)0);
	pRakClient->RPC(&RPC_DialogResponse, &bsR, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
	return true;
}
static bool OasisKillListDialog(const char *szTitle, const char *szInfo)
{
	if(!strstr(szTitle, "Group Zone War [")) return false;
	if(g_zwKillListSent) return true;
	char szTagA[32] = "", szTagB[32] = "";
	int iSA = 0, iSB = 0;
	const char *pB = strchr(szTitle, '[');
	if(pB)
	{
		const char *pPar = strchr(pB, '(');
		if(pPar && pPar - pB - 1 > 0 && pPar - pB - 1 < 31)
		{
			size_t nA = (size_t)(pPar - pB - 1);
			memcpy(szTagA, pB + 1, nA); szTagA[nA] = 0;
			while(nA > 0 && szTagA[nA-1] == ' ') szTagA[--nA] = 0;
			iSA = atoi(pPar + 1);
		}
		const char *pVs = strstr(pB, " vs ");
		if(pVs)
		{
			const char *pR = pVs + 4;
			const char *pPar2 = strchr(pR, '(');
			if(pPar2 && pPar2 - pR > 0 && pPar2 - pR < 31)
			{
				size_t nB = (size_t)(pPar2 - pR);
				memcpy(szTagB, pR, nB); szTagB[nB] = 0;
				while(nB > 0 && szTagB[nB-1] == ' ') szTagB[--nB] = 0;
				iSB = atoi(pPar2 + 1);
			}
		}
	}
	char szRows[2048] = "";
	char szMvp[64] = "?", szFeeder[64] = "?";
	int iBestK = -1, iBestD = -1, iTotK = 0, iTotD = 0;
	double fBestDmg = 0, fTotDmg = 0;
	int nRows = 0;
	char szCopy[2048];
	strncpy_s(szCopy, sizeof(szCopy), szInfo ? szInfo : "", _TRUNCATE);
	for(char *pLine = strtok(szCopy, "\n"); pLine; pLine = strtok(NULL, "\n"))
	{
		while(*pLine == ' ' || *pLine == '\t' || *pLine == '\r') pLine++;
		if(!*pLine) continue;
		if(strstr(pLine, "Player") && (strstr(pLine, "Damage") || strstr(pLine, "K/D"))) continue;
		char *pRow = pLine;
		char *pDot = strchr(pLine, '.');
		if(pDot && pDot - pLine >= 1 && pDot - pLine <= 3)
		{
			bool bNum = true;
			for(char *q = pLine; q < pDot; q++) if(*q < '0' || *q > '9') { bNum = false; break; }
			if(bNum) pRow = pDot + 1;
		}
		while(*pRow == ' ') pRow++;
		const char *pOp2 = strchr(pRow, '(');
		if(!pOp2 || pOp2 == pRow) continue;
		char szName[64] = "";
		{
			size_t nN = (size_t)(pOp2 - pRow);
			if(nN > 63) nN = 63;
			memcpy(szName, pRow, nN); szName[nN] = 0;
			while(nN > 0 && szName[nN-1] == ' ') szName[--nN] = 0;
		}
		if(!szName[0]) continue;
		const char *pSl = NULL;
		for(const char *qSl = pRow; *qSl; qSl++)
		{
			if(*qSl != '/') continue;
			const char *qa = qSl - 1;
			while(qa > pRow && *qa == ' ') qa--;
			const char *qb = qSl + 1;
			while(*qb == ' ') qb++;
			if(qa >= pRow && *qa >= '0' && *qa <= '9' && *qb >= '0' && *qb <= '9') { pSl = qSl; break; }
		}
		if(!pSl) continue;
		int iK = 0, iD = 0;
		{
			const char *q = pSl;
			while(q > pRow && (*(q-1) < '0' || *(q-1) > '9')) q--;
			const char *e = q;
			while(q > pRow && (*(q-1) >= '0' && *(q-1) <= '9')) q--;
			if(e > q) iK = atoi(q);
			iD = atoi(pSl + 1);
		}
		char szDmg[16] = "0";
		{
			const char *e2 = pRow + strlen(pRow);
			while(e2 > pRow && (*(e2-1) == ' ' || *(e2-1) == '\r' || *(e2-1) == '\t')) e2--;
			const char *s2 = e2;
			while(s2 > pRow && (*(s2-1) != ' ') && (*(s2-1) != '\t')) s2--;
			size_t nD = (size_t)(e2 - s2);
			if(nD > 0 && nD < 15 && *s2 >= '0' && *s2 <= '9') { memcpy(szDmg, s2, nD); szDmg[nD] = 0; }
		}
		char szRow[128];
		sprintf_s(szRow, sizeof(szRow), "%-16s %4d %4d %4d %6s", szName, iK, 0, iD, szDmg);
		if(szRows[0]) strncat_s(szRows, sizeof(szRows), "\n", _TRUNCATE);
		strncat_s(szRows, sizeof(szRows), szRow, _TRUNCATE);
		nRows++;
		iTotK += iK; iTotD += iD; fTotDmg += atof(szDmg);
		if(iK > 0 || iD > 0)
		{
			if(iK > iBestK || (iK == iBestK && atof(szDmg) > fBestDmg))
			{ iBestK = iK; fBestDmg = atof(szDmg); strncpy_s(szMvp, sizeof(szMvp), szName, _TRUNCATE); }
			if(iD > iBestD) { iBestD = iD; strncpy_s(szFeeder, sizeof(szFeeder), szName, _TRUNCATE); }
		}
	}
	if(nRows == 0) return true;
	char szHeader[64] = "", szBox[3072] = "", szTot[128] = "";
	sprintf_s(szHeader, sizeof(szHeader), "%-16s %4s %4s %4s %6s", "Name", "K", "A", "D", "DMG");
	sprintf_s(szTot, sizeof(szTot), "Total            %4d %4d %4d %6.0f", iTotK, 0, iTotD, fTotDmg);
	sprintf_s(szBox, sizeof(szBox), "%s\n--------------------------------------\n%s\n--------------------------------------\n%s", szHeader, szRows, szTot);
	SendKillList(szBox, "", szTagA[0] ? szTagA : "A", szTagB[0] ? szTagB : "B", szTagA, szTagB,
		iSA, iSB, g_zwWeWon, szMvp, szFeeder,
		fBestDmg > 0 ? (int)fBestDmg : 0, iBestK < 0 ? 0 : iBestK, iBestD < 0 ? 0 : iBestD,
		g_zwFirstBlood, "");
	FILE *fl = fopen("gz sonucl.txt", "w");
	if(fl) { fprintf(fl, "%s\n", szBox); fclose(fl); }
	Log("[KILLLIST] Oasis dialog sonucu gonderildi (%d satir %s %d - %d %s).", nRows, szTagA, iSA, iSB, szTagB);
	g_zwKillListSent = true;
	g_zwCapturingKillList = false;
	g_zwKillTdCount = 0;
	g_zwFinalizeAt = 0;
	g_zwEmptyFinalizeSince = 0;
	return true;
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
	Log("[DLG] id=%d style=%d title='%s' btn1='%s' btn2='%s'", sampDialog.wDialogID, sampDialog.bDialogStyle, sampDialog.szTitle, sampDialog.szButton1, sampDialog.szButton2);

	// --- SPAWN MENUSU: "Where would you like to go?" otomatik Freeroam secimi ---
	// Spawn menusu (id=30, style 5 TABLIST_HEADERS) acildiginda "Freeroam" satirini
	// secip dialogu gondeririz. Bu sayede bot virtual map'e dusmez, gercek freeroam
	// bolgesine spawn olur. Bulunamazsa menu acik birakilir (asla kapatilmaz).
	bool bIsSpawnMenu = (strstr(sampDialog.szTitle, "Where would you like to go") != NULL);
	// Oasis: spawn basinda "Oasis | Mode" listesi gelir - secim ayni, /fr YOK.
	bool bIsOasisMode = (strstr(sampDialog.szTitle, "Oasis") != NULL && strstr(sampDialog.szTitle, "Mode") != NULL);
	g_watchDlgId = sampDialog.wDialogID;
	g_watchDlgAt = GetTickCount();
	g_watchDlgStyle = sampDialog.bDialogStyle;
	g_watchDlgRow[0] = 0;
	if(sampDialog.bDialogStyle == DIALOG_STYLE_LIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS)
	{
		const char *pFR = szDlgInfo;
		if(sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) { const char *pNL = strchr(pFR, 10); if(pNL) pFR = pNL + 1; }
		size_t nFR = 0; while(pFR[nFR] && pFR[nFR] != 13 && pFR[nFR] != 10 && nFR < 200) nFR++;
		memcpy(g_watchDlgRow, pFR, nFR); g_watchDlgRow[nFR] = 0;
	}
	g_watchDlgKeep = false;
	if(strstr(sampDialog.szTitle, "Giris") || strstr(sampDialog.szTitle, "Login") ||
		strstr(sampDialog.szTitle, "Authentication"))
		g_watchDlgKeep = true;
	else if(bIsSpawnMenu || bIsOasisMode)
		g_watchDlgKeep = true;
	else if(sampDialog.bDialogStyle == DIALOG_STYLE_INPUT && g_waitingForGStats && !g_gstatsAutoSubmit)
		g_watchDlgKeep = true;
	if((bIsSpawnMenu || bIsOasisMode) && (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
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
					// sendDialogResponse WORD kullaniyor â€” sunucu BYTE bekledigi icin "Freeroam"
					// secimi islenmiyor, bot virtual world'de kaliyordu. (Login'de ayni sorun
					// cozuldu; spawn menusu de ayni formatla gonderilir.)
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

					// Freeroam secimi cevaplandigina gore /fr'yi gonder.
					// VIRTUAL WORLD FIX: BYTE formatli cevap + /fr â€” bot gercek freeroam'da
					// spawn olur (SF kulesi/virtual world'de kalmaz).
					if(bIsSpawnMenu)
					{
						Log("[SPAWN] UIF spawn menusu cevaplandi, /fr gonderiliyor.");
						sendServerCommand((char *)"/fr");
					}
					else
						Log("[SPAWN] Oasis Mode menusu cevaplandi (/fr yok).");
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
		Log("[SPAWNMENU] Freeroam satiri bulunamadi, menu acik birakildi.");
	}

	// /gstats dialogu sonucu: "- Tag: [XX]" satirindan kisa etiketi (tag) ogren ve
	// group_colors'a yaz. /gzinfo isim/renk baglar; tag ise /gstats'tan gelir (her
	// /gstats acilisinda - manuel ?gstats, Discord komutu, ?zgrp harvest). ?zgrp
	// harvest aktifse (g_ghpActive) zinciri de ilerlet (sonraki /gstats <ID>).
	if(g_waitingForGStats)
	{
		// Uye listesi dialogu (gmembers/grole/grank/gkick) grup stats DEGIL: ogrenme ve harvest adimi atlanir
		bool bGmListDlg = (szDlgInfo && (strstr(szDlgInfo, "Last Active") != NULL || strstr(sampDialog.szTitle, "Group Members") != NULL));
		// gmembers harvest gstats makinesini ODUNC alir (uye listesi/astats/gvips):		// bu dialoglar grup stats DEGIL - grup ogrenmeye girerse gangs tablosuna
		// account ID gibi cop yazilir ve ?zf tag eslesmesi bozulur.
		bool bGmDialog = (g_gmPhase == 1 || g_gmAstatsWaiting || g_gmGvipsWaiting);
		bool bRoleKickDlg = (g_groleActive || g_grankActive || g_gkickActive || g_groleListActive || g_grankListActive || g_gkickListActive);
		// /gzinfo dialogu gstats CEVABI degil. Tarama sirasinda gelip harvest'i
		// yiyordu: "Zone: Bayside" diye sahte grup + tagsiz kayit yaziliyordu.
		bool bZoneInfoDlg = (strstr(sampDialog.szTitle, "Zone Info") != NULL);
		if(!bGmDialog && !bGmListDlg && !bRoleKickDlg && !bZoneInfoDlg) { LearnGroupTagFromGStats(szDlgInfo); SaveGroupColors(); }
		if(g_ghpActive && !bGmListDlg && !bRoleKickDlg && !bZoneInfoDlg) OnGStatsResultForHarvest(szDlgInfo);
	}

	// /pnetstats ve /gstats dialog yakalamasi: yakalandiysa ekrana cizdirme
	if(HandleCapturedDialog(sampDialog.wDialogID, sampDialog.szTitle, szDlgInfo))
	{
		sampDialog.iIsActive = 0;
		return;
	}

	// Oasis savas-sonu kill list dialogu (tek OK butonu): tabloyu al + Discord, OK ile kapat.
	if(OasisKillListDialog(sampDialog.szTitle, szDlgInfo))
	{
		sendDialogResponse(sampDialog.wDialogID, 1, 0xFFFF, (char *)"");
		sampDialog.iIsActive = 0;
		return;
	}
	if(WarResultDialogTry(sampDialog.szTitle, szDlgInfo, sampDialog.wDialogID, sampDialog.bDialogStyle))
	{
		sampDialog.iIsActive = 0;
		return;
	}

	// Bekleyen /stats cevabi (ACC probe): govdeyi bir kez dokum - format daha
	// sonra kesin parse ile doldurulacak. Sadece bizim sordugumuz isim icin.
	if(!g_accPendingName.empty() && GetTickCount() - g_accPendingAt < ACC_PROBE_TIMEOUT_MS)
		Log("[ACCDBG] isim='%s' baslik='%s' govde=%.300s", g_accPendingName.c_str(), sampDialog.szTitle, szDlgInfo);

	if(g_groleActive || g_grankActive || g_gkickActive || g_waitingForGmembers || g_waitingForGStats)
		Log("[DLG-SKIP] otomasyon aktifken yakalanmadi (grole=%d grank=%d gkick=%d gm=%d gs=%d): '%s'",
		g_groleActive, g_grankActive, g_gkickActive, g_waitingForGmembers, g_waitingForGStats, sampDialog.szTitle);
	// --- GLOBAL AUTOCLOSE ---
	// Acik kalmasi zorunlu diyaloglar disindakileri otomatik kapat (ekranda birikmesin).
	// Acik kalanlar: Login/Giris/Authentication, spawn menusu (freeroam secilmediyse),
	// /astats icin account ID girisi (kullanici yazacak).
	bool bKeepOpen = false;
	if(strstr(sampDialog.szTitle, "Giris") || strstr(sampDialog.szTitle, "Login") ||
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
			// ONCE kapat, SONRA ogren. Ogrenme siradaki /pos'u tetikliyor ve
			// sunucu acik diyalogla aksiyonu reddediyordu ("You cannot perform
			// this action with a dialog open") -> bot hic teleport olmuyor, her
			// /gzinfo ayni zone'i (Bayside #0) bildiriyor, butun zone'lar ayni
			// sahibe yaziliyordu.
			sendDialogResponse(sampDialog.wDialogID, 0, 0xFFFF, (char *)"");
			sampDialog.iIsActive = 0;
			Log("[AUTOCLOSE] Dialog id=%d '%s' (style %d) kapatildi.", sampDialog.wDialogID, sampDialog.szTitle, sampDialog.bDialogStyle);
			// Her gzinfo dialogundan ogren (tarama aktif olmasa da): ?zf'nin kendi
			// /gzinfo'su su anki gercek sahibi getirir, eski/eski cache'i ezer.
			GangZoneScanGzDialog(szDlgInfo);
			return;
		}
		Log("[AUTOCLOSE] Dialog id=%d '%s' (style %d) kapatildi.", sampDialog.wDialogID, sampDialog.szTitle, sampDialog.bDialogStyle);
		if(sampDialog.bDialogStyle == DIALOG_STYLE_LIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS)
			SendListClose(sampDialog.wDialogID);
		else
			sendDialogResponse(sampDialog.wDialogID, 0, 0xFFFF, (char *)"");
		sampDialog.iIsActive = 0;
		g_watchDlgId = -1;
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
	// Komut mutesi gametext olarak da gelir (textdraw yakalamasi yetmez)
	TriggerMuteReconnect(szMessage, "gametext");

	// "Your group is not in a zone war": bot /gz2 kontrol girdisi g?nderdi ama
	// sava? yok. Sava? durumunu sifirla ve kontrol etmeyi BIRAK.
	if(strstr(szMessage, "not in a zone war"))
	{
		if(g_ztkAttackPending)
			g_ztkOutcome = 2; // zsnip /gz saldirisi savas baslatamadi -> kaybedildi say
		if(g_inZoneWar)
			Log("[GAMETEXT] not in a zone war - sava? durumu temizleniyor, kontroller durduruluyor.");
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
			// Chat yoluyla ayni: kill list yakalayici aciksa bitis textdraw'lari
			// icin finalize kur; yoksa /fr ile freeroam'a don. Aksi halde yakalayici
			// acik kaliyor ve ?zf surekli capturing etiketi basiyordu (gametext lost vakasi).
			if(g_zwCapturingKillList && !g_zwKillListSent)
			{
				g_zwFinalizeAt = GetTickCount() + 3000;
				Log("[KILLLIST] Gametext: savas bitti, GZ'de kalinip sonuc textdraw'lari bekleniyor...");
			}
			else if(g_zwReturnPending)
			{
				g_zwReturnPending = false;
				Log("[ZONEWAR] Gametext: savas bitti, /fr ile freeroam'a donuluyor.");
				sendServerCommand((char *)"/fr");
			}
		}
	}
	else if(g_ztkAttackPending && strstr(szMessage, "can be attacked in"))
	{
		Log("[GAMETEXT] Hedef zone saldiri korumali (cooldown), saldiri basarisiz.");
		ZtkAttackFailed();
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
			// Chat yoluyla ayni: kill list yakalayici aciksa bitis textdraw'lari
			// icin finalize kur; yoksa /fr ile freeroam'a don. Aksi halde yakalayici
			// acik kaliyor ve ?zf surekli capturing etiketi basiyordu (gametext lost vakasi).
			if(g_zwCapturingKillList && !g_zwKillListSent)
			{
				g_zwFinalizeAt = GetTickCount() + 3000;
				Log("[KILLLIST] Gametext: savas bitti, GZ'de kalinip sonuc textdraw'lari bekleniyor...");
			}
			else if(g_zwReturnPending)
			{
				g_zwReturnPending = false;
				Log("[ZONEWAR] Gametext: savas bitti, /fr ile freeroam'a donuluyor.");
				sendServerCommand((char *)"/fr");
			}
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
// float'larý g-nderir; zone ID client tarafýnda sýralý atanýr. UIF zone numaralarý
// (#0'dan itibaren, haritanýn üst solundan) bu sýrayla e-sle-sir -> ?zgo i-gin kaydet.
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

// RPC 120 (RPC_ScrRemoveGangZone): zone kaldýrýlýr. Listeyi sýfýrlamayýz -günkü
// zone numaralarý (#N) sabittir; sadece gerekirse tekrar toplanýr.
void ScrRemoveGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned int uiZoneID = 0;
	if(bsData.Read(uiZoneID))
		Log("[GANGZONE] zone kaldirildi: %u", uiZoneID);
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
	// StartFlashScan /gzinfo ile sahibini ogrenir ve ?zf'ye ekler. ?zgrp harvest
	// surerken tetikLENMEZ - o an yalnizca taglar ogrenilir.
	if(g_zoneOwnerName[usZoneId][0] == 0 && g_zscanAuto && !g_ghpActive)
	{
		if(g_zscanPausedForSnip) { StashUnknownZone((int)usZoneId); }
		else
		{
			Log("[GANGZONE:FLASH] Ismi bilinmeyen zone #%u flashlandi, otomatik tarama tetikleniyor.", usZoneId);
			StartFlashScan();
			if(g_zoneScanIdx < 0) StashUnknownZone((int)usZoneId);
		}
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
		// Kirmizi yanip somne durdu (grup rengine dondü): o zone artik o grubun degil -
		// DB'den sil, grubun zone sayisi duser. Sahip bilinmiyorsa StartFlashScan
		// /gzinfo ile ogrenir (RemoveZoneFromDb g_zoneOwnerName'e bagli degil, id yeter).
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

// UIF sava-s zamanlayýcý textdraw'ý: "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> "08:27" (dk:sn).
// Ger-gek kalan süreyi yakalar; ?gzt bunu baz alir (chat duyuru anindan hesaplamak sagliksiz).
static void CaptureWarTimer(const char *pText, int iLen)
{
	if(!pText || !g_inZoneWar) return;
	int iEnd = iLen;
	while(iEnd > 0 && (pText[iEnd-1] == ' ' || pText[iEnd-1] == '' || pText[iEnd-1] == '	')) iEnd--;
	int iStart = 0;
	while(iStart < iEnd && (pText[iStart] == ' ' || pText[iStart] == '	')) iStart++;
	if(iEnd - iStart > 5) return; // saat (07:39 PM) gibi uzun metinler timer degil
	const char *pColon = NULL;
	for(int i = iStart; i < iEnd - 1; i++)
	{
		if(pText[i] == ':' && i > iStart && pText[i+1] >= '0' && pText[i+1] <= '9')
		{ pColon = pText + i; break; }
	}
	if(!pColon) return;
	const char *p = pColon - 1;
	while(p >= pText + iStart && *p >= '0' && *p <= '9') p--;
	const char *pNum = p + 1;
	if(pNum >= pColon) return;
	int iM = atoi(pNum), iS = atoi(pColon + 1);
	if(iS < 0 || iS > 59 || iM < 0 || iM > 120) return;
	g_warRemainingSec = iM * 60 + iS;
	g_warTimerTick = GetTickCount();
	g_warTimerViaTdw = true; // textdraw kaynagi: KENDI savasimizin GZ timer'i
	// UIF savaslari 10 dk DEGIL, timer 3 dk gibi kisa surebilir. ZONE_WAR_TOTAL_MS
	// fallback'i (4104) yanlis bitis ani veriyordu -> bot 7 dk gec girip kill list'i
	// kaciriyordu. Timer bilindiginde savas bitis anini geri hesapla: bitis =
	// timer_tick + kalan. Boylece son 00:02 girisi dogru zamanda yapilir.
	if(g_warRemainingSec > 0)
	{
		DWORD dwEnd = g_warTimerTick + (DWORD)g_warRemainingSec * 1000;
		DWORD dwStart = dwEnd - ZONE_WAR_TOTAL_MS;
		// Start negatif/gelecege isaret ediyorsa (savas ortasinda yakalandi) simdiki
		// ana yasla: giris guard'i zaten kalan sureyi oncelikli kullaniyor.
		if((int)dwStart <= 0 || dwStart > g_warTimerTick)
			dwStart = g_warTimerTick - (DWORD)g_warRemainingSec * 1000;
		g_zoneWarStartTick = dwStart;
		// war_timer.txt KALDIRILDI: savas durumu sadece hafizada, dosyaya yazilmaz.
		Log("[WAR:TIMER] Savas bitis ani hesaplandi: start=%u end=%u", dwStart, dwEnd);
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
	// metin yakalanir, gruba dile gore bildirilir; pencere 8 sn dolar.
	if(g_zaPermActive && GetTickCount() < g_zaPermUntil)
	{
		if(strstr(cText, "leader") || strstr(cText, "Leader") || strstr(cText, "yetk"))
		{
			Log("[ZA-PERM] Yetki textdraw'i yakalandi: %s", cText);
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Savas baslatma yetkim yok (Co-Leader/Leader role gerekiyor).",
				"!I can't start the war (Co-Leader/Leader role required)."));
			g_zaPermActive = false;
		}
		else if(strstr(cText, "Teleported"))
		{
			// Isinlama mesaji: yetki uyarisindan once gelir, pencere acik kalir.
		}
		else if(g_inZoneWar)
		{
			// Savas basladi: yetki sorunu yok demektir.
			g_zaPermActive = false;
		}
	}

	if(settings.uiTextDrawsLogging != 0)
		SaveTextDrawData(wTextID, &TextDrawTransmit, cText);

	// Komut mutesi (UIF: "~g~YOU ARE COMMAND MUTED FOR 60 SECONDS"): ?zscan SIRASINDA
	// (/pos /gzinfo) VEYA ?zgrp HARVEST SIRASINDA (/gstats) sunucu komutlari susturursa
	// zincir takilir. Mute'i gorunce botu HEMEN yeniden gir (reconnect olunca cmd mute
	// server-side kalkar; 60 sn beklemeye gerek yok). Scan/harvest state korunur;
	// yeniden spawn'da kalinan adimdan devam edilir. Ayni mute penceresindeki tekrar
	// textdraw'lar yeniden tetiklemesin diye bayrak ile bir kez.
	if((g_zoneScanIdx >= 0 || g_ghpActive || g_gmPhase != 0) && strstr(cText, "MUTED") && !g_muteReconnectPending && iSpawned)
	{
		g_muteReconnectPending = true;
		if(g_gmPhase != 0){
			Log("[GM] Mute - gmembers harvest yeniden baslatilacak (reconnect sonrasi).");
			g_gmPhase = 1; g_gmCount = 0; g_gmPage = 0; g_gmAstatsIdx = 0; g_gmAstatsWaiting = false; g_gmGvipsWaiting = false; g_gmSendDone = false;
		}
		Log("[MUTE] Komut mutesi yakalandi (%s) - hemen yeniden baglanilip islem surdurulecek.", cText);
		resetPools(1, 0); // aninda reconnect; cmd mute reconnect ile kalkar
	}

	if(TextDrawTransmit.byteSelectable)
		Log("[SELECTABLE-TEXTDRAW] ID: %d, Text: %s.", wTextID, cText);

	// --- Kill list yakalayici: GZ sonucu textdraw'larini X/Y ile topla ---
	// Kill-list textdraw'lari AYRI AYRI gelir (ID 70 "Name", 71 "Kills", ... basliklar
	// Y=80; her oyuncu satiri Y=95+ ve kolonlar X ile hizali). Ayni Y'deki textdraw'lar
	// X'e gore siralanip satir olusturur (FinalizeKillList). Timer textdraw'i hari-g
	// GZ icinde gelen her sey toplanir; gereksiz textdraw'lar finalize'da elenir.
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		// Timer textdraw'i (ID 58/65, "0-0 ~n~ 09:59" gibi) toplanmaz
		if(wTextID == 58 || wTextID == 65) { /* timer */ }
		// Dev/büyük UI mesaj textdraw'lari (sava? disi uyarilari "YOU ARE OUTSIDE THE
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

			// Takim rengi ogrenme: kendi adimizin gectigi textdraw'in rengi bizim takim rengi.
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
								Log("[KILLLIST] Bizim takim rengi ogrenildi: 0x%06X (ad: %s)", uiCol, pMy);
							}
							break;
						}
					}
				}
			}

			// X/Y + text olarak sakla (ayný ID'nin tekrari olmasin)
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
	// EDIT logu kapali: sayac her saniye konsolu dolduruyordu (TextDraws.log dosyasi aynen yazar).
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
	// Kendi grup adimiz chat mesajindan otomatik ogrenilir; burada ASLA ezilmez.
	// Dusman grup adini bosken al (welcome back icin); o da tam isim degilse sorun degil.
	if(g_enemyGroupName.empty())
		g_enemyGroupName = szEnemy;
	g_inZoneWar = (szEnemy[0] != '\0');

	Log("[WARGROUP] Kendi Grubu: %s | Dusman Grup: %s", szOwn, szEnemy);
}

// Sonucu "gz sonucl.txt" dosyasina yaz ve Discord'a gonder.
// Textdraw'lar X/Y koordinatlariyla toplanir (her kolon/satir ayri textdraw).
// Burada ayni Y'deki textdraw'lar X'e gore siralanip satir birlestirilir;
// renklerine gore iki takima ayrilir ve "Name Kills Assists Deaths Dmg" baslikli
// box halinde duzenlenir.
void FinalizeKillList()
{
	if(g_zwKillTdCount == 0)
	{
		// Bos finalize KAPTaji oldurmez: gercek skorboard son 2 sn + bitis
		// textdraw'larinda gelir (bot o an disarida olabilir). Pencere yenilenir,
		// KillListSent YOK - sonraki toplama devam eder. Eski davranis bos
		// finalize'de sent=true yapip kaptaji tum savas boyunca kilitliyordu
		// ("gz sonucl.txt yazilmadi" vakasi).
		g_zwCaptureStart = GetTickCount();
		DWORD dwEmptyNow = GetTickCount();
		if(g_zwEmptyFinalizeSince == 0)
			g_zwEmptyFinalizeSince = dwEmptyNow;
		if(dwEmptyNow - g_zwEmptyFinalizeSince < 30000)
		{
			// 30 sn: bitis textdraw'lari gec gelebilir; 10 sn'de bir yeniden dene.
			// FIX: eskiden g_zwFinalizeAt sifirlanmiyordu -> scheduler her frame
			// tekrar cagirip saniyede ~20 log spam'i uretiyordu.
			g_zwFinalizeAt = dwEmptyNow + 10000;
			Log("[KILLLIST] Sonuc metni henuz bos, 10 sn sonra tekrar denenecek.");
			return;
		}
		Log("[KILLLIST] Bitis textdraw'lari gelmedi, kill list bu savas icin atlaniliyor.");
		g_zwEmptyFinalizeSince = 0;
		g_zwCapturingKillList = false;
		g_zwKillListSent = true;
		g_zwFinalizeAt = 0;
		if(g_zwReturnPending)
		{
			g_zwReturnPending = false;
			Log("[KILLLIST] GZ'de kalindi, /fr ile freeroam'a donuluyor.");
			sendServerCommand((char *)"/fr");
		}
		return;
	}

	// X'e gore sirala (kolon sirasi: ayni Y icinde soldan saga)
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

	// Y'ye gore satir gruplama: satirlar 15 birim arayla (Y=95,110,125...); tolerans
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

		// Hucreleri X'e gore sirala (kolon sirasi) ve alanlara dagit.
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
			// Rank sütunu (X=135, "1 2 3...") E1 formatinda yok - atilir.
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

	// Takim yonumuz: "vs" duyurusundaki konum (sol=kirmizi, sag=mavi) en guvenilir;
	// bilinmiyorsa renk fallback'i (mavi 0x0000FF). bWeAreBlue=true ise biz sag grup.
	bool bWeAreBlue = g_zwWeAreBlueKnown ? g_zwWeAreBlue : ((g_zwMyColor & 0xFFFFFF) == 0x0000FF);
	// Takim adlari: once "vs" duyurusu (sol=kirmizi, sag=mavi). Duyuru yoksa isimleri
	// bWeAreBlue ile TUTARLI sec: biz kirmiziysak kirmizi=biz, maviysek mavi=biz.
	const char *pRedName  = !g_zwWarTeamLeft.empty()  ? g_zwWarTeamLeft.c_str()
		: (bWeAreBlue ? (!g_enemyGroupName.empty() ? g_enemyGroupName.c_str() : "Kirmizi")
		              : (!g_myGroupName.empty()    ? g_myGroupName.c_str()    : "Kirmizi"));
	const char *pBlueName = !g_zwWarTeamRight.empty() ? g_zwWarTeamRight.c_str()
		: (bWeAreBlue ? (!g_myGroupName.empty()    ? g_myGroupName.c_str()    : "Mavi")
		              : (!g_enemyGroupName.empty() ? g_enemyGroupName.c_str() : "Mavi"));
	// Yedek: savas zonunun hafizadaki sahibi (duyuru kacmissa "Kirmizi"/"Mavi" kalirdi).
	{
		int wz = g_zwWarZone[0] ? atoi(g_zwWarZone) : -1;
		const char *pOwn = (wz >= 0 && wz < MAX_GANG_ZONES) ? g_zoneOwnerName[wz] : NULL;
		const char *pMe = g_myGroupName.empty() ? NULL : g_myGroupName.c_str();
		if(pOwn && pOwn[0] && _stricmp(pOwn, "Free") != 0)
		{
			if(pMe && !_stricmp(pOwn, pMe))
			{
				if(bWeAreBlue && !strcmp(pBlueName, "Mavi")) pBlueName = pOwn;
				if(!bWeAreBlue && !strcmp(pRedName, "Kirmizi")) pRedName = pOwn;
			}
			else
			{
				if(bWeAreBlue && !strcmp(pRedName, "Kirmizi")) pRedName = pOwn;
				if(!bWeAreBlue && !strcmp(pBlueName, "Mavi")) pBlueName = pOwn;
			}
		}
	}
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
	// Tagsiz grubun UIF tag'i '~' sentinel'idir (netrpc 2705). Discord'a ham gidince
	// embed'te isim yerine '~' gorunuyordu (orn: Supreme tagsiz -> '~'). Sentinel /
	// NULL / - tag'leri BOS gonder -> Discord isimle gosterir.
	if(pRedTag[0] == '~' || !_stricmp(pRedTag, "NULL") || !_stricmp(pRedTag, "-")) pRedTag = "";
	if(pBlueTag[0] == '~' || !_stricmp(pBlueTag, "NULL") || !_stricmp(pBlueTag, "-")) pBlueTag = "";
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
		sprintf_s(szB, sizeof(szB), "%-16s %4s %4s %4s %6s\n%s\n%s\n%s\nTotal            %4d %4d %4d %6d",
			"Name", "K", "A", "D", "DMG", dash,
			strRedRows.empty() ? "(bos)" : strRedRows.c_str(), dash, iRedK, iRedA, iRedD, iRedDmg);
		strRedBox = szB;
		sprintf_s(szB, sizeof(szB), "%-16s %4s %4s %4s %6s\n%s\n%s\n%s\nTotal            %4d %4d %4d %6d",
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

	// Discord'a gonder (yapilandirilmis: kazandi mi, kirmizi/mavi takim + tag + kutu, MVP/feeder)
	// --- Top Weapons: RPC 55 kill feed'i iki takimin uyelerinin kill'leriyle
	// filtrele, top 5 silahi Discord'a gonder. Uye isimleri scoreboard
	// satirlarindan gelir (ilk alan). GZ pencerelerinde (3sn bas + 00:02 son)
	// gorunmeyen nadir oyuncular sayima girmez. (ponytail: oyuncu kacmasi onemli
	// olursa kill feed'ini katil-victim oranina gore de filtreleyebiliriz.)
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
	g_zwEmptyFinalizeSince = 0;
	g_zwBestK = 0; g_zwBestDmg = 0; g_zwBestD = 0;
	g_zwMvpName[0] = 0; g_zwFeederName[0] = 0;
	g_zwMineK = 0; g_zwMineA = 0; g_zwMineD = 0; g_zwMineDmg = 0;
	g_zwEnemyK = 0; g_zwEnemyA = 0; g_zwEnemyD = 0; g_zwEnemyDmg = 0;

	// GZ'de kaldiysak (kill list toplamak icin) simdi freeroam'a don
	if(g_zwReturnPending)
	{
		g_zwReturnPending = false;
		Log("[KILLLIST] Kill list tamam, /fr ile freeroam'a donuluyor.");
		sendServerCommand((char *)"/fr");
	}
}

// Ana donguden her iterasyonda cagrilir:
// 1. ?pl stats-post zaman asimi cevabi
// 2. Kisa GZ giris donusu (savas basi takim rengi ogrenimi)
// 3. Savas aktifse son 00:02'de /gz2 (kill-list textdraw'lari icin GZ icinde KALINIR)
// 4. Toplama penceresi dolunca sonucu finalize et (dosya + Discord)
// DB'deki players satiri: dosya kaldirildi, tek kaynak burasi.
static bool QueryBackendPlayerRow(int iPid, char *szName, size_t nNameLen, double *pLoss, int *pFps, int *pPing, int *pAgeSec)
{
	if(iPid < 0 || iPid >= MAX_PLAYERS) return false;
	if(szName && nNameLen > 0) szName[0] = 0;
	if(!g_isEngineActive || g_sessionToken.empty()) return false;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(4);
		char szUrl[64];
		sprintf_s(szUrl, sizeof(szUrl), "/api/player/cache?id=%d", iPid);
		auto res = cli.Get(szUrl);
		if(!res || res->status != 200) return false;
		json j = json::parse(res->body);
		if(!j.value("success", false) || !j.contains("rows") || !j["rows"].is_array() || j["rows"].empty()) return false;
		json r = j["rows"][0];
		std::string sN = r.value("name", "");
		if(sN.empty()) return false;
		if(szName && nNameLen > 0) strncpy_s(szName, nNameLen, sN.c_str(), _TRUNCATE);
		if(pLoss) { *pLoss = -1.0; if(r.contains("packetloss") && r["packetloss"].is_number()) *pLoss = r["packetloss"].get<double>(); }
		if(pFps) { *pFps = -1; if(r.contains("fps") && r["fps"].is_number()) *pFps = (int)r["fps"].get<double>(); }
		if(pPing) { *pPing = -1; if(r.contains("ping") && r["ping"].is_number()) *pPing = (int)r["ping"].get<double>(); }
		if(pAgeSec) { *pAgeSec = 999999; if(r.contains("ageSec") && r["ageSec"].is_number()) *pAgeSec = (int)r["ageSec"].get<double>(); }
		return true;
	}
	catch(...) { return false; }
}
// Hesap ID (/stats'in gosterdigi numara) -> oyuncu adi. players.id BAGLANTI slot
// ID'sidir (her reconnect'te degisir), hesap ID ayri kolonda durur.
static bool QueryBackendPlayerRowByAccount(int iAcc, char *szName, size_t nNameLen, int *pAgeSec)
{
	if(iAcc <= 0) return false;
	if(szName && nNameLen > 0) szName[0] = 0;
	if(!g_isEngineActive || g_sessionToken.empty()) return false;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(4);
		char szUrl[64];
		sprintf_s(szUrl, sizeof(szUrl), "/api/player/cache?accountId=%d", iAcc);
		auto res = cli.Get(szUrl);
		if(!res || res->status != 200) return false;
		json j = json::parse(res->body);
		if(!j.value("success", false) || !j.contains("rows") || !j["rows"].is_array() || j["rows"].empty()) return false;
		json r = j["rows"][0];
		std::string sN = r.value("name", "");
		if(sN.empty()) return false;
		if(szName && nNameLen > 0) strncpy_s(szName, nNameLen, sN.c_str(), _TRUNCATE);
		if(pAgeSec) { *pAgeSec = 999999; if(r.contains("ageSec") && r["ageSec"].is_number()) *pAgeSec = (int)r["ageSec"].get<double>(); }
		return true;
	}
	catch(...) { return false; }
}
// ?pl yedegi: DB satirini bellege al (cevap kodu aynen calissin).
static bool PlEnsureFromDb(int iPid)
{
	char szN[64]; double dL; int iF, iP;
	if(!QueryBackendPlayerRow(iPid, szN, sizeof(szN), &dL, &iF, &iP, NULL)) return false;
	if(!szN[0] || iPid < 0 || iPid >= MAX_PLAYERS) return false;
	strncpy_s(g_playerCache[iPid].szName, sizeof(g_playerCache[0].szName), szN, _TRUNCATE);
	g_playerCache[iPid].dLoss = dL;
	g_playerCache[iPid].iFps = iF;
	g_playerCache[iPid].iPing = iP;
	g_playerCache[iPid].dwTime = GetTickCount();
	Log("[PL-CACHE] id=%d DB'den tamamlandi: %s.", iPid, szN);
	return dL >= 0.0;
}
void ProcessZoneWarAutoJoin()
{
	DWORD dwNow = GetTickCount();
	// Stuck war temizleme
	if(g_inZoneWar && g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick > 11*60*1000){
		Log("[ZONEWAR] Savas 11dk'yi asti (stuck), temizleniyor.");
		g_inZoneWar = false;
		g_warRemainingSec = -1;
		g_warTimerTick = 0;
		g_warTimerViaTdw = false;
		g_gztCountdownAnnounced = 999;
		g_gztLastRem = -1;
		g_zoneWarStartTick = 0;
	}

	// ?gzt geri sayimi: uL.Hold/YS tarzi SABIT esikler - 09:10 / 09:03 / 09:01 /
	// 00:10 / 00:03 / 00:01. SADECE TEXTDRAW kaynakli timer gecerli (kendi savasimiz
	// GZ'sinde g-rünür); baska sava?larin "Time left:" chat mesajlari geri sayim
	// baslatmaz (bizde savas yokken "War ends in 05:00!" phantom duyurulariyordu).
	if(g_inZoneWar && g_gztOn)
	{
		int iRemainingSec = -1;
		if(g_warTimerViaTdw && g_warTimerTick != 0 && g_warRemainingSec > 0 && dwNow - g_warTimerTick < ZONE_WAR_TOTAL_MS)
			iRemainingSec = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
		else if(g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick < ZONE_WAR_TOTAL_MS)
			iRemainingSec = (int)((ZONE_WAR_TOTAL_MS - (dwNow - g_zoneWarStartTick)) / 1000); // textdraw yoksa baslangic hesabi

		if(iRemainingSec >= 0)
		{
			static const int iT[] = { 9*60+10, 9*60+3, 9*60+1, 10, 3, 1 };
			const int nT = 6;

			// 9:xx esikleri yalnizca GECIS'te duyurulur â€” ?gzt geÃ§ acilirsa geriye
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
						LM("!Savas %02d:%02d sonra bitiyor!", "!War ends in %02d:%02d!"),
						iT[i] / 60, iT[i] % 60);
					Log("[GZT] Geri sayim duyurusu: %02d:%02d kaldi.", iT[i] / 60, iT[i] % 60);
					g_bridgeSuppressEcho++;
					sendChat(szCnt);
				}
			}
			g_gztLastRem = iRemainingSec;
		}
		else
			g_gztLastRem = -1;
	}
	// ?zsnip ?yes on-sorusu: savas bitmeden (son 15sn) sor ki cevap hazir gelsin.
	if(g_ztkActive && !g_ztkAskNext && !g_ztkPreAsked
		&& g_snipZoneCount > 0 && g_snipZoneIdx + 1 < g_snipZoneCount)
	{
		int iRemQ = -1;
		if(g_warTimerTick != 0 && g_warRemainingSec > 0 && dwNow - g_warTimerTick < ZONE_WAR_TOTAL_MS)
			iRemQ = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
		else if(g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick < ZONE_WAR_TOTAL_MS)
			iRemQ = (int)((ZONE_WAR_TOTAL_MS - (dwNow - g_zoneWarStartTick)) / 1000);
		if(iRemQ >= 1 && iRemQ <= 15)
		{
			g_ztkPreAsked = true;
			g_ztkAskNext = true;
			Log("[ZSNIP] Savas bitmeden ?yes soruluyor (%ds kaldi).", iRemQ);
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Diger zone'a da saldirayim mi? (?yes)", "!Attack the next zone too? (?yes)"));
		}
	}

	// ?pl: Stats-post gecikmeli cevabi beklerken zaman asimi. Once taze Stats-post
	// dener (genelde ~1 sn icinde gelir); gelmezse cache'ten cevapla (fps yoksa "--").
	if(g_waitingForPNetStats && dwNow - g_pnetstatsReqTime > 3500)
	{
		g_waitingForPNetStats = false;
		if(g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS && (g_playerCache[g_plTargetId].dLoss >= 0.0 || PlEnsureFromDb(g_plTargetId)))
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
			Log("[PNETSTATS] Zaman asimi, cache/DB ile cevap: %s", szOut);
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

	// Kisa GZ giris donusu: takim rengi ogrenildi (ya da sure doldu), /fr ile freeroam'a don.
	// Kill-list girisinde (g_zwKillListStay) GZ'de KALINIR - sava-s bitince won/loses
	// textdraw'i gelir, FinalizeKillList /fr'yi kendisi gonderir. Kisa giris (savas basi
	// 6 sn / 09:02 kontrol 2 sn) her zaman cikar.
	if(g_zwReturnPending && dwNow >= g_zwReturnAt)
	{
		if(!g_zwKillListStay)
		{
			g_zwReturnPending = false;
			Log("[ZONEWAR] Kisa GZ giris bitti, /fr ile freeroam'a donuluyor.");
			sendServerCommand((char *)"/fr");
		}
	}

	// GZ aktif: son 00:02'de /gz2 gonder ve ICERIDE KAL.
	// Kill-list textdraw'lari sadece GZ icindekilere gelir; bitis textdraw'lari
	// g_zwCapturingKillList acikken toplanir, pencere dolunca finalize edilir.
	// Iki asamali giris (kullanici istegi):
	//  1) 09:02'de kisa kontrol girisii - sava-s aktif mi diye GZ'ye gir, 2 sn kal, /fr ile don.
	//     (Saldiri yapilan gruptan kimse gz2'ye katilmazsa savas ~1 dk icinde biter ve
	//      zone sahibi olunur; kontrol girisii sava-sin hala surdugunu ogrenir.)
	//  2) 00:02'de kill-list girisii - iceride kal, kill list textdraw'larini topla, bitince Discord'a at.
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

		// 00:03 kill-list girisi: son 3 saniyede GZ'ye gir, iceride kal.
		bool bEnterNow = false;
		if(iLeft >= 0 && iLeft <= 3) // son 3 sn: gir ve kal
			bEnterNow = true;

		if(bEnterNow)
		{
			g_zwAutoJoinSent = true;
			g_zwKillListStay = true; // GZ'de kal, savas bitince finalize + /fr
			// Yakalayici savas boyunca zaten acik (g_zwCapturingKillList true);
			// GZ'ye giris textdraw'larin toplanmasini saglar. Pencere dolunca finalize.
			g_zwCaptureStart = dwNow;
			g_zwFinalizeAt = 0; // 00:02 giris: pencere moduna don
			// Savas bitince /fr ile geri don (pencere sonu finalize + donus)
			g_zwReturnPending = true;
			g_zwReturnAt = dwNow + KILL_LIST_CAPTURE_WINDOW_MS;
			if(g_zwMyColor == 0)
				Log("[KILLLIST] GZ bitimine 00:03 kala giriliyor (takim rengi bilinmiyor, giris sirasinda ogrenilecek).");
			else
				Log("[KILLLIST] GZ bitimine 00:03 kala giriliyor, kill/death/assist/DMG textdraw'lari toplanacak.");
			if(g_zwNoGzEntry) { Log("[KILLLIST] 00:03 girisi atlandi (/gz once reddedildi)."); }
			else { g_zwLastGzAt = GetTickCount(); sendServerCommand((char *)"/gz"); }
			if(!g_znipHeldForWar && (g_ztkActive || g_snipInterrupt || g_snipPermPending))
			{
				g_znipHeldForWar = true;
				g_znipHoldAt = GetTickCount();
				Log("[ZSNIP] Sniper bizim savas icin duraklatildi (kill-list girisi).");
			}
		}
	}

	// Toplama: yakalayici acikken sadece GERCEK bitis/veri aninda finalize et.
	// 1) Savas bitisi (wins/loses blo-su g_zwFinalizeAt = now+3000 kurar): bitis
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
				char szRowRole[32] = {0};
				{
					const char *pC1 = strchr(szLine, '\t');
					const char *pC2 = pC1 ? strchr(pC1 + 1, '\t') : NULL;
					if(pC2)
					{
						const char *pR = pC2 + 1;
						const char *pRE = strchr(pR, '\t');
						size_t nR = pRE ? (size_t)(pRE - pR) : strlen(pR);
						if(nR > 31) nR = 31;
						memcpy(szRowRole, pR, nR); szRowRole[nR] = 0;
						StripColorCodes(szRowRole);
						char *pT = szRowRole; while(*pT == ' ') pT++;
						if(pT != szRowRole) memmove(szRowRole, pT, strlen(pT) + 1);
						for(int k = (int)strlen(szRowRole) - 1; k >= 0 && szRowRole[k] == ' '; k--) szRowRole[k] = 0;
					}
				}
				bool bAuthRole = szRowRole[0] && (!_stricmp(szRowRole, "Owner") || !_stricmp(szRowRole, "Deputy Owner") || !_stricmp(szRowRole, "Leader") || !_stricmp(szRowRole, "Co-Leader"));
				if(bAuthRole)
				{
					if(!s_bGmBotOk && pBot[0] && strstr(szLine, pBot)) { s_bGmBotOk = true; strncpy_s(s_sGmBotRole, sizeof(s_sGmBotRole), szRowRole, _TRUNCATE); Log("[ZSNIP] Bot rolu eslesti: '%s' (satir %d).", szRowRole, iRow); }
					if(!s_bGmUserOk && pUser[0] && strstr(szLine, pUser)) { s_bGmUserOk = true; strncpy_s(s_sGmUserRole, sizeof(s_sGmUserRole), szRowRole, _TRUNCATE); Log("[ZSNIP] Kullanici rolu eslesti: '%s' (satir %d).", szRowRole, iRow); }
				}
				// Acik "sonraki sayfa" satiri varsa kaydet (tiklanacak ogge)
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

	Log("[ZSNIP] Yetki aramasi bitti (sayfa %d): bot=%s (%s), kullanici=%s (%s).",
		s_iGmPage, pBot, s_bGmBotOk ? s_sGmBotRole : "YOK", pUser, s_bGmUserOk ? s_sGmUserRole : "YOK");

	if(!pBot[0] || !pUser[0])
	{
		g_bridgeSuppressEcho++;
		sendChat((char *)LM("!Isimler tespit edilemedi, sniper iptal.", "!Names could not be resolved, sniper cancelled."));
		return;
	}
	if(!s_bGmBotOk || !s_bGmUserOk)
	{
		// Kullanici istegi: yetki reddi otomatik iptal degil. Kesilen zone taramasi
		// bekler; ?yes gelirse yine de saldirilir, ?no gelirse tarama devam eder.
		if(g_snipInterrupt && g_snipAction == 0)
		{
			Log("[ZSNIP] Yetki reddi: ?yes ile yine de saldirilir, ?no ile tarama devam eder.");
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Yetki yok - saldiri icin ?yes, taramaya donmek icin ?no.",
				"?No permission - ?yes to attack anyway, ?no to resume zone scan."));
			return;
		}
		g_bridgeSuppressEcho++;
		{
			char szPerm[224];
			sprintf_s(szPerm, sizeof(szPerm),
				LM("!Rol eslesmedi, sniper baslamadi (bot: %s, kullanici: %s).",
				"!Role mismatch, sniper not started (bot: %s, user: %s)."),
				s_bGmBotOk ? s_sGmBotRole : "yok", s_bGmUserOk ? s_sGmUserRole : "yok");
			sendChat(szPerm);
		}
		return;
	}

	// Yetki tamam: istege gore sniper'i kapat veya baslat.
	if(g_snipAction == 1)
	{
		g_snipInterrupt = false; // off: bekleyen istek de iptal
		g_ztkActive = false;
		g_ztkZoneId = -1;
		g_ztkWaitAnnounced = false;
		g_ztkAttackPending = false;
		g_ztkOutcome = 0;
		g_ztkTargetWar = false;
		g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
		g_snipGroupName[0] = 0;
		g_snipZoneCount = 0;
		g_snipZoneIdx = 0;
				ResumeZoneScanAfterSnip();
Log("[ZSNIP] Kullanici iptal etti (yetki dogrulandi), sniper kapatildi.");
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
	ZtkGotoZone(iZone);

	PauseZoneScanForSnip();
	g_snipInterrupt = false; // istek onayla tamamlandi
	g_ztkActive = true;
	Log("[ZSNIP] Zone sniper basladi: zone #%d, bekleyis %d sn.", iZone, iWaitSec);

	char szMsg[160];
	if(iWaitSec > 0)
		sprintf_s(szMsg, sizeof(szMsg),
			LM("!Zone #%d savasi bitiyor (%d sn), sniper bekliyor...", "!War in zone #%d ending (%d sec), sniper waiting..."), ZONE_SHOWN_FROM_IDX(iZone), iWaitSec);
	else
		sprintf_s(szMsg, sizeof(szMsg),
			LM("!Zone #%d hedeflendi, savas aciliyor...", "!Zone #%d targeted, starting war..."), ZONE_SHOWN_FROM_IDX(iZone));
	g_bridgeSuppressEcho++;
	sendChat(szMsg);
}

// ?zsnip (zone sniper): hedef zonede savas SURUYORSA bitisini bekle; bitince
// (ya da savas yoksa) /gz ile saldiri baslat. Savas bitisi g_inZoneWar=false ile anlasilir.
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
	ProcessAlogAstatsQueue();
	if(g_znipHeldForWar)
	{
		if(!g_inZoneWar && !g_zwKillListStay)
		{
			DWORD dwHeld = GetTickCount() - g_znipHoldAt;
			if(g_ztkAttackAt != 0) g_ztkAttackAt += dwHeld;
			if(g_ztkVerifyAt != 0) g_ztkVerifyAt += dwHeld;
			if(g_ztkTeleportAt != 0) g_ztkTeleportAt += dwHeld;
			g_znipHeldForWar = false;
			Log("[ZSNIP] Savas bitti, sniper kaldirildigi yerden devam.");
		}
		return;
	}
	if(!g_ztkActive) return;

	// Faz 2: /gz gonderildi, kendi savasimizin sonucu bekleniyor (won/lost game text
	// veya chat war-end). Sonuc gelince zincir kararini ver.
	if(g_ztkAttackPending)
	{
		if(g_inZoneWar) { g_ztkWarSeen = true; return; } // savasimiz suruyor
		if(!g_ztkWarSeen)
		{
			// /gz sonrasi savas hic baslamadi: 20 sn icinde war mesaji gelmezse basarisiz.
			// (Eskiden sahte KAZANILDI yaziliyordu: outcome==0 && weWon hep true idi.)
			if(GetTickCount() - g_ztkAttackAt < 20000) return;
			Log("[ZSNIP] Saldiri basarisiz: 20 sn icinde savas baslamadi, sniper durduruluyor.");
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Saldiri basarisiz: savas baslamadi, sniper durduruldu.", "!Attack failed: no war started, sniper stopped."));
			g_ztkAttackPending = false;
			g_ztkOutcome = 0;
			g_ztkActive = false;
			g_ztkZoneId = -1;
			g_snipZoneCount = 0;
			g_snipZoneIdx = 0;
			g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
			g_snipGroupName[0] = 0;
			ResumeZoneScanAfterSnip();
			return;
		}
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
			if(g_ztkYesPending)
			{
				g_ztkYesPending = false;
				g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
				g_ztkPreAsked = false;
				int iZoneY = g_snipZones[g_snipZoneIdx];
				ZtkGotoZone(iZoneY);
				PauseZoneScanForSnip();
				g_ztkActive = true;
				Log("[ZSNIP] Onceden verilen ?yes uygulaniyor: zone #%d.", iZoneY);
			}
			else if(!g_ztkAskNext)
			{
				g_ztkAskNext = true;
				Log("[ZSNIP] ?yes soruluyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Diger zone 'a da saldirayim mi? (?yes)", "!Attack the next zone too? (?yes)"));
			}
		}
	else
		{
			g_snipZoneCount = 0;
			g_snipZoneIdx = 0;
			g_ztkAskNext = false;
	g_ztkYesPending = false;
	g_ztkPreAsked = false;
			g_snipGroupName[0] = 0;
			g_bridgeSuppressEcho++;
						ResumeZoneScanAfterSnip();
		sendChat((char *)LM("!Sniper bitti.", "!Sniper finished."));
		}
		return;
	}

	// Faz 1: hedef zonede savas suruyorsa beklemeye devam et (bir kez duyur).
	// Hem kendi savasimiz (g_inZoneWar) hem dusmanin savasi (g_ztkTargetWar -
	// "takes over zone '# N'" mesajiyla biter, zamanlayiciya guvenilmez).
	if(g_ztkTargetWar || g_inZoneWar)
	{
		if(!g_ztkWaitAnnounced)
		{
			g_ztkWaitAnnounced = true;
			Log("[ZSNIP] Zone #%d savasi suruyor, bitisi bekleniyor.", g_ztkZoneId);
			g_bridgeSuppressEcho++;
			// LM tek basina cagrilamaz (sendChat 1 arg), once sprintf ile doldurulur:
			// onceki halde %d hic dolmadan gonderiliyordu.
			char szWarMsg[160];
			sprintf_s(szWarMsg, sizeof(szWarMsg), LM("!Zone #%d savasi devam ediyor, sniper bitisini bekliyor.",
				"!War in zone #%d is ongoing, sniper waiting for it to end."), ZONE_SHOWN_FROM_IDX(g_ztkZoneId));
			sendChat(szWarMsg);
		}
		return;
	}

	// Faz 1 bitti (savas yok/bitmis): hedef zone /gzinfo ile dogrulanir, eslesirse /gz.
	// ?zgo mantigi: teleport sonrasi 6 sn varis payi (duserken atilan /gzinfo eski
	// zone'u okuyup yanlis dogrulama uretiyordu); cevapsiz /gzinfo 3 denemede kesilir.
	if(g_ztkZoneId >= 0 && g_ztkZoneId < g_zoneCount)
	{
		if(GetTickCount() - g_ztkTeleportAt < 6000) return;
		if(g_ztkVerifyPending && GetTickCount() - g_ztkVerifyAt > 12000)
		{
			g_ztkVerifyPending = false;
			if(++g_ztkVerifyTries >= 3)
			{
				g_ztkVerifyTries = 0;
				Log("[ZSNIP] Zone #%d /gzinfo cevapsiz (3 deneme), siradakine geciliyor.", g_ztkZoneId);
				ZtkChainAdvance();
				return;
			}
			Log("[ZSNIP] Dogrulama cevabi gelmedi, /gzinfo tekrar gonderiliyor.");
		}
		if(!g_ztkVerifyPending)
		{
			g_ztkVerifyPending = true;
			g_ztkVerifyZone = g_ztkZoneId;
			g_ztkVerifyAt = GetTickCount();
			Log("[ZSNIP] Zone #%d sahibi dogrulaniyor (/gzinfo)...", g_ztkZoneId);
			sendServerCommand((char *)"/gzinfo");
		}
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
		if(strncmp(s, "Zone:", 5) == 0) return false;       // Oasis /gzinfo ("Zone: Bayside")
		if(strstr(s, "Account ID")) return false;           // /stats cevabindan ("Account ID: 3682305")
		// gstats dugmeleri/yardim metni grup adi degildir.
		if(_stricmp(s, "my group") == 0 || strstr(s, "move to new group") || strstr(s, "delete group") || strstr(s, "invite player")) return false;
		{
			size_t nL = strlen(s);
			if(nL >= 7 && _stricmp(s + nL - 7, " Group") == 0) return false; // "... - Group", "Everybody Hates This Group"
			int nAl = 0;
			for(const char *p = s; *p; p++)
				if((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) nAl++;
			if(nAl < 3) return false;                         // "-", "!", "...", "[G.C.]", "nO." -> isaret yigini
		}
		if(strstr(s, "Welcome")) return false;              // karsilama dialogu
		if(strncmp(s, "ID Name", 7) == 0 && strstr(s, "Members")) return false; // /groups basligi
		if(strstr(s, "Slot") && strstr(s, "Members Online")) return false; // Oasis /groups basligi
		if(strstr(s, "Group Name") && strstr(s, "Members Online")) return false;
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
	// Oasis /gstats ilk satiri "9 Inches [9]" biciminde: isim + [TAG].
	if(!strstr(szInfo, "Tag:"))
	{
		const char *pBrO = strrchr(szName, '[');
		const char *pBrC = pBrO ? strchr(pBrO, ']') : NULL;
		if(pBrO && pBrC && pBrC > pBrO + 1 && pBrC - pBrO < 12 && pBrO > szName)
		{
			size_t nT = (size_t)(pBrC - pBrO - 1);
			if(nT > 31) nT = 31;
			memcpy(szTag, pBrO + 1, nT); szTag[nT] = 0;
			char *pTrim = (char *)pBrO;
			while(pTrim > szName && *(pTrim-1) == ' ') pTrim--;
			*pTrim = 0;
			NormalizeGroupName(szName);
			Log("[GSTATS-TAG] Oasis basligi: '%s' -> tag '%s'.", szName, szTag);
		}
	}
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
	// "~" sentinel'iyle isaretlenir (harvest bir daha taramasin; ekranda tam isim gosterilir).
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
// gondererek grubun kisa TAG'ini ogrenir. Zincir bir sonraki adimi her gstats
// sonucunda SIGILIR (SAMPRPC -> OnGStatsResultForHarvest).
// ponytail: /groups ve /gstats ciktilari canli sunucuya gore; format degisirse
// parse kurali (OnGroupsListCaptured) guncellenir.
#define GHP_MAX_GROUPS 256
static int   g_ghpIds[GHP_MAX_GROUPS];
static char  g_ghpNames[GHP_MAX_GROUPS][80];

// /groups sonuc dialogunu bekleyen flag (SAMPRPC yakalar, OnGroupsListCaptured'a verir)
bool g_waitingForGroups = false;
unsigned long g_groupsReqTime = 0;

// /groups sonuc dialogunu parse et + harvest'i baslat. SAMPRPC cagirir.
void OnGroupsListCaptured(const char *szInfo)
{
	Log("[GHP] OnGroupsListCaptured: ghpActive=%d len=%d", g_ghpActive, szInfo ? (int)strlen(szInfo) : -1);
	g_ghpForce = false; // tek seferlik: tarama basladi, tuket
	if(g_ghpActive) return;
	Log("[GROUPS-DATA] %.500s", szInfo ? szInfo : "");

	g_ghpCount = 0; g_ghpIdx = 0; g_ghpActive = true; g_ghpWaitingGstats = false;
	if(!szInfo) { Log("[GHP] /groups ciktisi bos, harvest iptal."); g_ghpActive = false; return; }

	// Satir bazli parse: "N. isim" / "N: isim" / "N- isim" (ilk token sayi = grup ID)
	char szLine[200];
	int nSkipped = 0; // tag'i zaten olan gruplar (/gstats gonderilmez)
	const char *pL = szInfo;
	while(pL && *pL && g_ghpCount < GHP_MAX_GROUPS)
	{
		const char *pE = strchr(pL, '\n');
		size_t nLen = pE ? (size_t)(pE - pL) : strlen(pL);
		if(nLen == 0 || nLen >= sizeof(szLine)) { pL = pE ? pE + 1 : NULL; continue; }
		memcpy(szLine, pL, nLen); szLine[nLen] = 0;
		StripColorCodes(szLine); // Oasis satirlari {RRGGBB} kodlu gelir
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
		// Oasis: "9 Inches [9]" - isim yanindaki [TAG] ayikla, tag aninda kaydet.
		char szRowTag[16] = {0};
		{
			char *pBrO = strrchr(szName, '[');
			char *pBrC = pBrO ? strchr(pBrO, ']') : NULL;
			if(pBrO && pBrC && pBrC > pBrO + 1 && pBrC - pBrO < 12 && pBrO > szName)
			{
				size_t nT = (size_t)(pBrC - pBrO - 1);
				if(nT > 15) nT = 15;
				memcpy(szRowTag, pBrO + 1, nT); szRowTag[nT] = 0;
				char *pTrim = pBrO;
				while(pTrim > szName && *(pTrim-1) == ' ') pTrim--;
				*pTrim = 0;
			}
			if(szRowTag[0])
			{
				int rIdx = FindGroupColorByName(szName);
				if(rIdx >= 0)
				{
					if(!g_groupColors[rIdx].szTag[0] || !_stricmp(g_groupColors[rIdx].szTag, g_groupColors[rIdx].szName))
					{
						strncpy_s(g_groupColors[rIdx].szTag, sizeof(g_groupColors[0].szTag), szRowTag, sizeof(g_groupColors[0].szTag) - 1);
						SaveGroupColors();
						Log("[GHP] Liste tagi: '%s' -> '%s'.", szName, szRowTag);
					}
				}
				else if(g_groupColorCount < MAX_GROUP_COLORS)
				{
					strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName), szName, sizeof(g_groupColors[0].szName) - 1);
					strncpy_s(g_groupColors[g_groupColorCount].szTag, sizeof(g_groupColors[0].szTag), szRowTag, sizeof(g_groupColors[0].szTag) - 1);
					g_groupColorCount++;
					SaveGroupColors();
					Log("[GHP] Liste tagi (yeni): '%s' -> '%s'.", szName, szRowTag);
				}
			}
		}

		// /groups satiri "N. Isim <online-sayisi>" biciminde: sondaki sayi uyelik
		// adedi, ismin parcasi DEGIL. Kesmezsek DB'deki temiz isimle eslesmez ve
		// hicbir grup ATLANDI sayilmazdi (her periyotta hepsi yeniden taranýyordu).
		{
			int nLen = (int)strlen(szName);
			int nEnd = nLen;
			while(nEnd > 0 && szName[nEnd-1] >= '0' && szName[nEnd-1] <= '9') nEnd--;
			if(nEnd > 0 && nEnd < nLen && szName[nEnd-1] == ' ')
				szName[nEnd-1] = 0;
		}
		// Oasis: "son (ID: 67)" soneki isimden dusulur; /gstats SLOT ile gider (ID degil).
		{
			char *pIdO = strstr(szName, "(ID:");
			if(pIdO)
			{
				char *pTrim = pIdO;
				while(pTrim > szName && *(pTrim-1) == ' ') pTrim--;
				*pTrim = 0;
			}
		}
		if(iId < 0 || !szName[0]) continue;

		// DB'de GERCEK kisa tag'i olan grup atlanir: /gstats sadece BILINMEYENLER
		// icin gonderilir (komut mute riski + gereksiz tekrar).
		// Isim eslesmesi TEK BASINA yetmez: zone taramasinda ogrenilen gruplara once
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
			// Tag gorduk: bir isimle iliskilendirilmis kisa etiket.
			if(g_groupColors[i].szTag[0] && _stricmp(g_groupColors[i].szTag, "NULL") != 0
				&& _stricmp(g_groupColors[i].szTag, szNeed) == 0)
			{
				bKnown = true;
				break;
			}
		}
		if(bKnown && !g_ghpForce)
		{
			Log("[GHP] '%s' (id=%d) ATLANDI - grup datasi zaten var, eklenmedi.", szName, iId);
			nSkipped++;
			continue;
		}

		g_ghpIds[g_ghpCount] = iId; // Oasis: slot
		strncpy_s(g_ghpNames[g_ghpCount], 80, szName, 79);
		g_ghpCount++;
	}

	Log("[GHP] /groups: %d grup ATLANDI (datasi zaten var), %d yeni grup tag icin taranacak.",
		nSkipped, g_ghpCount);
	if(g_ghpCount == 0) { g_ghpActive = false; return; }
	g_ghpNextStepAt = GetTickCount() + GHP_STEP_DELAY_MS; // kapatma cevabi islesin diye ilk adim deadline ile (race olmasin)
}

// Sonraki harvester adimi: bir sonraki ID icin /gstats gonder (veya bitir).
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
		// ?zgrp on kalicidir: harvest bitti, oto bayrak acik kalir, sonraki
		// periyotta (15 dk) yeni gruplar icin tekrar calisir.
		SyncGroupColorsToDb();
		// Gruba mesaj ATILMAZ (harvest arka plan isidir; spam olmasin).
		if(g_ghpLearnedCount > 0)
			Log("[GHP] %d yeni grup tagi DB'ye kaydedildi.", g_ghpLearnedCount);
		// Tag'ler ogrenildi; ?zscan on ise SIMDI flashing zone taramasini baslat
		// (kullanici sirasi: once taglar, sonra zone ogrenme). Restore sonrasi.
		if(g_zscanAuto && s_bGroupDbRestored && iSpawned && g_zoneScanIdx < 0 && !g_inZoneWar && !g_zgoActive && !g_zscanPausedForSnip)
		{
			Log("[GHP] Harvest bitti - otomatik zone taramasi baslatiliyor.");
			StartZoneScan(true);
		}
		// Tag'ler ogrenildi; ?zf ozeti tag'lerle tekrar atilsin (kullanici istemedigi
		// surece bir daha harvest yapilmaz - tag'ler db'de kalici).
		if(g_zfFlashPendingFlag || g_zfFlashAt != 0)
		{
			g_zfFlashAt = GetTickCount() + 1000;
			Log("[GHP] Harvest sonrasi ?zf ozeti tag'lerle yeniden gonderilecek.");
		}
		return;
	}

	int iId = g_ghpIds[g_ghpIdx];
	Log("[GHP] (%d/%d) /gstats %d gonderiliyor (grup: %s).", g_ghpIdx + 1, g_ghpCount, iId, g_ghpNames[g_ghpIdx]);

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

// /gstats <ID> sonucu: harvest modundaysa TAG'i ogren + siradaki ID'ye gec.
// true doner => SAMPRPC bu sonucu Discord'a gondermez (harvest icin kullanildi).
// NOT: /gstats ciktisindan zone->sahip baglamasi YAPILMAZ. Diyalogdaki sayilar
// guvenilmezdi (uyelerin sayisi/kill istatistigi zone sanilip yanlis zone
// baglaniyordu: #26 -> dEV, #81 -> SG hatalari). Zone sahibi YALNIZCA /gzinfo
// taramasindan ve "ZONE WAR: X takes/keeps" mesajlarindan ogrenilir.
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

	// "- Tag: [nM]" satirindan kisa etiketi ogren (?zgrp ile toplu tag toplama)
	if(LearnGroupTagFromGStats(szInfo)) g_ghpLearnedCount++;

	g_ghpIdx++;
	g_ghpWaitingGstats = false;
	g_waitingForGStats = false;
	g_ghpStaleRetries = 0; // adim basariyla ilerledi: yeni adim icin retry sayisi sifir
	g_muteReconnectPending = false; // adim cevap verdi: baglanti saglikli, sonraki mute icin sifirla
	// Komut mute korumasi: siradaki /gstats'i aninda degil, GHP_STEP_DELAY_MS
	// sonra gonder (ProcessZoneScan deadline'i gorunce ProcessGhpHarvestStep cagirir).
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
	ResumeZoneScanAfterSnip();

	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", g_zgoReturnPos[0], g_zgoReturnPos[1], g_zgoReturnPos[2]);
	Log("[ZGO] Bekleme suresi doldu, eski pozisyona donuluyor (%s).", szCmd);
	sendServerCommand(szCmd);

	char szMsg[128];
	if(g_zgoZoneId >= 0)
		sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d beklemesi bitti, geri donuldu.", "!Zone #%d wait finished, returned back."), ZONE_SHOWN_FROM_IDX(g_zgoZoneId));
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


