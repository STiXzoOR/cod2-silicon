# =============================================================================
# WINDOWS / MSVC (cl.exe) build  -- full SDL2/GL/D3D client (cod2_win32.exe)
#
# Additive and self-contained: entered only from the WIN32 branch of the root
# CMakeLists when CMAKE_C_COMPILER is MSVC. The MinGW cross build is untouched.
#
# Status: STAGED PORT IN PROGRESS. This stage gets cl to *compile* the source
# set (an OBJECT library) so the real frontend-error surface is visible; it does
# not link yet -- the data blobs (Stage 2), __asm__ removal (Stage 3) and the
# C++ class reconstruction (Stage 5) are prerequisites for a final link.
#
#   Configure/build from a "x86 Native Tools for VS" command prompt:
#     cmake --preset msvc-client
#     cmake --build --preset msvc-client
# =============================================================================

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR
    "MSVC build must be 32-bit: the reconstructed data layout is ILP32 "
    "(4-byte pointers, hardcoded sizes). Use the x86 cl (Hostx64/x86 or "
    "Hostx86/x86), e.g. `vcvarsall.bat x86`. Got ${CMAKE_SIZEOF_VOID_P}-byte pointers.")
endif()

enable_language(CXX)   # full client pulls in the C++ renderer/UI surface

# Strip CMake's Debug /RTC1 runtime checks: decompiler-output C reads stack
# variables the original initialized via control flow the decompiler didn't
# perfectly preserve, so RTCu/RTCs abort with a CRT dialog on otherwise-fine
# code. (Keep /Z7 debug info and the debug CRT for symbolized crash traces.)
foreach(cfg "" _DEBUG _RELWITHDEBINFO)
  string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS${cfg}   "${CMAKE_C_FLAGS${cfg}}")
  string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS${cfg} "${CMAKE_CXX_FLAGS${cfg}}")
endforeach()

# --- include search path -----------------------------------------------------
# shims-msvc FIRST so the MSVC POSIX shims win over anything else; the shared
# win32/shims provides the socket/net headers used by both win toolchains.
include_directories(
  ${COD2_SRC_DIR}/win32/shims-msvc
  ${COD2_SRC_DIR}/PC/speex ${COD2_SRC_DIR} ${COD2_SRC_DIR}/headers
  ${COD2_SRC_DIR}/win32/shims ${COD2_SRC_DIR}/win32/sdl2/include ${CMAKE_SOURCE_DIR})

# --- compile options ---------------------------------------------------------
# /FI win32_compat.h mirrors gcc's -include. /w matches the engine's -w (the
# reconstructed C is intentionally warning-noisy). Permissive mode + the C
# legacy-lenience flags keep cl's frontend from rejecting decompiler-output C.
# /FI order matters: the GCC-compat shim must be seen before anything else so
# its keyword/__attribute__ macros are in scope while the engine headers parse.
add_compile_options(
  "/FI${COD2_SRC_DIR}/win32/shims-msvc/msvc_gcc_compat.h"
  "$<$<COMPILE_LANGUAGE:C>:/FI${COD2_SRC_DIR}/headers/win32_compat.h>"
  # /GS- : the stack-cookie/buffer-security check trips on decompiler-output
  # stack layouts (binary-faithful local buffers) -> spurious CRT runtime abort.
  /w /MP /permissive- /Zc:preprocessor /utf-8 /bigobj /Oy- /Z7 /GS-)
add_compile_definitions(
  WIN32 _WIN32 _GNU_SOURCE=1 W32_CLIENT
  _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
  _WINSOCK_DEPRECATED_NO_WARNINGS WIN32_LEAN_AND_MEAN
  NOMINMAX SDL_DISABLE_IMMINTRIN_H)
# C as C11, C++ as C++14 (matches the era of the reconstructed C++ surface).
add_compile_options("$<$<COMPILE_LANGUAGE:C>:/std:c11>"
                    "$<$<COMPILE_LANGUAGE:CXX>:/std:c++14>")

# --- source set (mirrors the MinGW WIN_C gathering) --------------------------
file(GLOB_RECURSE M_PC_C    CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "${COD2_SRC_DIR}/PC/*.c")
file(GLOB_RECURSE M_MAC_C   CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "${COD2_SRC_DIR}/Mac/*.c")
file(GLOB_RECURSE M_STUBS_C CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "${COD2_SRC_DIR}/stubs/*.c")
file(GLOB_RECURSE M_WIN_C   CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "${COD2_SRC_DIR}/win32/*.c")
file(GLOB         M_ROOT_C  CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "${COD2_SRC_DIR}/*.c")
set(MSVC_C ${M_PC_C} ${M_MAC_C} ${M_STUBS_C} ${M_WIN_C} ${M_ROOT_C})

# native-asm data .c and cpp_trampoline/agl_stubs excluded as on MinGW. The
# bundled zlib IS kept (MinGW used -lz; MSVC has no system zlib, so compile it).
list(FILTER MSVC_C EXCLUDE REGEX "^src/(data|import_pointers|literals)\\.c$")
list(FILTER MSVC_C EXCLUDE REGEX "^src/stubs/(cpp_trampoline|agl_stubs)\\.c$")
list(APPEND MSVC_C src/blobs/bss.c src/unix/sysdiff_statehash.c src/unix/linux_input.c)

# When a real MSVC SDL2.lib is supplied, drop the name-only stub (else the stub
# would win under /FORCE:MULTIPLE and SDL calls would be no-ops).
find_library(COD2_SDL2_LIB SDL2 PATHS ${COD2_SRC_DIR}/win32/sdl2/lib NO_DEFAULT_PATH)
if(COD2_SDL2_LIB)
  list(FILTER MSVC_C EXCLUDE REGEX "shims-msvc/sdl2_stub\\.c$")
  message(STATUS "MSVC client: using real SDL2 (${COD2_SDL2_LIB})")
endif()

# The engine object set (all ~410 TUs compile clean under cl).
add_library(cod2_msvc_objs OBJECT ${MSVC_C})
set_target_properties(cod2_msvc_objs PROPERTIES LINKER_LANGUAGE CXX)

# --- data blobs (Stage 6) ----------------------------------------------------
# The reconstructed .data/.rodata, as portable C (the native_gen variants, which
# cl accepts: &sym and &sym+offset address-constants are fine). Built with /Zp1
# so the packed _d32_ layout structs keep their exact byte layout (no padding).
set(MSVC_BLOBS
  ${CMAKE_SOURCE_DIR}/build/native_gen/data32.c
  ${CMAKE_SOURCE_DIR}/build/native_gen/literals32.c
  ${CMAKE_SOURCE_DIR}/build/native_gen/import_pointers_native.c)
add_library(cod2_msvc_blobs OBJECT ${MSVC_BLOBS})
target_compile_options(cod2_msvc_blobs PRIVATE /Zp1)

# --- seam aliases (Stage 6) --------------------------------------------------
# MSVC /alternatename replaces the GNU build's --defsym engine-seam aliases:
# the engine references `<x>_ptr` which aliases the real `<x>` symbol. x86 C
# symbols carry one leading '_'.
set(MSVC_SEAM_ALIASES
  "/alternatename:_level_ptr=_level"
  "/alternatename:_g_entities_ptr=_g_entities"
  "/alternatename:_scr_const_ptr=_scr_const"
  "/alternatename:_playerCorpseInfo_ptr=_g_scr_data"
  "/alternatename:_g_renderer_ptr=_re"
  "/alternatename:_scrAnimPub_ptr=_scrAnimPub"
  "/alternatename:_scrCompPub_ptr=_scrCompilePub"
  "/alternatename:_scrParserPub_ptr=_scrParserPub"
  "/alternatename:_r_frontEndData_ptr=_rg"
  "/alternatename:_r_sys_ptr=_ri"
  "/alternatename:_r_limits_ptr=_vidConfig"
  "/alternatename:_sv_ptr=_sv"
  "/alternatename:_svs_ptr=_svs"
  "/alternatename:_cg_globUI=_legacyHacks"
  "/alternatename:_g_time=_imp_level_bgs"
  "/alternatename:_g_time_ptr=_imp_bgs"
  "/alternatename:_methods=_methods_003138c0")

# --- executable (Stage 6, first link) ----------------------------------------
# /FORCE:MULTIPLE stands in for GNU --allow-multiple-definition (the blob and
# bss/home-.c overlap on some tentative defs). This is a FIRST link to surface
# the unresolved-symbol set; libs/wrap/boot are iterated from there.
add_executable(cod2_win32
  $<TARGET_OBJECTS:cod2_msvc_objs> $<TARGET_OBJECTS:cod2_msvc_blobs>)
# WinMain (mac_main.c) is the entry -> Windows subsystem. /FORCE:MULTIPLE ~=
# GNU --allow-multiple-definition (blob vs home-.c tentative-def overlap).
# SDL2 is a user-supplied external (README); COD2_SDL2_LIB (found above) links a
# real MSVC SDL2.lib when present, else sdl2_stub.c lets the exe link.
target_link_options(cod2_win32 PRIVATE
  /FORCE:MULTIPLE /SAFESEH:NO /SUBSYSTEM:WINDOWS /MAP ${MSVC_SEAM_ALIASES})
target_link_libraries(cod2_win32 PRIVATE
  $<$<BOOL:${COD2_SDL2_LIB}>:${COD2_SDL2_LIB}>
  opengl32
  ws2_32 winmm dbghelp user32 gdi32 advapi32 shell32 ole32 oleaut32
  imm32 version setupapi)
set_target_properties(cod2_win32 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

message(STATUS "MSVC client: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}, "
               "${CMAKE_SIZEOF_VOID_P}*8-bit; Stage 6 first-link target cod2_win32.")
