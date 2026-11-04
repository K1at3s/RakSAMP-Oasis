// A command waits before changing game state, then returns to the normal queue.
static bool s_backendOperationPending = false;
static unsigned int s_backendOperationTicket = 0;
static QueuedOperation s_backendOperation;
static unsigned int RuntimeReadEpoch() { return s_backendOperationTicket; }
static bool RuntimeActorValid(const QueuedOperation &item) {
    return item.sender == g_myPlayerID ? item.actor == g_szNickName :
        item.sender >= 0 && item.sender < MAX_PLAYERS && playerInfo[item.sender].iIsConnected &&
        item.actor == playerInfo[item.sender].szPlayerName;
}
static void CancelBackendOperation() {
    ++s_backendOperationTicket;
    s_backendOperationPending = false;
    s_backendOperation = QueuedOperation();
}
static bool RuntimeWaitForRead(const QueuedOperation &item, const std::string &path) {
    const auto cached = GetBackendSnapshot(path);
    if(cached && cached->status == 200) return false;
    s_backendOperationPending = true;
    s_backendOperation = item;
    const unsigned int ticket = ++s_backendOperationTicket;
    const auto failed = []() {
        g_bridgeSuppressEcho++;
        sendChat((char*)LM("!Backend sorgusu tamamlanamadi.", "!Backend query could not be completed."));
    };
    if(!ReadBackendOnce(path, [item, ticket, failed](const BackendResponse &response) {
        if(!s_backendOperationPending || ticket != s_backendOperationTicket) return;
        s_backendOperationPending = false;
        if(item.generation != s_automationGeneration || item.group != g_myGroupName || !RuntimeActorValid(item)) return;
        bool valid = response && response->status == 200;
        try { if(valid) json::parse(response->body); } catch(...) { valid = false; }
        if(!valid || !s_operations.Push(item, ClientOptions().operationQueueLimit)) failed();
    })) {
        s_backendOperationPending = false;
        failed();
    }
    return true;
}
static bool PrepareRuntimeBackendRead(const QueuedOperation &item) {
    if(item.action) return false;
    const size_t separator = item.command.find_first_of(" #");
    const std::string token = item.command.substr(0, separator);
    std::string argument = separator == std::string::npos ? "" : item.command.substr(separator);
    const size_t first = argument.find_first_not_of(" #");
    argument = first == std::string::npos ? "" : argument.substr(first);
    if(token == "?gmembers") {
        return RuntimeWaitForRead(item, "/api/gmembers/known?hwid=" + BackendQueryValue(g_myHWID) +
            "&group=" + BackendQueryValue(GameChatToUtf8(item.group.c_str())));
    }
    const bool zone = token == "?zsnip" || token == "?zgo" || token == "?za" || token == "?zattack" || token == "?gz" || token == "?gz2";
    if(!zone || argument.empty() || argument == "off" || argument.find_first_not_of("0123456789") == std::string::npos) return false;
    if(!_stricmp(argument.c_str(), "Free")) return false;
    std::string localName = argument;
    int localMatches = 0;
    for(int i = 0; i < g_groupColorCount; ++i) {
        if(!_stricmp(argument.c_str(), g_groupColors[i].szName) || !_stricmp(argument.c_str(), g_groupColors[i].szTag)) {
            localName = g_groupColors[i].szName; ++localMatches;
        }
    }
    if(localMatches > 1) return false;
    for(int zoneId = 0; zoneId < g_zoneCount; ++zoneId)
        if(!_stricmp(g_zoneOwnerName[zoneId], localName.c_str())) return false;
    if(RuntimeWaitForRead(item, "/api/groups")) return true;
    LoadGroupsFromDb();
    std::string name = argument;
    int matches = 0;
    for(int i = 0; i < g_groupColorCount; ++i) {
        if(!_stricmp(argument.c_str(), g_groupColors[i].szName) || !_stricmp(argument.c_str(), g_groupColors[i].szTag)) {
            name = g_groupColors[i].szName; ++matches;
        }
    }
    if(matches > 1) return false; // The command's existing ambiguity check reports this.
    for(int zoneId = 0; zoneId < g_zoneCount; ++zoneId)
        if(!_stricmp(g_zoneOwnerName[zoneId], name.c_str())) return false;
    return RuntimeWaitForRead(item, "/api/groups/zone?name=" + BackendQueryValue(GameChatToUtf8(name.c_str())));
}
