#if defined(COD2_CODX) && COD2_CODX
#include "cod2x.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int Cod2x_ConnectProtocol(int advertised)
{
    /* CoD2x src/shared/server.cpp:388-423: 120 is discovery, connect stays 118. */
    return advertised == 118 || advertised == 120 ? COD2X_CONNECT_PROTOCOL : 0;
}

/* Stock connectionless payload, including the extension's text userinfo fields.
   NET_OutOfBandData supplies the four 0xff framing bytes separately. */
size_t Cod2x_EncodeConnect(char *packet, size_t capacity, const char *userinfo)
{
    size_t len = strlen(userinfo);
    if (len > capacity || capacity - len < 11)
        return 0;
    memcpy(packet, "connect \"", 9);
    memcpy(packet + 9, userinfo, len);
    packet[len + 9] = '"';
    packet[len + 10] = '\0';
    return len + 10;
}

int Cod2x_HwidValid(const char *id)
{
    size_t i;
    if (!id)
        return 0;
    for (i = 0; i < 32; ++i) {
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')))
            return 0;
    }
    return id[32] == '\0';
}

int Cod2x_LimitedFPS(int requested, int limited)
{
    /* CoD2x src/mss32/competitive.cpp:54-59. Zero means unlimited in stock. */
    return limited && (requested < 125 || requested > 250) ? 250 : requested;
}

/* ---- CoD2x services ----------------------------------------------------------
   Tests send CoD2x's web requests to a stand-in: COD2X_TEST_SERVICE_URL (for
   example http://127.0.0.1:8080) replaces "https://cod2x.me" or
   "https://master.cod2x.me" at the start of a URL. Without it the URL is used as
   it is. */
const char *Cod2x_ServiceURL(const char *url, char *buffer, size_t size)
{
    static const char *const hosts[] = {"https://master.cod2x.me", "https://cod2x.me"};
    const char *stand = getenv("COD2X_TEST_SERVICE_URL");
    size_t i;
    if (!stand || !*stand)
        return url;
    for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); ++i) {
        size_t length = strlen(hosts[i]);
        if (!strncmp(url, hosts[i], length) && (url[length] == '/' || !url[length])) {
            snprintf(buffer, size, "%s%s", stand, url + length);
            return buffer;
        }
    }
    return url;
}
#endif
