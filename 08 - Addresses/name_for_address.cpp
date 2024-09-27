#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <cstdint>
#include <cstring>
#include <iostream>


int main()
{
    // const char* ip_address = "192.168.86.10";
    const char* ip_address = "127.0.1.1";
    uint16_t port = 22;

    in_addr addr;
    if (inet_pton(AF_INET, ip_address, &addr) != 1)
    {
        throw std::runtime_error("Failed to connect");
    }

    sockaddr_in servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    std::memcpy(&servaddr.sin_addr, &addr, sizeof(addr));
    servaddr.sin_port = htons(port);

    char host[NI_MAXHOST];
    int result = getnameinfo((sockaddr*)&servaddr, sizeof(servaddr), host, sizeof(host), nullptr, 0, 0);
    if (result != 0)
    {
        throw std::runtime_error(gai_strerror(result));
    }

    std::cout << "host: " << (const char*)host << std::endl;

    return 0;
}
