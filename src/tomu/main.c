#include <unistd.h>
#include <signal.h>
#include <poll.h>

#include "DATA.h"
#include "args.h"
#include "config.h"
#include "socket_utils.h"

PlayBackContext ctx = {0}; // tomu_context

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);
  signal(SIGPIPE, SIG_IGN); // don't crash on write to closed socket

  // discord_init();  // ADD THIS - connects to Discord
  if (argc > 1) args_handle(argv[argc - 1]); // argument handle

  // init config folder
  load_config();
  if (ctx.discord_rich_presence) {
    run_command("python3 ~/.config/tomu/discord_rich_presence.py &");
  }

  // 1. init socket
  int *server_fd = &ctx.server_fd;
  int *client_fd = &ctx.client_fd;
  server_socket_mode(server_fd, 1); // socket on

  // 2. init poll
  struct pollfd fds[1 + MAX_CLIENT]; // (0) = server | (1..MAX_CLIENT) = clients
  fds[0] = (struct pollfd){ *server_fd, POLLIN }; // setup server

  // 3. init client array struct
  CLIENTS_SYSTEM *client = ctx.client;

  while(1) { // main loop (server will stay here)
    // 4. add client active inside poll
    int nfds = add_client_into_poll(fds, client); // number clients active now

    // 5. start poll mode
    poll(fds, nfds, 100);

    if (ctx.state.running) {
      broadcast_status(client); // update status when play audio
      // discord_update_presence();  // ADD THIS - updates Discord when song changes
    }

    // fd[0] Accept new Client
    if (fds[0].revents & POLLIN) accept_new_client(server_fd, client_fd, client);

    // fd[1] read keys
    int index = 1; // begin from 1 for clients, server take 0
    for_each_num(MAX_CLIENT) {
      if (fds[index].revents & POLLIN && index < nfds && client[i].active) 
          handle_client_events(i, fds, client);

      index++;
    }
    sleep_ms(100);
  }

bye: 
  return 0;
}
