/* Real rcheevos runtime, fake transport compiled without native network code.
 * All accounts, tokens, game IDs and achievements below are local fixtures. */
#include "ra_client.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SDL_AtomicInt calls, awards, blocked, release_login, block_login;
static uint8_t ram[16];
static uint64_t clock_ms = 1000;
static const char *hash = "0123456789abcdef0123456789abcdef";
#define ACH(ID, ADDR) "{\"ID\":" ID ",\"Title\":\"Fixture\",\"Description\":\"Local only\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" ADDR "\",\"Author\":\"Test\",\"Created\":1,\"Modified\":1,\"BadgeName\":\"00001\"}"
static const char *patch = "{\"Success\":true,\"GameId\":1234,\"Title\":\"Offline fixture\",\"ConsoleId\":21,"
    "\"ImageIconUrl\":\"https://invalid.test/icon.png\",\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\","
    "\"Sets\":[{\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
    "\"ImageIconUrl\":\"https://invalid.test/icon.png\",\"Achievements\":["
    ACH("1", "0xH0000=1") "," ACH("2", "0xHFFFF=1") "," ACH("4", "0xH0001=1") "],\"Leaderboards\":[{"
    "\"ID\":3,\"Title\":\"Fixture leaderboard\",\"Description\":\"Local\","
    "\"Mem\":\"STA:0xH0000=1::CAN:0=1::SUB:1=1::VAL:0xH0000\",\"Format\":\"SCORE\"}]}]}";
static const char *transport(const char *url, const char *post, int *status, void *unused) {
    (void)unused;
    assert(strstr(url, "https://retroachievements.org/") == url);
    assert(post != NULL);
    SDL_AddAtomicInt(&calls, 1); *status = 200;
    if (strstr(post, "r=login2")) {
        if (strstr(post, "u=Alice")) {
            if (SDL_GetAtomicInt(&block_login)) {
                SDL_SetAtomicInt(&blocked, 1);
                while (!SDL_GetAtomicInt(&release_login)) SDL_Delay(1);
            }
            return "{\"Success\":true,\"User\":\"Alice\",\"Token\":\"fake-a\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
        }
        if (strstr(post, "u=Bob"))
            return "{\"Success\":true,\"User\":\"Bob\",\"Token\":\"fake-b\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
        return "{\"Success\":false,\"Error\":\"fixture rejection\"}";
    }
    if (strstr(post, "r=achievementsets")) return patch;
    if (strstr(post, "r=startsession")) {
        assert(strstr(post, "&h=0"));
        return "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}";
    }
    if (strstr(post, "r=awardachievement")) {
        assert(strstr(post, "&h=0"));
        assert(strstr(post, "&u=Alice") || strstr(post, "u=Alice&"));
        if (SDL_AddAtomicInt(&awards, 1) == 0) { *status = -2; return ""; }
        return "{\"Success\":true,\"Score\":0,\"SoftcoreScore\":5,\"AchievementID\":1,\"AchievementsRemaining\":1}";
    }
    if (strstr(post, "r=ping")) return "{\"Success\":true}";
    assert(!"unexpected API request (including a leaderboard submission)");
    return "";
}
static uint32_t read_memory(uint32_t address, uint8_t *buffer, uint32_t bytes, void *unused) {
    (void)unused;
    if (address >= sizeof(ram) || bytes > sizeof(ram) - address) return 0;
    memcpy(buffer, ram + address, bytes); return bytes;
}
static RaClientStatus tick(int eligible) {
    RaClientStatus status;
    RaClient_TestTime(clock_ms);
    RaClient_Tick(read_memory, NULL, hash, eligible, "Fixture mapping unavailable");
    RaClient_GetStatus(&status);
    return status;
}
static RaClientStatus wait_loaded(void) {
    int i; RaClientStatus status;
    for (i = 0; i < 3000; ++i) {
        status = tick(1);
        if (status.game_loaded) return status;
        SDL_Delay(1);
    }
    fprintf(stderr, "load timeout: %s\n", status.error); assert(0); return status;
}
int main(void) {
    RaClientStatus status;
    int i, before;
    RaClient_TestTransport(transport, NULL);
    assert(RaClient_Init());
    status = tick(0); assert(status.available && !status.enabled && !status.game_loaded);
    assert(!RaClient_RequestLogin("", "fake"));
    assert(RaClient_RequestLogin("Alice", "fake"));
    assert(!RaClient_RequestLogin("Bob", "fake"));
    status = wait_loaded();
    assert(!strcmp(status.username, "Alice") && status.total == 3 && status.unsupported == 1);
    assert(status.unlocked == 0);
    for (i = 0; i < 3; ++i) tick(1); /* leave the runtime's initial waiting state while false */
    ram[0] = 1; status = tick(1);
    assert(!strcmp(status.notification_title, "Fixture") && status.notification_points == 5);
    /* First award has a retryable connection failure; retry must preserve user
     * and softcore flags, without ever running on the render thread. */
    for (i = 0; i < 3000 && SDL_GetAtomicInt(&awards) < 2; ++i) {
        clock_ms += 1000; tick(1); SDL_Delay(1);
    }
    assert(SDL_GetAtomicInt(&awards) == 2);
    status = tick(0); assert(!status.game_loaded && !status.eligible);
    before = SDL_GetAtomicInt(&awards);
    ram[1] = 1; /* a still-locked condition becomes true while processing is blocked */
    for (i = 0; i < 10; ++i) tick(0);
    assert(SDL_GetAtomicInt(&awards) == before);
    RaClient_RequestLogout(); status = tick(0);
    assert(!status.logged_in && !status.username[0] && !status.notification_title[0]);

    /* A completed response for an aborted account must not replace Bob's
     * session or status when it is finally dispatched by Tick. */
    SDL_SetAtomicInt(&block_login, 1);
    assert(RaClient_RequestLogin("Alice", "fake")); tick(0);
    for (i = 0; i < 3000 && !SDL_GetAtomicInt(&blocked); ++i) SDL_Delay(1);
    assert(SDL_GetAtomicInt(&blocked));
    RaClient_RequestLogout(); tick(0);
    assert(RaClient_RequestLogin("Bob", "fake")); tick(0);
    SDL_SetAtomicInt(&release_login, 1); ram[0] = ram[1] = 0;
    status = wait_loaded(); assert(!strcmp(status.username, "Bob") && status.unlocked == 0);
    assert(SDL_GetAtomicInt(&awards) == before);
    RaClient_RequestLogout(); tick(0);
    assert(RaClient_RequestLogin("Rejected", "fake"));
    for (i = 0; i < 3000; ++i) { status = tick(0); if (!status.logging_in) break; SDL_Delay(1); }
    assert(!status.logged_in && status.error[0] && !status.username[0]);
    RaClient_Shutdown();
    assert(SDL_GetAtomicInt(&calls) > 5);
    assert(RaClient_Init()); RaClient_Shutdown();
    puts("ra_client: offline login/load/unsupported/award retry/cancellation/account isolation passed");
    return 0;
}
