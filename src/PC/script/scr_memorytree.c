#include "common_types.h"
#include "imports.h"

extern struct scrMemTreePub_t scrMemTreePub;
extern unsigned char scrMemTreeGlob[];

extern void *Z_VirtualAllocInternal(int size);
extern void Z_VirtualFreeInternal(void *ptr);
extern void Com_Printf(const char *fmt, ...);
extern void Scr_TerminalError(const char *msg);

unsigned int Scr_GetStringUsage(void);
byte *MT_InitForceAlloc(void);
void MT_Init(void);
void MT_DumpTree(void);
short unsigned int MT_AllocIndex(int numBytes, int type);
unsigned int *MT_Alloc(int numBytes, int type);
void MT_ForceAllocIndex(byte *allocBits, unsigned int nodeNum, int numBytes);
int MT_Realloc(int oldNumBytes, int newNumbytes);
void MT_Free(void *p, int numBytes);
void MT_FreeIndex(unsigned int nodeNum, int numBytes);
void MT_FinishForceAlloc(byte *allocBits);

#define MEMORY_NODE_BITS 16
#define MEMORY_NODE_COUNT 0x10000

#if defined(COD2_X64)
_Static_assert(sizeof(MemoryNode) == 8, "script arena bucket width");
_Static_assert(offsetof(scrMemTreeGlob_t, leftBits) == 0x80000, "script arena extent");
_Static_assert(sizeof(scrMemTreeGlob_t) <= 0x80330, "script arena encoding bound");
#endif

#define NODE_PREV(i) (*(unsigned short *)(scrMemTreeGlob + (unsigned)(i) * 8))
#define NODE_NEXT(i) (*(unsigned short *)(scrMemTreeGlob + (unsigned)(i) * 8 + 2))

#define LOG_BITS(i) ((unsigned char)scrMemTreeGlob[0x80200 + (unsigned char)(i)])

#define MEMTREE_GLOB ((struct scrMemTreeGlob_t *)(scrMemTreeGlob))
#define TOTAL_ALLOC (MEMTREE_GLOB->totalAlloc)
#define TOTAL_ALLOC_BUCKETS (MEMTREE_GLOB->totalAllocBuckets)

static unsigned char mt_size[MEMORY_NODE_COUNT];

static int MT_GetSize(int numBytes)
{
    int numBuckets;
    if (numBytes > 0xFFFF) {
        MT_DumpTree();
        Com_Printf("%s: failed memory allocation of %d bytes for script usage\n",
                   "MT_GetSize: max allocation exceeded", numBytes);
        Scr_TerminalError("failed memory allocation for script usage");
        return 0;
    }
    numBuckets = (numBytes + 7) / 8 - 1;
    if (numBuckets < 0)
        numBuckets = 0;
    if (numBuckets > 255)
        return LOG_BITS((unsigned)numBuckets >> 8) + 8;
    return LOG_BITS(numBuckets);
}

static void MT_InitBits(void)
{
    int i, temp;
    unsigned char bits;

    for (i = 0; i < 256; i++) {

        bits = 0;
        for (temp = i; temp; temp >>= 1)
            if (temp & 1)
                bits++;
        scrMemTreeGlob[0x80100 + i] = bits;

        for (bits = 8; i & ((1 << bits) - 1); bits--) {}
        scrMemTreeGlob[0x80000 + i] = bits;

        bits = 0;
        for (temp = i; temp; temp >>= 1)
            bits++;
        scrMemTreeGlob[0x80200 + i] = bits;
    }
}

static void MT_ListAdd(int b, int size)
{
    unsigned short h = (MEMTREE_GLOB->head[(unsigned)(size)]);
    NODE_PREV(b) = 0;
    NODE_NEXT(b) = h;
    if (h)
        NODE_PREV(h) = (unsigned short)b;
    (MEMTREE_GLOB->head[(unsigned)(size)]) = (unsigned short)b;
    mt_size[b] = (unsigned char)(size + 1);
}

static void MT_ListRemove(int b, int size)
{
    unsigned short p = NODE_PREV(b);
    unsigned short n = NODE_NEXT(b);
    if (p)
        NODE_NEXT(p) = n;
    else
        (MEMTREE_GLOB->head[(unsigned)(size)]) = n;
    if (n)
        NODE_PREV(n) = p;
    mt_size[b] = 0;
}

#if defined(COD2_X64)
/* Mac 1.3 orders each size class by this score, rather than free order. */
static int MT_GetScore(unsigned int node)
{
    unsigned int value = MEMORY_NODE_COUNT - node;
    unsigned int low = value & 255;
    unsigned int high = (value >> 8) & 255;
    unsigned int bits = (unsigned char)scrMemTreeGlob[0x80000 + low];

    if (!low)
        bits += (unsigned char)scrMemTreeGlob[0x80000 + high];
    return (int)value - (unsigned char)scrMemTreeGlob[0x80100 + low]
           - (unsigned char)scrMemTreeGlob[0x80100 + high] + (1 << bits);
}

static void MT_AddMemoryNode(int newNode, int size)
{
    unsigned short *link = &MEMTREE_GLOB->head[size];
    int cursor = 0;
    int step = MEMORY_NODE_COUNT;

    mt_size[newNode] = (unsigned char)(size + 1);
    while (*link) {
        int node = *link;
        if (MT_GetScore(newNode) > MT_GetScore(node)) {
            *link = (unsigned short)newNode;
            NODE_PREV(newNode) = NODE_PREV(node);
            NODE_NEXT(newNode) = NODE_NEXT(node);
            newNode = node;
        }
        step >>= 1;
        if (newNode < cursor) {
            link = &NODE_PREV(*link);
            cursor -= step;
        } else {
            link = &NODE_NEXT(*link);
            cursor += step;
        }
    }
    *link = (unsigned short)newNode;
    NODE_PREV(newNode) = 0;
    NODE_NEXT(newNode) = 0;
}

static void MT_RemoveTreeRoot(unsigned short *link)
{
    unsigned short left = NODE_PREV(*link);
    unsigned short right = NODE_NEXT(*link);

    while (left || right) {
        int useLeft = left && (!right || MT_GetScore(left) > MT_GetScore(right));
        unsigned short node = useLeft ? left : right;
        unsigned short nextLeft = NODE_PREV(node);
        unsigned short nextRight = NODE_NEXT(node);

        *link = node;
        NODE_PREV(node) = left;
        NODE_NEXT(node) = right;
        link = useLeft ? &NODE_PREV(node) : &NODE_NEXT(node);
        left = nextLeft;
        right = nextRight;
    }
    *link = 0;
}

static unsigned int MT_GetSubTreeSize(unsigned int node)
{
    if (!node)
        return 0;
    return 1 + MT_GetSubTreeSize(NODE_PREV(node)) + MT_GetSubTreeSize(NODE_NEXT(node));
}

static void MT_RemoveHeadMemoryNode(int size)
{
    unsigned short node = MEMTREE_GLOB->head[size];
    MT_RemoveTreeRoot(&MEMTREE_GLOB->head[size]);
    mt_size[node] = 0;
}

static int MT_RemoveMemoryNode(int target, int size)
{
    unsigned short *link = &MEMTREE_GLOB->head[size];
    int cursor = 0;
    int step = MEMORY_NODE_COUNT;

    if (target <= 0 || target >= MEMORY_NODE_COUNT ||
        mt_size[target] != (unsigned char)(size + 1))
        return 0;
    while (*link != target) {
        if (!*link || target == cursor)
            return 0;
        step >>= 1;
        if (target < cursor) {
            link = &NODE_PREV(*link);
            cursor -= step;
        } else {
            link = &NODE_NEXT(*link);
            cursor += step;
        }
    }
    MT_RemoveTreeRoot(link);
    mt_size[target] = 0;
    return 1;
}
#else
static void MT_AddMemoryNode(int newNode, int size)
{    MT_ListAdd(newNode, size);
}

static void MT_RemoveHeadMemoryNode(int size)
{
    int b = (MEMTREE_GLOB->head[(unsigned)(size)]);    if (b)
        MT_ListRemove(b, size);
}

static int MT_RemoveMemoryNode(int target, int size)
{
    if (target <= 0 || target >= MEMORY_NODE_COUNT)
        return 0;
    if (mt_size[target] != (unsigned char)(size + 1))
        return 0;    MT_ListRemove(target, size);
    return 1;
}
#endif

int MT_IsNodeCovered(int nodeNum)
{
    int size;
    if (nodeNum <= 0 || nodeNum >= MEMORY_NODE_COUNT)
        return 0;
    for (size = 0; size <= MEMORY_NODE_BITS; size++) {
        int base = nodeNum & ~((1 << size) - 1);
        if (mt_size[base] == (unsigned char)(size + 1))
            return 1;
    }
    return 0;
}

int MT_SearchTreeAny(int nodeNum)
{
    if (nodeNum <= 0 || nodeNum >= MEMORY_NODE_COUNT)
        return 0;
    return mt_size[nodeNum] != 0;
}

void MT_VerifyNotInTree(int nodeNum)
{
    if (nodeNum > 0 && nodeNum < MEMORY_NODE_COUNT && mt_size[nodeNum])
        Com_Printf("TREE BUG: allocated node %d found free (size %d)!\n",
                   nodeNum, mt_size[nodeNum] - 1);
}

unsigned int Scr_GetStringUsage(void)
{
    return TOTAL_ALLOC_BUCKETS;
}

byte *MT_InitForceAlloc(void)
{
    TOTAL_ALLOC = 0;
    TOTAL_ALLOC_BUCKETS = 0;
    return (byte *)Z_VirtualAllocInternal(0x2000);
}

void MT_Init(void)
{
    int i;

    *(char **)&scrMemTreePub = (char *)scrMemTreeGlob;

    MT_InitBits();

    for (i = 0; i <= MEMORY_NODE_BITS; i++)
        (MEMTREE_GLOB->head[(unsigned)(i)]) = 0;
    for (i = 0; i < MEMORY_NODE_COUNT; i++)
        mt_size[i] = 0;

    NODE_PREV(0) = 0;
    NODE_NEXT(0) = 0;

    for (i = 0; i < MEMORY_NODE_BITS; i++)
        MT_AddMemoryNode(1 << i, i);

    TOTAL_ALLOC = 0;
    TOTAL_ALLOC_BUCKETS = 0;
}

void MT_DumpTree(void)
{
    int size, count;
    unsigned short n;

    Com_Printf("********************************\n");
    for (size = 0; size <= MEMORY_NODE_BITS; size++) {
#if defined(COD2_X64)
        count = (int)MT_GetSubTreeSize(MEMTREE_GLOB->head[size]);
        Com_Printf("%d subtree has %d * %d = %d free buckets\n",
#else
        count = 0;
        for (n = (MEMTREE_GLOB->head[(unsigned)(size)]); n; n = NODE_NEXT(n))
            count++;
        Com_Printf("%d list has %d * %d = %d free buckets\n",
#endif
                   size, count, 1 << size, count << size);
    }
    Com_Printf("********************************\n");
#if defined(COD2_X64)
    Com_Printf("********************************\n");
#endif
    Com_Printf("total memory alloc buckets: %d (%d instances)\n",
               TOTAL_ALLOC_BUCKETS, TOTAL_ALLOC);
    Com_Printf("total memory free buckets: %d\n",
               MEMORY_NODE_COUNT - 1 - TOTAL_ALLOC_BUCKETS);
    Com_Printf("********************************\n");
}

short unsigned int MT_AllocIndex(int numBytes, int type)
{
    int size, newSize, nodeNum;

    (void)type;

    size = MT_GetSize(numBytes);
    if (size > MEMORY_NODE_BITS)
        return 0;

    for (newSize = size;; newSize++) {
        if (newSize > MEMORY_NODE_BITS) {
            MT_DumpTree();
            Com_Printf("%s: failed memory allocation of %d bytes for script usage\n",
                       "MT_AllocIndex", numBytes);
            Scr_TerminalError("failed memory allocation for script usage");
            return 0;
        }
        nodeNum = (MEMTREE_GLOB->head[(unsigned)(newSize)]);
        if (nodeNum)
            break;
    }

    MT_RemoveHeadMemoryNode(newSize);

    while (newSize != size) {
        newSize--;
        MT_AddMemoryNode(nodeNum + (1 << newSize), newSize);
    }

    TOTAL_ALLOC++;
    TOTAL_ALLOC_BUCKETS += 1 << size;

    return (unsigned short)nodeNum;
}

unsigned int *MT_Alloc(int numBytes, int type)
{
    unsigned short idx = MT_AllocIndex(numBytes, type);
    return (unsigned int *)(scrMemTreeGlob + (unsigned)idx * 8);
}

void MT_FreeIndex(unsigned int nodeNum, int numBytes)
{
    int size, lowBit;

    size = MT_GetSize(numBytes);
    TOTAL_ALLOC--;
    TOTAL_ALLOC_BUCKETS -= 1 << size;

    while (1) {
        lowBit = 1 << size;
        if (size == MEMORY_NODE_BITS || !MT_RemoveMemoryNode((int)(nodeNum ^ lowBit), size))
            break;
        nodeNum &= ~(unsigned)lowBit;
        size++;
    }

    MT_AddMemoryNode((int)nodeNum, size);
}

void MT_Free(void *p, int numBytes)
{
#if defined(COD2_X64)
    /* MT_Alloc returns a bucket within the fixed 512 KiB arena, not an address. */
    unsigned int nodeIdx = SCR_ARENA_ENC(p) / sizeof(MemoryNode);
#else
    unsigned int nodeIdx = ((unsigned char *)p - scrMemTreeGlob) / 8;
#endif
    MT_FreeIndex(nodeIdx, numBytes);
}

int MT_Realloc(int oldNumBytes, int newNumbytes)
{
    return MT_GetSize(oldNumBytes) >= MT_GetSize(newNumbytes);
}

void MT_ForceAllocIndex(byte *allocBits, unsigned int nodeNum, int numBytes)
{
    int size, count;

    size = MT_GetSize(numBytes);
    TOTAL_ALLOC++;
    count = 1 << size;
    TOTAL_ALLOC_BUCKETS += count;

    while (count) {
        allocBits[nodeNum >> 3] |= (1 << (nodeNum & 7));
        nodeNum++;
        count--;
    }
}

static void MT_SafeFreeIndex(unsigned int nodeNum)
{
    int size = 0, lowBit;

#if defined(COD2_X64)
    if (MT_IsNodeCovered((int)nodeNum))
        return;
#endif
    while (1) {
        lowBit = 1 << size;
        if (size == MEMORY_NODE_BITS || !MT_RemoveMemoryNode((int)(nodeNum ^ lowBit), size))
            break;
        nodeNum &= ~(unsigned)lowBit;
        size++;
    }

    MT_AddMemoryNode((int)nodeNum, size);
}

void MT_FinishForceAlloc(byte *allocBits)
{
    unsigned int nodeNum;

    for (nodeNum = 1; nodeNum < MEMORY_NODE_COUNT; nodeNum++) {
        if (!(allocBits[nodeNum >> 3] >> (nodeNum & 7) & 1))
            MT_SafeFreeIndex(nodeNum);
    }

    Z_VirtualFreeInternal(allocBits);
}
