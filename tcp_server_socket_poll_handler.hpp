#ifndef JETBLACK_NET_SERVER_SOCKET_POLL_HANDLER_HPP
#define JETBLACK_NET_SERVER_SOCKET_POLL_HANDLER_HPP

#include <poll.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

#include "tcp_socket.hpp"
#include "tcp_listener_socket.hpp"
#include "tcp_stream.hpp"

#include "match.hpp"

#include "poll_handler.hpp"
#include "poller.hpp"

namespace jetblack::net
{
  class TcpServerSocketPollHandler : public PollHandler
  {
  private:
    TcpStream stream_;
    typedef TcpStream::buffer_type buffer_type;
    std::deque<buffer_type> read_queue_;
    std::deque<std::pair<buffer_type, std::size_t>> write_queue_;

  public:
    const std::size_t read_bufsiz;
    const std::size_t write_bufsiz;

    TcpServerSocketPollHandler(
      std::shared_ptr<TcpSocket> socket,
      std::size_t read_bufsiz,
      std::size_t write_bufsiz)
      : stream_(socket),
        read_bufsiz(read_bufsiz),
        write_bufsiz(write_bufsiz)
    {
    }
    ~TcpServerSocketPollHandler() override
    {
    }

    bool is_listener() const noexcept override { return false; }
    int fd() const noexcept override { return stream_.socket->fd(); }
    bool is_open() const noexcept override { return stream_.socket->is_open(); }
    bool want_read() const noexcept override { return stream_.socket->is_open(); }
    bool want_write() const noexcept override { return stream_.socket->is_open() && !write_queue_.empty(); }

    bool read(Poller& poller) noexcept override
    {
      bool ok = stream_.socket->is_open();
      while (ok) {
        ok = std::visit(match {
          
          [](blocked&&)
          {
            return false;
          },

          [](eof&&)
          {
            return false;
          },

          [&](buffer_type&& buf) mutable
          {
            read_queue_.push_back(std::move(buf));
            return stream_.socket->is_open();
          }

        },
        stream_.read(read_bufsiz));
      }
      return stream_.socket->is_open();
    }

    bool write() noexcept override
    {
      bool can_write = stream_.socket->is_open() && !write_queue_.empty();
      while (can_write) {

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
            return stream_.socket->is_open() && !write_queue_.empty();
          }
          
        },
        stream_.write(buf));
      }

      return stream_.socket->is_open();
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

    void enqueue(std::vector<char> buf) noexcept override
    {
      write_queue_.push_back(std::make_pair(std::move(buf), 0));
    }
  };
}

#endif // JETBLACK_NET_SERVER_SOCKET_POLL_HANDLER_HPP
