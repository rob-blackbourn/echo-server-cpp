#ifndef __client_state_hpp
#define __client_state_hpp

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "io.hpp"

// Wraps a socket.
// Provides access to the file descriptor, and
// a method to enable or disable blocking..
class tcp_socket
{
private:
    int fd_;

public:
    tcp_socket() {}
    tcp_socket(int fd) : fd_(fd) {}

    int fd() const { return fd_; }

    std::expected<void*, int> set_blocking(bool is_blocking)
    {
        return jetblack::fcntl::set_flag<O_NONBLOCK>(fd_, is_blocking);
    }
};

const std::size_t NETWORK_READ_BUFSIZE = 8096;
const std::size_t NETWORK_WRITE_BUFSIZE = 8096;

class ClientState : public tcp_socket
{
private:
    std::size_t read_bufsiz_;
    std::deque<std::vector<char>> read_queue_;

    std::size_t write_bufsiz_;
    std::deque<std::pair<std::vector<char>,std::size_t>> write_queue_;

    bool is_open_ = true;

public:
    ClientState() {}

    ClientState(
        int fd_,
        std::size_t read_bufsiz = NETWORK_READ_BUFSIZE,
        std::size_t write_bufsiz = NETWORK_WRITE_BUFSIZE)
        :   tcp_socket { fd_ },
            read_bufsiz_ { read_bufsiz },
            write_bufsiz_ { write_bufsiz }
    {
    }

    bool can_write() const { return is_open_ && !write_queue_.empty(); }

    std::optional<std::vector<char>> read()
    {
        if (read_queue_.empty())
            return std::nullopt;

        auto buf = read_queue_.front();
        read_queue_.pop_front();
        return buf;
    }

    bool enqueue_reads()
    {
        while (is_open_)
        {
            auto buf = std::vector<char>(read_bufsiz_);
            auto bytes_read = jetblack::read(fd(), buf.data(), buf.size());

            // Check for an error.
            if (!bytes_read)
            {
                // Check if it's flow control.
                if (!(bytes_read.error() == EAGAIN || bytes_read.error() == EWOULDBLOCK))
                {
                    // Not a flow control error; the socket has faulted.
                    is_open_ = false;
                }

                // Any error ends the read session.
                break;
            }
            else if (*bytes_read == 0)
            {
                // A read of zero bytes indicates socket has closed.
                is_open_ = false;
            }
            else
            {
                // Data has been read successfully.
                // Size the data to the amount read and add the data to the read queue.
                buf.resize(*bytes_read);
                read_queue_.push_back(std::move(buf));
            }
        }

        return is_open_;
    }

    void enqueue_write(std::vector<char> buf)
    {
        write_queue_.push_back(std::make_pair(std::move(buf), 0));
    }

    bool write()
    {
        while (is_open_ && !write_queue_.empty())
        {
            const auto& buf = write_queue_.front().first;
            auto& offset = write_queue_.front().second;
            auto bytes_written = jetblack::write(fd(), buf.data(), std::min(buf.size() - offset, write_bufsiz_));

            // Check for errors.
            if (!bytes_written)
            {
                // Check if it's flow control.
                if (!(bytes_written.error() == EAGAIN || bytes_written.error() == EWOULDBLOCK))
                {
                    // Not flow control; the socket has faulted.
                    is_open_ = false;
                }

                // Any error terminates the write session.
                break;
            }

            if (*bytes_written == 0)
            {
                // A write of zero bytes indicates socket has closed.
                is_open_ = false;
            }
            else
            {
                // Some bytes were written. Manage the write queue.
                offset += *bytes_written;
                if (offset == buf.size())
                {
                    // The buffer has been completely used. Remove it from the queue.
                    write_queue_.pop_front();
                }
            }
        }

        return is_open_;
    }
};

#endif // __client_state_hpp
