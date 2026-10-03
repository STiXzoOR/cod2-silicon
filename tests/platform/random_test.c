#include "PC/universal/com_math.c"
#include <assert.h>
int main(void)
{
    Rand_Init(1);
    for (int i=0; i<10000; ++i) {
        int index=irand(0,17);
        float unit=flrand(-1,1);
        if (index<0 || index>=17 || unit < -1 || unit >= 1) {
            fprintf(stderr,"random range violated: index=%d float=%f\n",index,unit);
            return 1;
        }
    }
    Rand_Init(1);
    assert(irand(0,32768)==20);
    assert(irand(0,32768)==25617);
    puts("32-bit LCG sequence and bounded native integer/float ranges: PASS");
}
