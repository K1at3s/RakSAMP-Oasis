#ifndef HARDWARE_IDENTITY_H
#define HARDWARE_IDENTITY_H
#include <windows.h>
#include <bcrypt.h>
#include <string>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

inline std::string IdentityDigest(const std::string &input) {
    BCRYPT_ALG_HANDLE algorithm = NULL;
    if(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return "";
    unsigned char digest[32];
    const NTSTATUS status = BCryptHash(algorithm, NULL, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()),
        digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if(status < 0) return "";
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    for(unsigned char value : digest) { result += hex[value >> 4]; result += hex[value & 15]; }
    return result;
}

inline std::string BuildHardwareIdentity(const std::string &cpu, const std::string &volume,
    const std::string &machineGuid) {
    if(volume.empty() && machineGuid.empty()) return "";
    const std::string digest = IdentityDigest("RakSAMP-v2|cpu:" + cpu + "|volume:" + volume + "|machine:" + machineGuid);
    return digest.empty() ? "" : "BURHAN2-" + digest.substr(0, 32);
}

inline std::string ScopeHardwareIdentity(const std::string &hardwareId, const char *nick) {
    if(hardwareId.empty() || !nick || !*nick) return "";
    std::string name(nick);
    for(char &ch : name) if(ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    const std::string digest = IdentityDigest(name);
    return digest.empty() ? "" : hardwareId + "-" + digest.substr(0, 16);
}
#endif
