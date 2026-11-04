#ifndef LOG_PRIVACY_H
#define LOG_PRIVACY_H
#include <string>
#include <algorithm>
inline std::string RedactLogSecrets(std::string line) {
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
    if(lower.find("[login]") != std::string::npos) return "[LOGIN] [private dialog details redacted]";
    if(lower.find("[sec]") != std::string::npos) return "[SEC] [private input redacted]";
    const char *commands[] = { "/login", "/register", "/changepass", "?changepass", "/email", "?email", "/rcon login" };
    for(const char *command : commands) {
        size_t at = lower.find(command);
        if(at != std::string::npos) return line.substr(0, at) + std::string(command) + " [REDACTED]";
    }
    const char *keys[] = { "sessiontoken", "authorization", "password", "passwd", "bot_token", "secret_key", "aes_secret" };
    for(const char *key : keys) {
        size_t at = lower.find(key);
        if(at != std::string::npos) return line.substr(0, at) + std::string(key) + "=[REDACTED]";
    }
    return line;
}
#endif
