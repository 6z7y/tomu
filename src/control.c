#include "backend.h"
#include "control.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>

void *control_place(void *arg){
  PlayBackState *state = (PlayBackState*)arg;
  
  struct termios old, raw;
  char c;

  tcgetattr(STDIN_FILENO, &old);
  raw = old;

  raw.c_lflag &= ~(ICANON | ECHO);
  // raw.c_cc[VMIN] = 0;  // Non-blocking: don't wait for any characters
  // raw.c_cc[VTIME] = 0; // Non-blocking: no timeout

  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  printf("\033[?25l");   // hide cursor
  fflush(stdout);
  
  while (state->running){

    int n = read(STDIN_FILENO, &c, 1);
    if (n > 0 ){
      if (c == 'q'){
        playback_stop(state);
        usleep(300000);
        break;

      } else if (c == 'p'){
        if (state->paused) {
          playback_resume(state);
        } else {
          playback_pause(state);
        }
      } else if (c == 0){
        usleep(100000);
      }
    } else if (n == 0){
      usleep(100000);
    }
  }
  
  tcsetattr(STDIN_FILENO, TCSANOW, &old);
  return NULL;
}

void playback_pause(PlayBackState *state){
  pthread_mutex_lock(&state->lock);
  state->paused = 1;
  pthread_mutex_unlock(&state->lock);
}

void playback_resume(PlayBackState *state){
  pthread_mutex_lock(&state->lock);
  state->paused = 0;
  pthread_cond_signal(&state->waitKudasai);
  pthread_mutex_unlock(&state->lock);
}

void playback_stop(PlayBackState *state){
  pthread_mutex_lock(&state->lock);
  state->paused = 0;
  state->running = 0;
  pthread_cond_signal(&state->waitKudasai);
  pthread_mutex_unlock(&state->lock);
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

