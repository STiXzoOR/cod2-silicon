#include "common_types.h"
#include "imports.h"

extern void *Hunk_AllocateTempMemoryHighInternal(int size);

uintptr_t node1_(uintptr_t val1);
uintptr_t node_pos(uintptr_t pos);
uintptr_t node0(int type);
uintptr_t node1(int type, uintptr_t val1);
uintptr_t node2(int type, uintptr_t val1, uintptr_t val2);
uintptr_t node2_(uintptr_t val1, uintptr_t val2);
uintptr_t node3(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3);
uintptr_t node4(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4);
uintptr_t node5(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5);
uintptr_t node6(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6);
uintptr_t node7(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6, uintptr_t val7);
uintptr_t node8(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6, uintptr_t val7, uintptr_t val8);
uintptr_t linked_list_end(uintptr_t val);
uintptr_t prepend_node(uintptr_t val1, uintptr_t val2);
uintptr_t append_node(uintptr_t val1, uintptr_t val2);

uintptr_t node1_(uintptr_t val1)
{
    return val1;
}

uintptr_t node_pos(uintptr_t pos)
{
    return pos;
}

uintptr_t node0(int type)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(sizeof(intptr_t));
    n[0] = type;
    return (uintptr_t)n;
}

uintptr_t node1(int type, uintptr_t val1)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    return (uintptr_t)n;
}

uintptr_t node2(int type, uintptr_t val1, uintptr_t val2)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(3 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    return (uintptr_t)n;
}

uintptr_t node2_(uintptr_t val1, uintptr_t val2)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    n[0] = (intptr_t)val1;
    n[1] = (intptr_t)val2;
    return (uintptr_t)n;
}

uintptr_t node3(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(4 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    return (uintptr_t)n;
}

uintptr_t node4(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(5 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    n[4] = (intptr_t)val4;
    return (uintptr_t)n;
}

uintptr_t node5(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(6 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    n[4] = (intptr_t)val4;
    n[5] = (intptr_t)val5;
    return (uintptr_t)n;
}

uintptr_t node6(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(7 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    n[4] = (intptr_t)val4;
    n[5] = (intptr_t)val5;
    n[6] = (intptr_t)val6;
    return (uintptr_t)n;
}

uintptr_t node7(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6, uintptr_t val7)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(8 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    n[4] = (intptr_t)val4;
    n[5] = (intptr_t)val5;
    n[6] = (intptr_t)val6;
    n[7] = (intptr_t)val7;
    return (uintptr_t)n;
}

uintptr_t node8(int type, uintptr_t val1, uintptr_t val2, uintptr_t val3, uintptr_t val4, uintptr_t val5, uintptr_t val6, uintptr_t val7, uintptr_t val8)
{
    intptr_t *n = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(9 * sizeof(intptr_t));
    n[0] = type;
    n[1] = (intptr_t)val1;
    n[2] = (intptr_t)val2;
    n[3] = (intptr_t)val3;
    n[4] = (intptr_t)val4;
    n[5] = (intptr_t)val5;
    n[6] = (intptr_t)val6;
    n[7] = (intptr_t)val7;
    n[8] = (intptr_t)val8;
    return (uintptr_t)n;
}

uintptr_t linked_list_end(uintptr_t val)
{
    intptr_t *node = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    node[0] = (intptr_t)val;
    node[1] = 0;
    intptr_t *list = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    list[0] = (intptr_t)node;
    list[1] = (intptr_t)node;
    return (uintptr_t)list;
}

uintptr_t prepend_node(uintptr_t val1, uintptr_t val2)
{
    intptr_t *list = (intptr_t *)val2;
    intptr_t *newNode = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    newNode[0] = (intptr_t)val1;
    newNode[1] = list[0];
    list[0] = (intptr_t)newNode;
    return val2;
}

uintptr_t append_node(uintptr_t val1, uintptr_t val2)
{
    intptr_t *list = (intptr_t *)val1;
    intptr_t *newNode = (intptr_t *)Hunk_AllocateTempMemoryHighInternal(2 * sizeof(intptr_t));
    newNode[0] = (intptr_t)val2;
    newNode[1] = 0;
    intptr_t *oldTail = (intptr_t *)list[1];
    oldTail[1] = (intptr_t)newNode;
    list[1] = (intptr_t)newNode;
    return val1;
}
