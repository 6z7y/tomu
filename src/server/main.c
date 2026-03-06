#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "control.h"
#include "socket_utils.h"
#include "utils.h"

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);

  // 1. check args
  if (args_handle(argv) == 1) return 0;

  // 2. init socket
  int *server_fd = &ctx.server_fd;
  int *client_fd = &ctx.client_fd;

  socket_mode(1, server_fd); // socket on

  // 3. init client array
  Client_connection client[MAX_CLIENT] = {0};

  // 4. init poll
  struct pollfd fds[1 + MAX_CLIENT];
  fds[0] = (struct pollfd){ .fd = *server_fd, .events = POLLIN }; // [0] server for accept clients

  // 5. main loop (server will stay here)
  while(1) {

    // 6. add client active inside poll
    int nfds = add_client_into_poll(fds, client);

    // 7. start poll mode
    int ret = poll(fds, nfds, 80);

    if (ret < 0) { warn("Poll ret:"); continue; }

    // 8. broadcast status to all active clients on timeout
    if (ret == 0) {
      if (ctx.playback_active) {
        broadcast_status(client);
      }
    }

    // 9. new connection enter
    if (fds[0].revents & POLLIN) {
      accept_new_client(server_fd, client_fd, client);
    }

    // 10. read client keys
    int index = 1;
    for (int i=0; i<MAX_CLIENT; i++) {
      if (!client[i].active) continue;

      if (fds[index].revents & POLLIN) {
        handle_client_events(i, &index, fds,  client);
      }
      index++;
    }

  }

  // 11. close socket
  socket_mode(0, NULL);
  return 0;
}
