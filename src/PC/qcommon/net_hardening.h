#ifndef COD2_NET_HARDENING_H
#define COD2_NET_HARDENING_H

#include "cod2_feature_config.h"

/* Memory-safety bounds on untrusted network input. Native builds always apply
 * them; legacy builds keep their original code unless hardening is enabled. */
#if defined(COD2_X64) || COD2_FEATURE_NET_HARDENING
#    define COD2_NET_BOUNDS 1
#else
#    define COD2_NET_BOUNDS 0
#endif

#if COD2_FEATURE_NET_HARDENING

#    ifndef COD2_NET_HARDENING_NEED_NETADR
struct netadr_t;
#    endif

int NetHardening_ValidDownloadName(const char *name, int require_iwd_ext);

int NetHardening_ValidUrl(const char *url);

int NetHardening_ServerMaySetCvar(const char *key);

int NetHardening_SVC_RateLimit(int nowMsec);
int NetHardening_SVC_RateLimitAddress(struct netadr_t from, int nowMsec);

#endif

#endif
