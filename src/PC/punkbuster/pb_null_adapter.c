#include "common_types.h"
#include "cod2_feature_config.h"
#include "pb_public.h"

#if COD2_FEATURE_PUNKBUSTER

void PbClientInitialize(void *param)
{
    (void)param;
}
void PbServerInitialize(void) {}
void PbClientProcessEvents(void) {}
void PbServerProcessEvents(void) {}

void EnablePbCl(void) {}
void DisablePbCl(void) {}
void EnablePbSv(void) {}
void DisablePbSv(void) {}

const char *PB_BuildAuthToken(void)
{
    return "";
}

int PB_HandleClientOobPacket(const void *from, void *msg)
{
    (void)from;
    (void)msg;
    return 1;
}
int PB_HandleServerOobPacket(const void *from, void *msg)
{
    (void)from;
    (void)msg;
    return 1;
}

void PB_DropClient(int clientNum, const char *reason)
{
    (void)clientNum;
    (void)reason;
}

#endif
