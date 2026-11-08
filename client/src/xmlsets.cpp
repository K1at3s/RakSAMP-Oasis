/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "SettingsArguments.h"
#include "RuntimeOptions.h"

struct stSettings settings;
TiXmlDocument xmlSettings;

int LoadSettings()
{
	// load xml
	if(!xmlSettings.LoadFile("RakSAMPClient.xml"))
	{
		MessageBox(NULL, "Failed to load the config file", "Error", MB_ICONERROR);
		ExitProcess(0);
	}

	TiXmlElement* rakSAMPElement = xmlSettings.FirstChildElement("RakSAMPClient");
	if(!rakSAMPElement)
	{
		Log("[CONFIG] Missing RakSAMPClient root element");
		xmlSettings.Clear();
		return 0;
	}
	TiXmlElement* configuredServer = rakSAMPElement->FirstChildElement("server");
	const char *version = rakSAMPElement->Attribute("clientversion");
	const char *followName = rakSAMPElement->Attribute("followplayer");
	if(!configuredServer || !configuredServer->GetText() ||
	   !configuredServer->Attribute("nickname") || !configuredServer->Attribute("password") ||
	   !version || !followName || strlen(version) >= sizeof(settings.szClientVersion) ||
	   strlen(followName) >= sizeof(settings.szFollowingPlayerName) ||
	   strlen(configuredServer->Attribute("nickname")) >= sizeof(settings.server.szNickname) ||
	   strlen(configuredServer->Attribute("password")) >= sizeof(settings.server.szPassword) ||
	   strlen(configuredServer->GetText()) >= sizeof(settings.server.szAddr))
	{
		Log("[CONFIG] Missing or oversized required server/client setting");
		xmlSettings.Clear();
		return 0;
	}
	if(rakSAMPElement)
	{
        if(TiXmlElement *automation = rakSAMPElement->FirstChildElement("automation")) {
            RuntimeOptions &o = ClientOptions();
            const char *base = automation->Attribute("backend_url");
            if(base && !ValidBackendUrl(base)) { Log("[CONFIG] Invalid backend_url"); return 0; }
            if(base) { o.backendUrl = base; while(o.backendUrl.size() > 8 && o.backendUrl.back() == '/') o.backendUrl.pop_back(); }
            struct Option { const char *name; unsigned int *value; unsigned int low, high; } values[] = {
                {"connect_timeout_ms", &o.connectTimeoutMs, 100, 30000}, {"read_timeout_ms", &o.readTimeoutMs, 100, 30000},
                {"write_timeout_ms", &o.writeTimeoutMs, 100, 30000}, {"total_timeout_ms", &o.totalTimeoutMs, 100, 60000},
                {"poll_interval_ms", &o.pollIntervalMs, 500, 60000}, {"db_sync_interval_ms", &o.dbSyncIntervalMs, 5000, 3600000},
                {"scan_period_ms", &o.scanPeriodMs, 30000, 86400000}, {"scan_sample_ms", &o.scanSampleMs, 1000, 60000},
                {"scan_teleport_ms", &o.scanTeleportMs, 1000, 60000}, {"scan_gap_ms", &o.scanGapMs, 500, 60000},
                {"reconnect_ms", &o.reconnectMs, 1000, 60000}, {"operation_timeout_ms", &o.operationTimeoutMs, 10000, 3600000},
                {"queue_timeout_ms", &o.queueTimeoutMs, 5000, 3600000}, {"result_retry_ms", &o.resultRetryMs, 1000, 300000},
                {"war_notice_1", &o.warNotice1, 1, 59}, {"war_notice_2", &o.warNotice2, 1, 59}, {"war_notice_3", &o.warNotice3, 1, 59}
            };
            for(const Option &value : values) if(const char *raw = automation->Attribute(value.name)) {
                if(!RuntimeInteger(raw, value.low, value.high, *value.value)) { Log("[CONFIG] Invalid %s", value.name); return 0; }
            }
            if(o.warNotice1 <= o.warNotice2 || o.warNotice2 <= o.warNotice3) { Log("[CONFIG] Countdown thresholds must decrease"); return 0; }
        }
		// get console
		rakSAMPElement->QueryIntAttribute("console", (int *)&settings.iConsole);

		// get runmode
		rakSAMPElement->QueryIntAttribute("runmode", (int *)&settings.runMode);

		// get autorun
		rakSAMPElement->QueryIntAttribute("autorun", (int *)&settings.iAutorun);

		// get find
		rakSAMPElement->QueryIntAttribute("find", (int *)&settings.iFind);

		// get selected class id
		rakSAMPElement->QueryIntAttribute("select_classid", (int *)&settings.iClassID);

		// get manual spawn
		rakSAMPElement->QueryIntAttribute("manual_spawn", (int *)&settings.iManualSpawn);

		// get print_timestamps
		rakSAMPElement->QueryIntAttribute("print_timestamps", (int *)&settings.iPrintTimestamps);

		// get fps simulation
		rakSAMPElement->QueryIntAttribute("updatestats", (int *)&settings.iUpdateStats);

		// get min simulated fps
		rakSAMPElement->QueryIntAttribute("minfps", (int *)&settings.iMinFPS);

		// get max simulated fps
		rakSAMPElement->QueryIntAttribute("maxfps", (int *)&settings.iMaxFPS);

		// get client version
		strcpy(settings.szClientVersion, (char *)rakSAMPElement->Attribute("clientversion"));

		// get chat color
		rakSAMPElement->QueryColorAttribute("chatcolor_rgb",
			(unsigned char *)&settings.bChatColorRed, (unsigned char *)&settings.bChatColorGreen, (unsigned char *)&settings.bChatColorBlue);

		// get client message color
		rakSAMPElement->QueryColorAttribute("clientmsg_rgb",
			(unsigned char *)&settings.bCMsgRed, (unsigned char *)&settings.bCMsgGreen, (unsigned char *)&settings.bCMsgBlue);
		
		// get checkpoint alert color
		rakSAMPElement->QueryColorAttribute("cpalert_rgb",
			(unsigned char *)&settings.bCPAlertRed, (unsigned char *)&settings.bCPAlertGreen, (unsigned char *)&settings.bCPAlertBlue);

		// get followplayer
		strcpy(settings.szFollowingPlayerName, (char *)rakSAMPElement->Attribute("followplayer"));
		rakSAMPElement->QueryIntAttribute("followplayerwithvehicleid", &settings.iFollowingWithVehicleID);
		rakSAMPElement->QueryFloatAttribute("followXOffset", &settings.fFollowXOffset);
		rakSAMPElement->QueryFloatAttribute("followYOffset", &settings.fFollowYOffset);
		rakSAMPElement->QueryFloatAttribute("followZOffset", &settings.fFollowZOffset);

		// get the first server
		TiXmlElement* serverElement = rakSAMPElement->FirstChildElement("server");
		if(serverElement)
		{
			char *pszAddr = (char *)serverElement->GetText();
			if(pszAddr)
			{
				// Parse a copy: TinyXML owns GetText(), and invalid ports must not connect.
                const std::string endpoint(pszAddr);
                const size_t colon = endpoint.rfind(':');
                int port = 0;
                if(colon == std::string::npos || colon == 0 ||
                   !ParseServerPort(endpoint.substr(colon + 1), port)) {
                    Log("Invalid server address/port in RakSAMPClient.xml (expected host:1-65535)");
                    return 0;
                }
                CopySettingsArgument(settings.server.szAddr, endpoint.substr(0, colon));
                settings.server.iPort = port;
				strcpy(settings.server.szNickname, (char *)serverElement->Attribute("nickname"));
				strcpy(settings.server.szPassword, (char *)serverElement->Attribute("password"));
			}
		}

		// get intervals
		TiXmlElement* intervalsElement = rakSAMPElement->FirstChildElement("intervals");
		if(intervalsElement)
		{
			intervalsElement->QueryIntAttribute("spam", (int *)&settings.uiSpamInterval);
			intervalsElement->QueryIntAttribute("fakekill", (int *)&settings.uiFakeKillInterval);
			intervalsElement->QueryIntAttribute("lag", (int *)&settings.uiLagInterval);
			intervalsElement->QueryIntAttribute("joinflood", (int *)&settings.uiJoinFloodInterval);
			intervalsElement->QueryIntAttribute("chatflood", (int *)&settings.uiChatFloodInterval);
			intervalsElement->QueryIntAttribute("classflood", (int *)&settings.uiClassFloodInterval);
			intervalsElement->QueryIntAttribute("bulletflood", (int *)&settings.uiBulletFloodInterval);
		}

		// get logging settings
		TiXmlElement* logElement = rakSAMPElement->FirstChildElement("log");
		if(logElement)
		{
			logElement->QueryIntAttribute("objects", (int *)&settings.uiObjectsLogging);
			logElement->QueryIntAttribute("pickups", (int *)&settings.uiPickupsLogging);
			logElement->QueryIntAttribute("textlabels", (int *)&settings.uiTextLabelsLogging);
			logElement->QueryIntAttribute("textdraws", (int *)&settings.uiTextDrawsLogging);
		}

		// get sendrates settings
		TiXmlElement* sendratesElement = rakSAMPElement->FirstChildElement("sendrates");
		if(sendratesElement)
		{
			sendratesElement->QueryIntAttribute("force", (int *)&settings.uiForceCustomSendRates);
			sendratesElement->QueryIntAttribute("onfoot", (int *)&iNetModeNormalOnfootSendRate);
			sendratesElement->QueryIntAttribute("incar", (int *)&iNetModeNormalIncarSendRate);
			sendratesElement->QueryIntAttribute("firing", (int *)&iNetModeFiringSendRate);
			sendratesElement->QueryIntAttribute("multiplier", (int *)&iNetModeSendMultiplier);
		}

		// get normal mode pos
		TiXmlElement* normalPosElement = rakSAMPElement->FirstChildElement("normal_pos");
		if(normalPosElement)
		{
			normalPosElement->QueryVectorAttribute("position", (float *)&settings.fNormalModePos);
			normalPosElement->QueryFloatAttribute("rotation", &settings.fNormalModeRot);
			normalPosElement->QueryIntAttribute("force", (int *)&settings.iNormalModePosForce);
		}

		// get auto run commands
		TiXmlElement* autorunElement = rakSAMPElement->FirstChildElement("autorun");
		if(autorunElement)
		{
			for(int i = 0; i < MAX_AUTORUN_CMDS; i++)
			{
				if(autorunElement)
				{
					settings.autoRunCMDs[i].iExists = 1;
					strcpy(settings.autoRunCMDs[i].szCMD, autorunElement->GetText());
					autorunElement = autorunElement->NextSiblingElement("autorun");
				}
				else
					break;
			}
		}

		TiXmlElement* findElement = rakSAMPElement->FirstChildElement("find");
		if(findElement)
		{
			for(int i = 0; i < MAX_FIND_ITEMS; i++)
			{
				if(findElement)
				{
					settings.findItems[i].iExists = 1;
					strcpy(settings.findItems[i].szFind, findElement->Attribute("text"));
					strcpy(settings.findItems[i].szSay, findElement->Attribute("say"));
					findElement->QueryColorAttribute("bk_color",
						(unsigned char *)&settings.findItems[i].bBkRed,
						(unsigned char *)&settings.findItems[i].bBkGreen,
						(unsigned char *)&settings.findItems[i].bBkBlue);
					findElement->QueryColorAttribute("text_color",
						(unsigned char *)&settings.findItems[i].bTextRed,
						(unsigned char *)&settings.findItems[i].bTextGreen,
						(unsigned char *)&settings.findItems[i].bTextBlue);

					findElement = findElement->NextSiblingElement("find");
				}
				else
					break;
			}
		}

		// get teleport locations
		TiXmlElement* teleportElement = rakSAMPElement->FirstChildElement("teleport");
		if(teleportElement)
		{
			for(int i = 0; i < MAX_TELEPORT_ITEMS; i++)
			{
				if(teleportElement)
				{
					settings.TeleportLocations[i].bCreated = 1;

					strcpy(settings.TeleportLocations[i].szName, (char *)teleportElement->Attribute("name"));
					teleportElement->QueryVectorAttribute("position", (float *)&settings.TeleportLocations[i].fPosition);

					teleportElement = teleportElement->NextSiblingElement("teleport");
				}
				else
					break;
			}
		}
	}

	xmlSettings.Clear();

	ApplyServerArguments(GetCommandLineA(), settings.server);
	if(!settings.server.szAddr[0] || settings.server.iPort < 1 || settings.server.iPort > 65535 ||
	   !settings.server.szNickname[0])
	{
		Log("[CONFIG] Server address, port (1-65535), and nickname are required");
		return 0;
	}

	return 1;
}

int UnLoadSettings()
{
	memset(&settings, 0, sizeof(settings));

	return 1;
}

int ReloadSettings()
{
	if(UnLoadSettings() && LoadSettings())
	{
		Log("Settings reloaded");
		return 1;
	}

	Log("Failed to reload settings");

	return 0;
}
