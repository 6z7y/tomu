#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "control.h"
#include "socket_utils.h"
#include "utils.h"
#include "audio_data.h"

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);

      ctx.queue_count = 0;

  // 1. check args
  if (args_handle(argv) == 1) goto bye;

  // 2. init socket
  int *server_fd = &ctx.server_fd;
  int *client_fd = &ctx.client_fd;

  socket_mode(1, server_fd); // socket on

  // 3. init client array
  Client_connection client[MAX_CLIENT] = {0};

  // 4. init poll
  struct pollfd fds[1 + MAX_CLIENT];
  memset(fds, 0, sizeof(fds));
  fds[0].fd = *server_fd;
  fds[0].events = POLLIN;

  // 5. main loop (server will stay here)
  while(1) {
    // 6. add client active inside poll
    int nfds = add_client_into_poll(fds, client);

    // 7. start poll mode
    int ret = poll(fds, nfds, 100); // 100ms timeout

    if (ret < 0) { 
      if (errno == EINTR) continue; // Interrupted by signal
      warn("Poll ret:"); 
      continue; 
    }

    // 8. broadcast status to all active clients on timeout
    if (ret == 0) {
      if (ctx.playback_active) broadcast_status(client);
      continue;
    }

    // 9. new connection enter
    if (fds[0].revents & POLLIN) accept_new_client(server_fd, client_fd, client);

    // 10. read client keys
    int index = 1; // begin from 1 for clients, server take 0
    for (int i=0; i<MAX_CLIENT; i++) {
      if (!client[i].active) continue;

      if (index < nfds && fds[index].revents & POLLIN) {
        handle_client_events(i, fds, client);
      }
      index++;
    }
  }


  // 11. close socket
  socket_mode(0, &ctx.server_fd);

bye:
  return 0;
}
