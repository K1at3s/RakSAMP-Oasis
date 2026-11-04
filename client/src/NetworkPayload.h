#ifndef NETWORK_PAYLOAD_H
#define NETWORK_PAYLOAD_H
#include "BitStream.h"
#include <cstring>

// Parse into locals: a rejected packet must not change the active connection.
inline bool ReadAcceptedConnection(RakNet::BitStream &stream, unsigned short &id,
    unsigned int &challenge, unsigned int maxPlayers) {
    unsigned short candidate;
    unsigned int candidateChallenge;
    stream.IgnoreBits(8 + 32 + 16);
    if(!stream.Read(candidate) || candidate >= maxPlayers || !stream.Read(candidateChallenge)) return false;
    id = candidate;
    challenge = candidateChallenge;
    return true;
}

template<typename Length, size_t N>
inline bool ReadNetworkText(RakNet::BitStream &stream, char (&text)[N], Length length) {
    if(length < 0 || static_cast<unsigned long long>(length) >= N) return false;
    if(!stream.Read(text, static_cast<int>(length))) return false;
    text[length] = 0;
    return true;
}

inline bool IsSniperCommand(const char *text) {
    return text && std::strncmp(text, "?zsnip", 6) == 0 &&
        (text[6] == 0 || text[6] == ' ' || text[6] == '#');
}
#endif
