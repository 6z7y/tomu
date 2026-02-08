#include <stdio.h>
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>

#include "backend.h"
#include "control.h"
#include "utils.h"

struct keybinding { const char *key; void (*handler)(Audio_State*); };

#define CTRL_KEY(key) (const char[]){key - 'a' + 1 , '\0'} // remove this when you move the code to socket function

static const struct keybinding keybindings[] = {
    {"\n"    ,       playback_next_audio}, // enter
    {"\177"  ,       playback_speed_defualt}, // backspace
    {" "     ,       playback_toggle},
    {"q"     ,       playback_stop},
    {"-"     ,       volume_decrease},
    {"+"     ,       volume_increase},
    {"\x1b[A",       seek_forward_min}, // Up arrow
    {"\x1b[B",     	 seek_backward_min}, // Down arrow
    {"\x1b[D",       seek_backward_sec},   // Left arrow
    {"\x1b[C",       seek_forward_sec},    // Right arrow
    {"["     ,       playback_speed_decrease},
    {"]"     ,       playback_speed_increase},
    {"l"     ,       loop_toggle},
    {"s"     ,       shuffle_toggle},
    {">"     ,       playback_next_audio},
    {"<"     ,       playback_prev_audio}
};


static const int kbds_len = sizeof(keybindings) / sizeof(struct keybinding);

// For interactive player
void *handle_input(void *arg){
  Audio_State *state = (Audio_State*)arg;

  struct termios old, raw;

  tcgetattr(STDIN_FILENO, &old);
  raw = old;

  raw.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  printf("\033[?25l"); // hide cursor
  fflush(stdout);

  struct pollfd pfd = {
    .fd = STDIN_FILENO,
    .events = POLLIN
  };

  while (state->running){
    // wait 80ms for input
    int ret = poll(&pfd, 1, 80);

    if (!state->running) break;

    if (ret > 0 && (pfd.revents & POLLIN)) {
        char key_buf[4] = {0}; // for escape sequences

        // key press
        int n = read(STDIN_FILENO, key_buf, 1);
        // n shouldn't be zero since we polled successfully
        if (n < 0) { perror("read"); break; }; 

        // check if we have an escape sequence and ready bytes.
        if (key_buf[0] == '\x1b') {
            // TODO: remove the magic number
            int ret = poll(&pfd, 1, -1); // we're not sure of the sequence's size and we don't want to block

            if (ret < 0) {
                perror("poll ecsape sequence");
                break;
            } // fuck off on errors
            if (ret == 1 && (pfd.revents & POLLIN)) {
                int n = read(STDIN_FILENO, key_buf + 1, sizeof(key_buf) - 1);
                if (n < 0) {
                    perror("read escape sequence");
                    break;
                }
            }
        }

        // now we just find the proper keybinding..
        // a hashmap should be used here but allocating mem here is overkill
        for (uint i = 0; i < kbds_len; i++) {
            if (strcmp(key_buf, keybindings[i].key) == 0) keybindings[i].handler(state); 

        }

        if (!state->running) break; // leave if there's nothing playing
    }

    else if (ret == 0) {
      continue;
    }

    else 
      perror("[F] poll error");
  }

  printf("\033[?25h\r"); // show cursor
  fflush(stdout);

  tcsetattr(STDIN_FILENO, TCSANOW, &old);
  return NULL;
}

// =================================================================

// control playback (stop/resume/stop)

// functions for playback
// fn toggle pause/resume
inline void playback_toggle(Audio_State *state) {
    if (state->paused)
        playback_resume(state);
    else 
        playback_pause(state);
}

inline void playback_pause(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->paused = 1;
  pthread_mutex_unlock(&state->lock);
}

inline void playback_resume(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->paused = 0;
    pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);
}

// Stops playback and wakes any waiting threads
inline void playback_stop(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->paused = 0;
    state->running = 0;
    // state->shuffle = 0;
    // state->looping = 0;
    pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);
  KeepPlayingDirectory = false;
}

// =================================================================

// control audio seek

// Requests a seek forward by 5 seconds
void seek_forward_sec(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = +5000000; // +5 sec in microseconds
      pthread_cond_broadcast(&state->wait_cond);
    }
  pthread_mutex_unlock(&state->lock);
}


// Requests a seek forward by 1 min
void seek_forward_min(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = +60000000; // +60 sec in microseconds
      pthread_cond_broadcast(&state->wait_cond);
    }
  pthread_mutex_unlock(&state->lock);
}

// Requests a seek backward by 5 seconds
void seek_backward_sec(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = -5000000; // -5 sec in microseconds
      pthread_cond_broadcast(&state->wait_cond);
    }
  pthread_mutex_unlock(&state->lock);
}

// Requests a seek backward by 1 min
void seek_backward_min(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = -60000000; // -60 sec in microseconds
      pthread_cond_broadcast(&state->wait_cond);
    }
  pthread_mutex_unlock(&state->lock);
}

// =================================================================

// control speed playback
void playback_speed_defualt(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    state->speed = 1.00f;
  pthread_mutex_unlock(&state->lock);
}


void playback_speed_increase(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    state->speed += 0.05f;
    if (state->speed > 2.00f) state->speed = 2.00f;
  pthread_mutex_unlock(&state->lock);
}

void playback_speed_decrease(Audio_State *state)
{
  pthread_mutex_lock(&state->lock);
    state->speed -= 0.05f;
    if (state->speed < 0.25f) state->speed = 0.25f;
  pthread_mutex_unlock(&state->lock);
}

// =================================================================

// control volume playback

inline void volume_increase(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->volume += 0.02f;
    if (state->volume > 1.26f) state->volume = 1.26f;
  pthread_mutex_unlock(&state->lock);
}

inline void volume_decrease(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->volume -= 0.02f;
    if (state->volume < 0.00f) state->volume = 0.00f;
  pthread_mutex_unlock(&state->lock);
}
// ===================================================================

void change_Audio(Audio_State *state){
    pthread_mutex_lock(&state->lock);
    state->running = 0;
    state->paused = 0;
    pthread_cond_broadcast(&state->wait_cond);
    pthread_mutex_unlock(&state->lock);
    
    // Note: KeepPlayingDirectory stays 1 (true) by default, 
    // so utils.c knows to play the next file
}
void loop_toggle(Audio_State *state) {
    if (state->looping) loop_false(state);
    else loop_true(state);
}

inline void loop_true(Audio_State *state){
  pthread_mutex_lock(&state->lock);
  state->looping = true;
  DirFiles.DirLoopStop = false;
  pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);
}

inline void loop_false(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    state->looping = false;
    DirFiles.DirLoopStop = true;
  pthread_mutex_unlock(&state->lock);
}

void shuffle_toggle(Audio_State *state) {
    if (state->shuffle) shuffle_false(state);
    else shuffle_true(state);
}

inline void shuffle_true(Audio_State *state){
  pthread_mutex_lock(&state->lock);
  // state->shuffle = true;
  state->shuffle = true;
  pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);
}

inline void shuffle_false(Audio_State *state){
  pthread_mutex_lock(&state->lock);
    // state->shuffle = false;
    state->shuffle = false;
  pthread_mutex_unlock(&state->lock);
}

void stopAndShuffle(Audio_State* state){
  do_shuffle(); 
  change_Audio(state);
}

void do_shuffle(){
  srand(time(NULL));
  if(DirFiles.totalFiles > 0)
    DirFiles.currentFile = rand() % (DirFiles.totalFiles);
}

void next(Audio_State *state){
  DirFiles.currentFile++;
  if (DirFiles.currentFile >= DirFiles.totalFiles)
    DirFiles.currentFile = 0; 
    
  change_Audio(state); 
}

void prev(Audio_State *state){
  DirFiles.currentFile--;
  if (DirFiles.currentFile < 0)
    DirFiles.currentFile = DirFiles.totalFiles-1; 

  change_Audio(state); 
}

inline void playback_next_audio(Audio_State *state){
  if(state->shuffle)
    stopAndShuffle(state);
  else 
    next(state);
}

inline void playback_prev_audio(Audio_State *state){
  if(state->shuffle)
    stopAndShuffle(state);
  else
   prev(state);
}
