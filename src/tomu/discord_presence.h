#pragma once
#include <stdint.h>

typedef struct DiscordPresence DiscordPresence;

DiscordPresence *discord_create(void);
void discord_free(DiscordPresence *dp);
int discord_connect(DiscordPresence *dp, const char *app_id);
void discord_disconnect(DiscordPresence *dp);
void discord_update(DiscordPresence *dp,
                    const char *title,
                    const char *artist,
                    const char *album,
                    int64_t duration_ms,
                    int64_t position_ms,
                    const char *cover_path);
void discord_clear(DiscordPresence *dp);
