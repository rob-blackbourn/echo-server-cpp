#include <signal.h>

#include <cstdio>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>

#include "io/file.hpp"
#include "io/poller.hpp"
#include "io/file_poll_handler.hpp"
#include "io/tcp_client_socket.hpp"
#include "io/tcp_socket_poll_handler.hpp"
#include "io/tcp_stream.hpp"
#include "io/ssl_ctx.hpp"
#include "logging/log.hpp"
#include "utils/match.hpp"
#include "utils/utils.hpp"

#include "external/popl.hpp"

using namespace jetblack::io;
namespace logging = jetblack::logging;

namespace
{
  volatile std::sig_atomic_t last_signal = 0;
}

std::shared_ptr<SslContext> make_ssl_context(std::optional<std::string> capath)
{
  print_line("making ssl client context");
  auto ctx = std::make_shared<SslClientContext>();
  ctx->min_proto_version(TLS1_2_VERSION);
  if (capath.has_value())
  {
    print_line(std::format("Adding verify locations \"{}\"", capath.value()));
    ctx->load_verify_locations(capath.value());
  }
  else
  {
    print_line("setting default verify paths");
    ctx->set_default_verify_paths();
  }
  print_line("require ssl verification");
  ctx->verify();

  return ctx;
}

class Client : public PollClient
{
private:
  int client_fd_;

public:
  Client(int client_fd) : client_fd_(client_fd) {}

private:
  void on_startup(Poller& poller) override
  {
  }

  void on_interrupt(Poller& poller) override
  {
  }

  void on_open(Poller& poller, int fd, const std::string& host, std::uint16_t port) override
  {
    logging::info(std::format("on_open: {}", fd));
  }

  void on_close(Poller& poller, int fd) override
  {
    logging::info(std::format("on_close: {}", fd));
  }

  void on_read(Poller& poller, int fd, std::vector<std::vector<char>>&& bufs) override
  {
    logging::info(std::format("on_read: {}", fd));

    for (auto& buf : bufs)
    {
      std::string s {buf.begin(), buf.end()};
      logging::info(std::format("on_read: received {}", s));
      if (fd == STDIN_FILENO)
      {
        if (s == "CLOSE\n")
        {
          poller.close(client_fd_);
        }
        else
        {
          poller.write(client_fd_, buf);
        }
      }
      else if (fd == client_fd_)
      {
        poller.write(STDOUT_FILENO, buf);
      }
    }
  }

  void on_error(Poller& poller, int fd, std::exception error) override
  {
    logging::info(std::format("on_error: {}, {}", fd, error.what()));
  }
};

int main(int argc, char** argv)
{
  bool use_tls = false;
  std::uint16_t port = 22000;
  std::string host = "localhost";

  popl::OptionParser op("options");
  op.add<popl::Switch>("s", "ssl", "Connect with TLS", &use_tls);
  auto help_option = op.add<popl::Switch>("", "help", "produce help message");
  op.add<popl::Value<decltype(port)>>("p", "port", "port number", port, &port);
  op.add<popl::Value<decltype(host)>>("h", "host", "host name or ip address (use fqdn for tls)", host, &host);
  auto capath_option = op.add<popl::Value<std::string>>("", "capath", "path to certificate authority bundle file");

  try
  {
    op.parse(argc, argv);

    if (help_option->is_set())
    {
      if (help_option->count() == 1)
    		print_line(stderr, op.help());
	    else if (help_option->count() == 2)
		    print_line(stderr, op.help(popl::Attribute::advanced));
	    else
		    print_line(stderr, op.help(popl::Attribute::expert));
      exit(1);
    }

    std::optional<std::shared_ptr<SslContext>> ssl_ctx;
    
    if (use_tls)
    {
      std::optional<std::string> capath;
      if (capath_option->is_set())
        capath = capath_option->value();
      ssl_ctx = make_ssl_context(capath);
    }

    print_line(std::format(
      "connecting to host {} on port {}{}.",
      host,
      port,
      (use_tls ? " using tls" : "")));

    auto client_socket = std::make_shared<TcpClientSocket>();
    client_socket->connect(host, port);
    client_socket->blocking(false);

    auto poller = Poller(std::make_shared<Client>(client_socket->fd()));

    if (!ssl_ctx)
    {
      poller.add_handler(
        std::make_unique<TcpSocketPollHandler>(client_socket, 8096, 8096),
        host,
        port);
    }
    else
    {
      poller.add_handler(
        std::make_unique<TcpSocketPollHandler>(client_socket, *ssl_ctx, host, 8096, 8096),
        host,
        port);
    }

    auto console_input = std::make_shared<File>(STDIN_FILENO, O_RDONLY);
    console_input->blocking(false);
    poller.add_handler(std::make_unique<FilePollHandler>(console_input, 1024, 1024), "localhost", 1);

    auto console_output = std::make_shared<File>(STDOUT_FILENO, O_WRONLY);
    console_output->blocking(false);
    poller.add_handler(std::make_unique<FilePollHandler>(console_output, 1024, 1024), "localhost", 2);

    poller.event_loop(last_signal);
  }
  catch(const std::exception& error)
  {
    logging::error(std::format("Server failed: {}", error.what()));
  }
  catch (...)
  {
    logging::error("unknown error");
  }
 
  return 0;
}