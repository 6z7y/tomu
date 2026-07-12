#include <pthread.h>
#include <unistd.h>

#include "../../libs/miniaudio.h"
#include "output.h"
#include "player.h"
#include "player_utils.h"
#include "decoder.h"
#include "playlist.h"
#include "macros.h"
#include "stream.h"
#include "structs.h"
#include "utils.h"

void playback_handle()
{
  while (1)
  {
    WITH_LOCK(tctx.list.pt_lock) {
      while (tctx.list.queue_index >= tctx.list.queue_count)
        pthread_cond_wait(&tctx.list.pt_signal, &tctx.list.pt_lock);
    }

    // for automatic next
    if (tctx.list.queue_index == tctx.list.queue_history_count) {
      int idx;

      if (tctx.state.shuffle)
        idx = get_rand() % tctx.list.queue_count;

      else
        idx = tctx.list.queue_index;

      // init new space for history
      tctx.list.queue_history = realloc(tctx.list.queue_history, sizeof(int) * (tctx.list.queue_history_count + 1));
      if (!tctx.list.queue_history) continue;

      tctx.list.queue_history[tctx.list.queue_history_count] = idx; // add
      tctx.list.queue_history_count++; // increase history count
      tctx.list.queue_index = tctx.list.queue_history_count - 1; // change index to last
    }

    int idx = tctx.list.queue_history[tctx.list.queue_index];
    char *src = strdup(tctx.list.queue_lists[idx]);

    tctx.list.src_type = extract_src_type(src);
    printf("is src type is %d\n", tctx.list.src_type);
    playback_run(src); // run now

    // if next
    if (tctx.state.skip_to_next == 1) {
      if (tctx.state.shuffle) {
        if (tctx.list.queue_index + 1 < tctx.list.queue_history_count)
          tctx.list.queue_index++;
        else
          tctx.list.queue_index = tctx.list.queue_history_count;
      } else {
        if (tctx.state.loop == LOOP_PLAYLIST)
          tctx.list.queue_index = (tctx.list.queue_index + 1) % tctx.list.queue_count;
        else
          tctx.list.queue_index++;
      }
    }

    // // if prev
    // else if (tctx.state.skip_to_next == -1) {
    // }

    // automatic next
    else {
      // if (tctx.state.shuffle) {
      //   tctx.list.queue_index = tctx.list.queue_history_count;
      // } else {
      if (tctx.state.loop == LOOP_PLAYLIST)
        tctx.list.queue_index = (tctx.list.queue_index + 1) % tctx.list.queue_count;
      else
        tctx.list.queue_index++;
      // }
    }

    free(src);
    tctx.state.skip_to_next = 0;
  }

  return;
}

int playback_run(const char *src)
{
  av_log_set_level(AV_LOG_QUIET);

  init_playbackstatus(&tctx.state);

  if (tctx.list.src_type == SRC_URL_RAW) {
    if (streaming_init_playback(&tctx, src) < 0) {
      fprintf(stderr, "[player] Failed to initialize streaming\n");
      return -1;
    }
  } else {
    if (get_audio_info(src) < 0) {
      cleanUP();
      return -1;
    }
    get_metadata(src);
    extract_cover(tctx.fmtCTX);
  }

  int capacity = (tctx.inf.sample_rate) * (tctx.inf.ch) * (tctx.inf.sample_fmt_bytes);
  if (capacity <= 0) capacity = 4096;
  tctx.buf = audio_buffer_init(capacity);
  if (!tctx.buf) {
    fprintf(stderr, "[player] Failed to allocate audio buffer\n");
    cleanUP();
    return -1;
  }

  ma_device_config ma_config = init_miniaudioConfig(&tctx.inf);

  if (ma_device_init(NULL, &ma_config, &tctx.buf->device) != MA_SUCCESS) {
    fprintf(stderr, "[player] Failed to initialize miniaudio\n");
    audio_buffer_destroy(tctx.buf);
    tctx.buf = NULL;
    cleanUP();
    return -1;
  }

  pthread_t decoder_thread;
  if (pthread_create(&decoder_thread, NULL, run_decoder, NULL) != 0) {
    fprintf(stderr, "[player] Failed to create decoder thread\n");
    ma_device_uninit(&tctx.buf->device);
    audio_buffer_destroy(tctx.buf);
    tctx.buf = NULL;
    cleanUP();
    return -1;
  }

  if (ma_device_start(&tctx.buf->device) != MA_SUCCESS) {
    fprintf(stderr, "[player] Failed to start audio device\n");
    tctx.state.running = 0;
    pthread_join(decoder_thread, NULL);
    ma_device_uninit(&tctx.buf->device);
    audio_buffer_destroy(tctx.buf);
    tctx.buf = NULL;
    cleanUP();
    return -1;
  }

  pthread_join(decoder_thread, NULL);

  tctx.state.running = 0;

  pthread_mutex_lock(&tctx.buf->lock);
  while (tctx.buf->filled != 0)
    sleep_ms(50);
  pthread_mutex_unlock(&tctx.buf->lock);

  ma_device_stop(&tctx.buf->device);
  ma_device_uninit(&tctx.buf->device);

  if (tctx.buf) {
    audio_buffer_destroy(tctx.buf);
    tctx.buf = NULL;
  }

  cleanUP();
  memset(&tctx.state.metadata, 0, sizeof(Audio_Metadata));

  return 0;
}

void playback_toggle(PlaybackStatus *state)
{
  int paused;
  WITH_LOCK(state->lock) {
    paused = state->paused = !state->paused;
    pthread_cond_broadcast(&state->wait_cond);
  }

  if (tctx.buf) {
    if (paused) ma_device_stop(&tctx.buf->device);
    else ma_device_start(&tctx.buf->device);
  }
}

void playback_stop(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    if (tctx.buf) ma_device_stop(&tctx.buf->device);
    state->skip_to_next = 0;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

void seek_playback(PlaybackStatus *state, dbus_int64_t offset)
{
  WITH_LOCK(state->lock) {
    if (!state->seek_request) {
      state->seek_request = 1;
      state->seek_target = offset;
    }
  }
}

void playback_speed_defualt(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) { state->speed = 1.00f; }
}

void playback_speed_increase(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    state->speed += 0.05f;
    if (state->speed > 2.00f) state->speed = 2.00f;
  }
}

void playback_speed_decrease(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    state->speed -= 0.05f;
    if (state->speed < 0.25f) state->speed = 0.25f;
  }
}

void shuffle_toggle(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    state->shuffle = !state->shuffle;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

void playback_next_audio(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    state->skip_to_next = 1;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

void playback_prev_audio(PlaybackStatus *state)
{
  WITH_LOCK(state->lock) {
    state->skip_to_next = -1;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);

    // for queue can prev if need signal
    WITH_LOCK(tctx.list.pt_lock) {

      if (tctx.state.loop == LOOP_PLAYLIST)
        tctx.list.queue_index = (tctx.list.queue_index - 1 + tctx.list.queue_count) % tctx.list.queue_count;

      else {
        tctx.list.queue_index--;
        if (tctx.list.queue_index < 0) tctx.list.queue_index = 0;
      }
      pthread_cond_broadcast(&tctx.list.pt_signal);
    }
  }
}
