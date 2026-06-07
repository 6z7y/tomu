#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLIENT_DATA.h"
#include "../shared/share_utils1.h"

#define SIZE_BUF 64

const char *config_fmt = 
  "### Progress\n"
  "width: \"32\"\n"
  "done: \"=\"\n"
  "current: \">\"\n"
  "remaining: \".\"\n"
  "color: \"red\" # also have [red, cyan]"
;

void save_config(FILE **f, char *home_path, char *path_parent, char *path_file)
{
  // make dir first
  printf("Setting default config...\n");

  // write file
  *f = fopen(path_file, "w");
  if (!*f) die("config:");

  fprintf(*f, "%s", config_fmt);
//                                                                                      ^ NO \n at end

  fclose(*f);
  *f = fopen(path_file, "r");
}

void load_config()
{
  // 1. get home path
  char *home_path = getenv("HOME");

  char path_parent[128]; // file parent
    strcpy(path_parent, format("%s/.config/tomu", home_path));

  char path_file[128]; // file path
    strcpy(path_file, format("%s/.config/tomu/tomucli.conf", home_path));

  // 2. Read file
  FILE *f = fopen(path_file, "r");
  if (!f) {
    save_config(&f, home_path, path_parent, path_file);
  };

  // 3. init buffers
  char line[SIZE_BUF], key[SIZE_BUF], value[SIZE_BUF];

  // 4. loop read file config
  while(fgets(line, SIZE_BUF, f)) {
    // 5. trim new line
    line[strcspn(line, "\n")] = '\0'; // replace '\n' to '\0'

    // 6. skip empty line or comment
    if (line[0] == '\0' || line[0] == '#') continue;
    // 7. read line and split
    if (sscanf(line, "%[^:]: \"%[^\"]\"", key, value) == 2) {

      if (!strcmp(key, "width")) {
        ctx.cfg.progress.width = atoi(value);
      }

      else if (!strcmp(key, "done")) {
        strcpy(ctx.cfg.progress.done, value);
        ctx.cfg.progress.done[sizeof(ctx.cfg.progress.done) - 1] = '\0';

      }

      else if (!strcmp(key, "current")) {
        strcpy(ctx.cfg.progress.current, value);
        ctx.cfg.progress.current[sizeof(ctx.cfg.progress.current) - 1] = '\0';
      }

      else if (!strcmp(key, "remaining")) {
        strcpy(ctx.cfg.progress.remaining, value);
        ctx.cfg.progress.remaining[sizeof(ctx.cfg.progress.remaining) - 1] = '\0';
      }

      else if (!strcmp(key, "color")) {

        if      (!strcmp(value, "red")) strcpy(ctx.cfg.progress.color, RED);
        else if (!strcmp(value, "cyan")) strcpy(ctx.cfg.progress.color, CYN);

        ctx.cfg.progress.color[sizeof(ctx.cfg.progress.color) - 1] = '\0';
      }
    }
  }
  fclose(f);
}
