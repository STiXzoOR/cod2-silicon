#if defined(__x86_64__)

#    include "common_types.h"

#    define ARCH64_VTABLE(sym) unsigned char sym[256] __attribute__((aligned(16))) = { 0 }
ARCH64_VTABLE(__ZTV6Effect);
ARCH64_VTABLE(__ZTV16OrientedParticle);
ARCH64_VTABLE(__ZTV4Line);
ARCH64_VTABLE(__ZTV4Tail);
ARCH64_VTABLE(__ZTV5Cloud);
ARCH64_VTABLE(__ZTV7Emitter);
ARCH64_VTABLE(__ZTV8Cylinder);
ARCH64_VTABLE(__ZTV12IncludeClass);
#    undef ARCH64_VTABLE

void *__ZTIl = 0;

void *TheStringPackage = 0;
void *__ZN12CSoundObject13sReadCallbackE = 0;
void *__ZN12CSoundObject13sSeekCallbackE = 0;
void *__ZN12CSoundObject14sCloseCallbackE = 0;
void *__ZN12CSoundObject13sOpenCallbackE = 0;

#endif
