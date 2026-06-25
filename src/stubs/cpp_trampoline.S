/* C++ symbol trampolines: Mac Mach-O double-underscore -> Linux ELF */
/* On 32-bit: Mac uses 'm' (unsigned long) mangling, Linux uses 'j' (unsigned int) */
.text

.macro TRAMPOLINE from, to
    .globl \from
    .type \from, @function
\from:
    jmp \to
    .size \from, .-\from
.endm

/* Exception handling */
TRAMPOLINE ___cxa_allocate_exception, __cxa_allocate_exception
TRAMPOLINE ___cxa_begin_catch, __cxa_begin_catch  
TRAMPOLINE ___cxa_end_catch, __cxa_end_catch
TRAMPOLINE ___cxa_rethrow, __cxa_rethrow
TRAMPOLINE ___cxa_throw, __cxa_throw
/* ___dynamic_cast stub: all dynamic_casts in this codebase are valid downcasts,
   and we lack proper RTTI/typeinfo structures, so just return the source pointer */
.globl ___dynamic_cast
.type ___dynamic_cast, @function
___dynamic_cast:
    movl 4(%esp), %eax
    retl
.size ___dynamic_cast, .-___dynamic_cast
TRAMPOLINE __Unwind_Resume, _Unwind_Resume

/* operator new/delete - note m->j mangling fix */
TRAMPOLINE __ZdaPv, _ZdaPv
TRAMPOLINE __ZdlPv, _ZdlPv
TRAMPOLINE __Znam, _Znaj
TRAMPOLINE __Znwm, _Znwj

/* GNU extension */
TRAMPOLINE __ZN9__gnu_cxx18__exchange_and_addEPVii, _ZN9__gnu_cxx18__exchange_and_addEPVii

/* std::string methods - fix m->j in mangled names */
TRAMPOLINE __ZNKSs4findEPKcmm, _ZNKSs4findEPKcjj
TRAMPOLINE __ZNKSs7compareEPKc, _ZNKSs7compareEPKc
TRAMPOLINE __ZNSs12_M_leak_hardEv, _ZNSs12_M_leak_hardEv
TRAMPOLINE __ZNSs4_Rep10_M_destroyERKSaIcE, _ZNSs4_Rep10_M_destroyERKSaIcE
TRAMPOLINE __ZNSs6appendEPKcm, _ZNSs6appendEPKcj
TRAMPOLINE __ZNSs6appendERKSs, _ZNSs6appendERKSs
TRAMPOLINE __ZNSs6assignEPKcm, _ZNSs6assignEPKcj
TRAMPOLINE __ZNSs6assignERKSs, _ZNSs6assignERKSs
TRAMPOLINE __ZNSs7replaceEmmPKcm, _ZNSs7replaceEjjPKcj
TRAMPOLINE __ZNSs7reserveEm, _ZNSs7reserveEj
TRAMPOLINE __ZNSs9_M_mutateEmmm, _ZNSs9_M_mutateEjjj
TRAMPOLINE __ZNSsC1EPKcRKSaIcE, _ZNSsC1EPKcRKSaIcE
TRAMPOLINE __ZNSsC1ERKSs, _ZNSsC1ERKSs
TRAMPOLINE __ZNSsC1ERKSsmm, _ZNSsC1ERKSsjj
TRAMPOLINE __ZNSsD1Ev, _ZNSsD1Ev

/* std::list */
TRAMPOLINE __ZNSt15_List_node_base4hookEPS_, _ZNSt15_List_node_base4hookEPS_
TRAMPOLINE __ZNSt15_List_node_base6unhookEv, _ZNSt15_List_node_base6unhookEv

/* std::bad_alloc / exceptions */
TRAMPOLINE __ZSt17__throw_bad_allocv, _ZSt17__throw_bad_allocv
TRAMPOLINE __ZSt20__throw_length_errorPKc, _ZSt20__throw_length_errorPKc
TRAMPOLINE __ZSt20__throw_out_of_rangePKc, _ZSt20__throw_out_of_rangePKc

/* Red-black tree */
TRAMPOLINE __ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base, _ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base
TRAMPOLINE __ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base, _ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base
TRAMPOLINE __ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_, _ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_
TRAMPOLINE __ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_, _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_

/* Data symbols */
.data

.globl __ZNSs4_Rep11_S_terminalE
__ZNSs4_Rep11_S_terminalE:
    .long 0

# Import pointer: code does "movl __ZNSs4_Rep20_S_empty_rep_storageE, %edx"
# which loads the VALUE here (a pointer filled by Mach-O dyld). On ELF,
# we make it a pointer to the actual libstdc++ _S_empty_rep_storage.
.data
.globl __ZNSs4_Rep20_S_empty_rep_storageE
__ZNSs4_Rep20_S_empty_rep_storageE:
    .long _ZNSs4_Rep20_S_empty_rep_storageE
.text

.globl __ZTIl
__ZTIl:
    .space 16, 0

/* __mh_execute_header is now an absolute symbol (0x1000) via linker --defsym */

/* __DefaultRuneLocale is now in macos_compat.c with proper BSD ctype data */

