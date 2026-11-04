#ifndef RUNTIME_OPTIONS_H
#define RUNTIME_OPTIONS_H
#include <string>
#include <cstdlib>
struct RuntimeOptions {
    std::string backendUrl = "http://127.0.0.1:3000";
    unsigned int connectTimeoutMs = 2000, readTimeoutMs = 3000, writeTimeoutMs = 3000, totalTimeoutMs = 5000;
    unsigned int pollIntervalMs = 2000, dbSyncIntervalMs = 60000, scanPeriodMs = 900000;
    unsigned int scanSampleMs = 7000, scanTeleportMs = 3500, scanGapMs = 2000;
    unsigned int reconnectMs = 2000, operationTimeoutMs = 1800000, queueTimeoutMs = 120000, resultRetryMs = 5000;
    unsigned int warNotice1 = 10, warNotice2 = 3, warNotice3 = 1;
    size_t operationQueueLimit = 24, resultQueueLimit = 100;
};
inline RuntimeOptions &ClientOptions() { static RuntimeOptions options; return options; }
inline bool RuntimeInteger(const char *text, unsigned int low, unsigned int high, unsigned int &out) {
    if(!text || !*text) return false;
    unsigned long long value = 0;
    for(const char *p = text; *p; ++p) {
        if(*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0'); if(value > high) return false;
    }
    if(value < low) return false;
    out = static_cast<unsigned int>(value); return true;
}
inline bool ValidBackendUrl(const std::string &url) {
    if(url.compare(0, 7, "http://") != 0 || url.size() <= 7 || url.size() >= 512) return false;
    if(url.find_first_of(" \t\r\n?#@") != std::string::npos || url[7] == '/' || url[7] == ':') return false;
    const size_t slash = url.find('/', 7);
    return slash == std::string::npos || slash == url.size() - 1;
}
#endif
