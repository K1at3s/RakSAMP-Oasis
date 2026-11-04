struct BackendReadWaiter {
    BackendHttpQueue::Callback callback;
    std::string group, hwid;
};
struct BackendReadEntry {
    BackendResponse response;
    DWORD at = 0;
    bool pending = false;
    bool completed = false;
    std::vector<BackendReadWaiter> waiting;
};
static std::map<std::string, BackendReadEntry> s_backendReads;
static std::string s_resultWarning;
bool QueueBackendRequest(const std::string &method, const std::string &path, const std::string &body,
    BackendHttpQueue::Callback callback, const std::string &key, bool sessionBound) {
    const unsigned long generation = s_backendLinkGeneration;
    return QueueBackend(method, path, body, [generation, sessionBound, callback](const BackendResponse &response) {
        if(sessionBound && generation != s_backendLinkGeneration) return;
        if(callback) callback(response);
    }, key);
}
bool ReadBackendOnce(const std::string &path, BackendHttpQueue::Callback callback, unsigned int maxAgeMs) {
    auto found = s_backendReads.find(path);
    if(found == s_backendReads.end()) {
        if(s_backendReads.size() >= 256) {
            auto victim = s_backendReads.begin();
            while(victim != s_backendReads.end() && victim->second.pending) ++victim;
            if(victim == s_backendReads.end()) return false;
            s_backendReads.erase(victim);
        }
        found = s_backendReads.emplace(path, BackendReadEntry()).first;
    }
    BackendReadEntry &entry = found->second;
    if(maxAgeMs && entry.completed && ClientTick() - entry.at <= maxAgeMs) {
        if(callback) callback(entry.response);
        return true;
    }
    if(callback && entry.waiting.size() >= 16) return false;
    if(callback) entry.waiting.push_back(BackendReadWaiter{callback, g_myGroupName, g_myHWID});
    if(entry.pending) return true;
    entry.pending = true;
    if(!QueueBackendRequest("GET", path, "", [path](const BackendResponse &response) {
        auto found = s_backendReads.find(path);
        if(found == s_backendReads.end()) return;
        found->second.pending = false;
        found->second.completed = true;
        found->second.response = response;
        found->second.at = ClientTick();
        auto waiting = std::move(found->second.waiting);
        found->second.waiting.clear();
        for(const auto &waiter : waiting) {
            if(waiter.group != g_myGroupName || waiter.hwid != g_myHWID) continue;
            try { waiter.callback(response); } catch(...) { Log("[BACKEND] Read completion could not be processed."); }
        }
    }, "read:" + path)) {
        entry.pending = false;
        entry.waiting.clear();
        return false;
    }
    return true;
}
BackendResponse GetBackendSnapshot(const std::string &path, unsigned int maxAgeMs) {
    if(ClientReplay().enabled && !ClientReplay().asyncReads) {
        auto found = ClientReplay().responses.find("GET " + path);
        if(found == ClientReplay().responses.end()) return BackendResponse();
        BackendResponse response = std::make_shared<httplib::Response>();
        response->status = found->second.first; response->body = found->second.second;
        return response;
    }
    auto found = s_backendReads.find(path);
    if(found == s_backendReads.end() || (!found->second.pending && (!found->second.completed || ClientTick() - found->second.at > maxAgeMs)))
        ReadBackendOnce(path, BackendHttpQueue::Callback(), maxAgeMs);
    found = s_backendReads.find(path);
    if(found == s_backendReads.end() || !found->second.response) return BackendResponse();
    BackendResponse response = found->second.response;
    try {
        json data = json::parse(response->body);
        if(data.contains("rows") && data["rows"].is_array()) {
            for(auto &row : data["rows"]) if(row.contains("ageSec") && row["ageSec"].is_number())
                row["ageSec"] = row["ageSec"].get<double>() + (ClientTick() - found->second.at) / 1000;
            response = std::make_shared<httplib::Response>(*response); response->body = data.dump();
        }
    } catch(...) {}
    return response;
}
