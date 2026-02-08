#include <stdio.h>
#include <string.h>

#include "utils.h"

#define PROG_NAME "tomu"
#define PROG_VER "1.0.4"

int main(int argc, char *argv[])
{
  // 1. check user what want
  if ( argc == 1 ){
    printf("Usage: %s [FILE.mp3]\n", PROG_NAME); 
    return 0;
  }

  char *option = argv[1];
  char *path = argv[argc - 1];

  // 2. See what the user wants with "--" and handle it
  if ( argv[1][0] == '-' && argv[1][1] == '-' ){

    if ( strcmp("--loop", option ) == 0 ){
      path_handle(path, true, false);
      return 0;
    }

    else if ( strcmp("--shuffle", option) == 0 ){
      path_handle(path, false, true);
      return 0;
    }

    else if ( strcmp("--help", option) == 0 ){
      help();
      return 0;
    }

    else if ( strcmp("--version", option) == 0 ){
      printf("Tomu: %s\n", PROG_VER);
      return 0;
    }

    else {
      printf("[T]: unknown option '%s'\n", option);
      return 0;
    }
  }

  // 3. No options? Just handle the path (check file or directory)  
  path_handle(path, false, true);
  return 0;
}
