#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static clientConnection_t connection;
static clientConnection_t *connection_ptr = &connection;
byte *clc_ptr = (byte *)&connection_ptr;
int clc_x64_lastChallenge;

#include "netchan_decode_source.h"

int main(void)
{
    /* Mac 1.3 0x18fec4-0x18fed1: the current sequence and current challenge
       alone seed the key. A zero listen challenge must remain zero after a
       previous remote connection supplied a nonzero challenge. */
    const byte payload[] = {1, 0, 0x86, 7, 0x13, 0x41, 0xff, 0x20};
    byte packet[sizeof(payload)];
    connection.serverMessageSequence = 0x101;
    connection.reliableAcknowledge = 128;
    clc_x64_lastChallenge = 0x5a;
    for (int challenge = 0; challenge <= 0x5a; challenge += 0x5a) {
        connection.challenge = challenge;
        for (unsigned i = 0; i < sizeof(packet); ++i)
            packet[i] = payload[i] ^ (byte)(1 ^ challenge);
        CL_Netchan_Decode(packet, sizeof(packet));
        assert(!memcmp(packet, payload, sizeof(packet)));
    }
    /* Reliable command bytes must still participate in the decode key. */
    strcpy(connection.reliableCommands[0], "A");
    byte expected[] = {0x40, 0xc2, 0x83, 0x01};
    byte encoded[] = {0, 0, 0, 0};
    connection.challenge = 0;
    CL_Netchan_Decode(encoded, sizeof(encoded));
    assert(!memcmp(encoded, expected, sizeof(encoded)));
    puts("online: netchan uses the current challenge, including zero after a remote join");
}
