#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "errors.h"
#include "file_handle.h"
#include "macros.h"
#include "structs.h"
#include "utils.h"

// arg compare opts
const char *help_opts[] = {"--help", "help", "-h", NULL};       // help
const char *ver_opts[]  = {"--version", "version", "-v", NULL}; // version

void help()
{
  printf(
      "Usage: tomu [OPTIONS] [FILE|URL]\n\n"
      "Options:\n"
      "  -h, --help      Show this help message\n"
      "  -v, --version   Show version information\n\n"
      "Examples:\n"
      "  tomu song.mp3                    Play a local file\n"
      "  tomu /path/to/music/             Play all music in directory\n"
      "  tomu https://youtu.be/abc123     Play a YouTube URL\n"
      "  tomu https://soundcloud.com/...  Play a SoundCloud URL\n"
      "  tomu https://example.com/audio.mp3 Play a direct audio URL\n"
  );
}

int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

// handle command-line arguments
void args_handle(const char *option, const char *src)
{
  int result_src_checking = handle_src(src);
  if (result_src_checking > 0) return;

  else if (match_opt(option, help_opts)) help();

  else if (match_opt(option, ver_opts)) printf("tomu: %s\n", TOMU_VER);

  else 
    warn("tomu: unknown option '%s'\n  try '%s --help'", option, TOMU_NAME);

  exit(0);
}
