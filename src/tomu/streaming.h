#ifndef STREAMING_H
#define STREAMING_H

#include <curl/curl.h>
#include <libavformat/avformat.h>

#include "structs.h"

// Initialize streaming context
int streaming_init(PlayBackContext *ctx);

// Resolve URL using yt-dlp
char *resolve_url(const char *url);
int resolve_playlist(const char *url);

// Start streaming from URL
int streaming_start(PlayBackContext *ctx, const char *url);

// Stop streaming
void streaming_stop(PlayBackContext *ctx);

// Check if streaming is active
int streaming_is_active(PlayBackContext *ctx);

// AVIO callbacks for streaming (exposed for backend.c)
int avio_read_packet(void *opaque, uint8_t *buf, int want);
int64_t avio_seek_packet(void *opaque, int64_t offset, int whence);

// Initialize streaming playback (sets up fmtCTX with AVIO)
int streaming_init_playback(PlayBackContext *ctx, const char *filename);

void streaming_cleanup(PlayBackContext *ctx);

// Extract metadata from URL
int extract_metadata_from_url(const char *url, Audio_Metadata *metadata);
// void download_thumbnail(const char *url);

// Add to streaming.h


// Extract metadata from URL
int extract_metadata_from_url(const char *url, Audio_Metadata *metadata);
void download_thumbnail(const char *url);

int get_metadata_from_url(const char *url, Audio_Metadata *metadata);

#endif
