#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "DATA.h"
#include "args.h"
#include "socket_utils.h"
#include "utils.h"

PlayBackContext ctx = {0}; // tomu_context

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);

  ctx.queue_count = 0;

  if (args_handle(argv[argc - 1])) goto bye;

  // 1. init socket
  int *server_fd = &ctx.server_fd;
  int *client_fd = &ctx.client_fd;
  socket_mode(1, server_fd); // socket on

  // 2. init poll
  struct pollfd fds[1 + MAX_CLIENT]; // (0) = server | (1..MAX_CLIENT) = clients
  fds[0] = (struct pollfd){ *server_fd, POLLIN };

  // 3. init client array struct
  Client_connection client[MAX_CLIENT] = {0};

  // main loop (server will stay here)
  while(1) {
    // 4. add client active inside poll
    int nfds = add_client_into_poll(fds, client); // number clients active now

    // 5. start poll mode
    poll(fds, nfds, 100);

    // Accept new Client
    if (fds[0].revents & POLLIN) accept_new_client(server_fd, client_fd, client);

    client_checker_event(nfds, fds, client); // check event control

    if (ctx.state.running) {
      broadcast_status(client); // update status when play audio
    }
  }

  // close socket
  socket_mode(0, &ctx.server_fd);

bye: return 0;
}
