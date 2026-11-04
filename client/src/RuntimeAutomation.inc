// Included by netrpc.cpp so transient state has one owner and one reset path.
static OperationQueue s_operations;
static unsigned int s_automationGeneration = 0;
static bool s_dispatchingOperation = false, s_scanSuspended = false;
static DWORD s_scanWakeAt = 0, s_reconnectAt = 0, s_operationAt = 0;
static DWORD s_freeroamAt = 0;
static std::string s_operationActor;
#include "RuntimeBackendReads.inc"

static std::string RuntimeBusyName(bool includeScan = true) {
    if(g_loginPending || !iAreWeConnected || !iGameInited) return "login/connection";
    if(s_backendOperationPending) return "backend read";
    if(g_groleActive || g_grankActive || g_gkickActive) return "group action";
    if(g_groleListActive || g_grankListActive || g_gkickListActive) return "member selection";
    if(g_gmPhase != 0) return "member roster";
    if(g_snipPermPending || g_waitingForGmembers) return "member permissions";
    if(g_ghpActive || g_waitingForGroups) return "tag harvest";
    if(g_waitingForGroupName || g_waitingForGStats || g_waitingForAdmins || g_waitingForPNetStats || g_waitingForGst) return "game dialog";
    if(g_ztkActive || g_ztkAttackPending || g_ztkAskNext || g_snipInterrupt) return "zone sniper";
    if(g_zgoActive) return "zone teleport";
    if(g_zwReturnPending || g_zwKillListStay) return "war capture";
    if(includeScan && g_followActive) return "follow";
    if(includeScan && g_zoneScanIdx >= 0 && !s_scanSuspended) return "zone scan";
    return "";
}
static bool RuntimeWarMayMove() {
    const std::string name = RuntimeBusyName(false);
    return name.empty() || name == "zone sniper" || name == "war capture";
}
static void CancelManualAutomation() {
    CancelBackendOperation();
    g_groleActive = g_grankActive = g_gkickActive = false;
    g_groleListActive = g_grankListActive = g_gkickListActive = false;
    g_grolePhase = g_grankPhase = g_gkickPhase = 0;
    g_groleDirectPending = g_grRoleDblPending = false; g_grClickAttempt = -1;
    g_grTargetItem = g_grLastSentItem = -1; g_grDlgId = 0; g_grDblPhase = 0;
    g_followActive = false; g_followSlot = -1; g_followMiss = 0; g_followLastTick = 0;
    g_watchDlgId = -1; g_watchDlgAt = 0; g_watchDlgKeep = false; g_watchCloseMode = 0;
    g_firstRejectTick = 0; g_zwFrRetry = 0; g_zwFrRetryAt = 0; g_harvestPausedForDlg = false;
    g_gmPhase = 0; g_gmAstatsWaiting = g_gmGvipsWaiting = false;
    g_ghpActive = g_ghpWaitingGstats = false; g_ghpNextStepAt = 0;
    g_ztkActive = g_ztkVerifyPending = g_ztkAttackPending = false;
    g_ztkAskNext = g_ztkPreAsked = g_ztkYesPending = false;
    g_snipPermPending = g_snipInterrupt = false; g_snipZoneCount = g_snipZoneIdx = 0;
    g_zgoActive = g_zaPermActive = false;
    g_waitingForGStats = g_waitingForGst = g_waitingForPNetStats = false;
    g_waitingForGroupName = g_waitingForGroups = g_waitingForGmembers = g_waitingForAdmins = false;
    g_zscanPausedForSnip = g_zsnipeHoldScan = false;
    GmembersReset();
}
void ResetAutomationSession() {
    ++s_automationGeneration; s_freeroamAt = 0;
    CancelManualAutomation(); s_operations.Clear();
    s_dispatchingOperation = s_scanSuspended = false; s_scanWakeAt = 0;
    s_operationActor.clear(); s_operationAt = 0;
    g_zoneScanIdx = -1; g_zoneScanQ = 0; g_zoneScanDeadline = 0;
    g_zoneScanDone = false; g_zoneScanWaitStart = g_zoneScanLastRun = 0;
    g_zoneCount = 0; memset(g_zoneFlash, 0, sizeof(g_zoneFlash));
    memset(g_zoneOwnerName, 0, sizeof(g_zoneOwnerName));
    g_zscanDbLoaded = false; s_bGroupDbRestored = false; s_dwRestoreLastTry = 0;
    g_inZoneWar = false; s_warResultContext = WarResultContext(); s_contextWarTick = 0;
    g_zwCapturingKillList = g_zwKillListSent = g_zwReturnPending = g_zwKillListStay = false;
    g_zwAutoJoinSent = g_zwCheckJoinSent = false; g_zwKillTdCount = 0;
    g_zwKillListText.clear(); g_zwWarTeamLeft.clear(); g_zwWarTeamRight.clear();
    g_zwWarZone[0] = g_zwFirstBlood[0] = 0; g_zwWarKillCount = 0;
    g_zwFinalizeAt = g_zwEmptyFinalizeSince = g_zoneWarStartTick = g_warTimerTick = 0;
    g_warRemainingSec = -1; g_warTimerViaTdw = false;
    g_gztCountdownAnnounced = 999; g_gztLastRem = -1;
    memset(g_knownGroupMembers, 0, sizeof(g_knownGroupMembers));
    memset(g_zwWelcomed, 0, sizeof(g_zwWelcomed)); ResetPlayerCache();
    g_myGroupName.clear(); g_myGroupId.clear(); g_enemyGroupName.clear();
    g_gstatsInput.clear(); g_gstatsRequesterId.clear(); g_adminsRequesterId.clear();
    g_loginPending = false; g_loginPendingUntil = 0;
    g_frSentOnce = false; g_dwFrFallbackStart = g_dwSpawnMenuHandled = 0;
    g_pushPending = g_pmPending = false; g_alogAstatsActive = false;
    g_secCmdPending = false; g_secCmdAt = 0; g_secCmdCount = 0;
    SecureZeroMemory(g_secCmdValue, sizeof(g_secCmdValue));
    SecureZeroMemory(g_secCmdValue2, sizeof(g_secCmdValue2));
    g_anyWarActive = false; g_anyWarStartTick = 0; g_anyWarDesc[0] = 0; g_zwNoGzEntry = false;
    sampDialog.iIsActive = 0; g_spawnTick = 0;
    s_reconnectAt = ClientTick() + ClientOptions().reconnectMs;
    Log("[RUNTIME] Connection state cleared; reconnect scheduled.");
}
bool RuntimeConnectionReady() {
    return !s_reconnectAt || static_cast<LONG>(ClientTick() - s_reconnectAt) >= 0;
}
bool ProcessLoginGate(unsigned long now) {
    if(!g_loginPending) return true;
    if(g_loginPendingUntil && static_cast<LONG>(now - g_loginPendingUntil) >= 0) {
        g_loginPending = false;
        Log("[RUNTIME] Login wait completed; automation ready.");
        return true;
    }
    return false;
}
static void ScheduleRuntimeFreeroam() { s_freeroamAt = ClientTick() + ClientOptions().scanGapMs; }
static void ScheduleRuntimeScanSample() { s_scanWakeAt = ClientTick() + ClientOptions().scanGapMs; }
static bool RuntimeScanMayAdvance() {
    if(g_followActive) return false;
    if(!s_scanWakeAt) return !s_scanSuspended && RuntimeBusyName(false).empty();
    if(s_operations.Size() && RuntimeBusyName(false).empty()) s_scanSuspended = true;
    if(s_scanSuspended || !RuntimeBusyName(false).empty()) return false;
    if(static_cast<LONG>(ClientTick() - s_scanWakeAt) < 0) return false;
    s_scanWakeAt = 0;
    if(g_zoneScanIdx >= 0 && g_zoneScanIdx < g_zoneScanQ) GangZoneScanGo(g_zoneScanQueue[g_zoneScanIdx]);
    return false;
}
std::string DescribeAutomation() {
    std::string current = RuntimeBusyName();
    std::string waiting = "none";
    if(g_loginPending) waiting = "login";
    else if(g_waitingForGmembers) waiting = "gmembers";
    else if(g_waitingForGStats) waiting = "gstats/astats";
    else if(g_waitingForGroups) waiting = "groups";
    else if(g_waitingForGroupName) waiting = "group-name";
    else if(g_waitingForAdmins) waiting = "admins";
    else if(g_gmPhase) waiting = "roster-dialog";
    else if(g_ztkVerifyPending || (g_zoneScanIdx >= 0 && !g_zoneScanWaitingTeleport)) waiting = "gzinfo";
    return "operation=" + (current.empty() ? "idle" : current) + " queued=" + std::to_string(s_operations.Size()) +
        " connected=" + std::to_string(iAreWeConnected != 0) + " login=" + std::to_string(g_loginPending) + " waiting=" + waiting;
}
bool RuntimeGateGroupCommand(const char *raw, int sender) {
    if(s_dispatchingOperation || !raw || raw[0] != '?') return false;
    const std::string command(raw), token = command.substr(0, command.find_first_of(" #"));
    if(sender < 0) sender = g_myPlayerID;
    const bool self = sender == g_myPlayerID;
    const std::string actor = self ? std::string(g_szNickName) :
        (sender >= 0 && sender < MAX_PLAYERS ? playerInfo[sender].szPlayerName : "");
    if(token == "?durum" || token == "?status") {
        const std::string message = "!" + DescribeAutomation() + " | " + DescribeBackend();
        g_bridgeSuppressEcho++; sendChat((char*)message.c_str()); return true;
    }
    if(token == "?iptal" || token == "?cancel") {
        if(actor.empty()) return true;
        if(self) s_operations.Clear(); else s_operations.CancelActor(actor);
        if(self || actor == s_operationActor) {
            CloseOpenListDlg(); s_freeroamAt = 0; CancelManualAutomation(); s_operationActor.clear(); s_operationAt = 0;
        }
        g_bridgeSuppressEcho++; sendChat((char*)LM("!Islem iptal edildi.", "!Operation cancelled.")); return true;
    }
    if(command == "?zsnip off" || command == "?zscan off") return false;
    const char *serialized[] = { "?zscan", "?zgo", "?za", "?zattack", "?zsnip", "?zf", "?zlist", "?zwho", "?gstats", "?astats", "?gname", "?zgrp", "?tagscan", "?gmembers", "?grole", "?grank", "?gkick", "?gz", "?gz2", "?pl" };
    bool managed = false; for(const char *name : serialized) if(token == name) managed = true;
    if(!managed || actor.empty()) return false;
    QueuedOperation item; item.command = command; item.actor = actor; item.sender = sender;
    item.group = g_myGroupName; item.generation = s_automationGeneration; item.started = ClientTick();
    if(s_operations.Push(item, ClientOptions().operationQueueLimit)) {
        g_bridgeSuppressEcho++; sendChat((char*)LM("!Islem kuyruga alindi. ?durum / ?iptal", "!Operation queued. ?durum / ?iptal"));
    } else { g_bridgeSuppressEcho++; sendChat((char*)LM("!Kuyruk dolu veya komut zaten kuyrukta.", "!Operation queue full or command already queued.")); }
    return true;
}
bool QueueAutomationAction(const std::string &key, const std::function<void()> &action) {
    QueuedOperation item; item.command = key; item.actor = g_szNickName; item.sender = g_myPlayerID;
    item.group = g_myGroupName; item.generation = s_automationGeneration; item.started = ClientTick(); item.action = action;
    return s_operations.Push(item, ClientOptions().operationQueueLimit);
}
void ProcessAutomationRuntime() {
    ProcessDurableResults();
    if(!iAreWeConnected || !iGameInited || g_loginPending || !iSpawned) return;
    if(s_backendOperationPending && (s_backendOperation.group != g_myGroupName || !RuntimeActorValid(s_backendOperation) || ClientTick() - s_backendOperation.started > ClientOptions().queueTimeoutMs)) CancelBackendOperation();
    std::string busy = RuntimeBusyName(false);
    if(!s_operationActor.empty() && !busy.empty() && ClientTick() - s_operationAt > ClientOptions().operationTimeoutMs) {
        Log("[RUNTIME] Operation timed out: %s", busy.c_str()); CancelManualAutomation(); busy.clear();
    }
    if(!busy.empty()) return;
    if(s_freeroamAt && static_cast<LONG>(ClientTick() - s_freeroamAt) >= 0) { s_freeroamAt = 0; if(!g_followActive) ZwFrOut(); }
    s_operationActor.clear(); s_operationAt = 0;
    if(g_zoneScanIdx >= 0 && !s_scanSuspended) {
        if(!s_scanWakeAt) return;
        s_scanSuspended = true;
    }
    QueuedOperation item;
    if(s_operations.Pop(ClientTick(), s_automationGeneration, ClientOptions().queueTimeoutMs, g_myGroupName,
        [](const QueuedOperation &op) { return op.sender == g_myPlayerID ? op.actor == g_szNickName :
            op.sender >= 0 && op.sender < MAX_PLAYERS && playerInfo[op.sender].iIsConnected && op.actor == playerInfo[op.sender].szPlayerName; }, item)) {
        s_operationActor = item.actor; s_operationAt = ClientTick();
        if(PrepareRuntimeBackendRead(item)) return;
        s_dispatchingOperation = true;
        if(item.action) item.action(); else HandleGroupCommand(item.command.c_str(), item.sender);
        s_dispatchingOperation = false;
        return;
    }
    s_scanSuspended = false;
}

static void RuntimePauseForFollow() { if(g_zoneScanIdx >= 0) { s_scanSuspended = true; s_scanWakeAt = ClientTick() + ClientOptions().scanGapMs; } }
