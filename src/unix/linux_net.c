#include "common_types.h"
#include "imports.h"
#include "bytematch.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

int closesocket(SOCKET s)
{
    return close(s);
}

int ioctlsocket(SOCKET s, long int cmd, u_long *argp)
{
    if (cmd == 0x8004667e)
        cmd = FIONBIO;
    return ioctl(s, cmd, argp);
}

int WSAStartup(int wVersionRequired, LPWSADATA lpWSAData)
{
    memset(lpWSAData, 0, sizeof(WSADATA));
    *(short *)lpWSAData = (short)wVersionRequired;
    return 0;
}

int WSAGetLastError(void)
{
    int err = errno;

    if (err == EAGAIN || err == EWOULDBLOCK)
        return 10035;
    if (err == ECONNRESET)
        return 10054;
    if (err == EAFNOSUPPORT)
        return 10047;
    if (err == EADDRNOTAVAIL)
        return 10049;

    if ((unsigned)(err - 1) <= 0x57)
        return err + 10000;
    return err;
}
