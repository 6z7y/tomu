#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "streaming.h"
#include "macros.h"
#include "structs.h"
#include "file_handle.h"
#include "audio_backend.h"
#include "backend_utils.h"
#include "mpris.h"

#define DEBUG_STREAMING 1

#if DEBUG_STREAMING
#define STREAM_DEBUG(fmt, ...) printf("[STREAM_DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define STREAM_DEBUG(fmt, ...)
#endif

CURL *curl = &tctx.stream_ctx.curl;

const char *is_direct[] = {
  ".mp3", ".ogg", ".opus", ".m4a",
  ".flac", ".wav", ".webm", "googlevideo.com", 
  "cf-hls", "sndcdn.com/media"
};

// AVIO callbacks
int avio_read_packet(void *opaque, uint8_t *buf, int want) {
    StreamBuf *sb = (StreamBuf *)opaque;
    pthread_mutex_lock(&sb->lock);
    
    while (sb->read_pos >= sb->size && !sb->done) {
        if (sb->error) {
            pthread_mutex_unlock(&sb->lock);
            return AVERROR(EIO);
        }
        pthread_cond_wait(&sb->more_data, &sb->lock);
    }
    
    size_t available = sb->size - sb->read_pos;
    if (available == 0) {
        pthread_mutex_unlock(&sb->lock);
        return AVERROR_EOF;
    }
    
    size_t give = (available < (size_t)want) ? available : (size_t)want;
    memcpy(buf, sb->data + sb->read_pos, give);
    sb->read_pos += give;
    
    pthread_mutex_unlock(&sb->lock);
    return (int)give;
}

int64_t avio_seek_packet(void *opaque, int64_t offset, int whence) {
    StreamBuf *sb = (StreamBuf *)opaque;
    pthread_mutex_lock(&sb->lock);
    
    if (whence == AVSEEK_SIZE || whence == SEEK_END) {
        pthread_mutex_unlock(&sb->lock);
        return -1;
    }
    
    int64_t new_pos;
    if (whence == SEEK_SET) {
        new_pos = offset;
    } else if (whence == SEEK_CUR) {
        new_pos = (int64_t)sb->read_pos + offset;
    } else {
        pthread_mutex_unlock(&sb->lock);
        return -1;
    }
    
    if (new_pos < 0) new_pos = 0;
    
    while ((size_t)new_pos > sb->size && !sb->done) {
        if (sb->error) {
            pthread_mutex_unlock(&sb->lock);
            return -1;
        }
        pthread_cond_wait(&sb->more_data, &sb->lock);
    }
    
    sb->read_pos = (size_t)new_pos;
    pthread_mutex_unlock(&sb->lock);
    return new_pos;
}

// Curl write callback
static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    StreamBuf *sb = (StreamBuf *)userdata;
    size_t bytes = size * nmemb;
    
    pthread_mutex_lock(&sb->lock);
    
    if (sb->size + bytes > sb->capacity) {
        size_t new_capacity = sb->capacity;
        while (sb->size + bytes > new_capacity) {
            new_capacity *= 2;
        }
        uint8_t *new_data = realloc(sb->data, new_capacity);
        if (!new_data) {
            sb->error = 1;
            pthread_mutex_unlock(&sb->lock);
            return 0;
        }
        sb->data = new_data;
        sb->capacity = new_capacity;
    }
    
    memcpy(sb->data + sb->size, ptr, bytes);
    sb->size += bytes;
    
    pthread_cond_broadcast(&sb->more_data);
    pthread_mutex_unlock(&sb->lock);
    
    return bytes;
}

// Curl thread function
typedef struct {
    const char *url;
    StreamBuf *sb;
    PlayBackContext *ctx;
} CurlArgs;

static void *curl_thread_fn(void *arg) {
    CurlArgs *ca = (CurlArgs *)arg;
    CURL *curl = curl_easy_init();
    
    if (!curl) {
        pthread_mutex_lock(&ca->sb->lock);
        ca->sb->error = 1;
        ca->sb->done = 1;
        pthread_cond_broadcast(&ca->sb->more_data);
        pthread_mutex_unlock(&ca->sb->lock);
        free(ca);
        return NULL;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, ca->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ca->sb);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (compatible; tomu/1.0)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16384L);
    
    CURLcode res = curl_easy_perform(curl);
    
    pthread_mutex_lock(&ca->sb->lock);
    if (res != CURLE_OK) {
        fprintf(stderr, "[curl] error: %s\n", curl_easy_strerror(res));
        ca->sb->error = 1;
    }
    ca->sb->done = 1;
    pthread_cond_broadcast(&ca->sb->more_data);
    pthread_mutex_unlock(&ca->sb->lock);
    
    curl_easy_cleanup(curl);
    free(ca);
    return NULL;
}


/*
 * Expand a playlist URL into the queue.
 * Calls yt-dlp to get all audio URLs and adds each one via queue_add().
 * Returns number of entries added, -1 on error.
 */

int extract_playlist_url(const char *url)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "yt-dlp --flat-playlist --print url \"%s\" 2>/dev/null",
      url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "[playlist] popen failed\n");
        return -1;
    }

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // strip newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len == 0) continue;

        printf("[playlist] +%d: %s\n", count + 1, line);
        queue_add(line, SRC_URL_RAW);
        count++;
    }
    pclose(fp);

    printf("[playlist] Added %d tracks\n", count);
    return count;
}

// Resolve URL using yt-dlp
char *resolve_url(const char *url) {
    
  // checking from direct url
  for_each_arr(is_direct) {
    if (strstr(is_direct[i], url)) {
      printf("[url] direct audio URL detected\n");
      return strdup(url);
    }
  }
    
  // Try to get best audio URL
  char cmd[4096];
  snprintf(cmd, sizeof(cmd),
           "yt-dlp -f bestaudio/best --no-playlist -g \"%s\" 2>/dev/null | head -1",
           url);
  
  printf("[yt-dlp] Running: %s\n", cmd);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
      fprintf(stderr, "[yt-dlp] popen failed\n");
      return strdup(url);
  }
  
  char resolved[8192] = {0};
  if (!fgets(resolved, sizeof(resolved), fp)) {
      fprintf(stderr, "[yt-dlp] no output - is yt-dlp installed?\n");
      pclose(fp);
      return strdup(url);
  }
  pclose(fp);
  
  size_t len = strlen(resolved);
  while (len > 0 && (resolved[len-1] == '\n' || resolved[len-1] == '\r'))
      resolved[--len] = '\0';
  
  if (len == 0) {
      fprintf(stderr, "[yt-dlp] empty result\n");
      return strdup(url);
  }
  
  printf("[yt-dlp] resolved → %s\n", resolved);
  return strdup(resolved);
}

// Sanitize filename - remove invalid characters
static void sanitize_filename(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '/' || str[i] == '\\' || str[i] == ':' || 
            str[i] == '*' || str[i] == '?' || str[i] == '"' || 
            str[i] == '<' || str[i] == '>' || str[i] == '|') {
            str[i] = '_';
        }
    }
}

// Extract metadata from URL using yt-dlp
// Extract metadata from URL using yt-dlp - FIXED VERSION
// Get metadata from URL - called BEFORE streaming starts
int get_metadata_from_url(const char *url, Audio_Metadata *metadata) {
    if (!url || !metadata) {
        fprintf(stderr, "[metadata] Invalid arguments\n");
        return -1;
    }
    
    printf("[metadata] Extracting metadata from: %s\n", url);
    
    // Check if yt-dlp is installed
    int has_ytdlp = 0;
    FILE *check = popen("which yt-dlp 2>/dev/null", "r");
    if (check) {
        char line[256] = {0};
        if (fgets(line, sizeof(line), check)) {
            has_ytdlp = 1;
            printf("[metadata] ✅ yt-dlp found\n");
        }
        pclose(check);
    }
    
    if (!has_ytdlp) {
        fprintf(stderr, "[metadata] ⚠️ yt-dlp not installed!\n");
        const char *base = strrchr(url, '/');
        if (base) {
            base = base + 1;
            char *clean = strdup(base);
            char *q = strchr(clean, '?');
            if (q) *q = '\0';
            strncpy(metadata->title, clean, sizeof(metadata->title) - 1);
            free(clean);
        }
        return 0;
    }
    
    char title[1024] = {0};
    char artist[1024] = {0};
    char thumbnail[2048] = {0};
    char cmd[4096];
    
    // 1. Get title
    snprintf(cmd, sizeof(cmd), 
             "yt-dlp -e --no-playlist \"%s\" 2>/dev/null | head -1", 
             url);
    
    printf("[metadata] Getting title...\n");
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(title, sizeof(title), fp)) {
            char *newline = strchr(title, '\n');
            if (newline) *newline = '\0';
            if (strlen(title) > 0 && strcmp(title, "NA") != 0) {
                strncpy(metadata->title, title, sizeof(metadata->title) - 1);
                printf("[metadata] ✅ Title: '%s'\n", metadata->title);
            }
        }
        pclose(fp);
    }
    
    // 2. Get artist
    snprintf(cmd, sizeof(cmd), 
             "yt-dlp --print uploader --no-playlist \"%s\" 2>/dev/null | head -1", 
             url);
    
    printf("[metadata] Getting artist...\n");
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(artist, sizeof(artist), fp)) {
            char *newline = strchr(artist, '\n');
            if (newline) *newline = '\0';
            if (strlen(artist) > 0 && strcmp(artist, "NA") != 0) {
                strncpy(metadata->artist, artist, sizeof(metadata->artist) - 1);
                printf("[metadata] ✅ Artist: '%s'\n", metadata->artist);
            }
        }
        pclose(fp);
    }
    
    // 3. Get thumbnail URL and download
    snprintf(cmd, sizeof(cmd), 
             "yt-dlp --print thumbnail --no-playlist \"%s\" 2>/dev/null | head -1", 
             url);
    
    printf("[metadata] Getting thumbnail...\n");
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(thumbnail, sizeof(thumbnail), fp)) {
            char *newline = strchr(thumbnail, '\n');
            if (newline) *newline = '\0';
            if (strlen(thumbnail) > 0 && strcmp(thumbnail, "NA") != 0) {
                printf("[metadata] 📷 Thumbnail URL: %s\n", thumbnail);
                download_thumbnail(thumbnail);
            }
        }
        pclose(fp);
    }
    
    // Fallback if no title
    if (!metadata->title[0] || strcmp(metadata->title, "NA") == 0) {
        const char *base = strrchr(url, '/');
        if (base) {
            base = base + 1;
            char *clean = strdup(base);
            char *q = strchr(clean, '?');
            if (q) *q = '\0';
            strncpy(metadata->title, clean, sizeof(metadata->title) - 1);
            free(clean);
            printf("[metadata] ⚠️ Using URL fallback title: '%s'\n", metadata->title);
        }
    }
    
    printf("[metadata] FINAL: title='%s', artist='%s', cover='%s'\n",
           metadata->title, metadata->artist, metadata->cover_path);
    
    return 0;
}

// Download thumbnail - saves as title.jpg
void download_thumbnail(const char *url) {
    if (!url || strlen(url) == 0) {
        printf("[thumbnail] ⚠️ No URL provided\n");
        return;
    }
    
    strncpy(tctx.state.metadata.cover_path, url, sizeof(tctx.state.metadata.cover_path) - 1);
}

// Initialize streaming context
int streaming_init_zero(StreamContext *stream) {
    memset(stream, 0, sizeof(StreamContext));
    stream->is_streaming = 0;
    stream->streaming_active = 0;
    stream->stream_buf = NULL;
    return 0;
}

// Cleanup streaming resources
void streaming_cleanup(StreamContext *stream) {
    if (!stream) return;
    
    if (stream->stream_buf) {
        StreamBuf *sb = (StreamBuf *)stream->stream_buf;
        if (sb->data) {
            free(sb->data);
        }
        pthread_mutex_destroy(&sb->lock);
        pthread_cond_destroy(&sb->more_data);
        free(sb);
        stream->stream_buf = NULL;
    }
    
    if (stream->stream_url) {
        free(stream->stream_url);
        stream->stream_url = NULL;
    }
    
    if (stream->original_url) {
        free(stream->original_url);
        stream->original_url = NULL;
    }
    
    stream->is_streaming = 0;
    stream->streaming_active = 0;
}

// Start streaming from URL
int streaming_start(PlayBackContext *ctx, const char *url) {
    streaming_cleanup(&tctx.stream_ctx);
    
    char *stream_url = resolve_url(url);
    if (!stream_url) {
        return -1;
    }
    
    StreamBuf *sb = calloc(1, sizeof(StreamBuf));
    if (!sb) {
        free(stream_url);
        return -1;
    }
    
    sb->data = malloc(1024 * 1024);
    if (!sb->data) {
        free(sb);
        free(stream_url);
        return -1;
    }
    
    sb->capacity = 1024 * 1024;
    sb->size = 0;
    sb->read_pos = 0;
    sb->done = 0;
    sb->error = 0;
    pthread_mutex_init(&sb->lock, NULL);
    pthread_cond_init(&sb->more_data, NULL);
    
    ctx->stream_ctx.stream_buf = sb;
    ctx->stream_ctx.stream_url = stream_url;
    ctx->stream_ctx.original_url = strdup(url);
    ctx->stream_ctx.is_streaming = 1;
    ctx->stream_ctx.streaming_active = 1;
    
    CurlArgs *ca = malloc(sizeof(CurlArgs));
    if (!ca) {
        streaming_cleanup(&tctx.stream_ctx);
        return -1;
    }
    
    ca->url = stream_url;
    ca->sb = sb;
    ca->ctx = ctx;
    
    if (pthread_create(&ctx->stream_ctx.stream_thread, NULL, curl_thread_fn, ca) != 0) {
        free(ca);
        streaming_cleanup(&tctx.stream_ctx);
        return -1;
    }
    pthread_join(ctx->stream_ctx.stream_thread, NULL);
    
    printf("[stream] Started streaming from: %s\n", url);
    return 0;
}

// Stop streaming
void streaming_stop(PlayBackContext *ctx) {
    if (!ctx || !ctx->stream_ctx.is_streaming) return;
    
    ctx->stream_ctx.streaming_active = 0;
    
    if (ctx->stream_ctx.stream_buf) {
        StreamBuf *sb = (StreamBuf *)ctx->stream_ctx.stream_buf;
        pthread_mutex_lock(&sb->lock);
        sb->done = 1;
        pthread_cond_broadcast(&sb->more_data);
        pthread_mutex_unlock(&sb->lock);
    }
    
    streaming_cleanup(&tctx.stream_ctx);
    printf("[stream] Stopped streaming\n");
}

// Check if streaming is active
int streaming_is_active(PlayBackContext *ctx) {
    return ctx->stream_ctx.is_streaming && ctx->stream_ctx.streaming_active;
}

// Initialize streaming playback
int streaming_init_playback(PlayBackContext *ctx, const char *filename) {
    streaming_init_zero(&tctx.stream_ctx); // zero
    if (streaming_start(ctx, filename) < 0) {
        fprintf(stderr, "Failed to start streaming\n");
        return -1;
    }
    
    StreamBuf *sb = (StreamBuf *)ctx->stream_ctx.stream_buf;
    
    pthread_mutex_lock(&sb->lock);
    int timeout = 300;
    // Use smaller probe size for faster start
    while (sb->size < 64 * 1024 && !sb->done && timeout > 0) {
        printf("Buffering... %zu / %d bytes\n", sb->size, 64 * 1024);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&sb->more_data, &sb->lock, &ts);
        timeout--;
    }
    
    if (sb->error) {
        pthread_mutex_unlock(&sb->lock);
        streaming_stop(ctx);
        return -1;
    }
    
    // Even if we don't have enough data, try to open if we have some data
    if (sb->size == 0 && sb->done) {
        pthread_mutex_unlock(&sb->lock);
        fprintf(stderr, "No data received from URL\n");
        streaming_stop(ctx);
        return -1;
    }
    pthread_mutex_unlock(&sb->lock);
    
    uint8_t *avio_buf = (uint8_t *)av_malloc(64 * 1024);
    if (!avio_buf) {
        streaming_stop(ctx);
        return -1;
    }
    
    AVIOContext *avio = avio_alloc_context(
        avio_buf, 64 * 1024,
        0, sb,
        avio_read_packet, NULL, avio_seek_packet
    );
    
    if (!avio) {
        av_free(avio_buf);
        streaming_stop(ctx);
        return -1;
    }
    
    ctx->fmtCTX = avformat_alloc_context();
    if (!ctx->fmtCTX) {
        avio->opaque = NULL;
        avio = NULL;
        streaming_stop(ctx);
        return -1;
    }
    
    ctx->fmtCTX->pb = avio;
    ctx->fmtCTX->flags |= AVFMT_FLAG_CUSTOM_IO;
    
    // Set smaller probe size for faster opening
    ctx->fmtCTX->probesize = 64 * 1024;
    ctx->fmtCTX->max_analyze_duration = 3 * AV_TIME_BASE; // 3 seconds
    
    if (avformat_open_input(&ctx->fmtCTX, NULL, NULL, NULL) < 0) {
        ctx->fmtCTX->pb = NULL;
        avio = NULL;
        streaming_stop(ctx);
        return -1;
    }
    
    return 0;
}

