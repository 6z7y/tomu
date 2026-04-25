#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLIENT_DATA.h"
#include "../shared/share_utils.h"

#define SIZE_BUF 64


// void mkdir_p(char *path) {
//     for (char *p = path + 1; *p; p++) {
//         if (*p == '/') {
//             *p = '\0';
//             mkdir(path, 0755);
//             *p = '/';
//         }
//     }
//     mkdir(path, 0755);
// }

void save_config(FILE **f, char *home_path, char *cfg_path_parent, char *cfg_path_file)
{
  // make dir first
  printf("set deafult config\n");

  run_command(format("mkdir -p %s/.config/tomucli", home_path));

  // write file
  *f = fopen(cfg_path_file, "w");
  fprintf(*f, "### Progress\ndone: =\ncurrent: >\nremaining: .");

  rewind(*f);
}

void load_config()
{
  // 1. get home path
  char *home_path = getenv("HOME");

  char cfg_path_parent[64];
  char cfg_path_file[64];
  snprintf(cfg_path_parent, sizeof(cfg_path_parent), "%s/.config/tomucli", home_path);
  snprintf(cfg_path_file, sizeof(cfg_path_file), "%s/.config/tomucli/config.conf", home_path);


  // 2. Read file
  FILE *f = fopen(cfg_path_file, "r");

  if (!f) save_config(&f, home_path, cfg_path_parent, cfg_path_file);

  // 3. init buffers
  char line[SIZE_BUF], key[SIZE_BUF], value[SIZE_BUF];

  // 4. loop read file config
  while(fgets(line, SIZE_BUF, f)) {
    // 5. trim new line
    line[strcspn(line, "\n")] = '\0'; // replace '\n' to '\0'

    // 6. skip empty line or comment
    if (line[0] == '\0' || line[0] == '#') continue;

    // 7. read line and split
    if (sscanf(line, "%[^:]: %s", key, value) == 2) {

      if (!strcmp(key, "done")) {
        // printf("1key: (%s), with value: (%s)\n", key, value);
        client_ctx.cfg.progress.done = value[0];
      }

      if (!strcmp(key, "current")) {
        // printf("2key: (%s), with value: (%s)\n", key, value);
        client_ctx.cfg.progress.current = value[0];
      }

      if (!strcmp(key, "remaining")) {
        // printf("3key: (%s), with value: (%s)\n", key, value);
        client_ctx.cfg.progress.remaining = value[0];
      }
    }
  }
  fclose(f);
}
