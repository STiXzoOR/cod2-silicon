#include "../../src/platform/macos_cpp_abi.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

extern "C" {
size_t _ZNKSs4findEPKcmm(const Cod2MacString *, const char *, size_t, size_t);
unsigned int _ZNKSs4findEPKcjj(const Cod2MacString *, const char *, unsigned int, unsigned int);
Cod2MacString *_ZNSs6appendEPKcj(Cod2MacString *, const char *, unsigned int);
Cod2MacString *_ZNSs6assignEPKcj(Cod2MacString *, const char *, unsigned int);
Cod2MacString *_ZNSs7replaceEjjPKcj(Cod2MacString *, unsigned int, unsigned int, const char *, unsigned int);
void _ZNSs7reserveEj(Cod2MacString *, unsigned int);
void _ZNSs9_M_mutateEjjj(Cod2MacString *, unsigned int, unsigned int, unsigned int);
void _ZNSsC1ERKSsjj(Cod2MacString *, const Cod2MacString *, unsigned int, unsigned int);
void _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(bool, Cod2MacTreeNode *, Cod2MacTreeNode *, Cod2MacTreeNode *);
void *__Znwm(size_t);
void *__Znam(size_t);
void __ZdlPv(void *);
void __ZdaPv(void *);
void *___cxa_allocate_exception(size_t);
[[noreturn]] void ___cxa_throw(void *, std::type_info *, void (*)(void *));
[[noreturn]] void ___cxa_rethrow();
void *___dynamic_cast(const void *, const void *, const void *, ptrdiff_t);
[[noreturn]] void __ZSt17__throw_bad_allocv();
[[noreturn]] void __ZSt20__throw_length_errorPKc(const char *);
[[noreturn]] void __ZSt20__throw_out_of_rangePKc(const char *);
extern void *const __ZNSs4_Rep20_S_empty_rep_storageE;
extern const char __ZNSs4_Rep11_S_terminalE;
}

static const Cod2MacStringRep *rep(const Cod2MacString &string)
{
    return reinterpret_cast<const Cod2MacStringRep *>(string.data) - 1;
}

static void check_string(const Cod2MacString &string, const std::string &expected)
{
    assert(rep(string)->length == expected.size());
    assert(rep(string)->capacity >= expected.size());
    assert(std::memcmp(string.data, expected.data(), expected.size()) == 0);
    assert(string.data[expected.size()] == 0);
}

static void string_test()
{
    Cod2MacString a, b, c;
    __ZNSsC1EPKcRKSaIcE(&a, "abcdef", nullptr);
    __ZNSsC1ERKSs(&b, &a);
    assert(a.data == b.data && rep(a)->references == 1);
    assert(__ZNSs6appendEPKcm(&a, a.data + 2, 3) == &a);
    check_string(a, "abcdefcde");
    check_string(b, "abcdef");
    assert(a.data != b.data && rep(b)->references == 0);
    __ZNSsC1ERKSsmm(&c, &a, 2, 4);
    check_string(c, "cdef");
    __ZNSs6assignERKSs(&c, &b);
    assert(c.data == b.data);
    __ZNSs12_M_leak_hardEv(&b);
    assert(rep(b)->references == -1 && rep(c)->references == 0);
    b.data[0] = 'B';
    check_string(c, "abcdef");
    __ZNSs6assignERKSs(&a, &b);
    assert(a.data != b.data); /* Leaked reps cannot be shared. */
    check_string(a, "Bbcdef");
    __ZNSs6appendERKSs(&a, &a);
    check_string(a, "BbcdefBbcdef");
    __ZNSs7replaceEmmPKcm(&a, 2, 5, a.data + 1, 7);
    check_string(a, "BbbcdefBbbcdef");
    __ZNSs6assignEPKcm(&a, a.data + 3, 4);
    check_string(a, "cdef");
    assert(__ZNKSs4findEPKcmm(&a, "ef", 0, 2) == 2);
    assert(_ZNKSs4findEPKcmm(&a, "ef", 0, 2) == 2);
    assert(_ZNKSs4findEPKcjj(&a, "q", 0, 1) == ~0u);
    assert(__ZNKSs4findEPKcmm(&a, "", 4, 0) == 4);
    assert(__ZNKSs4findEPKcmm(&a, "", 5, 0) == size_t(-1));
    assert(__ZNKSs7compareEPKc(&a, "cdef") == 0);
    assert(__ZNKSs7compareEPKc(&a, "cde") > 0);
    assert(__ZNKSs7compareEPKc(&a, "cdeg") < 0);
    __ZNSsD1Ev(&a);
    __ZNSsD1Ev(&b);
    __ZNSsD1Ev(&c);

    __ZNSsC1EPKcRKSaIcE(&a, "", nullptr);
    assert(rep(a) == __ZNSs4_Rep20_S_empty_rep_storageE);
    assert(__ZNSs4_Rep11_S_terminalE == 0);
    __ZNSsC1ERKSs(&b, &a);
    __ZNSs12_M_leak_hardEv(&a);
    assert(a.data == b.data && rep(a)->references == 0);
    __ZNSs6appendEPKcm(&a, ";", 1);
    check_string(a, ";");
    check_string(b, "");
    __ZNSsD1Ev(&a);
    __ZNSsD1Ev(&b);

    const char binary[] = { 'a', 0, 'b', 0 };
    __ZNSsC1EPKcRKSaIcE(&a, "", nullptr);
    _ZNSs6assignEPKcj(&a, binary, sizeof(binary));
    check_string(a, std::string(binary, sizeof(binary)));
    assert(_ZNKSs4findEPKcjj(&a, binary + 1, 0, 2) == 1);
    _ZNSs6appendEPKcj(&a, "!", 1);
    _ZNSs7replaceEjjPKcj(&a, 1, 2, "XY", 2);
    check_string(a, std::string("aXY\0!", 5));
    _ZNSs7reserveEj(&a, 500);
    _ZNSs9_M_mutateEjjj(&a, 1, 2, 1);
    a.data[1] = '#';
    check_string(a, std::string("a#\0!", 4));
    _ZNSsC1ERKSsjj(&b, &a, 1, 3);
    check_string(b, std::string("#\0!", 3));
    __ZNSsD1Ev(&a);
    __ZNSsD1Ev(&b);

    std::mt19937 random(0xC0D2);
    std::array<Cod2MacString, 8> actual;
    std::array<std::string, 8> model;
    for (auto &string : actual) {
        __ZNSsC1EPKcRKSaIcE(&string, "", nullptr);
    }
    for (unsigned i = 0; i < 12000; ++i) {
        unsigned slot = random() % actual.size();
        unsigned other = random() % actual.size();
        auto &string = actual[slot];
        auto &value = model[slot];
        std::string text(random() % 24, char('a' + random() % 26));
        if (value.size() > 1500) {
            __ZNSs6assignEPKcm(&string, "", 0);
            value.clear();
        }
        switch (random() % 8) {
        case 0:
            __ZNSs6appendEPKcm(&string, text.data(), text.size());
            value.append(text);
            break;
        case 1:
            __ZNSs6assignERKSs(&string, &actual[other]);
            value = model[other];
            break;
        case 2: {
            size_t position = random() % (value.size() + 1);
            size_t removed = random() % 40;
            __ZNSs7replaceEmmPKcm(&string, position, removed, text.data(), text.size());
            value.replace(position, removed, text);
            break;
        }
        case 3: {
            size_t start = random() % (value.size() + 1);
            size_t count = random() % (value.size() - start + 1);
            text = value.substr(start, count);
            __ZNSs6appendEPKcm(&string, string.data + start, count);
            value.append(text);
            break;
        }
        case 4: {
            size_t start = random() % (value.size() + 1);
            size_t count = random() % (value.size() - start + 1);
            text = value.substr(start, count);
            size_t position = random() % (value.size() + 1);
            __ZNSs7replaceEmmPKcm(&string, position, 5, string.data + start, count);
            value.replace(position, 5, text);
            break;
        }
        case 5:
            __ZNSs7reserveEm(&string, random() % 500);
            break;
        case 6:
            __ZNSs12_M_leak_hardEv(&string);
            if (!value.empty()) {
                string.data[0] = value[0] = 'Z';
            }
            break;
        case 7:
            __ZNSs6assignEPKcm(&string, text.data(), text.size());
            value.assign(text);
            break;
        }
        for (unsigned j = 0; j < actual.size(); ++j) {
            check_string(actual[j], model[j]);
        }
        size_t position = random() % (value.size() + 2);
        assert(__ZNKSs4findEPKcmm(&string, text.data(), position, text.size()) == value.find(text, position));
    }
    for (auto &string : actual) {
        __ZNSsD1Ev(&string);
    }
    std::puts("COW strings: aliasing, leak, copy, find, mutate; 12000 differential operations passed");
}

struct TreeValue {
    Cod2MacTreeNode base;
    int key;
};

static int black_height(Cod2MacTreeNode *node, Cod2MacTreeNode *parent)
{
    if (!node) {
        return 1;
    }
    assert(node->parent == parent);
    assert(node->color == 0 || node->color == 1);
    if (!node->color) {
        assert(!node->left || node->left->color == 1);
        assert(!node->right || node->right->color == 1);
    }
    int left = black_height(node->left, node);
    int right = black_height(node->right, node);
    assert(left == right);
    return left + node->color;
}

static void check_tree(Cod2MacTreeNode &header, const std::multiset<int> &model)
{
    assert(header.color == 0);
    if (model.empty()) {
        assert(!header.parent && header.left == &header && header.right == &header);
        return;
    }
    assert(header.parent->color == 1);
    black_height(header.parent, &header);
    auto *node = header.left;
    for (int key : model) {
        assert(node && node != &header);
        assert(reinterpret_cast<TreeValue *>(node)->key == key);
        node = __ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base(node);
    }
    assert(node == &header);
    node = &header;
    for (auto i = model.rbegin(); i != model.rend(); ++i) {
        node = __ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base(node);
        assert(node && node != &header);
        assert(reinterpret_cast<TreeValue *>(node)->key == *i);
    }
    assert(node == header.left);
    assert(reinterpret_cast<TreeValue *>(header.left)->key == *model.begin());
    assert(reinterpret_cast<TreeValue *>(header.right)->key == *model.rbegin());
}

static void insert_value(Cod2MacTreeNode &header, TreeValue &value)
{
    auto *parent = &header;
    auto *node = header.parent;
    bool left = true;
    while (node) {
        parent = node;
        left = value.key < reinterpret_cast<TreeValue *>(node)->key;
        node = left ? node->left : node->right;
    }
    if (value.key & 1) {
        _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(left, &value.base, parent, &header);
    } else {
        __ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(left, &value.base, parent, &header);
    }
}

static void tree_test()
{
    for (unsigned seed = 0; seed < 128; ++seed) {
        std::mt19937 random(seed);
        Cod2MacTreeNode header = {};
        header.left = header.right = &header;
        std::vector<TreeValue> values(256);
        std::vector<unsigned> order(values.size());
        std::iota(order.begin(), order.end(), 0);
        std::multiset<int> model;
        for (unsigned j = 0; j < values.size(); ++j) {
            values[j].key = seed & 1 ? j : random() % 71; /* Duplicates and unique keys. */
        }
        std::shuffle(order.begin(), order.end(), random);
        for (unsigned j : order) {
            insert_value(header, values[j]);
            model.insert(values[j].key);
            check_tree(header, model);
        }
        std::shuffle(order.begin(), order.end(), random);
        for (unsigned j : order) {
            auto *removed = __ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_(&values[j].base, &header);
            assert(removed == &values[j].base); /* Existing iterators retain identity. */
            model.erase(model.find(values[j].key));
            check_tree(header, model);
        }
    }
    /* Every insertion/erase ordering for five unique nodes, checking invariants
     * after each operation. Includes leaf, one-child, two-child and root erases. */
    std::array<unsigned, 5> insertion = { 0, 1, 2, 3, 4 };
    do {
        std::array<unsigned, 5> deletion = { 0, 1, 2, 3, 4 };
        do {
            Cod2MacTreeNode header = {};
            header.left = header.right = &header;
            std::array<TreeValue, 5> values = {};
            std::multiset<int> model;
            for (unsigned j : insertion) {
                values[j].key = j;
                insert_value(header, values[j]);
                model.insert(j);
                check_tree(header, model);
            }
            for (unsigned j : deletion) {
                assert(__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_(&values[j].base, &header) == &values[j].base);
                model.erase(j);
                check_tree(header, model);
            }
        } while (std::next_permutation(deletion.begin(), deletion.end()));
    } while (std::next_permutation(insertion.begin(), insertion.end()));
    std::puts("RB trees: 128 randomized 256-node runs + all 14400 five-node insertion/erase orders passed");
}

static void list_test()
{
    Cod2MacListNode head = { &head, &head }, a, b, c;
    __ZNSt15_List_node_base4hookEPS_(&a, &head);
    __ZNSt15_List_node_base4hookEPS_(&b, &head);
    __ZNSt15_List_node_base4hookEPS_(&c, &b);
    assert(head.next == &a && a.next == &c && c.next == &b && b.next == &head);
    assert(head.prev == &b && b.prev == &c && c.prev == &a && a.prev == &head);
    __ZNSt15_List_node_base6unhookEv(&c);
    assert(a.next == &b && b.prev == &a);
    __ZNSt15_List_node_base6unhookEv(&a);
    __ZNSt15_List_node_base6unhookEv(&b);
    assert(head.next == &head && head.prev == &head);
    std::puts("Lists: hook/unhook at beginning, middle and end passed");
}

struct Left { virtual ~Left() = default; int left = 31; };
struct Right { virtual ~Right() = default; int right = 47; };
struct Both : Left, Right { int both = 63; };
struct Other : Left {};

static unsigned destroyed;
struct ExceptionPayload { uint64_t words[4]; };
static void destroy_payload(void *payload)
{
    assert(static_cast<ExceptionPayload *>(payload)->words[3] == 0x123456789ABCDEF0ULL);
    ++destroyed;
}

static void runtime_test()
{
    void *one = __Znwm(23);
    void *many = __Znam(31);
    std::memset(one, 1, 23);
    std::memset(many, 2, 31);
    __ZdlPv(one);
    __ZdaPv(many);
    volatile int atomic = 3;
    assert(__ZN9__gnu_cxx18__exchange_and_addEPVii(&atomic, 4) == 3 && atomic == 7);
    Both object;
    Left *left = &object;
    Right *right = &object;
    assert(static_cast<void *>(left) != static_cast<void *>(right));
    assert(___dynamic_cast(left, &typeid(Left), &typeid(Right), -1) == right);
    assert(___dynamic_cast(left, &typeid(Left), &typeid(Both), 0) == &object);
    assert(___dynamic_cast(right, &typeid(Right), &typeid(Other), -1) == nullptr);
    try {
        auto *payload = static_cast<ExceptionPayload *>(___cxa_allocate_exception(sizeof(ExceptionPayload)));
        *payload = { { 7, 8, 9, 0x123456789ABCDEF0ULL } };
        ___cxa_throw(payload, const_cast<std::type_info *>(&typeid(ExceptionPayload)), destroy_payload);
    } catch (const ExceptionPayload &payload) {
        assert(payload.words[0] == 7 && payload.words[3] == 0x123456789ABCDEF0ULL);
        try {
            ___cxa_rethrow();
        } catch (const ExceptionPayload &again) {
            assert(&payload == &again);
        }
    }
    assert(destroyed == 1);
    try { __ZSt17__throw_bad_allocv(); } catch (const std::bad_alloc &) { ++destroyed; }
    try { __ZSt20__throw_length_errorPKc("length"); } catch (const std::length_error &e) { assert(std::strcmp(e.what(), "length") == 0); ++destroyed; }
    try { __ZSt20__throw_out_of_rangePKc("range"); } catch (const std::out_of_range &e) { assert(std::strcmp(e.what(), "range") == 0); ++destroyed; }
    assert(destroyed == 4);
    Cod2MacString string;
    __ZNSsC1EPKcRKSaIcE(&string, "x", nullptr);
    try { __ZNSs7reserveEm(&string, std::numeric_limits<size_t>::max()); } catch (const std::length_error &) { ++destroyed; }
    try { __ZNSs7replaceEmmPKcm(&string, 2, 0, "", 0); } catch (const std::out_of_range &) { ++destroyed; }
    assert(destroyed == 6);
    check_string(string, "x");
    __ZNSsD1Ev(&string);
    std::puts("libc++abi: allocation, typed exception/rethrow/destructor, MI crosscast and failing cast passed");
}

#if defined(COD2_CPP_TEST_STRINGED)
extern "C" {
int giFilesFound;
int SE_BuildFileList(const char *, Cod2MacString *);
static unsigned freed_lists;
int FS_ReadFile(const char *, void **) { return 0; }
void FS_FreeFile(void *) {}
const char **FS_ListFiles(const char *path, const char *extension, int, int *count, int)
{
    static const char *directories[] = { ".", "", "sub" };
    static const char *root_files[] = { "a.str", "b.str" };
    static const char *sub_files[] = { "c.str" };
    if (std::strcmp(extension, "/") == 0) {
        *count = std::strcmp(path, "root") == 0 ? 3 : 0;
        return directories;
    }
    assert(std::strcmp(extension, "str") == 0);
    if (std::strcmp(path, "root") == 0) {
        *count = 2;
        return root_files;
    }
    assert(std::strcmp(path, "root/sub") == 0);
    *count = 1;
    return sub_files;
}
void FS_FreeFileList(const char **, int) { ++freed_lists; }
}

static void stringed_test()
{
    Cod2MacString result, shared;
    __ZNSsC1EPKcRKSaIcE(&result, "before", nullptr);
    __ZNSsC1ERKSs(&shared, &result);
    assert(SE_BuildFileList("root", &result) == 3);
    check_string(result, "root/sub/c.str;root/a.str;root/b.str;");
    check_string(shared, "before");
    assert(freed_lists == 4);
    __ZNSsD1Ev(&result);
    __ZNSsD1Ev(&shared);
    std::puts("Engine SE_BuildFileList: recursive listing, delimiters and COW detach passed");
}
#endif

int main()
{
    static_assert(sizeof(void *) == 8, "native-only fixture");
    static_assert(sizeof(Cod2MacString) == 8 && sizeof(Cod2MacStringRep) == 24, "native COW ABI");
    static_assert(sizeof(Cod2MacTreeNode) == 32 && sizeof(Cod2MacListNode) == 16, "native container ABI");
    string_test();
    tree_test();
    list_test();
    runtime_test();
#if defined(COD2_CPP_TEST_STRINGED)
    stringed_test();
#endif
    std::puts("Native C++ compatibility checks passed");
}
