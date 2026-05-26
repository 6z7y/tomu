#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shared/SHARE_DATA.h"
#include "../shared/share_utils.h"

// arg compare opts
const char *help_opts[] = {"--help", "help", "-h", NULL};       // help
const char *ver_opts[]  = {"--version", "version", "-v", NULL}; // version

void help()
{
  printf(
      "Usage: tomu\n\n"

      " TODO: Later\n"
  );
}

int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

// handle command-line arguments
void args_handle(const char *option)
{
  if      (match_opt(option, help_opts)) help();

  else if (match_opt(option, ver_opts)) printf("tomu: %s\n", TOMU_VER);

  else    warn("tomu: unknown option '%s'\n\ntry '%s --help'", option, TOMU_NAME);

  exit(0);
}
