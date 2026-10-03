# Apple LP64 compile bring-up. The typed data migration supplies blobs later.
set(CMAKE_OSX_ARCHITECTURES arm64)
find_package(SDL2 CONFIG REQUIRED)
find_library(COD2_OPENGL_FRAMEWORK OpenGL REQUIRED)
find_package(ZLIB REQUIRED)

file(GLOB_RECURSE MACOS_PC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/PC/*.c")
file(GLOB_RECURSE MACOS_MAC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/Mac/*.c")
file(GLOB_RECURSE MACOS_STUBS_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/stubs/*.c")
file(GLOB MACOS_ROOT_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/*.c")
set(MACOS_C ${MACOS_PC_C} ${MACOS_MAC_C} ${MACOS_STUBS_C} ${MACOS_ROOT_C})
# Use the SDK zlib, as in the native Linux build. No 32-bit data generators or ASM.
list(FILTER MACOS_C EXCLUDE REGEX "/PC/zlib/(inflate|infblock|infcodes|inffast|inftrees|infutil|adler32|zutil)\\.c$")
list(FILTER MACOS_C EXCLUDE REGEX "/(data|import_pointers|literals)\\.c$")
list(APPEND MACOS_C src/unix/linux_common.c src/unix/linux_net.c src/unix/sysdiff_statehash.c)

# Typed LP64 data generated from the Mac binary's STABS (WS2, tools/datagen).
include(${CMAKE_SOURCE_DIR}/cmake/datagen.cmake)
cod2_generate_typed_blobs(MACOS_GEN_C)
list(APPEND MACOS_C ${MACOS_GEN_C})

foreach(target cod2_macos cod2_macos_ded)
  add_executable(${target} ${MACOS_C})
  target_include_directories(${target} PRIVATE
    ${COD2_SRC_DIR}/PC/speex ${COD2_SRC_DIR} ${COD2_SRC_DIR}/headers ${CMAKE_SOURCE_DIR})
  target_compile_options(${target} PRIVATE -g -O0 -fcommon -ffp-contract=off
    -fno-strict-aliasing ${COD2_WNO}
    -Wshorten-64-to-32 -Wpointer-to-int-cast -Wint-to-pointer-cast
    -Wint-conversion -Wincompatible-pointer-types -Wvoid-pointer-to-int-cast
    -Wno-typedef-redefinition -Wno-duplicate-decl-specifier
    -ferror-limit=0)
  target_link_libraries(${target} PRIVATE ZLIB::ZLIB ${COD2_OPENGL_FRAMEWORK})
  # ld64 equivalents of the MinGW --defsym seam aliases in CMakeLists.txt.
  target_link_options(${target} PRIVATE
    "LINKER:-alias,_g_entities,_g_entities_ptr"
    "LINKER:-alias,_imp_bgs,_g_time_ptr"
    "LINKER:-alias,_vidConfig,_r_limits_ptr"
    "LINKER:-alias,_scr_const,_scr_const_ptr"
    "LINKER:-alias,_sv,_sv_ptr"
    "LINKER:-alias,_svs,_svs_ptr")
endforeach()
target_sources(cod2_macos PRIVATE src/unix/linux_input.c)
target_link_libraries(cod2_macos PRIVATE SDL2::SDL2)
target_compile_definitions(cod2_macos_ded PRIVATE DEDICATED)
set_source_files_properties(src/PC/qcommon/crash_handler.c PROPERTIES
  COMPILE_DEFINITIONS "COD2_GIT_HASH=\"${COD2_GIT_HASH}\"")
