#include <stdarg.h>
extern int vsnprintf(char*,unsigned,const char*,va_list);
extern char ri[];
#define RI_PRINTF (*(void(**)(int,const char*,...))(ri+0))
void Com_DPrintf(const char*fmt,...){char b[4096];va_list ap;va_start(ap,fmt);vsnprintf(b,sizeof b,fmt,ap);va_end(ap);RI_PRINTF(1,"%s",b);}
void Com_Printf(const char*fmt,...){char b[4096];va_list ap;va_start(ap,fmt);vsnprintf(b,sizeof b,fmt,ap);va_end(ap);RI_PRINTF(0,"%s",b);}
