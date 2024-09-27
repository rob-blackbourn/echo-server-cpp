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
    int retcode;

    char buf[1024];
    retcode = gethostname(buf, sizeof(buf));
    std::cout <<"hostname=\"" << buf << "\"\n";
    retcode = getdomainname(buf, sizeof(buf));
    std::cout <<"domainname=\"" << buf << "\"\n";

    utsname uname_buf;
    retcode = uname(&uname_buf);
    std::cout << "sysname=\"" << (const char *)uname_buf.sysname << "\"" << std::endl;
    std::cout << "nodename=\"" << (const char *)uname_buf.nodename << "\"" << std::endl;
    std::cout << "release=\"" << (const char *)uname_buf.release << "\"" << std::endl;
    std::cout << "version=\"" << (const char *)uname_buf.version << "\"" << std::endl;
    std::cout << "machine=\"" << (const char *)uname_buf.machine << "\"" << std::endl;
    std::cout << "domainname=\"" << (const char *)uname_buf.domainname << "\"" << std::endl;

    return 0;
}