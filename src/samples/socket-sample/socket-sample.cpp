#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_wsa_startup()
    {
        WSADATA wsa{};
        int res = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (res != 0)
            return false;
        WSACleanup();
        return true;
    }

    bool test_getaddrinfo()
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res = nullptr;
        int ret = getaddrinfo("localhost", nullptr, &hints, &res);
        if (ret == 0)
            freeaddrinfo(res);

        WSACleanup();
        return ret == 0;
    }

    bool test_udp_loopback()
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;

        SOCKET s1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        SOCKET s2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s1 == INVALID_SOCKET || s2 == INVALID_SOCKET)
        {
            if (s1 != INVALID_SOCKET)
                closesocket(s1);
            if (s2 != INVALID_SOCKET)
                closesocket(s2);
            WSACleanup();
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        bind(s2, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        int len = sizeof(addr);
        getsockname(s2, reinterpret_cast<sockaddr*>(&addr), &len);

        const char msg[] = "udp test";
        sendto(s1, msg, sizeof(msg) - 1, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        char buf[64]{};
        sockaddr_in from{};
        int fromlen = sizeof(from);
        int received = recvfrom(s2, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);

        closesocket(s1);
        closesocket(s2);
        WSACleanup();

        return received == static_cast<int>(sizeof(msg) - 1) && std::memcmp(buf, msg, received) == 0;
    }
}

int main()
{
    bool ok = true;
    ok &= report("WSAStartup/Cleanup", test_wsa_startup());
    ok &= report("getaddrinfo localhost", test_getaddrinfo());
    ok &= report("UDP loopback", test_udp_loopback());
    return ok ? 0 : 1;
}
