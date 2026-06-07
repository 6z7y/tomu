#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DATA.h"
#include "../shared/share_utils1.h"

#define SIZE_BUF 64

void save_config(FILE **f, char *home_path, char *path_parent, char *path_file)
{
  // make dir first
  printf("Setting default config...\n");

  run_command(format("mkdir -p %s/.config/tomu", home_path));

  // write file
  *f = fopen(path_file, "w");
  if (!*f) die("config:");

  fprintf(*f, "\ndiscord_rich_presence: '0' # 1=ON, 0=OFF");

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
    strcpy(path_file, format("%s/.config/tomu/tomu.conf", home_path));

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
    if (sscanf(line, "%[^:]: '%[^']'", key, value) == 2) {

      if (!strcmp(key, "discord_rich_presence")) {
        ctx.discord_rich_presence = atoi(value);
      }
    }
  }
  fclose(f);
}
