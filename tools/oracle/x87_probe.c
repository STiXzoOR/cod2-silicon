/* Compile in the reference toolchain container, not with the native compiler:
 * gcc-3.3 -std=c99 -O0 -fno-builtin -mfpmath=387 x87_probe.c -lm -o x87_probe
 * Raw libm results identify the guest/compiler/emulator combination. They are
 * a diagnostic baseline, not a claim of hardware x87 transcendental identity.
 */
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(__i386__)
#error "Run this probe in the linux/386 reference container"
#endif

static void result(const char *name, double x, double y, double value)
{
    uint64_t xb, yb, db;
    uint32_t fb;
    float single = (float)value;
    memcpy(&xb, &x, sizeof(xb));
    memcpy(&yb, &y, sizeof(yb));
    memcpy(&db, &value, sizeof(db));
    memcpy(&fb, &single, sizeof(fb));
    printf("%s x=%016" PRIx64 " y=%016" PRIx64 " f64=%016" PRIx64 " f32=%08" PRIx32 "\n",
           name, xb, yb, db, fb);
}

int main(void)
{
    static const double inputs[] = {
        -0.0, 0.0, 0x1.0p-20, 0x1.0p-1, 1.0, 2.0,
        0x1.921fb54442d18p+1, 1000000.0
    };
    unsigned short control;
    unsigned int i;
    __asm__ volatile("fnstcw %0" : "=m"(control));
    printf("x87 control=%04x sizeof_float=%u sizeof_double=%u sizeof_long_double=%u\n",
           control, (unsigned)sizeof(float), (unsigned)sizeof(double), (unsigned)sizeof(long double));
    for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        volatile double x = inputs[i];
        result("sin", x, 0.0, sin(x));
        result("cos", x, 0.0, cos(x));
        result("atan2", x, -0.75, atan2(x, -0.75));
        result("sqrt", x, 0.0, sqrt(x));
        result("pow", x, 1.5, pow(x, 1.5));
    }
    return 0;
}
