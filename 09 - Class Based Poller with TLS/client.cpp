#include <cstdio>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "tcp_client_socket.hpp"
#include "tcp_stream.hpp"
#include "ssl_ctx.hpp"

#include "external/popl.hpp"

#include "match.hpp"
#include "utils.hpp"

using namespace jetblack::net;

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

    auto socket = std::make_shared<TcpClientSocket>();
    socket->connect(host, port);

    auto stream = TcpStream(socket, ssl_ctx, host);

    if (use_tls)
    {
      stream.do_handshake();
      stream.verify();
    }

    while (1)
    {
      std::fputs("Enter a message: ", stdout);
      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin) == nullptr)
      {
        break;
      }
      std::string message {buf, buf + (std::strlen(buf) - 1)};

      if (message == "SHUTDOWN")
      {
        bool is_shutdown = false;
        while (!is_shutdown)
        {
          print_line("shutting down");
          is_shutdown = stream.do_shutdown();
        }
        continue;
      }
      else if (message == "CLOSE")
      {
        print_line("closing");
        stream.socket->close();
        continue;
      }
      else if (message == "EXIT")
      {
        break;
      }

      print_line(std::format("Sending \"{}\"", message));
      bool write_ok = std::visit(
        match
        {
          [](eof&&)
          {
            print_line("eof");
            return false;
          },

          [](blocked&&)
          {
            print_line("block");
            return false;
          },

          [](ssize_t&& bytes_written) mutable
          {
            print_line(std::format("wrote {} bytes", bytes_written));
            return true;
          }            
        },
        stream.write(message));
      if (!write_ok)
      {
        print_line("write failed - quitting");
        break;
      }

      std::vector<char> read_buf;
      bool read_ok = std::visit(
        match
        {
          [](blocked&&)
          {
            print_line("read blocked");
            return false;
          },

          [](eof&&)
          {
            print_line("read eof");
            return false;
          },

          [&](std::vector<char>&& buf) mutable
          {
            std::string message {buf.begin(), buf.end()};
            print_line(std::format("read ok \"{}\"", message));
            read_buf = std::move(buf);
            return true;
          }
        },
        stream.read(1024));
      if (!read_ok)
      {
        print_line("read failed - quitting");
      }
    }

  }
  catch(const std::exception& e)
  {
    print_line(e.what());
  }
  
  return 0;
}