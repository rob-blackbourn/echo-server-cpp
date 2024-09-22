#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <utility>

#include "match.hpp"
#include "tcp.hpp"
#include "echo_server.hpp"
#include "chat_server.hpp"
#include "stream_out.hpp"

int
main(int argc, char** argv)
{
  const uint16_t port = 22000;

  auto server = chat_server(port);
  server.event_loop();

  return 0;
}
