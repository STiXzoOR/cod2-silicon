#include <assert.h>
#include <stdint.h>
#include "f1_xmodel_source.h"

int strnicmp(const char *a,const char *b,size_t n)
{
    while (n--) {
        unsigned char x=(unsigned char)*a++, y=(unsigned char)*b++;
        if (x>='A' && x<='Z') x+='a'-'A';
        if (y>='A' && y<='Z') y+='a'-'A';
        if (x!=y || !x) return x-y;
    }
    return 0;
}

void TestNames(void)
{
    XModel model={0}; XModelParts parts={0}; XBoneHierarchy hierarchy={0}; unsigned short names[1]={17};
    assert(Com_ValidXModelName("xmodel/test"));
    assert(Com_ValidXModelName("XMODEL/test"));
    assert(!Com_ValidXModelName("xmodel\\test"));
    assert(!Com_ValidXModelName("xmodels/test"));
    model.parts=(void (*)())&parts; parts.hierarchy=&hierarchy; hierarchy.names=names;
    /* The Mac pointer result must retain the complete native address. */
    assert((uintptr_t)XModelBoneNames(&model)==(uintptr_t)names);
}
