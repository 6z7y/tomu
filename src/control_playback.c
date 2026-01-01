#include "backend.h"
#include "control_playback.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

void help(){
  printf(
    "Usage: tomu [COMMAND] [PATH]\n"
    " Commands:\n\n"

    "   loop            : loop same sound\n"
    "   shuffle-loop    : select random file audio and loop\n"
    "   version         : show version of program\n"
    "   help            : show help message\n"

    "\nExample: tomu loop [FILE.mp3]\n"
  );
}

void shuffle(const char *path){
  srand(time(NULL));

  struct dirent *entry;

  DIR *dir = opendir(path);
  if (!dir ){
    perror("F: dir");
    exit(-1);
  }
  
  int count = 0;
  while ((entry = readdir(dir)) != NULL ){
    if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue;

    count++;
  }

  if (count == 0){
    perror("F: files");
    closedir(dir);
    exit(-1);
  }

  int index_rand = rand() % count;
  rewinddir(dir);

  for (int i = 0; i <= index_rand;){
    entry = readdir(dir);
    if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue;

    if (i == index_rand){
      char filename[277];
      snprintf(filename, sizeof(filename), "%s/%s", path, entry->d_name);
      scan_now(filename);
      break;
    }
    i++;
  }
}
