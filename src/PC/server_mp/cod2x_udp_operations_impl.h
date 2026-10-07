/* Independently authored native operational policy. Compile in sv_main_mp.c
 * so the original source/object set is unchanged when either gate is off. */
#ifndef COD2X_UDP_OPERATIONS_IMPL_H
#define COD2X_UDP_OPERATIONS_IMPL_H
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_udp_operations.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

extern int Sys_Milliseconds(void);
extern qboolean Sys_IsLANAddress(netadr_t from);
extern const dvar_t *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags);
extern const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, unsigned short flags);
extern void Dvar_SetInt(const dvar_t *dvar, int value);

const dvar_t *sv_rateLimiter;
const dvar_t *sv_rateLimiterRequestCount;
const dvar_t *sv_rateLimiterUniqueIPCount;

#define COD2X_UDP_ADDRESS_COUNT 16384
#define COD2X_UDP_HASH_COUNT 1024

typedef struct {
    uint32_t time;
    unsigned int burst;
} cod2xUdpBucket_t;

typedef struct {
    cod2xUdpBucket_t bucket;
    byte ip[4];
    unsigned int next; /* Index plus one; zero ends the hash chain. */
    int active;
} cod2xUdpAddress_t;

static cod2xUdpAddress_t cod2xUdpAddresses[COD2X_UDP_ADDRESS_COUNT];
static unsigned int cod2xUdpHashes[COD2X_UDP_HASH_COUNT];
static cod2xUdpBucket_t cod2xUdpGlobals[3];
static uint32_t cod2xUdpStatsTime;

static unsigned int SV_Cod2xUdpHash(const byte ip[4])
{
    uint32_t hash = UINT32_C(2166136261);
    for (int i = 0; i < 4; ++i)
        hash = (hash ^ ip[i]) * UINT32_C(16777619);
    return hash % COD2X_UDP_HASH_COUNT;
}

static cod2xUdpBucket_t *SV_Cod2xUdpAddress(netadr_t from, unsigned int limit, uint32_t now)
{
    unsigned int hash = SV_Cod2xUdpHash(from.ip);
    for (unsigned int link = cod2xUdpHashes[hash]; link; link = cod2xUdpAddresses[link - 1].next) {
        cod2xUdpAddress_t *entry = &cod2xUdpAddresses[link - 1];
        if (!memcmp(entry->ip, from.ip, sizeof(entry->ip)))
            return &entry->bucket;
    }

    for (unsigned int i = 0; i < COD2X_UDP_ADDRESS_COUNT; ++i) {
        cod2xUdpAddress_t *entry = &cod2xUdpAddresses[i];
        uint32_t elapsed = now - entry->bucket.time;
        /* Keep the reference's strict reuse boundary and action-dependent
         * window, including disconnect's shorter address retention. */
        if (entry->active && elapsed <= limit * 1000 && elapsed <= INT32_MAX)
            continue;
        if (entry->active) {
            unsigned int *link = &cod2xUdpHashes[SV_Cod2xUdpHash(entry->ip)];
            while (*link != i + 1)
                link = &cod2xUdpAddresses[*link - 1].next;
            *link = entry->next;
        }
        entry->active = 1;
        memcpy(entry->ip, from.ip, sizeof(entry->ip));
        entry->bucket.time = now;
        entry->bucket.burst = 0;
        entry->next = cod2xUdpHashes[hash];
        cod2xUdpHashes[hash] = i + 1;
        return &entry->bucket;
    }
    return NULL;
}

static int SV_Cod2xUdpTake(cod2xUdpBucket_t *bucket, unsigned int limit, unsigned int period, uint32_t now)
{
    if (!bucket)
        return 0;
    uint32_t elapsed = now - bucket->time;
    unsigned int drained = elapsed / period;
    if (elapsed > INT32_MAX || drained > bucket->burst) {
        bucket->burst = 0;
        bucket->time = now;
    } else {
        bucket->burst -= drained;
        bucket->time = now - elapsed % period;
    }
    if (bucket->burst >= limit)
        return 0;
    ++bucket->burst;
    return 1;
}

void SV_Cod2xUdpInit(void)
{
    sv_rateLimiter = Dvar_RegisterBool("sv_rateLimiter", 1, 0x1000);
    sv_rateLimiterRequestCount = Dvar_RegisterInt("sv_rateLimiterRequestCount", 0, 0, INT_MAX, 0x40);
    sv_rateLimiterUniqueIPCount = Dvar_RegisterInt("sv_rateLimiterUniqueIPCount", 0, 0, INT_MAX, 0x40);
    memset(cod2xUdpAddresses, 0, sizeof(cod2xUdpAddresses));
    memset(cod2xUdpHashes, 0, sizeof(cod2xUdpHashes));
    memset(cod2xUdpGlobals, 0, sizeof(cod2xUdpGlobals));
    cod2xUdpStatsTime = 0;
    Dvar_SetInt(sv_rateLimiterRequestCount, 0);
    Dvar_SetInt(sv_rateLimiterUniqueIPCount, 0);
}

int SV_Cod2xUdpAllow(netadr_t from, cod2xUdpOperation_t operation)
{
    if (!sv_rateLimiter || !sv_rateLimiter->current.enabled)
        return 1;
    uint32_t now = (uint32_t)Sys_Milliseconds();
    unsigned int addressLimit = operation == COD2X_UDP_DISCONNECT ? 2 : 10;
    unsigned int globalLimit = operation == COD2X_UDP_DISCONNECT ? 64 : 10;
    unsigned int globalPeriod = operation == COD2X_UDP_QUERY ? 100 : 1000;
    if (!Sys_IsLANAddress(from) &&
        !SV_Cod2xUdpTake(SV_Cod2xUdpAddress(from, addressLimit, now), addressLimit, 1000, now))
        return 0;
    return SV_Cod2xUdpTake(&cod2xUdpGlobals[operation], globalLimit, globalPeriod, now);
}

void SV_Cod2xUdpStats(void)
{
    if (!sv_rateLimiter || !sv_rateLimiter->current.enabled)
        return;
    uint32_t now = (uint32_t)Sys_Milliseconds();
    if (now - cod2xUdpStatsTime < 1000)
        return;
    cod2xUdpStatsTime = now;
    int requests = 0, uniqueIPs = 0;
    for (unsigned int i = 0; i < COD2X_UDP_ADDRESS_COUNT; ++i) {
        const cod2xUdpAddress_t *entry = &cod2xUdpAddresses[i];
        /* These are the reference's recent bucket count and sum of already
         * drained bursts, including addresses rejected by a global bucket. */
        if (entry->active && now - entry->bucket.time <= 1000) {
            ++uniqueIPs;
            requests += (int)entry->bucket.burst;
        }
    }
    if (requests != sv_rateLimiterRequestCount->current.integer)
        Dvar_SetInt(sv_rateLimiterRequestCount, requests);
    if (uniqueIPs != sv_rateLimiterUniqueIPCount->current.integer)
        Dvar_SetInt(sv_rateLimiterUniqueIPCount, uniqueIPs);
}
#endif
#endif
