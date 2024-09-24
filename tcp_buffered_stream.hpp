#ifndef __tcp_buffered_stream_hpp
#define __tcp_buffered_stream_hpp

#include <deque>

#include "match.hpp"
#include "tcp_stream.hpp"

namespace jetblack {
  namespace net {
    
    class tcp_buffered_stream : public tcp_stream
    {
    private:
      std::deque<std::vector<char>> read_queue_;
      std::deque<std::pair<std::vector<char>, std::size_t>> write_queue_;

    public:
      const std::size_t read_bufsiz;
      const std::size_t write_bufsiz;

      tcp_buffered_stream(
        std::shared_ptr<tcp_socket> socket,
        std::size_t read_bufsiz,
        std::size_t write_bufsiz)
        : tcp_stream(socket),
          read_bufsiz(read_bufsiz),
          write_bufsiz(write_bufsiz)
      {
      }

      bool has_reads() const noexcept { return !read_queue_.empty(); }

      std::vector<char> deque_read() noexcept
      {
        auto buf { std::move(read_queue_.front()) };
        read_queue_.pop_front();
        return buf;
      }

      bool enqueue_reads()
      {
        bool ok = socket->is_open();
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

            [&](std::vector<char>&& buf) mutable
            {
              read_queue_.push_back(std::move(buf));
              return socket->is_open();
            }

          },
          read(read_bufsiz));
        }
        return socket->is_open();
      }

      bool has_writes() const noexcept { return !write_queue_.empty(); }

      void enqueue_write(std::vector<char> buf)
      {
        write_queue_.push_back(std::make_pair(std::move(buf), 0));
      }

      bool write_enqueued()
      {
        bool has_writes = socket->is_open() && !write_queue_.empty();
        while (has_writes) {

          auto& [orig_buf, offset] = write_queue_.front();
          std::size_t count = std::min(orig_buf.size() - offset, write_bufsiz);
          const auto& buf = std::span<char>(orig_buf).subspan(offset, count);

          has_writes = std::visit(match {
            
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
              return socket->is_open() && !write_queue_.empty();
            }
            
          },
          write(buf));
        }

        return socket->is_open();
      }
    };

  }
}

#endif // __tcp_buffered_stream_hpp
