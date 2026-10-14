/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"

RakClientInterface *pRakClient = NULL;
int iAreWeConnected = 0, iConnectionRequested = 0, iSpawned = 0, iGameInited = 0, iSpawnsAvailable = 0;
unsigned long g_spawnTick = 0; // son spawn ani (tag harvest 60 sn korumasi)
int iReconnectTime = 2 * 1000, iNotificationDisplayedBeforeSpawn = 0;

PLAYERID g_myPlayerID;
char g_szNickName[32];

struct stPlayerInfo playerInfo[MAX_PLAYERS];
struct stVehiclePool vehiclePool[MAX_VEHICLES];

FILE *flLog = NULL, *flTextDrawsLog = NULL;

DWORD dwAutoRunTick = GetTickCount();

extern int iMoney, iDrunkLevel, iLocalPlayerSkin;
extern BOOL bIsSpectating;
// netrpc.cpp: UIF Login dialogu a??k mi? A??kken spawn istegi g?nderilmez
// (yoksa sunucu tmp hesap spawn eder, gercek hesap kaybedilir).
extern bool g_loginPending;
extern unsigned long g_loginPendingUntil;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	srand((unsigned int)GetTickCount());

	// load up settings
	if(!LoadSettings())
	{
		Log("Failed to load settings");
		getchar();
		return 0;
	}

	// Local testte: "http://127.0.0.1:3000"
	// VDS canlida: "http://144.48.83.9:3000"
	// Ayni PC 2. bot: nick HWID sonuna eklenir (kopya basina kimlik).
	if (!PerformSecurityHandshake("http://127.0.0.1:3000", settings.server.szNickname)) {
		return 0;
	}

	if(settings.iConsole)
		SetUpConsole();
	else
	{
		SetUpWindow(hInstance);
		Sleep(500); // wait a bit for the dialog to create
	}

	// RCON mode
	if(settings.runMode == RUNMODE_RCON)
	{
		if(RCONReceiveLoop())
		{
			if(flLog != NULL)
			{
				fclose(flLog);
				flLog = NULL;
			}

			return 0;
		}
	}	

	// set up networking
	pRakClient = RakNetworkFactory::GetRakClientInterface();
	if(pRakClient == NULL)
		return 0;

	pRakClient->SetMTUSize(576);

	resetPools(1, 0);
	RegisterRPCs(pRakClient);

	SYSTEMTIME time;
	GetLocalTime(&time);
	if(settings.iConsole)
	{
		Log(" ");
		Log("* ===================================================== *");
		Log("  RakSAMP " RAKSAMP_VERSION " initialized on %02d/%02d/%02d %02d:%02d:%02d",
			time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
		Log("  Authors: " AUTHOR "");
		Log("* ===================================================== *");
		Log(" ");
	}

	char szInfo[400];
	char szLastInfo[400];
	
	int iLastMoney = iMoney;
	int iLastDrunkLevel = iDrunkLevel;

	int iLastStatsUpdate = GetTickCount();
	
	while(1)
	{
		UpdateNetwork(pRakClient);

		if(settings.bSpam)
			sampSpam();

		if (settings.bFakeKill)
			sampFakeKill();

		if (settings.bLag)
			sampLag();

		if (settings.bJoinFlood)
			sampJoinFlood();

		if (settings.bChatFlood)
			sampChatFlood();

		if (settings.bClassFlood)
			sampClassFlood();

		processPulsator();
		processBulletFlood();

		if (!iConnectionRequested)
		{
			if(!iGettingNewName)
				sampConnect(settings.server.szAddr, settings.server.iPort, settings.server.szNickname, settings.server.szPassword, pRakClient);
			else
				sampConnect(settings.server.szAddr, settings.server.iPort, g_szNickName, settings.server.szPassword, pRakClient);

			iConnectionRequested = 1;
		}

		if (iAreWeConnected && iGameInited)
		{
			// UIF LOGIN BEKLEMESI: kullan?c? ?ifreyle hesabina girene kadar bot
			// aksiyonlari (war, ztk, db sync, fr, tarama, discord chat) BASLAMAZ.
			// Sadece chat kuyrugu ve dialog yakalama ("You logged in" algilama) calisir.
			// (g_loginPending: InitGame'de true; login dialogu gelirse SAMPRPC korur;
			//  "You logged in to your account" mesaj? gelince false olur.)
			if(g_loginPending)
			{
				// Timeout: login dialogu hic gelmezse 10 sn sonra otomatik devam et.
				// (g_loginPendingUntil: InitGame'de now+10s; login dialogu gelirse
				// SAMPRPC 0 yapar = kullan?c? girene kadar bekle, timeout yok.)
				if(g_loginPendingUntil != 0 && GetTickCount() > g_loginPendingUntil)
				{
					g_loginPending = false;
					Log("[LOGIN] Login bekleme timeout (10 sn), normal akisa devam ediliyor.");
				}
				else
				{
					ProcessChatOutQueue();
					PollDiscordChat();
					continue;
				}
			}

			// Zone war: ?pl zaman asimi + GZ sonuc textdraw yakalama (bot GZ'ye girmez)
			ProcessZoneWarAutoJoin();

			// ?gmembers: Google Sheets roster koleksiyonu (uye + astats + gvips)
			ProcessGmembers();

			// ?ztk zone sniper: hedef zonede sava? bitince /gz (isine bekleme)
			ProcessZtkTick();

			// ?follow (whisper): hedef oyuncuyu /pos ile takip (3 sn'de bir)
			ProcessFollowTick();

			// Cache + grup renklerini backend'e (MySQL) yazar (60 sn'de bir)
			ProcessDbSync();

			// Cikis chat kuyrugu: bot mesajlari 5 sn arayla g?nderilir
			ProcessChatOutQueue();

			// ?zgo: zone bekleme suresi dolduysa eski pozisyona don
			ProcessZgoReturn();

			// Freeroam: /fr'yi sendServerCommand ile aralikli g?nder (chat'e yansimaz)
			ProcessFrRetry();

			// ?zf renk->grup taramasi: zgo mantigiyla /gzinfo (ba?lant? sonrasi oto, bir kez)
			ProcessZoneScan();

			// ?zf: /gzinfo asenkron oldugu icin flash paketleri geldikten sonra (3 sn) dagilim
			ProcessZfFlashSummary();

			// Discord grup chat kopusu: bekleyen Discord mesajlarini cek (2 sn'de bir)
			PollDiscordChat();

			// Discord -> Oyun komut kuyrugu (/send, /gstats): oyunda calistir
			PollCommands();

			// Welcome ayarlarini backend'den tazele (kisisel mesaj, 10 sn'de bir)
			RefreshWelcomeState();

			static DWORD dwLastInfoUpdate = GetTickCount();
			if(dwLastInfoUpdate && dwLastInfoUpdate < (GetTickCount() - 1000))
			{
				char szHealthText[16], szArmourText[16];

				if(settings.fPlayerHealth > 200.0f)
					sprintf_s(szHealthText, sizeof(szHealthText), "N/A");
				else
					sprintf_s(szHealthText, sizeof(szHealthText), "%.2f", settings.fPlayerHealth);

				if(settings.fPlayerArmour > 200.0f)
					sprintf_s(szArmourText, sizeof(szArmourText), "N/A");
				else
					sprintf_s(szArmourText, sizeof(szArmourText), "%.2f", settings.fPlayerArmour);

				sprintf_s(szInfo, 400, "Hostname: %s     Players: %d     Ping: %d     Authors: %s\nHealth: %s     Armour: %s     Skin: %d     X: %.4f     Y: %.4f     Z: %.4f     Rotation: %.4f",
				g_szHostName, getPlayerCount(), playerInfo[g_myPlayerID].dwPing, AUTHOR, szHealthText, szArmourText, iLocalPlayerSkin, settings.fNormalModePos[0], settings.fNormalModePos[1], settings.fNormalModePos[2], settings.fNormalModeRot);
				
				if(strcmp(szInfo, szLastInfo) != 0)
				{
					SetWindowText(texthwnd, szInfo);
					sprintf_s(szLastInfo, szInfo);
				}
			}

			if (settings.iUpdateStats)
			{
				if((GetTickCount() - iLastStatsUpdate >= 1000) || iMoney != iLastMoney || iDrunkLevel != iLastDrunkLevel)
				{
					RakNet::BitStream bsSend;

					bsSend.Write((BYTE)ID_STATS_UPDATE);

					iDrunkLevel -= (rand() % settings.iMaxFPS + settings.iMinFPS);

					if(iDrunkLevel < 0)
						iDrunkLevel = 0;

					bsSend.Write(iMoney);
					bsSend.Write(iDrunkLevel);

					pRakClient->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);

					iLastMoney = iMoney;
					iLastDrunkLevel = iDrunkLevel;

					iLastStatsUpdate = GetTickCount();
				}
			}

			if(settings.runMode == RUNMODE_BARE)
				goto bare;

			if(!iSpawned)
			{
				if(settings.iManualSpawn != 0)
				{
					if(!iNotificationDisplayedBeforeSpawn)
					{
						sampRequestClass(settings.iClassID);
						
						Log("Please write !spawn into the console when you're ready to spawn.");

						iNotificationDisplayedBeforeSpawn = 1;
					}
				}
				else
				{
					// UIF Login dialogu a??kken spawn GONDERME: sunucu tmp hesap
					// spawn ederdi. Login (kullan?c? ?ifreyle girdi) bitene kadar beklenir.
					if(g_loginPending)
					{
						if(!iNotificationDisplayedBeforeSpawn)
						{
							Log("[LOGIN] Login bekleniyor, spawn ertelendi (tmp hesap onleniyor).");
							iNotificationDisplayedBeforeSpawn = 1;
						}
					}
					else
					{
						sampRequestClass(settings.iClassID);
						sampSpawn();

						iSpawned = 1;
						g_spawnTick = GetTickCount(); // harvest 60 sn korumasi
						iNotificationDisplayedBeforeSpawn = 1;
					}
				}
			}
			else
			{
				if(settings.runMode == RUNMODE_STILL)
				{
					// Nothing left to do. :-)
				}

				if(settings.runMode == RUNMODE_NORMAL)
				{
					if(!bIsSpectating)
					{
						if(settings.AutoGotoCP && settings.CurrentCheckpoint.bActive)
						{
							settings.fNormalModePos[0] = settings.CurrentCheckpoint.fPosition[0];
							settings.fNormalModePos[1] = settings.CurrentCheckpoint.fPosition[1];
							settings.fNormalModePos[2] = settings.CurrentCheckpoint.fPosition[2];
						}

						onFootUpdateAtNormalPos();
					}
					else
						spectatorUpdate();
				}

				// Run autorun commands
				if(settings.iAutorun)
				{
					if(dwAutoRunTick && dwAutoRunTick < (GetTickCount() - 2000))
					{
						static int autorun;
						if(!autorun)
						{
							Log("Loading autorun...");
							for(int i = 0; i < MAX_AUTORUN_CMDS; i++)
								if(settings.autoRunCMDs[i].iExists)
									RunCommand(settings.autoRunCMDs[i].szCMD, 1);

							autorun = 1;
						}
					}
				}

				// Following player mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYER)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						onFootUpdateFollow(copyingID);
				}

				// Following a player with a vehicle mode.
				if(settings.runMode == RUNMODE_FOLLOWPLAYERSVEHICLE)
				{
					PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);
					if(copyingID != (PLAYERID)-1)
						inCarUpdateFollow(copyingID, (VEHICLEID)settings.iFollowingWithVehicleID);
				}

			}
		}

bare:;
		Sleep(30);
	}

	if(flLog != NULL)
	{
		fclose(flLog);
		flLog = NULL;
	}

	if(flTextDrawsLog != NULL)
	{
		fclose(flTextDrawsLog);
		flTextDrawsLog = NULL;
	}

	return 0;
}

// Konsol cikis kod sayfasi UTF-8 (Turkce karakterler dogru gorunsun)
void SetConsoleUtf8() { SetConsoleOutputCP(65001); SetConsoleCP(65001); }

static FILE *flLogScan = NULL; // tarama/debug gurultusu ayri dosyaya

void Log(char *fmt, ...)
{
	if(flLog == NULL)
	{
		// Her acilista sifirdan: eski log silinir, yeniden olusur.
		flLog = fopen("RakSAMPClient.log", "wb");
		if(flLog == NULL)
			return;
		{
			unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
			fwrite(bom, 1, 3, flLog);
		}
	}

	SYSTEMTIME time;
	GetLocalTime(&time);

	

	if(settings.iPrintTimestamps && settings.iConsole)
		printf("[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	char buffer[2048];
	memset(buffer, 0, 2048);

	va_list args;
	va_start(args, fmt);
	vsprintf_s(buffer, 2048, fmt, args);
	va_end(args);

	char utf8buf[4096];
	SmartToUtf8(buffer, utf8buf, sizeof(utf8buf));
		// Gurultulu tarama satirlari ayri dosyaya (ana log okunakli kalsin).
	// Secim ham bufferdan (onekler ASCII, cevrimden etkilenmez).
	FILE *flOut = flLog;
	{
		static const char *pfx[] = {
			"[GANGZONE:FLASH]", "[GANGZONE:STOPFLASH]",
			"[ZF:SCAN] Ornek", "[ZF:SCAN] Serbest", "[ZF:SCAN] Dialog zone",
			"[AUTOCLOSE]", "[GZINFO-DATA]", "[GZINFO-BTNS]",
			"[GSTATS-DATA]", "[GSTATS-TAG]", "[GM-ASTATS-DATA]", "[GM-VIPS-DATA]",
			"[KILLLIST] TD #", "[PNETSTATS-DATA]", NULL };
		for(int pi = 0; pfx[pi]; pi++)
		{
			size_t npl = strlen(pfx[pi]);
			if(!strncmp(buffer, pfx[pi], npl)) { flOut = NULL; break; }
		}
		if(flOut == NULL)
		{
			if(flLogScan == NULL)
			{
				flLogScan = fopen("RakSAMPClient-scan.log", "wb");
				if(flLogScan)
				{
					unsigned char bom2[3] = {0xEF, 0xBB, 0xBF};
					fwrite(bom2, 1, 3, flLogScan);
				}
			}
			flOut = flLogScan;
		}
	}
	FILE *flDst = flOut ? flOut : flLog;
	if(flDst)
	{
		fprintf(flDst, "[%02d:%02d:%02d.%03d] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		fprintf(flDst, "%s", utf8buf);
	}

	if(settings.iConsole)
	{
		// Konsol da UTF-8 (Windows Terminal / chcp 65001 ile dogru gosterir)
		printf("%s", utf8buf);
	}
	else
	{
		// Self-heal: loghwnd lives in the window thread; if it was lost
		// (stale/zeroed), re-resolve it from the main window before each add.
		if(!IsWindow(loghwnd))
			loghwnd = GetDlgItem(hwnd, IDC_LSTCUSTOM);

		if(IsWindow(loghwnd))
		{
			// Log penceresi UTF-8 metin tutar (SmartToUtf8 zaten dogru donusturdu);
			// cizim window.cpp WM_DRAWITEM'da Unicode (ExtTextOutW) ile yapilir -
			// ACP'den bagimsiz dogru Turkce karakterler.
			TCHAR *tbuf = new TCHAR[1024];
			memset(tbuf, 0, 1024 * sizeof(TCHAR));
			lstrcpyn(tbuf, utf8buf, 1024);

			int lbCount = SendMessage(loghwnd, LB_GETCOUNT, 0, 0);
			WPARAM idx = SendMessage(loghwnd, LB_ADDSTRING, 0, (LPARAM)tbuf);
			SendMessage(loghwnd, LB_SETITEMDATA, idx, (LPARAM)tbuf);

			SendMessage(loghwnd, LB_SETCURSEL, lbCount - 1, 0);
			SendMessage(loghwnd, LB_SETTOPINDEX, idx, 0);
		}
	}

	if(flDst) fprintf(flDst, "\n");

	if(settings.iConsole)
		printf("\n");

	if(flDst) fflush(flDst);
}

void SaveTextDrawData ( WORD wTextID, TEXT_DRAW_TRANSMIT *pData, CHAR* cText )
{
	if ( flTextDrawsLog == NULL )
	{
		flTextDrawsLog = fopen( "TextDraws.log", "a" );

		if ( flTextDrawsLog == NULL )
			return;
	}

	fprintf( flTextDrawsLog, "TextDraw ID: %d, Text: %s\n", wTextID, cText );
	fprintf( flTextDrawsLog, "Flags: box(%i), left(%i), right(%i), center(%i), proportional(%i), padding(%i)\n", pData->byteBox, pData->byteLeft, pData->byteRight, pData->byteCenter, pData->byteProportional, pData->bytePadding );
	fprintf( flTextDrawsLog, "LetterWidth: %.3f, LetterHeight: %.3f, LetterColor: %X, LineWidth: %.3f, LineHeight: %.3f\n", pData->fLetterWidth, pData->fLetterHeight, pData->dwLetterColor, pData->fLineWidth, pData->fLineHeight );
	fprintf( flTextDrawsLog, "BoxColor: %X, Shadow: %i, Outline: %i, BackgroundColor: %X, Style: %i, Selectable: %i\n", pData->dwBoxColor, pData->byteShadow, pData->byteOutline, pData->dwBackgroundColor, pData->byteStyle, pData->byteSelectable );
	fprintf( flTextDrawsLog, "X: %.3f, Y: %.3f, ModelID: %d, RotX: %.3f, RotY: %.3f, RotZ: %.3f, Zoom: %.3f, Colors: %d, %d", pData->fX, pData->fY, pData->wModelID, pData->fRotX, pData->fRotY, pData->fRotZ, pData->fZoom, pData->wColor1, pData->wColor2 );

	fprintf( flTextDrawsLog, "\n\n" );

	fflush( flTextDrawsLog );
}

void gen_random(char *s, const int len)
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

	s[len] = 0;
}
