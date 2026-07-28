#ifndef STREAM_H
#define STREAM_H

#include <curl/curl.h>
#include <libavformat/avformat.h>

#include "structs.h"

int streaming_init(StreamContext *stream);
char *resolve_url(const char *url);
int extract_playlist_url(const char *url);
int streaming_start(PlayBackContext *ctx, const char *url);
void streaming_stop(PlayBackContext *ctx);
int streaming_is_active(PlayBackContext *ctx);
int avio_read_packet(void *opaque, uint8_t *buf, int want);
int64_t avio_seek_packet(void *opaque, int64_t offset, int whence);
int streaming_init_playback(PlayBackContext *ctx, const char *filename);
void streaming_cleanup(StreamContext *stream);
int get_metadata_from_url(const char *url, Audio_Metadata *metadata);
void download_thumbnail(const char *url);
void init_curl();

#endif
