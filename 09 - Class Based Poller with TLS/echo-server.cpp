#include <signal.h>

#include <cstdio>
#include <set>

#include <spdlog/spdlog.h>

#include "tcp.hpp"
#include "poller.hpp"
#include "tcp_listener_socket_poll_handler.hpp"
#include "ssl_ctx.hpp"
#include "utils.hpp"

#include "external/popl.hpp"

using namespace jetblack::net;

void eputs(const std::string& message)
{
  std::fputs(message.c_str(), stderr);
}

std::shared_ptr<SslContext> make_ssl_context(const std::string& certfile, const std::string& keyfile)
{
  spdlog::info("making ssl server context");
  auto ctx = std::make_shared<SslServerContext>();
  ctx->min_proto_version(TLS1_2_VERSION);
  spdlog::info("Adding certificate file \"{}\"", certfile);
  ctx->use_certificate_file(certfile);
  spdlog::info("Adding key file \"{}\"", keyfile);
  ctx->use_private_key_file(keyfile);
  return ctx;
}

int main(int argc, char** argv)
{
  // signal(SIGPIPE,SIG_IGN);

  bool use_tls = false;
  uint16_t port = 22000;
  popl::OptionParser op("options");
  op.add<popl::Switch>("s", "ssl", "Connect with TLS", &use_tls);
  auto help_option = op.add<popl::Switch>("", "help", "produce help message");
  op.add<popl::Value<decltype(port)>>("p", "port", "port number", port, &port);
  auto certfile_option = op.add<popl::Value<std::string>>("c", "certfile", "path to certificate file");
  auto keyfile_option = op.add<popl::Value<std::string>>("k", "keyfile", "path to key file");

  try
  {
    op.parse(argc, argv);

    if (help_option->is_set())
    {
      if (help_option->count() == 1)
        eputs(op.help());
	    else if (help_option->count() == 2)
		    eputs(op.help(popl::Attribute::advanced));
	    else
		    eputs(op.help(popl::Attribute::expert));
      exit(1);
    }

    spdlog::info(
      "starting echo server on port {}{}.",
      static_cast<int>(port),
      (use_tls ? " with TLS" : ""));

    std::optional<std::shared_ptr<SslContext>> ssl_ctx;

    if (use_tls)
    {
      if (!certfile_option->is_set())
      {
        eputs("For ssl must use certfile");
        eputs(op.help());
        exit(1);
      }
      if (!keyfile_option->is_set())
      {
        eputs("For ssl must use keyfile");
        eputs(op.help());
        exit(1);
      }
      ssl_ctx = make_ssl_context(certfile_option->value(), keyfile_option->value());
    }

    auto poller = Poller(
      [](Poller&, int fd)
      {
        spdlog::info("on_open: {}", fd);
      },
      [](Poller&, int fd)
      {
        spdlog::info("on_close: {}", fd);
      },
      [](Poller& poller, int fd, std::vector<std::vector<char>> bufs)
      {
        spdlog::info("on_read: {}", fd);

        for (auto& buf : bufs)
        {
          spdlog::info("on_read: received {}", to_string(buf));
          poller.write(fd, buf);
        }
      },
      [](Poller&, int fd, std::exception error)
      {
        spdlog::info("on_error: {}, {}", fd, error.what());
      }
    );
    poller.add_handler(std::make_unique<TcpListenerSocketPollHandler>(port, ssl_ctx));
    poller.event_loop();
  }
  catch(const std::exception& error)
  {
    spdlog::error("Server failed: {}", error.what());
  }
  catch (...)
  {
    spdlog::error("unknown error");
  }

  spdlog::info("server stopped");

  return 0;
}
