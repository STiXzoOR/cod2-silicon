#ifndef COD2_MACOS_CPP_ABI_H
#define COD2_MACOS_CPP_ABI_H

#include <stddef.h>

/* The reconstructed C++ objects use the GCC 4 COW ABI, not libc++ objects.
 * Native pointers and size_t fields are required even though the symbol names
 * came from i386. This header must not be used to describe serialized data. */
typedef struct {
    char *data;
} Cod2MacString;

typedef struct {
    size_t length;
    size_t capacity;
    int references; /* -1: leaked, 0: unique, n > 0: n + 1 owners. */
} Cod2MacStringRep;

typedef struct Cod2MacTreeNode {
    int color; /* GCC ABI: red = 0, black = 1. */
    struct Cod2MacTreeNode *parent;
    struct Cod2MacTreeNode *left;
    struct Cod2MacTreeNode *right;
} Cod2MacTreeNode;

typedef struct Cod2MacListNode {
    struct Cod2MacListNode *next;
    struct Cod2MacListNode *prev;
} Cod2MacListNode;

#ifdef __cplusplus
extern "C" {
#endif

void *__Znwm(size_t size);
void *__Znam(size_t size);
void __ZdlPv(void *pointer);
void __ZdaPv(void *pointer);
int __ZN9__gnu_cxx18__exchange_and_addEPVii(volatile int *word, int delta);
size_t __ZNKSs4findEPKcmm(const Cod2MacString *string, const char *text, size_t position, size_t length);
int __ZNKSs7compareEPKc(const Cod2MacString *string, const char *text);
void __ZNSs12_M_leak_hardEv(Cod2MacString *string);
void __ZNSs4_Rep10_M_destroyERKSaIcE(Cod2MacStringRep *rep, const void *allocator);
Cod2MacString *__ZNSs6appendEPKcm(Cod2MacString *string, const char *text, size_t length);
Cod2MacString *__ZNSs6appendERKSs(Cod2MacString *string, const Cod2MacString *other);
Cod2MacString *__ZNSs6assignEPKcm(Cod2MacString *string, const char *text, size_t length);
Cod2MacString *__ZNSs6assignERKSs(Cod2MacString *string, const Cod2MacString *other);
Cod2MacString *__ZNSs7replaceEmmPKcm(Cod2MacString *string, size_t position, size_t removed, const char *text, size_t length);
void __ZNSs7reserveEm(Cod2MacString *string, size_t capacity);
void __ZNSs9_M_mutateEmmm(Cod2MacString *string, size_t position, size_t removed, size_t inserted);
void __ZNSsC1EPKcRKSaIcE(Cod2MacString *string, const char *text, const void *allocator);
void __ZNSsC1ERKSs(Cod2MacString *string, const Cod2MacString *other);
void __ZNSsC1ERKSsmm(Cod2MacString *string, const Cod2MacString *other, size_t position, size_t length);
void __ZNSsD1Ev(Cod2MacString *string);
void __ZNSt15_List_node_base4hookEPS_(Cod2MacListNode *node, Cod2MacListNode *position);
void __ZNSt15_List_node_base6unhookEv(Cod2MacListNode *node);
Cod2MacTreeNode *__ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base(Cod2MacTreeNode *node);
Cod2MacTreeNode *__ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base(Cod2MacTreeNode *node);
void __ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(int insert_left, Cod2MacTreeNode *node, Cod2MacTreeNode *parent, Cod2MacTreeNode *header);
Cod2MacTreeNode *__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_(Cod2MacTreeNode *node, Cod2MacTreeNode *header);

#ifdef __cplusplus
}
#endif
#endif
