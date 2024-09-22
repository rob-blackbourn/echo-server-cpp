#ifndef __tcp_listener_hpp
#define __tcp_listener_hpp

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "tcp_socket.hpp"
#include "tcp_client.hpp"

template<class TClient = tcp_client>
class tcp_listener : public tcp_socket
{
public:
  typedef std::shared_ptr<TClient> client_pointer;
  typedef std::function<client_pointer(int fd, const std::string& address, uint16_t port)> accept_callback;

public:
  tcp_listener()
    : tcp_socket()
  {
  }

  tcp_listener(int) = delete;

  void bind(uint16_t port) {
    uint32_t addr = htonl(INADDR_ANY);
    bind(addr, port);
  }

  void bind(const std::string& address, uint16_t port)
  {
    in_addr addr;
    int result = inet_pton(AF_INET, address.c_str(), &addr);
    if (result == 0) {
      throw std::runtime_error("invalid network address");
    } else {
      throw std::system_error(
        errno, std::generic_category(), "failed to parse network address");
    }

    bind(addr.s_addr, port);
  }

  void bind(uint32_t addr, uint16_t port)
  {
    sockaddr_in servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(port);

    if (::bind(_fd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
      throw std::system_error(
        errno, std::generic_category(), "failed to bind listener socket");
    }
  }

  void listen(int backlog = 10)
  {
    if (::listen(_fd, backlog) == -1) {
      throw std::system_error(
        errno, std::generic_category(), "failed to listen on bound socket");
    }
  }

  client_pointer accept(const accept_callback& make_client)
  {
    sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);

    int client_fd = ::accept(_fd, (sockaddr*)&clientaddr, &clientlen);
    if (client_fd == -1) {
      throw std::system_error(
        errno, std::generic_category(), "failed to accept socket");
    }

    char address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &clientaddr.sin_addr, address, sizeof(address)) == nullptr)
    {
      throw std::system_error(
        errno, std::generic_category(), "failed to find client address");
    }
    uint16_t port = ntohs(clientaddr.sin_port);

    return make_client(client_fd, address, port);
  }
};

#endif // __tcp_listener_hpp
