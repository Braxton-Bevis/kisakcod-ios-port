#pragma once
// Winsock 1.1 → BSD sockets, for iOS (DEPENDENCY_MAP §5 "Networking").
// The net layer (win32/win_net.cpp, win32/win_net_debug.cpp) uses the
// identically-named BSD calls (socket/bind/sendto/recvfrom/select/...) that
// Darwin already provides; this header supplies the few winsock-only pieces
// so those call sites stay untouched. Win32 builds never include this file.
//
// Not mapped here (call sites carry their own KISAK_IOS gates instead):
//  - hardcoded winsock error numbers (10035/10047/10049/10054) → errno names
//  - fd_set fd_count/fd_array pokes → FD_ZERO/FD_SET (NET_Select)
//  - sockaddr type punning → proper sockaddr_in fields, incl. Darwin sin_len
//  - IPX (wsipx.h) → dead code, compiled out
#ifdef KISAK_IOS

#include <sys/types.h>   // u_short / u_long spellings used by the decomp
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>       // gethostbyname / getaddrinfo / hostent
#include <ifaddrs.h>     // NET_GetLocalAddress (getifaddrs)
#include <net/if.h>      // IFF_LOOPBACK
#include <unistd.h>      // close / gethostname / usleep
#include <fcntl.h>       // O_NONBLOCK for ioctlsocket(FIONBIO)
#include <cerrno>
#include <cstring>

// Winsock socket handle → BSD descriptor. The engine's "0 == no socket"
// convention survives: fd 0 (stdin) can never be a freshly created socket.
typedef int SOCKET;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR   (-1)
#endif

#define closesocket(s) close(s)

// BSD sockets need no library init/teardown — explicit no-ops.
// WSADATA keeps the win32 size (400) so TRACK_win_net's byte count stays honest.
typedef struct WSADATA
{
    unsigned char wsa_unused[400];
} WSADATA;

static inline int WSAStartup(unsigned short /*wVersionRequested*/, WSADATA * /*lpWSAData*/)
{
    return 0; // success — sockets are always available
}

static inline int WSACleanup(void)
{
    return 0;
}

// Winsock keeps a per-thread last-error slot; BSD reports through errno.
static inline int WSAGetLastError(void)
{
    return errno;
}

// ioctlsocket: the engine only ever issues FIONBIO, always as the hardcoded
// winsock request value 0x8004667E with a pointer to a 32-bit nonzero flag
// (winsock u_long is 32-bit; LP64 'unsigned long' at the cast sites is a lie,
// so deref as int). Implemented via fcntl(O_NONBLOCK) per DEPENDENCY_MAP §5.
static inline int ioctlsocket(SOCKET s, unsigned long cmd, void *argp)
{
    if (cmd == 0x8004667Eul) // winsock FIONBIO
    {
        int flags = fcntl(s, F_GETFL, 0);
        if (flags == -1)
            return -1;
        if (*(const int *)argp)
            flags |= O_NONBLOCK;
        else
            flags &= ~O_NONBLOCK;
        return fcntl(s, F_SETFL, flags) == -1 ? -1 : 0;
    }
    errno = EINVAL;
    return -1;
}

#endif // KISAK_IOS
