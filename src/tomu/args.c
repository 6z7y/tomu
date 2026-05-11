#include <stdio.h>
#include <string.h>

#include "../shared/SHARE_DATA.h"
#include "../shared/share_utils.h"

// arg compare opts
const char *help_opts[] = {"--help", "-h", NULL};       // help
const char *ver_opts[]  = {"--version", "-v", NULL}; // version

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
int args_handle(const char *option)
{
  if (!option) return 0; // no arguments given, continue normally

  // there "--arg"
  if (option[0] == '-') {

    if      (match_opt(option, help_opts)) help();

    else if (match_opt(option, ver_opts)) printf("tomu: %s\n", TOMU_VER);

    else    warn("[T]: unknown option '%s'\n\ntry '%s --help'", option, TOMU_NAME);

    return 1;
  }

  return 0;
}
