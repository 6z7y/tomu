#include <libavformat/avformat.h>
#include <pthread.h>
#include <curl/curl.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <sys/file.h>

// #include "control.h"
#include "errors.h"
#include "macros.h"
#include "structs.h"

// toggle the FIFO status file: 1=Create, 0=Close
void fifo_file(PlayBackContext *ctx, int ON) 
{
  if (ON) {
    mkfifo(FIFO_PATH, 0644);

    int f = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
    if (f < 0) die("open fifo:");

    ctx->fifoCTX.fifo_file = fdopen(f, "w");
    if (!ctx->fifoCTX.fifo_file) die("fdopen:");

    setvbuf(ctx->fifoCTX.fifo_file, NULL, _IOLBF, 0); // flush automatic with '\n'
  }
  else {
    fclose(ctx->fifoCTX.fifo_file);
    unlink(FIFO_PATH);
  }
}

void *fifo_writer_thread(void *arg)
{
  PlayBackContext *ctx = arg;
  while (true) {
    const char *status = ctx->state.paused ? "Paused" : "Playing";
    char *loop;
    if (ctx->state.loop == LOOP_TRACK)  loop = "Track";
    else if (ctx->state.loop == LOOP_PLAYLIST)  loop = "Playlist";
    else loop = "None";

    fprintf(ctx->fifoCTX.fifo_file, "STATUS:%s\n",        status);
    fprintf(ctx->fifoCTX.fifo_file, "ARTIST:%s\n",        ctx->state.metadata.artist);
    fprintf(ctx->fifoCTX.fifo_file, "TITLE:%s\n",         ctx->state.metadata.title);
    fprintf(ctx->fifoCTX.fifo_file, "ALBUM:%s\n",         ctx->state.metadata.album);
    fprintf(ctx->fifoCTX.fifo_file, "ALBUM_ARTIST:%s\n",  ctx->state.metadata.album_artist);
    fprintf(ctx->fifoCTX.fifo_file, "COMPOSER:%s\n",      ctx->state.metadata.composer);
    fprintf(ctx->fifoCTX.fifo_file, "GENRE:%s\n",         ctx->state.metadata.genre);
    fprintf(ctx->fifoCTX.fifo_file, "DATE:%s\n",          ctx->state.metadata.date);
    fprintf(ctx->fifoCTX.fifo_file, "TRACK:%s\n",         ctx->state.metadata.track);
    fprintf(ctx->fifoCTX.fifo_file, "DISC:%s\n",          ctx->state.metadata.disc);
    fprintf(ctx->fifoCTX.fifo_file, "COVER:%s\n",         ctx->state.metadata.cover_path);
    fprintf(ctx->fifoCTX.fifo_file, "URL:%s\n",           ctx->state.metadata.url);
    fprintf(ctx->fifoCTX.fifo_file, "LOOP:%s\n",          loop);
    fprintf(ctx->fifoCTX.fifo_file, "DURATION:%d\n",      ctx->state.duration);
    fprintf(ctx->fifoCTX.fifo_file, "POSITION:%d\n",      ctx->state.position);
    fprintf(ctx->fifoCTX.fifo_file, "VOLUME:%.2f\n",      ctx->state.volume);
    fprintf(ctx->fifoCTX.fifo_file, "SPEED:%.2f\n",       ctx->state.speed);
    fprintf(ctx->fifoCTX.fifo_file, "SHUFFLE:%d\n",       ctx->state.shuffle);

    fflush(ctx->fifoCTX.fifo_file);   // safety — even with _IOLBF

    sleep_ms(200);
  }
}
