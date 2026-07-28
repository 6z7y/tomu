#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../../libs/arg_match.h"
#include "errors.h"
#include "playlist.h"
#include "macros.h"
#include "structs.h"

void help()
{
  printf(
    "usage: tomu [options] [file|url]\n\n"
    "options:\n"
    "  -h, --help          show this help message\n"
    "  -v, --version       show version information\n"
    "  --shuffle           enable shuffle\n"
    "  --no-shuffle        disable shuffle\n"
    "  --loop none|track|playlist  set loop mode\n\n"
    "examples:\n"
    "  tomu song.mp3\n"
    "  tomu /path/to/music/\n"
    "  tomu --shuffle --loop playlist /path/to/music/\n"
    "  tomu https://youtu.be/abc123\n"
  );
}

void args_handle(PlayBackContext *ctx, int argc, char **argv)
{
  const char *loop_opts[]     = {"--loop", "loop", "-l", NULL};
  const char *shuffle_opts[]  = {"--shuffle", "shuffle", "-s", NULL};
  const char *loop_vals[]     = {"none", "track", "playlist", NULL};
  const char *help_opts[]     = {"--help", "help", "-h", NULL};
  const char *ver_opts[]      = {"--version", "version", "-v", NULL};

  ctx->state.shuffle = 0;
  ctx->state.loop = LOOP_NONE;
  ctx->list.filter_files = 1;

  for (int i = 1; i < argc; i++) {
    if (arg_check_opts(argc, argv, loop_opts)) {
      switch(arg_match_opts_with_values(&argc, argv, loop_opts, loop_vals)) {
        case 0: ctx->state.loop = LOOP_NONE; break;
        case 1: ctx->state.loop = LOOP_TRACK; break;
        case 2: ctx->state.loop = LOOP_PLAYLIST; break;
        case -1: die("tomu: unknown loop mode '%s' try (none|track|playlist)", argv[i+1]); break;
        default: ctx->state.loop = LOOP_NONE;
      }
    }
    if (arg_match_opts(&argc, argv, shuffle_opts) == 1) ctx->state.shuffle = 1;
    if (arg_match_opt(&argc, argv, "--no-filter") == 1) ctx->list.filter_files = 0;
    if (arg_match_opts(&argc, argv, help_opts) == 1) { help(); exit(0); }
    if (arg_match_opts(&argc, argv, ver_opts) == 1)  { printf("tomu: %s\n", TOMU_VER); exit(0); }
  }

  const char *src = argv[1];

  if (src) {
    if (src_handle(ctx, src) < 0) die("invalid '%s':", src);
  }
}
