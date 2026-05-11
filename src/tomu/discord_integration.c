#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "discord_integration.h"
#include "discord_presence.h"
#include "DATA.h"

#define DISCORD_APP_ID "1503183287235903521"
#define COVER_DIR "/tmp/tomu_cover_img"

static DiscordPresence *g_dp = NULL;
static char last_title[256] = {0};
static char last_cover_url[512] = {0};
static int is_connected = 0;

// Get cover path from filename
const char* get_cover_path(const char *filename) {
    static char cover_path[512];
    if (!filename) return NULL;
    
    // Extract base name without extension
    char temp[512];
    strncpy(temp, filename, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *base = temp;
    char *last_slash = strrchr(temp, '/');
    if (last_slash) base = last_slash + 1;
    
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    
    snprintf(cover_path, sizeof(cover_path), "%s/%s.jpg", COVER_DIR, base);
    
    return access(cover_path, F_OK) == 0 ? cover_path : NULL;
}

// Upload image to catbox.moe (free, no API key)
char* upload_to_catbox(const char *image_path) {
    static char url[512];
    char cmd[1024];
    
    snprintf(cmd, sizeof(cmd),
        "curl -s -F \"fileToUpload=@%s\" -F \"reqtype=fileupload\" https://catbox.moe/user/api.php 2>/dev/null",
        image_path);
    
    FILE *fp = popen(cmd, "r");
    if (fp && fgets(url, sizeof(url), fp)) {
        url[strcspn(url, "\n")] = 0;
        pclose(fp);
        if (strstr(url, "https://") == url) {
            return url;
        }
    }
    return NULL;
}

void discord_init(void)
{
    g_dp = discord_create();
    if (discord_connect(g_dp, DISCORD_APP_ID) == 0) {
        is_connected = 1;
        printf("[discord] Ready!\n");
    } else {
        is_connected = 0;
        printf("[discord] Not connected (is Discord running?)\n");
    }
}

void discord_update_presence(void)
{
    if (!is_connected || !g_dp) return;
    if (!ctx.state.running) return;
    
    Audio_Metadata *meta = &ctx.state.metadata;
    
    char current[256];
    snprintf(current, sizeof(current), "%s|%s", 
             meta->title ? meta->title : "", 
             meta->artist ? meta->artist : "");
    
    if (strcmp(current, last_title) == 0) return;
    strcpy(last_title, current);
    
    const char *title  = meta->title  ? meta->title  : "Unknown";
    const char *artist = meta->artist ? meta->artist : "Unknown";
    const char *album  = meta->album  ? meta->album  : "";
    
    int duration_ms = ctx.state.duration * 1000;
    int position_ms = ctx.state.position * 1000;
    
    // Get cover image
    const char *image_key = "music";  // fallback
    const char *cover_path = NULL;
    
    if (ctx.queue_index >= 0 && ctx.queue_index < ctx.queue_count && ctx.queue_list[ctx.queue_index]) {
        cover_path = get_cover_path(ctx.queue_list[ctx.queue_index]);
        if (cover_path) {
            // Upload to catbox (only once per song)
            char url_key[512];
            snprintf(url_key, sizeof(url_key), "cover_%s", current);
            if (strcmp(url_key, last_cover_url) != 0) {
                char *url = upload_to_catbox(cover_path);
                if (url) {
                    strcpy(last_cover_url, url_key);
                    image_key = url;
                    printf("[discord] Cover uploaded: %s\n", url);
                }
            }
        }
    }
    
    discord_update(g_dp, title, artist, album, duration_ms, position_ms, image_key);
}

void discord_cleanup(void)
{
    if (g_dp) {
        discord_clear(g_dp);
        discord_disconnect(g_dp);
        discord_free(g_dp);
        g_dp = NULL;
        is_connected = 0;
    }
}
