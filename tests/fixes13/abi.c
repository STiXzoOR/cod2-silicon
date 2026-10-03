#include <assert.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(__builtin_types_compatible_p(__typeof__(&qsort),
    void (*)(void *, size_t, size_t, int (*)(const void *, const void *))), "qsort uses size_t");
_Static_assert(__builtin_types_compatible_p(__typeof__(&malloc), void *(*)(size_t)), "malloc uses size_t");
_Static_assert(__builtin_types_compatible_p(__typeof__(&memcpy), void *(*)(void *, const void *, size_t)), "memcpy uses size_t");
_Static_assert(__builtin_types_compatible_p(__typeof__(&memset), void *(*)(void *, int, size_t)), "memset uses size_t");
int main(void)
{
    assert(sizeof(size_t) == 8);
    void *p = malloc(16);
    assert(p); memset(p, 0, 16); free(p);
    return 0;
}
