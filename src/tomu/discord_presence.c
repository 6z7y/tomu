#include "discord_presence.h"
#include "DATA.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

/* ── IPC opcodes ─────────────────────────────────────────────────────── */
#define OP_HANDSHAKE 0
#define OP_FRAME     1

/* ── Internal struct ─────────────────────────────────────────────────── */
struct DiscordPresence {
    int  fd;
    char app_id[32];
};

/* ── Tiny JSON string escaper ────────────────────────────────────────── */
static void escape_json(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];
        if      (c == '"')  { out[j++] = '\\'; out[j++] = '"';  }
        else if (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n';  }
        else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r';  }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't';  }
        else                { out[j++] = (char)c; }
    }
    out[j] = '\0';
}

/* ── Send one IPC frame ──────────────────────────────────────────────── */
static void ipc_send(int fd, uint32_t opcode, const char *json) {
    uint32_t len = (uint32_t)strlen(json);

    uint8_t header[8];
    header[0] = (opcode >>  0) & 0xFF;
    header[1] = (opcode >>  8) & 0xFF;
    header[2] = (opcode >> 16) & 0xFF;
    header[3] = (opcode >> 24) & 0xFF;
    header[4] = (len    >>  0) & 0xFF;
    header[5] = (len    >>  8) & 0xFF;
    header[6] = (len    >> 16) & 0xFF;
    header[7] = (len    >> 24) & 0xFF;

    write(fd, header, 8);
    write(fd, json,   len);

    char buf[4096];
    recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
}

/* ── Public API ──────────────────────────────────────────────────────── */
DiscordPresence *discord_create(void) {
    DiscordPresence *dp = calloc(1, sizeof(DiscordPresence));
    if (dp) dp->fd = -1;
    return dp;
}

void discord_free(DiscordPresence *dp) {
    if (!dp) return;
    discord_disconnect(dp);
    free(dp);
}

int discord_connect(DiscordPresence *dp, const char *app_id) {
    if (!dp || !app_id) return -1;

    strncpy(dp->app_id, app_id, sizeof(dp->app_id) - 1);

    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime) runtime = "/tmp";

    for (int i = 0; i < 10; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/discord-ipc-%d", runtime, i);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) continue;

        struct sockaddr_un addr = {0};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            dp->fd = fd;

            char hs[128];
            snprintf(hs, sizeof(hs),
                     "{\"v\":1,\"client_id\":\"%s\"}", app_id);
            ipc_send(dp->fd, OP_HANDSHAKE, hs);

            printf("[discord] Connected on %s\n", path);
            return 0;
        }
        close(fd);
    }

    fprintf(stderr, "[discord] Could not connect — is Discord running?\n");
    return -1;
}

void discord_disconnect(DiscordPresence *dp) {
    if (!dp || dp->fd < 0) return;
    discord_clear(dp);
    close(dp->fd);
    dp->fd = -1;
}

// ADD THIS MISSING FUNCTION
void discord_clear(DiscordPresence *dp) {
    if (!dp || dp->fd < 0) return;

    char json[128];
    snprintf(json, sizeof(json),
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d},\"nonce\":\"clr\"}",
        (int)getpid());

    ipc_send(dp->fd, OP_FRAME, json);
    printf("[discord] Cleared presence\n");
}

void discord_update(DiscordPresence *dp,
                    const char *title,
                    const char *artist,
                    const char *album,
                    int64_t     duration_ms,
                    int64_t     position_ms,
                    const char *cover_path) {
    if (!dp || dp->fd < 0) return;

    int64_t now      = (int64_t)time(NULL);
    int64_t start_ts = now - (position_ms / 1000);
    int64_t end_ts   = start_ts + (duration_ms / 1000);

    char t[256], ar[256], al[256], track_num[32] = "";
    escape_json(title  ? title  : "Unknown", t,  sizeof(t));
    escape_json(artist ? artist : "Unknown", ar, sizeof(ar));
    escape_json(album  ? album  : "",        al, sizeof(al));
    
    // Get track number from metadata if available
    Audio_Metadata *meta = &ctx.state.metadata;
    if (meta->track && meta->track[0]) {
        snprintf(track_num, sizeof(track_num), " ♯%s", meta->track);
    }

    // RICE: Custom state text with emojis
    char state[512];
    snprintf(state, sizeof(state), "%s • test", ar);  // Artist • Album
    
    // RICE: Custom details (can add extra info)
    char details[512];
    snprintf(details, sizeof(details), "%s", t);  // Just song title

    char json[2048];
    snprintf(json, sizeof(json),
        "{"
          "\"cmd\":\"SET_ACTIVITY\","
          "\"args\":{"
            "\"pid\":%d,"
            "\"activity\":{"
              "\"details\":\"%s\","
              "\"state\":\"%s\","
              "\"timestamps\":{"
                "\"start\":%lld,"
                "\"end\":%lld"
              "},"
              "\"assets\":{"
                "\"large_image\":\"%s\","
                "\"large_text\":\"♪ %s ♪\""
              "},"
              "\"type\":2"
            "}"
          "},"
          "\"nonce\":\"upd\""
        "}",
        (int)getpid(), details, state,
        (long long)start_ts, (long long)end_ts,
        cover_path ? cover_path : "music",
        al[0] ? al : "Now Playing");

    ipc_send(dp->fd, OP_FRAME, json);
    printf("[discord] %s - %s\n", title, artist);
}
