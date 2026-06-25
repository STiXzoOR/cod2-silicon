#include <stdlib.h>

void *_Znwm(unsigned int n)
{
    return malloc(n);
}
void *_Znam(unsigned int n)
{
    return malloc(n);
}
void *__Znwm(unsigned int n)
{
    return malloc(n);
}
void *__Znam(unsigned int n)
{
    return malloc(n);
}
void _ZdlPv(void *p)
{
    free(p);
}
void __ZdlPv(void *p)
{
    free(p);
}
void _ZdaPv(void *p)
{
    free(p);
}
void __ZdaPv(void *p)
{
    free(p);
}
