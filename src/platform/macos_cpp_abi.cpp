#if defined(__APPLE__) && defined(COD2_X64)

#include "macos_cpp_abi.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cxxabi.h>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unwind.h>

/* Only the old GCC ABI operations called by reconstructed C are exported.
 * libc++ owns native new/delete, exceptions and RTTI; its std::string and
 * container layouts must never be substituted for these objects. */
struct Cod2MacEmptyString {
    Cod2MacStringRep rep;
    char terminal[sizeof(size_t)];
};

extern "C" {
Cod2MacEmptyString _ZNSs4_Rep20_S_empty_rep_storageE = {};
extern void *const __ZNSs4_Rep20_S_empty_rep_storageE = &_ZNSs4_Rep20_S_empty_rep_storageE;
extern const char _ZNSs4_Rep11_S_terminalE = 0;
extern const char __ZNSs4_Rep11_S_terminalE = 0;
}

namespace {

Cod2MacStringRep *empty_rep()
{
    return &_ZNSs4_Rep20_S_empty_rep_storageE.rep;
}

char *rep_data(Cod2MacStringRep *rep)
{
    return reinterpret_cast<char *>(rep + 1);
}

Cod2MacStringRep *string_rep(const Cod2MacString *string)
{
    return string->data ? reinterpret_cast<Cod2MacStringRep *>(string->data) - 1 : empty_rep();
}

int owners(const Cod2MacStringRep *rep)
{
    return __atomic_load_n(&rep->references, __ATOMIC_ACQUIRE);
}

size_t max_length()
{
    return (std::numeric_limits<size_t>::max() - sizeof(Cod2MacStringRep) - 1) / 4;
}

Cod2MacStringRep *allocate_rep(size_t length, size_t capacity)
{
    if (capacity > max_length()) {
        throw std::length_error("basic_string capacity");
    }
    auto *rep = static_cast<Cod2MacStringRep *>(::operator new(sizeof(Cod2MacStringRep) + capacity + 1));
    rep->length = length;
    rep->capacity = capacity;
    rep->references = 0;
    rep_data(rep)[length] = 0;
    return rep;
}

void dispose_rep(Cod2MacStringRep *rep)
{
    if (rep != empty_rep() && __atomic_fetch_sub(&rep->references, 1, __ATOMIC_ACQ_REL) <= 0) {
        ::operator delete(rep);
    }
}

Cod2MacStringRep *clone_rep(const Cod2MacStringRep *rep, size_t capacity)
{
    auto *copy = allocate_rep(rep->length, capacity);
    std::memcpy(rep_data(copy), rep + 1, rep->length);
    return copy;
}

Cod2MacStringRep *grab_rep(Cod2MacStringRep *rep)
{
    if (rep == empty_rep()) {
        return rep;
    }
    if (owners(rep) < 0) {
        return clone_rep(rep, rep->length);
    }
    __atomic_fetch_add(&rep->references, 1, __ATOMIC_ACQ_REL);
    return rep;
}

size_t checked_length(size_t old_length, size_t removed, size_t inserted)
{
    if (inserted > max_length() - (old_length - removed)) {
        throw std::length_error("basic_string length");
    }
    return old_length - removed + inserted;
}

size_t grown_capacity(size_t old_capacity, size_t required)
{
    if (required > old_capacity && old_capacity <= max_length() / 2) {
        return std::max(required, old_capacity * 2);
    }
    return std::max(required, old_capacity);
}

void mutate(Cod2MacString *string, size_t position, size_t removed, size_t inserted)
{
    auto *rep = string_rep(string);
    if (position > rep->length) {
        throw std::out_of_range("basic_string position");
    }
    removed = std::min(removed, rep->length - position);
    size_t length = checked_length(rep->length, removed, inserted);
    size_t tail = rep->length - position - removed;
    if (rep == empty_rep() && length == 0) {
        string->data = rep_data(rep);
        return;
    }
    if (rep == empty_rep() || owners(rep) > 0 || length > rep->capacity) {
        auto *copy = allocate_rep(length, grown_capacity(rep->capacity, length));
        std::memcpy(rep_data(copy), rep_data(rep), position);
        std::memcpy(rep_data(copy) + position + inserted, rep_data(rep) + position + removed, tail);
        dispose_rep(rep);
        string->data = rep_data(copy);
        rep = copy;
    } else {
        string->data = rep_data(rep);
        std::memmove(string->data + position + inserted, string->data + position + removed, tail);
    }
    rep->length = length;
    __atomic_store_n(&rep->references, 0, __ATOMIC_RELEASE);
    string->data[length] = 0;
}

Cod2MacString *replace(Cod2MacString *string, size_t position, size_t removed, const char *text, size_t length)
{
    auto *rep = string_rep(string);
    if (position > rep->length) {
        throw std::out_of_range("basic_string position");
    }
    removed = std::min(removed, rep->length - position);
    checked_length(rep->length, removed, length);
    if (!text && length) {
        throw std::logic_error("basic_string null source");
    }
    std::string snapshot;
    uintptr_t start = reinterpret_cast<uintptr_t>(rep_data(rep));
    uintptr_t source = reinterpret_cast<uintptr_t>(text);
    if (length && source >= start && source - start <= rep->length) {
        snapshot.assign(text, length);
        text = snapshot.data();
    }
    mutate(string, position, removed, length);
    if (length) {
        std::memcpy(string->data + position, text, length);
    }
    return string;
}

void reserve(Cod2MacString *string, size_t requested)
{
    auto *rep = string_rep(string);
    if (requested > max_length()) {
        throw std::length_error("basic_string reserve");
    }
    size_t capacity = std::max(requested, rep->length);
    if (owners(rep) > 0 || capacity != rep->capacity) {
        auto *copy = clone_rep(rep, capacity);
        dispose_rep(rep);
        string->data = rep_data(copy);
    } else {
        string->data = rep_data(rep);
    }
}

size_t find(const Cod2MacString *string, const char *text, size_t position, size_t length)
{
    auto *rep = string_rep(string);
    if (position > rep->length || length > rep->length - position) {
        return size_t(-1);
    }
    if (!length) {
        return position;
    }
    const char *data = rep_data(rep);
    for (size_t i = position; i <= rep->length - length; ++i) {
        if (std::memcmp(data + i, text, length) == 0) {
            return i;
        }
    }
    return size_t(-1);
}

int compare(const Cod2MacString *string, const char *text)
{
    auto *rep = string_rep(string);
    size_t length = std::strlen(text);
    int result = std::memcmp(rep_data(rep), text, std::min(length, rep->length));
    if (result) {
        return result;
    }
    if (rep->length >= length) {
        return static_cast<int>(std::min(rep->length - length, size_t(std::numeric_limits<int>::max())));
    }
    return -static_cast<int>(std::min(length - rep->length, size_t(std::numeric_limits<int>::max())));
}

void construct(Cod2MacString *string, const char *text, size_t length)
{
    if (!length) {
        string->data = rep_data(empty_rep());
        return;
    }
    auto *rep = allocate_rep(length, length);
    std::memcpy(rep_data(rep), text, length);
    string->data = rep_data(rep);
}

Cod2MacTreeNode *minimum(Cod2MacTreeNode *node)
{
    while (node->left) {
        node = node->left;
    }
    return node;
}

Cod2MacTreeNode *maximum(Cod2MacTreeNode *node)
{
    while (node->right) {
        node = node->right;
    }
    return node;
}

bool black(const Cod2MacTreeNode *node)
{
    return !node || node->color == 1;
}

void rotate_left(Cod2MacTreeNode *node, Cod2MacTreeNode *header)
{
    auto *right = node->right;
    node->right = right->left;
    if (right->left) {
        right->left->parent = node;
    }
    right->parent = node->parent;
    if (node->parent == header) {
        header->parent = right;
    } else if (node == node->parent->left) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }
    right->left = node;
    node->parent = right;
}

void rotate_right(Cod2MacTreeNode *node, Cod2MacTreeNode *header)
{
    auto *left = node->left;
    node->left = left->right;
    if (left->right) {
        left->right->parent = node;
    }
    left->parent = node->parent;
    if (node->parent == header) {
        header->parent = left;
    } else if (node == node->parent->right) {
        node->parent->right = left;
    } else {
        node->parent->left = left;
    }
    left->right = node;
    node->parent = left;
}

void insert_node(int insert_left, Cod2MacTreeNode *node, Cod2MacTreeNode *parent, Cod2MacTreeNode *header)
{
    node->color = 0;
    node->parent = parent;
    node->left = node->right = nullptr;
    if (parent == header) {
        header->parent = header->left = header->right = node;
    } else if (insert_left) {
        parent->left = node;
        if (parent == header->left) {
            header->left = node;
        }
    } else {
        parent->right = node;
        if (parent == header->right) {
            header->right = node;
        }
    }
    while (node->parent != header && node->parent->color == 0) {
        auto *parent_node = node->parent;
        auto *grandparent = parent_node->parent;
        bool left_side = parent_node == grandparent->left;
        auto *uncle = left_side ? grandparent->right : grandparent->left;
        if (!black(uncle)) {
            parent_node->color = uncle->color = 1;
            grandparent->color = 0;
            node = grandparent;
            continue;
        }
        if (left_side) {
            if (node == parent_node->right) {
                node = parent_node;
                rotate_left(node, header);
                parent_node = node->parent;
            }
            parent_node->color = 1;
            grandparent->color = 0;
            rotate_right(grandparent, header);
        } else {
            if (node == parent_node->left) {
                node = parent_node;
                rotate_right(node, header);
                parent_node = node->parent;
            }
            parent_node->color = 1;
            grandparent->color = 0;
            rotate_left(grandparent, header);
        }
    }
    header->parent->color = 1;
}

void transplant(Cod2MacTreeNode *node, Cod2MacTreeNode *replacement, Cod2MacTreeNode *header)
{
    if (node->parent == header) {
        header->parent = replacement;
    } else if (node == node->parent->left) {
        node->parent->left = replacement;
    } else {
        node->parent->right = replacement;
    }
    if (replacement) {
        replacement->parent = node->parent;
    }
}

void repair_erase(Cod2MacTreeNode *node, Cod2MacTreeNode *parent, Cod2MacTreeNode *header)
{
    while (node != header->parent && black(node)) {
        bool left_side = node == parent->left;
        auto *sibling = left_side ? parent->right : parent->left;
        if (!black(sibling)) {
            sibling->color = 1;
            parent->color = 0;
            if (left_side) {
                rotate_left(parent, header);
            } else {
                rotate_right(parent, header);
            }
            sibling = left_side ? parent->right : parent->left;
        }
        if (!sibling || (black(sibling->left) && black(sibling->right))) {
            if (sibling) {
                sibling->color = 0;
            }
            node = parent;
            parent = node->parent;
            continue;
        }
        auto *far_child = left_side ? sibling->right : sibling->left;
        if (black(far_child)) {
            auto *near_child = left_side ? sibling->left : sibling->right;
            near_child->color = 1;
            sibling->color = 0;
            if (left_side) {
                rotate_right(sibling, header);
            } else {
                rotate_left(sibling, header);
            }
            sibling = left_side ? parent->right : parent->left;
        }
        sibling->color = parent->color;
        parent->color = 1;
        (left_side ? sibling->right : sibling->left)->color = 1;
        if (left_side) {
            rotate_left(parent, header);
        } else {
            rotate_right(parent, header);
        }
        node = header->parent;
        break;
    }
    if (node) {
        node->color = 1;
    }
}

Cod2MacTreeNode *erase_node(Cod2MacTreeNode *node, Cod2MacTreeNode *header)
{
    auto *successor = node;
    int removed_color = node->color;
    Cod2MacTreeNode *child;
    Cod2MacTreeNode *parent;
    if (!node->left) {
        child = node->right;
        parent = node->parent;
        transplant(node, child, header);
    } else if (!node->right) {
        child = node->left;
        parent = node->parent;
        transplant(node, child, header);
    } else {
        successor = minimum(node->right);
        removed_color = successor->color;
        child = successor->right;
        if (successor->parent == node) {
            parent = successor;
            if (child) {
                child->parent = successor;
            }
        } else {
            parent = successor->parent;
            transplant(successor, child, header);
            successor->right = node->right;
            successor->right->parent = successor;
        }
        transplant(node, successor, header);
        successor->left = node->left;
        successor->left->parent = successor;
        successor->color = node->color;
    }
    if (removed_color == 1) {
        repair_erase(child, parent, header);
    }
    header->left = header->parent ? minimum(header->parent) : header;
    header->right = header->parent ? maximum(header->parent) : header;
    return node;
}

Cod2MacTreeNode *increment(Cod2MacTreeNode *node)
{
    if (node->right) {
        return minimum(node->right);
    }
    auto *parent = node->parent;
    while (node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return node->right == parent ? node : parent;
}

Cod2MacTreeNode *decrement(Cod2MacTreeNode *node)
{
    if (node->color == 0 && node->parent && node->parent->parent == node) {
        return node->right; /* --end(), GCC's red sentinel. */
    }
    if (node->left) {
        return maximum(node->left);
    }
    auto *parent = node->parent;
    while (node == parent->left) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

void hook(Cod2MacListNode *node, Cod2MacListNode *position)
{
    node->next = position;
    node->prev = position->prev;
    position->prev->next = node;
    position->prev = node;
}

void unhook(Cod2MacListNode *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

} // namespace

/* Explicit forwarding functions keep both reconstructed Mach-O spellings and
 * external Itanium spellings callable, including arguments and return values. */
#define ABI_PAIR(result, name, arguments, call) \
    extern "C" result _##name arguments { return call; } \
    extern "C" result __##name arguments { return call; }

ABI_PAIR(int, ZN9__gnu_cxx18__exchange_and_addEPVii, (volatile int *word, int delta), __atomic_fetch_add(word, delta, __ATOMIC_ACQ_REL))
ABI_PAIR(size_t, ZNKSs4findEPKcmm, (const Cod2MacString *string, const char *text, size_t position, size_t length), find(string, text, position, length))
ABI_PAIR(int, ZNKSs7compareEPKc, (const Cod2MacString *string, const char *text), compare(string, text))
ABI_PAIR(Cod2MacString *, ZNSs6appendEPKcm, (Cod2MacString *string, const char *text, size_t length), replace(string, string_rep(string)->length, 0, text, length))
ABI_PAIR(Cod2MacString *, ZNSs6appendERKSs, (Cod2MacString *string, const Cod2MacString *other), replace(string, string_rep(string)->length, 0, rep_data(string_rep(other)), string_rep(other)->length))
ABI_PAIR(Cod2MacString *, ZNSs6assignEPKcm, (Cod2MacString *string, const char *text, size_t length), replace(string, 0, string_rep(string)->length, text, length))
ABI_PAIR(Cod2MacString *, ZNSs7replaceEmmPKcm, (Cod2MacString *string, size_t position, size_t removed, const char *text, size_t length), replace(string, position, removed, text, length))
ABI_PAIR(void, ZNSs7reserveEm, (Cod2MacString *string, size_t capacity), reserve(string, capacity))
ABI_PAIR(void, ZNSs9_M_mutateEmmm, (Cod2MacString *string, size_t position, size_t removed, size_t inserted), mutate(string, position, removed, inserted))
ABI_PAIR(void, ZNSt15_List_node_base4hookEPS_, (Cod2MacListNode *node, Cod2MacListNode *position), hook(node, position))
ABI_PAIR(void, ZNSt15_List_node_base6unhookEv, (Cod2MacListNode *node), unhook(node))
ABI_PAIR(Cod2MacTreeNode *, ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, (Cod2MacTreeNode *node), increment(node))
ABI_PAIR(Cod2MacTreeNode *, ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base, (Cod2MacTreeNode *node), decrement(node))
extern "C" void _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(bool insert_left, Cod2MacTreeNode *node, Cod2MacTreeNode *parent, Cod2MacTreeNode *header)
{
    insert_node(insert_left, node, parent, header);
}
extern "C" void __ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_(int insert_left, Cod2MacTreeNode *node, Cod2MacTreeNode *parent, Cod2MacTreeNode *header)
{
    insert_node(insert_left != 0, node, parent, header);
}
ABI_PAIR(Cod2MacTreeNode *, ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_, (Cod2MacTreeNode *node, Cod2MacTreeNode *header), erase_node(node, header))

extern "C" void *__Znwm(size_t size) { return ::operator new(size); }
extern "C" void *__Znam(size_t size) { return ::operator new[](size); }
extern "C" void __ZdlPv(void *pointer) { ::operator delete(pointer); }
extern "C" void __ZdaPv(void *pointer) { ::operator delete[](pointer); }

static void leak(Cod2MacString *string)
{
    auto *rep = string_rep(string);
    if (rep == empty_rep()) {
        return;
    }
    if (owners(rep) > 0) {
        mutate(string, 0, 0, 0);
        rep = string_rep(string);
    }
    __atomic_store_n(&rep->references, -1, __ATOMIC_RELEASE);
}

static void destroy(Cod2MacStringRep *rep, const void *)
{
    if (rep != empty_rep()) {
        ::operator delete(rep);
    }
}

static Cod2MacString *assign_string(Cod2MacString *string, const Cod2MacString *other)
{
    auto *rep = string_rep(string);
    auto *source = string_rep(other);
    if (rep != source) {
        source = grab_rep(source);
        dispose_rep(rep);
        string->data = rep_data(source);
    }
    return string;
}

static void construct_text(Cod2MacString *string, const char *text, const void *)
{
    if (!text) {
        throw std::logic_error("basic_string null source");
    }
    construct(string, text, std::strlen(text));
}

static void construct_copy(Cod2MacString *string, const Cod2MacString *other)
{
    string->data = rep_data(grab_rep(string_rep(other)));
}

static void construct_substring(Cod2MacString *string, const Cod2MacString *other, size_t position, size_t length)
{
    auto *rep = string_rep(other);
    if (position > rep->length) {
        throw std::out_of_range("basic_string position");
    }
    construct(string, rep_data(rep) + position, std::min(length, rep->length - position));
}

static void destruct(Cod2MacString *string)
{
    dispose_rep(string_rep(string));
}

ABI_PAIR(void, ZNSs12_M_leak_hardEv, (Cod2MacString *string), leak(string))
ABI_PAIR(void, ZNSs4_Rep10_M_destroyERKSaIcE, (Cod2MacStringRep *rep, const void *allocator), destroy(rep, allocator))
ABI_PAIR(Cod2MacString *, ZNSs6assignERKSs, (Cod2MacString *string, const Cod2MacString *other), assign_string(string, other))
ABI_PAIR(void, ZNSsC1EPKcRKSaIcE, (Cod2MacString *string, const char *text, const void *allocator), construct_text(string, text, allocator))
ABI_PAIR(void, ZNSsC1ERKSs, (Cod2MacString *string, const Cod2MacString *other), construct_copy(string, other))
ABI_PAIR(void, ZNSsC1ERKSsmm, (Cod2MacString *string, const Cod2MacString *other, size_t position, size_t length), construct_substring(string, other, position, length))
ABI_PAIR(void, ZNSsD1Ev, (Cod2MacString *string), destruct(string))

/* j manglings denote the imported 32-bit size_type. These are genuine typed
 * adapters, not aliases to functions expecting 64-bit argument/return widths. */
extern "C" unsigned int _ZNKSs4findEPKcjj(const Cod2MacString *string, const char *text, unsigned int position, unsigned int length)
{
    size_t result = find(string, text, position, length);
    return result >= std::numeric_limits<unsigned int>::max() ? ~0u : static_cast<unsigned int>(result);
}
extern "C" Cod2MacString *_ZNSs6appendEPKcj(Cod2MacString *string, const char *text, unsigned int length) { return _ZNSs6appendEPKcm(string, text, length); }
extern "C" Cod2MacString *_ZNSs6assignEPKcj(Cod2MacString *string, const char *text, unsigned int length) { return _ZNSs6assignEPKcm(string, text, length); }
extern "C" Cod2MacString *_ZNSs7replaceEjjPKcj(Cod2MacString *string, unsigned int position, unsigned int removed, const char *text, unsigned int length) { return replace(string, position, removed, text, length); }
extern "C" void _ZNSs7reserveEj(Cod2MacString *string, unsigned int capacity) { reserve(string, capacity); }
extern "C" void _ZNSs9_M_mutateEjjj(Cod2MacString *string, unsigned int position, unsigned int removed, unsigned int inserted) { mutate(string, position, removed, inserted); }
extern "C" void _ZNSsC1ERKSsjj(Cod2MacString *string, const Cod2MacString *other, unsigned int position, unsigned int length) { construct_substring(string, other, position, length); }

[[noreturn]] static void throw_bad_alloc() { throw std::bad_alloc(); }
[[noreturn]] static void throw_length_error(const char *text) { throw std::length_error(text); }
[[noreturn]] static void throw_out_of_range(const char *text) { throw std::out_of_range(text); }
ABI_PAIR(void, ZSt17__throw_bad_allocv, (), throw_bad_alloc())
ABI_PAIR(void, ZSt20__throw_length_errorPKc, (const char *text), throw_length_error(text))
ABI_PAIR(void, ZSt20__throw_out_of_rangePKc, (const char *text), throw_out_of_range(text))
#undef ABI_PAIR

extern "C" void *___cxa_allocate_exception(size_t size) { return __cxxabiv1::__cxa_allocate_exception(size); }
extern "C" void *___cxa_begin_catch(void *exception) { return __cxxabiv1::__cxa_begin_catch(exception); }
extern "C" void ___cxa_end_catch() { __cxxabiv1::__cxa_end_catch(); }
extern "C" [[noreturn]] void ___cxa_rethrow() { __cxxabiv1::__cxa_rethrow(); }
extern "C" [[noreturn]] void ___cxa_throw(void *exception, std::type_info *type, void (*destructor)(void *)) { __cxxabiv1::__cxa_throw(exception, type, destructor); }
extern "C" [[noreturn]] void __Unwind_Resume(_Unwind_Exception *exception) { _Unwind_Resume(exception); __builtin_unreachable(); }

namespace __cxxabiv1 {
class __class_type_info;
extern "C" void *__dynamic_cast(const void *, const __class_type_info *, const __class_type_info *, ptrdiff_t);
}
extern "C" void *___dynamic_cast(const void *object, const __cxxabiv1::__class_type_info *source, const __cxxabiv1::__class_type_info *destination, ptrdiff_t hint)
{
    return __cxxabiv1::__dynamic_cast(object, source, destination, hint);
}

#endif
