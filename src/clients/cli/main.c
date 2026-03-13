#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>

#include "backend.h"
#include "control.h"
#include "utils.h"
#include "../share_backend.h"
#include "../share_data.h"
#include "../../shared/shared_control.h"

int main(int argc, char **argv)
{
    int server_fd;
    PlaybackQueue queue = {0};

    // 1. connect to server
    socket_mode(1, &server_fd);

    // 2. enter raw terminal
    termios_mode(1);

    // Send client type
    ClientType my_type = CLIENT_CLI;
    write(server_fd, &my_type, sizeof(ClientType));

    // 3. if path given, send it to server
    if (argc == 2)
        path_handle(server_fd, argv[argc-1], &queue);

    TomuStatus status = {0};
    int playback_finished = 0;

    // 4. init poll
    struct pollfd fds[2];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    char key[8];

    // 5. main loop
    while(1) {
        int ret = poll(fds, 2, 1000);
        if (ret < 0) { continue; }  // FIX: was <= 0, timeout (0) was being eaten
                                    //
        if (ret == 0) {  // timeout = silence = song ended
            if (queue.has_queue && !playback_finished) {
                playback_finished = 1;
                handle_playback_complete(server_fd, &queue);
            }
            continue;
        }

        if (fds[0].revents & POLLIN) {
            TomuStatus tmp;
            int n;
            while ((n = read(server_fd, &tmp, sizeof(tmp))) == (int)sizeof(tmp)) {
                status = tmp;
                playback_finished = 0;  // ← ADD THIS: server talking = song alive, reset flag

                struct pollfd pfd = { .fd = server_fd, .events = POLLIN };
                if (poll(&pfd, 1, 0) <= 0) break;
            }
            if (n == 0 || (n < 0 && errno != EAGAIN)) {
                fprintf(stderr, "Server disconnected\n");
                break;
            }

            progress(&status, status.position, status.duration);
        }

        if (fds[1].revents & POLLIN) {
            int n = read(STDIN_FILENO, key, sizeof(key) - 1);
            if (n > 0) {
                key[n] = '\0';
                handle_control(server_fd, key);

                if (!strcmp(key, "\n") || !strcmp(key, ">")) {
                    if (queue.has_queue)
                        handle_playback_complete(server_fd, &queue);
                }
                else if (!strcmp(key, "<")) {
                    if (queue.has_queue) {
                        queue.dir.currentFile--;
                        if (queue.dir.currentFile < 0)
                            queue.dir.currentFile = queue.dir.totalFiles - 1;
                        send_next_from_queue(server_fd, &queue);
                    }
                }
            }
        }
    }

    termios_mode(0);
    socket_mode(0, &server_fd);

    if (queue.has_queue) {
        for (int i = 0; i < queue.dir.totalFiles; i++)
            free(queue.dir.files[i]);
        free(queue.dir.files);
    }

    return 0;
}
