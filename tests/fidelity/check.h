#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)
#define FINISH(name) do { printf("%s: %d failures\n", name, failures); return failures != 0; } while (0)
