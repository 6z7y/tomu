#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <sys/inotify.h>
#include <curl/curl.h>

#include "args.h"
#include "errors.h"
#include "player.h"
#include "playlist.h"
#include "stream.h"
#include "structs.h"
#include "macros.h"
#include "utils.h"

int main(int argc, char **argv)
{
  if (signal(SIGINT, signal_handle) == SIG_ERR) die("signal SIGINT");
  if (signal(SIGTERM, signal_handle) == SIG_ERR) die("signal SIGTERM");

  PlayBackContext ctx = {0};

  if (argc > 1) args_handle(&ctx, argc, argv);

  // init_threads(&ctx); // lllllllllatterrrrrrrrrrrrrrrrrrrrrrrr test it and continue forrrrrrrrrrrrr now
  // init_curl(); //also
  // pthread_t read_cmd_t;
  // pthread_create(&read_cmd_t, NULL, read_cmd, &ctx);
  // pthread_detach(read_cmd_t);

  pthread_mutex_init(&ctx.list.pt_lock, NULL);
  pthread_cond_init(&ctx.list.pt_signal, NULL);
  pthread_mutex_init(&ctx.state.lock, NULL);
  pthread_cond_init(&ctx.state.wait_cond, NULL);

  while (true) {
    while (ctx.list.queue_index >= ctx.list.queue_count)
      pthread_cond_wait(&ctx.list.pt_signal, &ctx.list.pt_lock);

    // for automatic next
    if (ctx.list.queue_index == ctx.list.queue_history_count) {
      int idx;

      if (ctx.state.shuffle)
        idx = get_rand() % ctx.list.queue_count;

      else
        idx = ctx.list.queue_index;

      // init new space for history
      ctx.list.queue_history = realloc(ctx.list.queue_history, sizeof(int) * (ctx.list.queue_history_count + 1));
      if (!ctx.list.queue_history) continue;

      ctx.list.queue_history[ctx.list.queue_history_count] = idx; // add
      ctx.list.queue_history_count++; // increase history count
      ctx.list.queue_index = ctx.list.queue_history_count - 1; // change index to last
    }

    int idx = ctx.list.queue_history[ctx.list.queue_index];
    char *src = strdup(ctx.list.queue_lists[idx]);

    ctx.list.src_type = extract_src_type(src);
    // printf("is src type is %d\n", ctx.list.src_type);
    playback_run(&ctx, src); // run now

    // if next
    if (ctx.state.skip_to_next == 1) {
      if (ctx.state.shuffle) {
        if (ctx.list.queue_index + 1 < ctx.list.queue_history_count)
          ctx.list.queue_index++;
        else
          ctx.list.queue_index = ctx.list.queue_history_count;
      } else {
        if (ctx.state.loop == LOOP_PLAYLIST)
          ctx.list.queue_index = (ctx.list.queue_index + 1) % ctx.list.queue_count;
        else
          ctx.list.queue_index++;
      }
    }


    // automatic next
    else {
      if (ctx.state.loop == LOOP_PLAYLIST)
        ctx.list.queue_index = (ctx.list.queue_index + 1) % ctx.list.queue_count;
      else
        ctx.list.queue_index++;
    }

    free(src);
    ctx.state.skip_to_next = 0;
  }

  return 0;
}
