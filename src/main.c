#include <stdio.h>
#include <string.h>

// #include "control.h"
#include "backend.h"
#include "utils.h"

#define PROG_NAME "tomu"
#define PROG_VER "0.0.11"

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
      path_handle(path, true);
      return 0;
    }

    else if ( strcmp("--shuffle", option) == 0 ){
      DirFiles.shuffle = true; // TODO mv this later
      path_handle(path, false);
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
  DirFiles.shuffle = true; // TODO mv this later
  path_handle(path, false);
  return 0;
}
