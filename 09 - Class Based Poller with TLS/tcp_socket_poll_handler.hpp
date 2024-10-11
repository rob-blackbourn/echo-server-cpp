#ifndef JETBLACK_NET_SOCKET_POLL_HANDLER_HPP
#define JETBLACK_NET_SOCKET_POLL_HANDLER_HPP

#include <poll.h>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "tcp_socket.hpp"
#include "tcp_listener_socket.hpp"
#include "tcp_stream.hpp"

#include "ssl_ctx.hpp"

#include "match.hpp"

#include "poll_handler.hpp"
#include "poller.hpp"

namespace jetblack::net
{
  class TcpSocketPollHandler : public PollHandler
  {
  private:
    TcpStream stream_;
    std::deque<std::vector<char>> read_queue_;
    std::deque<std::pair<std::vector<char>, std::size_t>> write_queue_;

  public:
    const std::size_t read_bufsiz;
    const std::size_t write_bufsiz;

    TcpSocketPollHandler(
      std::unique_ptr<TcpSocket> socket,
      std::size_t read_bufsiz,
      std::size_t write_bufsiz)
      : stream_(std::move(socket), false),
        read_bufsiz(read_bufsiz),
        write_bufsiz(write_bufsiz)
    {
    }
    TcpSocketPollHandler(
      std::unique_ptr<TcpSocket> socket,
      std::shared_ptr<SslContext> ssl_ctx,
      std::size_t read_bufsiz,
      std::size_t write_bufsiz)
      : stream_(std::move(socket), ssl_ctx, false),
        read_bufsiz(read_bufsiz),
        write_bufsiz(write_bufsiz)
    {
    }
    TcpSocketPollHandler(
      std::unique_ptr<TcpSocket> socket,
      std::shared_ptr<SslContext> ssl_ctx,
      const std::string& server_name,
      std::size_t read_bufsiz,
      std::size_t write_bufsiz)
      : stream_(std::move(socket), ssl_ctx, server_name),
        read_bufsiz(read_bufsiz),
        write_bufsiz(write_bufsiz)
    {
    }
    ~TcpSocketPollHandler() override
    {
    }

    bool is_listener() const noexcept override { return false; }
    int fd() const noexcept override { return stream_.socket->fd(); }
    bool is_open() const noexcept override { return stream_.socket->is_open(); }
    bool want_read() const noexcept override { return is_open() || stream_.want_read(); }
    bool want_write() const noexcept override { return is_open() && (!write_queue_.empty() || stream_.want_write()); }

    bool read(Poller& poller) noexcept override
    {
      try
      {
        bool can_read = true;
        while (can_read && stream_.socket->is_open())
        {
          can_read = std::visit(match {
            
            [](blocked&&)
            {
              return false;
            },

            [](eof&&)
            {
              return false;
            },

            [&](std::vector<char>&& buf) mutable
            {
              read_queue_.push_back(std::move(buf));
              return true;
            }

          },
          stream_.read(read_bufsiz));
        }
      }
      catch (...)
      {
        stream_.socket->is_open(false);
        throw;
      }

      return stream_.socket->is_open();
    }

    bool write() override
    {
      try
      {
        bool can_write = true;
        while (can_write && stream_.socket->is_open() && !write_queue_.empty())
        {

          auto& [orig_buf, offset] = write_queue_.front();
          std::size_t count = std::min(orig_buf.size() - offset, write_bufsiz);
          const auto& buf = std::span<char>(orig_buf).subspan(offset, count);

          can_write = std::visit(match {
            
            [](eof&&)
            {
              return false;
            },

            [](blocked&&)
            {
              return false;
            },

            [&](ssize_t&& bytes_written) mutable
            {
              // Update the offset reference by the number of bytes written.
              offset += bytes_written;
              // Are we there yet?
              if (offset == orig_buf.size()) {
                // The buffer has been completely used. Remove it from the
                // queue.
                write_queue_.pop_front();
              }
              return true;
            }
            
          },
          stream_.write(buf));
        }

        if (stream_.socket->is_open() && stream_.want_write())
        {
          // Some SSL writes are pending. Typically at shutdown.
          stream_.write(std::span<char> {});
        }
      }
      catch(const std::exception& e)
      {
        stream_.socket->is_open(false);
        throw;
      }      

      return stream_.socket->is_open();
    }

    void close() override
    {
      if (stream_.socket->is_open())
      {
        stream_.socket->close();
      }
    }

    bool has_reads() const noexcept { return !read_queue_.empty(); }

    std::optional<std::vector<char>> dequeue() noexcept override
    {
      if (read_queue_.empty())
        return std::nullopt;

      auto buf { std::move(read_queue_.front()) };
      read_queue_.pop_front();
      return buf;
    }

    void enqueue(const std::vector<char>& buf) noexcept override
    {
      write_queue_.push_back(std::make_pair(buf, 0));
    }
  };
}

#endif // JETBLACK_NET_SOCKET_POLL_HANDLER_HPP
