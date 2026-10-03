#include "cod2x.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char id[33], same[33], other[33], hash[33];
    char info[1024], packet[2048], decoded[33];
    size_t encoded;
    int protocol, revision, challenge, qport;
    assert(Cod2x_ConnectProtocol(120) == 118);
    assert(Cod2x_ConnectProtocol(118) == 118);
    assert(Cod2x_ConnectProtocol(117) == 0);
    assert(Cod2x_ConnectProtocol(0) == 0);
    assert(Cod2x_HwidFromUUID("00112233-4455-6677-8899-AABBCCDDEEFF", id));
    assert(Cod2x_HwidFromUUID("00112233-4455-6677-8899-aabbccddeeff", same));
    assert(!strcmp(id, same));
    assert(!strcmp(id, "ef2c32ec00ddb6cd4f029d95cdb712a5"));
    assert(Cod2x_HwidValid(id));
    assert(Cod2x_HwidFromUUID("10112233-4455-6677-8899-AABBCCDDEEFF", other));
    assert(strcmp(id, other));
    assert(!Cod2x_HwidFromUUID("", other));
    assert(!Cod2x_HwidFromUUID("not-a-uuid", other));
    assert(!Cod2x_HwidValid("0123456789abcdef"));
    assert(!Cod2x_HwidValid("g123456789abcdef0123456789abcdef"));
    snprintf(info, sizeof(info), "\\protocol\\118\\protocol_cod2x\\6\\cl_hwid2\\%s\\challenge\\-2147483647\\qport\\65535", id);
    encoded = Cod2x_EncodeConnect(packet, sizeof(packet), info);
    assert(encoded == strlen(info) + 10);
    assert(!strncmp(packet, "connect \"", 9));
    assert(packet[encoded - 1] == '"' && packet[encoded] == '\0');
    assert(sscanf(packet, "connect \"\\protocol\\%d\\protocol_cod2x\\%d\\cl_hwid2\\%32[0-9a-f]\\challenge\\%d\\qport\\%d\"",
                  &protocol, &revision, decoded, &challenge, &qport) == 5);
    assert(protocol == 118 && revision == 6 && !strcmp(decoded, id));
    assert(challenge == -2147483647 && qport == 65535);
    assert(Cod2x_EncodeConnect(packet, encoded, info) == 0);
    assert(Cod2x_CDKeyHash("abcd-1234", hash));
    assert(!strcmp(hash, "77b51bb0130bd4bbbfc94d0c64bd6fdc")); /* seeded stock CD-key digest */
    assert(Cod2x_CDKeyHash("0123456789ABCDEF0123456789ABCDEF", hash));
    assert(!strcmp(hash, "97306e1d8843c95de2a232634b138d14"));
    assert(Cod2x_CDKeyHash("abc", hash));
    assert(!strcmp(hash, "4f74eb2f2811da3573a9e69e0a01a7de"));
    assert(Cod2x_LimitedFPS(0, 1) == 250);
    assert(Cod2x_LimitedFPS(85, 1) == 250);
    assert(Cod2x_LimitedFPS(125, 1) == 125);
    assert(Cod2x_LimitedFPS(200, 1) == 200);
    assert(Cod2x_LimitedFPS(333, 1) == 250);
    assert(Cod2x_LimitedFPS(0, 0) == 0);
    assert(Cod2x_LimitedFPS(333, 0) == 333);
    assert(Cod2x_ReadMachineHwid(id));
    assert(Cod2x_ReadMachineHwid(same));
    assert(Cod2x_HwidValid(id) && !strcmp(id, same));
    puts("cod2x: identity, protocol, CD-key encoding, FPS tests passed");
    return 0;
}
