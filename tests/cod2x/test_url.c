#include "cod2x_url.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    Cod2xURL parsed;
    char command[512];
    assert(Cod2x_ParseURL("cod2x://connect/127.0.0.1:29999", &parsed));
    assert(!strcmp(parsed.address, "127.0.0.1:29999"));
    assert(Cod2x_ParseURL("cod2x://connect/game.example:28960/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://connect/127.0.0.1:0", &parsed));
    assert(!Cod2x_ParseURL("cod2x://connect/localhost/quit", &parsed));
    assert(Cod2x_ParseURL("cod2x://%2Bconnect+88.198.58.188:27397/", &parsed));
    assert(!strcmp(parsed.address, "88.198.58.188:27397"));
    assert(!parsed.hasPassword);
    assert(Cod2x_ParseURL("cod2x://%20%2Bconnect%20game.example:28960%20%2Bpassword%20%22two%20words%22/", &parsed));
    assert(!strcmp(parsed.password, "two words"));
    assert(Cod2x_URLCommands(&parsed, command, sizeof(command)));
    assert(!strcmp(command, "password \"two words\"\nconnect game.example:28960\n"));
    assert(Cod2x_ParseURL("cod2x://%2Bpassword%20pivo%20%2Bconnect%20127.0.0.1/", &parsed));
    assert(Cod2x_ParseURL("COD2X://%2Bconnect%20localhost/", &parsed));
    assert(!Cod2x_ParseURL("http://%2Bconnect%20localhost/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1%20%2Brcon%20login%20aaa/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1%3Bquit/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1%0Aquit/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1%00quit/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1%ZZ/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1:65536/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1:0/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20127.0.0.1:bad/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20evil@host/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20host/path/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20localhost%20%2Bconnect%20other/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bpassword%20pivo/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20localhost%20%2Bpassword%20%22p%3Bquit%22/", &parsed));
    assert(!Cod2x_ParseURL("cod2x://%2Bconnect%20localhost%20%2Bpassword%20%22unterminated/", &parsed));
    assert(!Cod2x_URLCommands(&parsed, command, 4));
    puts("cod2x URL tests passed");
    return 0;
}
