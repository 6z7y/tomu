#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <signal.h>

#include "../shared/shared_control.h"
#include "../shared/share_utils.h"
#include "CLIENT_DATA.h"
#include "args.h"
#include "backend.h"
#include "config.h"
#include "control.h"
#include "file_handle.h"
#include "utils.h"

Client_CTX ctx = {0};

int main(int argc, char **argv)
{
    signal(SIGINT, sig_clean);

    init_context();
    int *server_fd = &ctx.server_fd;
    PlaybackQueue *queue = &ctx.queue;
    TomuStatus *status = &ctx.status;
    
    int first_file_sent = 0;
    int was_playing = 0;  // track previous playback_running state

    load_config();
    termios_mode(1);
    client_socket_mode(server_fd, 1);

    if (argc > 1) {
      if (args_handle(*server_fd, argc, argv)) goto bye;
      path_handle(*server_fd, argv[argc-1], queue);
    }

    struct pollfd fds[2];
    fds[0] = (struct pollfd){ .fd = STDIN_FILENO, .events = POLLIN };
    fds[1] = (struct pollfd){ .fd = *server_fd,   .events = POLLIN };

    char key[8];

    while (ctx.running) {
        int ret = poll(fds, 2, 80);
        if (ret < 0) continue;

        // Keyboard input
        if (fds[0].revents & POLLIN) {
            int n = read(STDIN_FILENO, key, sizeof(key) - 1);
            if (n > 0) {
                key[n] = '\0';
                handle_control(server_fd, key);
            }
        }

        // Server status update
        if (fds[1].revents & POLLIN) {
            int n = read(*server_fd, status, sizeof(*status));
            if (n == sizeof(*status)) {
                progress(status, status->position, status->duration);
                
                // Detect transition: was playing, now not playing
                if (was_playing && !status->playback_running) {
                    // Playback just finished -> send next file if in queue mode
                    if (queue->has_queue && queue->dir.totalFiles > 0) {
                        // Move to next file (or loop/shuffle)
                        queue->current_index++;
                        if (status->shuffle) {
                            queue->dir.rand_num = get_rand();
                            queue->current_index = queue->dir.rand_num % queue->dir.totalFiles;
                        } else if (queue->current_index >= queue->dir.totalFiles) {
                            if (status->loop)
                                queue->current_index = 0;
                            else
                                continue; // no more files
                        }
                        
                        char fullpath[2048];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s",
                                 queue->dir.base_path,
                                 queue->dir.files[queue->current_index]);
                        send_path(*server_fd, fullpath);
                        printf("\nPlaying: %s\n", queue->dir.files[queue->current_index]);
                    }
                }
                was_playing = status->playback_running;
                
            } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
                fprintf(stderr, "Server disconnected\n");
                break;
            }
        }

        // Send first file when server idle and we have a queue
        if (!status->playback_running && queue->has_queue && !first_file_sent && queue->dir.totalFiles > 0) {
            char fullpath[2048];
            if (status->shuffle) {
                queue->current_index = queue->dir.rand_num % queue->dir.totalFiles;
            } else {
                queue->current_index = 0;
            }
            snprintf(fullpath, sizeof(fullpath), "%s/%s",
                     queue->dir.base_path,
                     queue->dir.files[queue->current_index]);
            send_path(*server_fd, fullpath);
            printf("Playing: %s\n", queue->dir.files[queue->current_index]);
            first_file_sent = 1;
            was_playing = 0; // ensure transition detection works later
        }
    }

bye:
  queue_free(queue);
  termios_mode(0);
  client_socket_mode(server_fd, 0);
  return 0;
}
