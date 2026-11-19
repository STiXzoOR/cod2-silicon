# Native Linux x86-64 (LP64) dedicated server. Same engine source graph as
# cmake/macos-arm64.cmake, with a Linux platform layer and ELF link options. Typed
# engine data comes from the committed build/lp64_gen snapshot. Linux never shipped
# a client, so cod2_lnxded is the only target here.
enable_language(CXX)
find_package(ZLIB REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE LINUX_PC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/PC/*.c")
file(GLOB_RECURSE LINUX_MAC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/Mac/*.c")
file(GLOB_RECURSE LINUX_STUBS_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/stubs/*.c")
set(LINUX_C ${LINUX_PC_C} ${LINUX_MAC_C} ${LINUX_STUBS_C})
# Use the system zlib, as on macOS. No 32-bit data generators or ASM.
list(FILTER LINUX_C EXCLUDE REGEX "/PC/zlib/(inflate|infblock|infcodes|inffast|inftrees|infutil|adler32|zutil)\\.c$")
list(FILTER LINUX_C EXCLUDE REGEX "/(data|import_pointers|literals)\\.c$")
# The native platform files below own these APIs.
list(FILTER LINUX_C EXCLUDE REGEX "/Mac/Tools/(MacDisplay|MacThreads|CCircularBuffer|CAudioRecorder|MacMSS[^/]*)\\.c$")
list(FILTER LINUX_C EXCLUDE REGEX "/stubs/(agl_stubs|audio_stubs|cpp_compat|cpp_trampoline|fx_override)\\.c$")
list(FILTER LINUX_C EXCLUDE REGEX "/stubs/(iokit_stubs|corefoundation_stubs)\\.c$")
list(FILTER LINUX_C EXCLUDE REGEX "/Mac/Main/(mac_play_dsound|mac_record_dsound)\\.c$")
list(FILTER LINUX_C EXCLUDE REGEX "/PC/groupvoice/record\\.c$")
list(APPEND LINUX_C src/unix/linux_common.c src/unix/linux_net.c src/unix/linux_system.c
  src/unix/sysdiff_statehash.c src/platform/macos_threads.c src/platform/macos_ring.c
  src/platform/macos_cpp_abi.cpp)

# The dedicated graph matches cod2_macos_ded: no renderer, client, UI or sound.
set(LINUX_DED_C ${LINUX_C})
list(FILTER LINUX_DED_C EXCLUDE REGEX "/(Mac/DirectX_9|PC/gfx_d3d|PC/cgame|PC/cgame_mp|PC/ui|PC/ui_mp|PC/EffectsCore|PC/client_mp|PC/groupvoice)/")
foreach(unit FxUtil Fxexport FxScheduler FxScheduler_load_obj FxTemplate FxSystem
    GenericParser2 FxChannel FxCurve FxCurve_load_obj FxMemMgr)
  list(APPEND LINUX_DED_C "${COD2_SRC_DIR}/PC/EffectsCore/${unit}.c")
endforeach()
list(FILTER LINUX_DED_C EXCLUDE REGEX "/PC/(snd|win32/cinematics)\\.c$")
list(FILTER LINUX_DED_C EXCLUDE REGEX "/Mac/Main/(mac_input|mac_decode|mac_sound)\\.c$")
list(FILTER LINUX_DED_C EXCLUDE REGEX "/PC/win32/(snd_driver|win_voice)\\.c$")
# export-begin: codx
# The CoD2x client-only units stay out of the dedicated server.
list(FILTER LINUX_DED_C EXCLUDE REGEX "/PC/qcommon/cod2x_(features|demo|url|vmix)\\.c$")
# export-end
list(APPEND LINUX_DED_C src/platform/macos_server.c)

include(${CMAKE_SOURCE_DIR}/cmake/datagen.cmake)
cod2_generate_typed_blobs(LINUX_GEN_C)
# --gc-sections drops input sections, not objects, so the dedicated view gives
# every typed object its own section. Live references still fail to link if their
# owner is missing.
set(LINUX_DED_GEN_DIR "${CMAKE_BINARY_DIR}/dedicated_gen")
set(LINUX_DED_GEN_C)
foreach(source ${LINUX_GEN_C})
  get_filename_component(name "${source}" NAME)
  list(APPEND LINUX_DED_GEN_C "${LINUX_DED_GEN_DIR}/${name}")
endforeach()
add_custom_command(OUTPUT ${LINUX_DED_GEN_C} "${LINUX_DED_GEN_DIR}/typed_types.h"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/server/dedicated_data.py"
    --split-sections "${LINUX_DED_GEN_DIR}" ${LINUX_GEN_C}
  DEPENDS ${LINUX_GEN_C} "${CMAKE_SOURCE_DIR}/tools/server/dedicated_data.py"
  VERBATIM)
list(APPEND LINUX_DED_C ${LINUX_DED_GEN_C})
# bss_native.c owns its objects, and its ELF branch aliases g_banIPs_dvar with
# .set, which GAS refuses on a common symbol. Tentative definitions elsewhere
# still merge into these.
foreach(source ${LINUX_DED_GEN_C})
  if(source MATCHES "/bss_native\\.c$")
    set_property(SOURCE ${source} APPEND PROPERTY COMPILE_OPTIONS -fno-common)
  endif()
endforeach()

# Clang treats these MSVC calling-convention spellings as keywords; GCC does not.
# x86-64 has a single C calling convention, so they carry no meaning here.
set(COD2_LINUX_DEFINITIONS _GNU_SOURCE __cdecl= __stdcall=)
set(COD2_LINUX_C_OPTIONS -fcommon -fno-pie -ffp-contract=off -fno-strict-aliasing -fwrapv
  -fno-strict-overflow -fno-delete-null-pointer-checks -fno-aggressive-loop-optimizations
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 ${COD2_WNO}
  -Wpointer-to-int-cast -Wint-to-pointer-cast -Wint-conversion -Wincompatible-pointer-types
  -Wno-duplicate-decl-specifier)

add_executable(cod2_lnxded ${LINUX_DED_C})
target_include_directories(cod2_lnxded PRIVATE
  ${COD2_SRC_DIR}/PC/speex ${COD2_SRC_DIR} ${COD2_SRC_DIR}/headers ${CMAKE_SOURCE_DIR})
target_compile_options(cod2_lnxded PRIVATE -g "$<$<COMPILE_LANGUAGE:C>:${COD2_LINUX_C_OPTIONS}>")
target_compile_definitions(cod2_lnxded PRIVATE DEDICATED ${COD2_LINUX_DEFINITIONS})
target_compile_features(cod2_lnxded PRIVATE cxx_std_17)
target_link_libraries(cod2_lnxded PRIVATE ZLIB::ZLIB m dl pthread)
# export-begin: codx
if(COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
  # CoD2x's update check, zPAM downloads, script HTTP (src/PC/qcommon/cod2x_http.c) and reports.
  find_package(CURL REQUIRED)
  target_link_libraries(cod2_lnxded PRIVATE CURL::libcurl)
endif()
# export-end
# ELF equivalents of the ld64 aliases in macos-arm64.cmake.
target_link_options(cod2_lnxded PRIVATE -no-pie -Wl,-z,noexecstack
  -Wl,--defsym,__mh_execute_header=0x1000
  -Wl,--defsym,g_entities_ptr=g_entities
  -Wl,--defsym,g_time_ptr=imp_bgs
  -Wl,--defsym,scr_const_ptr=scr_const
  -Wl,--defsym,sv_ptr=sv
  -Wl,--defsym,svs_ptr=svs)
set_target_properties(cod2_lnxded PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# Keep libstdc++'s own std::string, list and tree symbols out of reach of the old
# GCC ABI operations this file defines for reconstructed C.
set_source_files_properties(src/platform/macos_cpp_abi.cpp PROPERTIES COMPILE_OPTIONS -fvisibility=hidden)
target_compile_options(cod2_lnxded PRIVATE -ffunction-sections -fdata-sections)
target_link_options(cod2_lnxded PRIVATE -Wl,--gc-sections)
set_source_files_properties(src/PC/qcommon/crash_handler.c PROPERTIES
  COMPILE_DEFINITIONS "COD2_GIT_HASH=\"${COD2_GIT_HASH}\"")

# export-begin: codx
# CoD2x-layer units as one run, for the error and crash reports (cod2x_report.c).
include(${CMAKE_SOURCE_DIR}/cmake/cod2x-layer.cmake)
cod2x_layer_order(cod2_lnxded)
# export-end
