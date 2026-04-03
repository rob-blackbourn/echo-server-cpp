#ifndef __io_hpp
#define __io_hpp

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <expected>
#include <vector>

namespace jetblack
{
    std::expected<ssize_t, int> read(int fd, char* buf, size_t buflen)
    {
        ssize_t bytes_read = ::read(fd, buf, buflen);
        if (bytes_read == -1)
            return std::unexpected(errno);
        return bytes_read;
    }

    std::expected<ssize_t, int> write(int fd, const char* buf, size_t buflen)
    {
        ssize_t bytes_read = ::write(fd, buf, buflen);
        if (bytes_read == -1)
            return std::unexpected(errno);
        return bytes_read;
    }

    std::expected<void*, int> write_all(int fd, const char* buf, size_t buflen)
    {
        ssize_t bytes_written = 0;
        while (static_cast<size_t>(bytes_written) < buflen)
        {
            auto nbytes = write(fd, buf, buflen);
            if (!nbytes)
                return std::unexpected(errno);
            bytes_written += *nbytes;
        }
        return nullptr;
    }

    namespace fcntl
    {
        template <int Flag> std::expected<bool, int> get_flag(int fd)
        {
            auto flags = ::fcntl(fd, F_GETFL);
            if (flags == -1)
                return std::unexpected(errno);
            return (flags | Flag) == Flag;
        }

        template <int Flag> std::expected<void*, int>  set_flag(int fd, bool is_set)
        {
            // Get the current flags.
            int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags == -1)
                return std::unexpected(errno);
            flags = is_set ? (flags & ~Flag) : (flags | Flag);
            int retval = ::fcntl(fd, F_SETFL, flags);
            if (retval == -1)
                return std::unexpected(errno);
            return nullptr;
        }
    }
}

#endif // __io_hpp
