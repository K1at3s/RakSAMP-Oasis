struct BackendReadEntry { BackendResponse response; DWORD at = 0; };
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
BackendResponse GetBackendSnapshot(const std::string &path, unsigned int maxAgeMs) {
    if(ClientReplay().enabled) {
        auto found = ClientReplay().responses.find("GET " + path);
        if(found == ClientReplay().responses.end()) return BackendResponse();
        BackendResponse response = std::make_shared<httplib::Response>(); response->status = found->second.first; response->body = found->second.second;
        return response;
    }
    const DWORD now = ClientTick();
    auto found = s_backendReads.find(path);
    if(found == s_backendReads.end() || now - found->second.at > maxAgeMs) {
        QueueBackendRequest("GET", path, "", [path](const BackendResponse &response) {
            if(s_backendReads.size() >= 256 && s_backendReads.find(path) == s_backendReads.end()) s_backendReads.erase(s_backendReads.begin());
            s_backendReads[path] = BackendReadEntry{response, ClientTick()};
        }, "read:" + path);
    }
    if(found == s_backendReads.end() || !found->second.response) return BackendResponse();
    BackendResponse response = found->second.response;
    try {
        json data = json::parse(response->body);
        if(data.contains("rows") && data["rows"].is_array()) {
            for(auto &row : data["rows"]) if(row.contains("ageSec") && row["ageSec"].is_number())
                row["ageSec"] = row["ageSec"].get<double>() + (now - found->second.at) / 1000;
            response = std::make_shared<httplib::Response>(*response); response->body = data.dump();
        }
    } catch(...) {}
    return response;
}
static DurableResults &WarOutbox() {
    static DurableResults outbox("war-outbox-" + g_myHWID + ".json"); return outbox;
}
void ProcessDurableResults() {
    if(ClientReplay().enabled) return;
    if(!g_isEngineActive || g_myHWID.empty()) return;
    static DWORD lastTry = 0;
    const DWORD now = ClientTick();
    if(lastTry && now - lastTry < ClientOptions().resultRetryMs) return;
    const json *front = WarOutbox().Front(); if(!front) return;
    lastTry = now;
    const json body = *front;
    // An old bot's payload is never relabelled with a new HWID or group.
    if(body.value("hwid", "") != g_myHWID) { Log("[OUTBOX] HWID mismatch; record preserved."); return; }
    QueueBackendRequest("POST", "/api/killlist", body.dump(), [body](const BackendResponse &response) {
        bool delivered = false;
        try { delivered = response && response->status == 200 && json::parse(response->body).value("success", false); } catch(...) {}
        if(delivered) {
            if(!WarOutbox().Ack(body.value("hwid", ""), body.value("warId", ""))) Log("[OUTBOX] Delivered but acknowledgement could not be saved; backend deduplication required.");
            else Log("[OUTBOX] War result delivered: %s", body.value("warId", "").c_str());
        } else if(response && response->status >= 400 && response->status < 500 && response->status != 408 && response->status != 429) {
            const std::string reason = "HTTP " + std::to_string(response->status) + ": result rejected";
            if(WarOutbox().Quarantine(body.value("hwid", ""), body.value("warId", ""), reason))
                s_resultWarning = "rejected result preserved in .failed file";
            Log("[OUTBOX] %s; inspect the preserved result before retrying.", reason.c_str());
        } else Log("[OUTBOX] Delivery not confirmed; record retained (HTTP %d).", response ? response->status : 0);
    }, "durable-war-result", false);
}
std::string DescribeBackend() {
    if(ClientReplay().enabled) return "backend=replay";
    const auto status = BackendQueue().GetStatus();
    return std::string("backend=") + (status.online ? "online" : "unavailable") + " http=" + std::to_string(status.lastHttp) +
        " requests=" + std::to_string(status.outstanding) + " war-outbox=" + std::to_string(WarOutbox().Size()) +
        (status.lastError.empty() ? "" : " last-error=" + status.lastError) + (WarOutbox().Error().empty() ? "" : " disk=" + WarOutbox().Error()) +
        (s_resultWarning.empty() ? "" : " result-warning=" + s_resultWarning);
}
