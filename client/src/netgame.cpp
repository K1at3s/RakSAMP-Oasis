/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "ReplayHooks.h"
#include "NetworkPayload.h"
#include "Security.h"

extern int iFollowingPassenger, iFollowingDriver;
extern int iDrunkLevel, iMoney, iLocalPlayerSkin;
extern BYTE m_bLagCompensation;

DWORD dwTimeReconnect = 10000;

int iPassengerNotificationSent = 0, iDriverNotificationSent = 0;

void Packet_AUTH_KEY(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	if(p->length < 2 || p->data[1] > p->length - 2) return;
	std::string incomingKey((char*)p->data + 2, p->data[1]);
	if(!incomingKey.empty() && incomingKey.back() == 0) incomingKey.pop_back();
	char* auth_key;
	bool found_key = false;

	for(int x = 0; x < 512; x++)
	{
		if(incomingKey == AuthKeyTable[x][0])
		{
			auth_key = AuthKeyTable[x][1];
			found_key = true;
		}
	}

	if(found_key)
	{
		RakNet::BitStream bsKey;
		BYTE byteAuthKeyLen;

		byteAuthKeyLen = (BYTE)strlen(auth_key);

		bsKey.Write((BYTE)ID_AUTH_KEY);
		bsKey.Write((BYTE)byteAuthKeyLen);
		bsKey.Write(auth_key, byteAuthKeyLen);

		pRakClient->Send(&bsKey, SYSTEM_PRIORITY, RELIABLE, NULL);

		//Log("[AUTH] %s -> %s", ((char*)p->data + 2), auth_key);
	}
	else
	{
		Log("Unknown AUTH_IN! (%s)", incomingKey.c_str());
	}
}

void Packet_ConnectionSucceeded(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsSuccAuth((unsigned char *)p->data, p->length, false);
	PLAYERID myPlayerID;
	unsigned int uiChallenge;

	if(!ReadAcceptedConnection(bsSuccAuth, myPlayerID, uiChallenge, MAX_PLAYERS)) return;

	g_myPlayerID = myPlayerID;
	playerInfo[myPlayerID].iIsConnected = 1;
	strcpy(playerInfo[myPlayerID].szPlayerName, g_szNickName);


	settings.uiChallange = uiChallenge;

	Log("Connected. Joining the game...");

	int iVersion = NETGAME_VERSION;
	unsigned int uiClientChallengeResponse = uiChallenge ^ iVersion;
	BYTE byteMod = 1;

	char auth_bs[4*16] = {0};
	gen_gpci(auth_bs, 0x3e9);

	BYTE byteAuthBSLen;
	byteAuthBSLen = (BYTE)strlen(auth_bs);
	BYTE byteNameLen = (BYTE)strlen(g_szNickName);
	BYTE iClientVerLen = (BYTE)strlen(settings.szClientVersion);

	RakNet::BitStream bsSend;

	bsSend.Write(iVersion);
	bsSend.Write(byteMod);
	bsSend.Write(byteNameLen);
	bsSend.Write(g_szNickName, byteNameLen);
	bsSend.Write(uiClientChallengeResponse);
	bsSend.Write(byteAuthBSLen);
	bsSend.Write(auth_bs, byteAuthBSLen);
	bsSend.Write(iClientVerLen);
	bsSend.Write(settings.szClientVersion, iClientVerLen);

	pRakClient->RPC(&RPC_ClientJoin, &bsSend, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

	iAreWeConnected = 1;
}

void Packet_PlayerSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsPlayerSync((unsigned char *)p->data, p->length, false);
	PLAYERID playerId;

	//Log("Packet_PlayerSync: %d \n%s\n", p->length, DumpMem((unsigned char *)p->data, p->length));

	bool bHasLR, bHasUD;
	bool bHasSurfInfo, bAnimation;

	bsPlayerSync.IgnoreBits(8);
	if(!bsPlayerSync.Read(playerId)) return;

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	// clear last data
	ONFOOT_SYNC_DATA sync = {};

	// LEFT/RIGHT KEYS
	if(!bsPlayerSync.Read(bHasLR)) return;
	if(bHasLR && !bsPlayerSync.Read(sync.lrAnalog)) return;

	// UP/DOWN KEYS
	if(!bsPlayerSync.Read(bHasUD)) return;
	if(bHasUD && !bsPlayerSync.Read(sync.udAnalog)) return;

	// GENERAL KEYS
	if(!bsPlayerSync.Read(sync.wKeys)) return;

	// VECTOR POS
	if(!bsPlayerSync.Read(sync.vecPos[0])) return;
	if(!bsPlayerSync.Read(sync.vecPos[1])) return;
	if(!bsPlayerSync.Read(sync.vecPos[2])) return;

	// ROTATION
	if(!bsPlayerSync.ReadNormQuat(
		sync.fQuaternion[0],
		sync.fQuaternion[1],
		sync.fQuaternion[2],
		sync.fQuaternion[3])) return;
	

	// HEALTH/ARMOUR (COMPRESSED INTO 1 BYTE)
	BYTE byteHealthArmour;
	BYTE byteHealth, byteArmour;
	BYTE byteArmTemp=0,byteHlTemp=0;

	if(!bsPlayerSync.Read(byteHealthArmour)) return;
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if(byteArmTemp == 0xF) byteArmour = 100;
	else if(byteArmTemp == 0) byteArmour = 0;
	else byteArmour = byteArmTemp * 7;

	if(byteHlTemp == 0xF) byteHealth = 100;
	else if(byteHlTemp == 0) byteHealth = 0;
	else byteHealth = byteHlTemp * 7;

	sync.byteHealth = byteHealth;
	sync.byteArmour = byteArmour;

	// CURRENT WEAPON
	if(!bsPlayerSync.Read(sync.byteCurrentWeapon)) return;

	// Special Action
	if(!bsPlayerSync.Read(sync.byteSpecialAction)) return;

	// READ MOVESPEED VECTORS
	if(!bsPlayerSync.ReadVector(
		sync.vecMoveSpeed[0],
		sync.vecMoveSpeed[1],
		sync.vecMoveSpeed[2])) return;

	if(!bsPlayerSync.Read(bHasSurfInfo)) return;
	if(bHasSurfInfo)
	{
		if(!bsPlayerSync.Read(sync.wSurfInfo)) return;
		if(!bsPlayerSync.Read(sync.vecSurfOffsets[0])) return;
		if(!bsPlayerSync.Read(sync.vecSurfOffsets[1])) return;
		if(!bsPlayerSync.Read(sync.vecSurfOffsets[2])) return;
	}
	else
		sync.wSurfInfo = -1;

	if(!bsPlayerSync.Read(bAnimation)) return;
	if(bAnimation)
		if(!bsPlayerSync.Read(sync.iCurrentAnimationID)) return;
	playerInfo[playerId].onfootData = sync;
	// Followed passenger exit
	if(settings.runMode == RUNMODE_FOLLOWPLAYER && playerId == getPlayerIDFromPlayerName(settings.szFollowingPlayerName))
	{
		if(iPassengerNotificationSent)
		{
			SendExitVehicleNotification(playerInfo[playerId].incarData.VehicleID);
			iPassengerNotificationSent = 0;
		}

		iFollowingPassenger = 0;

		if(iDriverNotificationSent)
		{
			SendExitVehicleNotification(playerInfo[playerId].incarData.VehicleID);
			iDriverNotificationSent = 0;
		}

		iFollowingDriver = 0;
	}
	playerInfo[playerId].incarData.VehicleID = -1;



}

//----------------------------------------------------

void Packet_UnoccupiedSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsUnocSync((unsigned char *)p->data, p->length, false);
	PLAYERID playerId;

	//Log("\n%s\n", DumpMem((unsigned char *)p->data + bsUnocSync.GetReadOffset() / 8, p->length));

	bsUnocSync.IgnoreBits(8);
	if(!bsUnocSync.Read(playerId)) return;

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	UNOCCUPIED_SYNC_DATA sync = {};
	if(!bsUnocSync.Read((char*)&sync, sizeof(sync))) return;
	playerInfo[playerId].unocData = sync;
}

void Packet_AimSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsAimSync((unsigned char *)p->data, p->length, false);
	PLAYERID playerId;

	//Log("Packet_AimSync:\n%s\n", DumpMem((unsigned char *)p->data, p->length));

	bsAimSync.IgnoreBits(8);
	if(!bsAimSync.Read(playerId)) return;

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	AIM_SYNC_DATA sync = {};
	if(!bsAimSync.Read((char*)&sync, sizeof(sync))) return;
	playerInfo[playerId].aimData = sync;
}

void Packet_VehicleSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsSync((unsigned char *)p->data, p->length, false);
	PLAYERID playerId;

	VEHICLEID VehicleID;
	bool bLandingGear;
	bool bHydra,bTrain,bTrailer;
	bool bSiren;

	//Log("Packet_VehicleSync: %d \n%s\n", p->length, DumpMem((unsigned char *)p->data, p->length));

	bsSync.IgnoreBits(8);
	if(!bsSync.Read(playerId)) return;
	if(!bsSync.Read(VehicleID)) return;

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;
	if(VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

	// clear last data
	INCAR_SYNC_DATA sync = {};
	sync.VehicleID = VehicleID;

	// LEFT/RIGHT KEYS
	if(!bsSync.Read(sync.lrAnalog)) return;

	// UP/DOWN KEYS
	if(!bsSync.Read(sync.udAnalog)) return;

	// GENERAL KEYS
	if(!bsSync.Read(sync.wKeys)) return;

	// ROLL / DIRECTION
	// ROTATION
	if(!bsSync.ReadNormQuat(
		sync.fQuaternion[0],
		sync.fQuaternion[1],
		sync.fQuaternion[2],
		sync.fQuaternion[3])) return;

	// POSITION
	if(!bsSync.Read(sync.vecPos[0])) return;
	if(!bsSync.Read(sync.vecPos[1])) return;
	if(!bsSync.Read(sync.vecPos[2])) return;

	// SPEED
	if(!bsSync.ReadVector(
		sync.vecMoveSpeed[0],
		sync.vecMoveSpeed[1],
		sync.vecMoveSpeed[2])) return;

	// VEHICLE HEALTH
	WORD wTempVehicleHealth;
	if(!bsSync.Read(wTempVehicleHealth)) return;
	sync.fCarHealth = (float)wTempVehicleHealth;

	// HEALTH/ARMOUR (COMPRESSED INTO 1 BYTE)
	BYTE byteHealthArmour;
	BYTE bytePlayerHealth, bytePlayerArmour;
	BYTE byteArmTemp=0,byteHlTemp=0;

	if(!bsSync.Read(byteHealthArmour)) return;
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if(byteArmTemp == 0xF) bytePlayerArmour = 100;
	else if(byteArmTemp == 0) bytePlayerArmour = 0;
	else bytePlayerArmour = byteArmTemp * 7;

	if(byteHlTemp == 0xF) bytePlayerHealth = 100;
	else if(byteHlTemp == 0) bytePlayerHealth = 0;
	else bytePlayerHealth = byteHlTemp * 7;

	sync.bytePlayerHealth = bytePlayerHealth;
	sync.bytePlayerArmour = bytePlayerArmour;

	// CURRENT WEAPON
	if(!bsSync.Read(sync.byteCurrentWeapon)) return;

	// SIREN
	if(!bsSync.ReadCompressed(bSiren)) return;
	if(bSiren)
		sync.byteSirenOn = 1;

	// LANDING GEAR
	if(!bsSync.ReadCompressed(bLandingGear)) return;
	if(bLandingGear)
		sync.byteLandingGearState = 1;

	// HYDRA THRUST ANGLE AND TRAILER ID
	if(!bsSync.ReadCompressed(bHydra)) return;
	if(!bsSync.ReadCompressed(bTrailer)) return;

	DWORD dwTrailerID_or_ThrustAngle;
	if(!bsSync.Read(dwTrailerID_or_ThrustAngle)) return;
	sync.TrailerID_or_ThrustAngle = (WORD)dwTrailerID_or_ThrustAngle;

	// TRAIN SPECIAL
	WORD wSpeed;
	if(!bsSync.ReadCompressed(bTrain)) return;
	if(bTrain)
	{
		if(!bsSync.Read(wSpeed)) return;
		sync.fTrainSpeed = (float)wSpeed;
	}
	playerInfo[playerId].incarData = sync;
	// Follower passenger enter
	playerInfo[playerId].incarData.VehicleID = VehicleID;
	if(settings.runMode == RUNMODE_FOLLOWPLAYER && playerId == getPlayerIDFromPlayerName(settings.szFollowingPlayerName))
	{
		if(!iPassengerNotificationSent)
		{
			SendEnterVehicleNotification(VehicleID, 1);
			iPassengerNotificationSent = 1;
		}

		SendPassengerFullSyncData(VehicleID);
		iFollowingPassenger = 1;
		return;
	}



}

void Packet_PassengerSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsPassengerSync((unsigned char *)p->data, p->length, false);
	PLAYERID	playerId;
	PASSENGER_SYNC_DATA psSync;

	bsPassengerSync.IgnoreBits(8);
	if(!bsPassengerSync.Read(playerId)) return;

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	if(!bsPassengerSync.Read((PCHAR)&psSync,sizeof(PASSENGER_SYNC_DATA)) || psSync.VehicleID >= MAX_VEHICLES) return;

	// Followed wants to drive the vehicle
	playerInfo[playerId].passengerData.VehicleID = psSync.VehicleID;
	if(settings.runMode == RUNMODE_FOLLOWPLAYER && playerId == getPlayerIDFromPlayerName(settings.szFollowingPlayerName))
	{
		if(!iDriverNotificationSent)
		{
			SendEnterVehicleNotification(psSync.VehicleID, 0);
			iDriverNotificationSent = 1;
		}

		INCAR_SYNC_DATA icSync;
		memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));
		icSync.VehicleID = psSync.VehicleID;
		icSync.fCarHealth = 1000.00f;
		icSync.bytePlayerHealth = (BYTE)settings.fPlayerHealth;
		icSync.bytePlayerArmour = (BYTE)settings.fPlayerArmour;
		SendInCarFullSyncData(&icSync, 1, -1);

		iFollowingDriver = 1;
		return;
	}
}

void Packet_TrailerSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsSpectatorSync((unsigned char *)p->data, p->length, false);

	PLAYERID playerId;
	//TRAILER_SYNC_DATA trSync;

	bsSpectatorSync.IgnoreBits(8);
	if(!bsSpectatorSync.Read(playerId)) return;
	//bsSpectatorSync.Read((PCHAR)&trSync, sizeof(TRAILER_SYNC_DATA));
}

void Packet_MarkersSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsMarkersSync((unsigned char *)p->data, p->length, false);

	int i, iNumberOfPlayers;
	PLAYERID playerID;
	short sPosX, sPosY, sPosZ;
	bool bIsPlayerActive;

	bsMarkersSync.IgnoreBits(8);
	if(!bsMarkersSync.Read(iNumberOfPlayers)) return;

	if(iNumberOfPlayers < 0 || iNumberOfPlayers > MAX_PLAYERS) return;

	for(i = 0; i < iNumberOfPlayers; i++)
	{
		if(!bsMarkersSync.Read(playerID)) return;

		if(playerID < 0 || playerID >= MAX_PLAYERS) return;

		if(!bsMarkersSync.ReadCompressed(bIsPlayerActive)) return;
		if(bIsPlayerActive == 0)
		{
			playerInfo[playerID].iGotMarkersPos = 0;
			continue;
		}

		if(!bsMarkersSync.Read(sPosX)) return;
		if(!bsMarkersSync.Read(sPosY)) return;
		if(!bsMarkersSync.Read(sPosZ)) return;

		playerInfo[playerID].iGotMarkersPos = 1;
		playerInfo[playerID].onfootData.vecPos[0] = (float)sPosX;
		playerInfo[playerID].onfootData.vecPos[1] = (float)sPosY;
		playerInfo[playerID].onfootData.vecPos[2] = (float)sPosZ;

		//Log("Packet_MarkersSync: %d %d %0.2f, %0.2f, %0.2f", playerID, bIsPlayerActive, (float)sPosX, (float)sPosY, (float)sPosZ);
	}
}

void Packet_BulletSync(Packet *p, RakClientInterface *pRakClient)
{
	if(!p || !p->data || p->length == 0) return;
	RakNet::BitStream bsBulletSync((unsigned char *)p->data, p->length, false);

	if(m_bLagCompensation)
	{
		PLAYERID PlayerID;

		bsBulletSync.IgnoreBits(8);
		if(!bsBulletSync.Read(PlayerID)) return;

		if(PlayerID < 0 || PlayerID >= MAX_PLAYERS) return;

		BULLET_SYNC_DATA sync = {};
		if(!bsBulletSync.Read((char*)&sync, sizeof(sync))) return;
		playerInfo[PlayerID].bulletData = sync;

		PLAYERID copyingID = getPlayerIDFromPlayerName(settings.szFollowingPlayerName);

		if(copyingID != (PLAYERID)-1 && (settings.runMode == RUNMODE_FOLLOWPLAYER || settings.runMode == RUNMODE_FOLLOWPLAYERSVEHICLE))
		{
			if(copyingID == PlayerID)
				SendBulletData(&playerInfo[PlayerID].bulletData);
		}
	}
}

void resetPools(int iRestart, DWORD dwTimeReconnect)
{
	InvalidateBackendResponses();
	ResetAutomationSession();
	ResetChatOutQueue();
	iGameInited = 0;
	memset(playerInfo, 0, sizeof(stPlayerInfo));
	memset(vehiclePool, 0, sizeof(stVehiclePool));

	if(iRestart)
	{
		iAreWeConnected = 0;
		iPassengerNotificationSent = iDriverNotificationSent = 0;
		iFollowingPassenger = iFollowingDriver = 0;
		iConnectionRequested = 0;
		iSpawned = 0;
		iMoney = 0;
		iDrunkLevel = 0;
		iLocalPlayerSkin = 0;

		settings.bPulsator = false;

		settings.fPlayerHealth = 100.0f;
		settings.fPlayerArmour = 0.0f;

		// Reconnect delay is serviced by the main loop; keep pumping network/backend.
	}
}

void UpdatePlayerScoresAndPings(int iWait, int iMS, RakClientInterface *pRakClient)
{
	static DWORD dwLastUpdateTick = 0;

	if(iWait)
	{
		if ((ClientTick() - dwLastUpdateTick) > (DWORD)iMS)
		{
			dwLastUpdateTick = ClientTick();
			RakNet::BitStream bsParams;
			pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
		}
	}
	else
	{
		RakNet::BitStream bsParams;
		pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
	}
}

void UpdateNetwork(RakClientInterface *pRakClient)
{
	unsigned char packetIdentifier;
	Packet *pkt;

	while(pkt = pRakClient->Receive())
	{
		if(!pkt->data || pkt->length == 0) { pRakClient->DeallocatePacket(pkt); continue; }
		if ( ( unsigned char ) pkt->data[ 0 ] == ID_TIMESTAMP )
		{
			if ( pkt->length > sizeof( unsigned char ) + sizeof( unsigned int ) )
				packetIdentifier = ( unsigned char ) pkt->data[ sizeof( unsigned char ) + sizeof( unsigned int ) ];
			else
				return;
		}
		else
			packetIdentifier = ( unsigned char ) pkt->data[ 0 ];

		//Log("[RAKSAMP] Packet received. PacketID: %d.", pkt->data[0]);

		switch(packetIdentifier)
		{
			case ID_DISCONNECTION_NOTIFICATION:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] Connection was closed by the server. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_CONNECTION_BANNED:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] You are banned. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_CONNECTION_ATTEMPT_FAILED:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] Connection attempt failed. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_NO_FREE_INCOMING_CONNECTIONS:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] The server is full. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_INVALID_PASSWORD:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] Invalid password. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_CONNECTION_LOST:
				if (pRakClient == ::pRakClient)
				{
					Log("[RAKSAMP] The connection was lost. Reconnecting in %d seconds.", iReconnectTime / 1000);
					resetPools(1, iReconnectTime);
				}
				break;
			case ID_CONNECTION_REQUEST_ACCEPTED:
				Packet_ConnectionSucceeded(pkt, pRakClient);
				break;
			case ID_AUTH_KEY:
				Packet_AUTH_KEY(pkt, pRakClient);
				break;
			case ID_PLAYER_SYNC:
				Packet_PlayerSync(pkt, pRakClient);
				break;
			case ID_VEHICLE_SYNC:
				Packet_VehicleSync(pkt, pRakClient);
				break;
			case ID_PASSENGER_SYNC:
				Packet_PassengerSync(pkt, pRakClient);
				break;
			case ID_AIM_SYNC:
				Packet_AimSync(pkt, pRakClient);
				break;
			case ID_TRAILER_SYNC:
				Packet_TrailerSync(pkt, pRakClient);
				break;
			case ID_UNOCCUPIED_SYNC:
				Packet_UnoccupiedSync(pkt, pRakClient);
				break;
			case ID_MARKERS_SYNC:
				Packet_MarkersSync(pkt, pRakClient);
				break;
			case ID_BULLET_SYNC:
				Packet_BulletSync(pkt, pRakClient);
				break;
		}

		pRakClient->DeallocatePacket(pkt);
	}

	UpdatePlayerScoresAndPings(1, 3000, pRakClient);
}
