#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "DATA.h"
#include "mpris.h"
#include "utils1.h"
#include "utils2.h"

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

// Check if argument is a directory
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

// Check if argument is a file
static int is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return 0;
}

// handle command-line arguments
int args_handle(const char *option)
{
  // if (is_url(option)) {
  //     // printf("[args] Playing URL: %s\n", option);
  //     // char *path = strdup(option);
  //     // start_playback(path);
  //     // return 0;
  // }
  // else if (is_valid_path(option)) {
  //     printf("[args] Playing file: %s\n", option);
  //     char *path = strdup(option);
  //     start_playback(path);
  //     return 0;
  // }
  // else if (is_directory(option)) {
  //     printf("[args] Directory not yet supported: %s\n", option);
  //     return 1;
  // }
  // else if (is_valid_path(option)) {
  //     printf("[args] Playing: %s\n", option);
  //     char *path = strdup(option);
  //     start_playback(path);
  //     return 0;
  // }
  if (match_opt(option, help_opts)) {
      help();
      return 1;
  }
  else if (match_opt(option, ver_opts)) {
      printf("tomu: %s\n", TOMU_VER);
      return 1;
  }
  else {
      warn("tomu: unknown option '%s'\n\ntry '%s --help'", option, TOMU_NAME);
      // return 1;
  }
  return 0;
}
