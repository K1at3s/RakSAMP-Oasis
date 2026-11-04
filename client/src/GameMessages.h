#ifndef GAME_MESSAGES_H
#define GAME_MESSAGES_H
inline const char *PlayerStatsMessage(bool tr) {
    return tr ? "!%s paket kaybi: %g%% | FPS: %s | Gecikme: %s ms" : "!%s's PL: %g%% | FPS: %s | Ping: %s ms";
}
#endif
