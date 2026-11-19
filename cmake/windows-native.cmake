# MinGW build of the reconciled engine, on i686 or x86_64. Same engine source graph
# as cmake/linux-x64.cmake and cmake/macos-arm64.cmake, with a Windows platform
# layer. Typed engine data comes from the committed build/lp64_gen snapshot, which
# the compiler lays out and so fits either architecture.
option(COD2_WIN32_CLIENT "SDL2/GL client instead of the headless server" OFF)
option(COD2_WIN32_D3D9 "Client: render through the system d3d9.dll instead of OpenGL" OFF)
if(COD2_WIN32_D3D9 AND NOT COD2_WIN32_CLIENT)
  message(FATAL_ERROR "COD2_WIN32_D3D9 needs COD2_WIN32_CLIENT")
endif()
enable_language(CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# -Wreturn-mismatch is GCC 14; older MinGW compilers reject the -Wno-error form.
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wno-error=return-mismatch COD2_WIN_HAS_RETURN_MISMATCH)
if(NOT COD2_WIN_HAS_RETURN_MISMATCH)
  list(REMOVE_ITEM COD2_WNO -Wno-error=return-mismatch)
endif()

file(GLOB_RECURSE WIN_PC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/PC/*.c")
file(GLOB_RECURSE WIN_MAC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/Mac/*.c")
file(GLOB_RECURSE WIN_STUBS_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/stubs/*.c")
set(WIN_C ${WIN_PC_C} ${WIN_MAC_C} ${WIN_STUBS_C})
# MinGW supplies zlib. No 32-bit data generators or ASM.
list(FILTER WIN_C EXCLUDE REGEX "/PC/zlib/(inflate|infblock|infcodes|inffast|inftrees|infutil|adler32|zutil)\\.c$")
list(FILTER WIN_C EXCLUDE REGEX "/(data|import_pointers|literals)\\.c$")
list(FILTER WIN_C EXCLUDE REGEX "/Mac/Tools/(MacThreads|CCircularBuffer)\\.c$")
list(FILTER WIN_C EXCLUDE REGEX "/stubs/(agl_stubs|cpp_compat|cpp_trampoline|fx_override|zlib_alloc)\\.c$")
list(FILTER WIN_C EXCLUDE REGEX "/stubs/(iokit_stubs|corefoundation_stubs)\\.c$")
list(APPEND WIN_C src/unix/sysdiff_statehash.c src/win32/win_system.c
  src/platform/macos_threads.c src/platform/macos_ring.c src/platform/macos_cpp_abi.cpp)

include(${CMAKE_SOURCE_DIR}/cmake/datagen.cmake)
cod2_generate_typed_blobs(WIN_GEN_C)

if(COD2_WIN32_CLIENT)
  set(WIN_TARGET cod2_win32)
  set(WIN_TARGET_C ${WIN_C} ${WIN_GEN_C} src/unix/linux_input.c src/win32/gl_loader.c
    src/win32/win_jpeg.c src/platform/macos_display.c)
  # Sound is the macOS mixer over SDL, in place of the Mac Miles layer.
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/Mac/Tools/(MacDisplay|CAudioRecorder|MacMSS[^/]*)\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/stubs/audio_stubs\\.c$")
  # Voice is the Windows 1.3 DirectSound layer, in place of the Mac one.
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/Mac/Main/(mac_play_dsound|mac_record_dsound)\\.c$")
  # The DirectSound layer itself is not in this source tree. A checkout that has one
  # lists its files in cmake/windows-voice.cmake as COD2_WIN_VOICE_C; without it the
  # client is built with voice chat off.
  include(${CMAKE_SOURCE_DIR}/cmake/windows-voice.cmake OPTIONAL)
  if(NOT COD2_WIN_VOICE_C)
    set(COD2_WIN_VOICE_C src/win32/win_voice_none.c)
  endif()
  list(APPEND WIN_TARGET_C src/platform/macos_audio.c ${COD2_WIN_VOICE_C} src/win32/win_window.c)
  if(COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
    # CoD2x raw mouse rate statistics, read by src/unix/linux_input.c; CoD2x's raw
    # input modes and the rest of its Windows client behaviour.
    list(APPEND WIN_TARGET_C src/platform/cod2x_native_mouse.c src/win32/win_rawmouse.c
      src/win32/win_cod2x.c src/win32/win_cod2x_report.c)
  endif()
  # The renderer renders into the exact r_mode framebuffer; presentation scales it.
  set_source_files_properties(src/PC/gfx_d3d/rb_state.c src/PC/gfx_d3d/r_screenshot.c PROPERTIES
    COMPILE_DEFINITIONS "glDrawBuffer=MacGL_DrawBuffer;glReadBuffer=MacGL_ReadBuffer")
  set(WIN_TARGET_GEN_C ${WIN_GEN_C})
  # The renderer drives the system Direct3D 9 device; the OpenGL device layer
  # stays linked for the helpers the renderer shares with it.
  if(COD2_WIN32_D3D9)
    list(APPEND WIN_TARGET_C src/win32/win_d3d9.c)
  endif()
else()
  # The dedicated graph matches cod2_lnxded: no renderer, client, UI or sound.
  set(WIN_TARGET cod2_win32_ded)
  set(WIN_TARGET_C ${WIN_C})
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/Mac/Tools/(MacDisplay|CAudioRecorder|MacMSS[^/]*)\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/stubs/audio_stubs\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/Mac/Main/(mac_play_dsound|mac_record_dsound)\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/(Mac/DirectX_9|PC/gfx_d3d|PC/cgame|PC/cgame_mp|PC/ui|PC/ui_mp|PC/EffectsCore|PC/client_mp|PC/groupvoice)/")
  foreach(unit FxUtil Fxexport FxScheduler FxScheduler_load_obj FxTemplate FxSystem
      GenericParser2 FxChannel FxCurve FxCurve_load_obj FxMemMgr)
    list(APPEND WIN_TARGET_C "${COD2_SRC_DIR}/PC/EffectsCore/${unit}.c")
  endforeach()
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/PC/(snd|win32/cinematics)\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/Mac/Main/(mac_input|mac_decode|mac_sound)\\.c$")
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/PC/win32/(snd_driver|win_voice)\\.c$")
  # The CoD2x client-only units stay out of the dedicated server.
  list(FILTER WIN_TARGET_C EXCLUDE REGEX "/PC/qcommon/cod2x_(features|demo|url|vmix)\\.c$")
  list(APPEND WIN_TARGET_C src/platform/macos_server.c src/win32/win_ded_stubs.c)
  # --gc-sections drops input sections, not objects, so the dedicated view gives
  # every typed object its own section.
  set(WIN_DED_GEN_DIR "${CMAKE_BINARY_DIR}/dedicated_gen")
  set(WIN_TARGET_GEN_C)
  foreach(source ${WIN_GEN_C})
    get_filename_component(name "${source}" NAME)
    list(APPEND WIN_TARGET_GEN_C "${WIN_DED_GEN_DIR}/${name}")
  endforeach()
  add_custom_command(OUTPUT ${WIN_TARGET_GEN_C} "${WIN_DED_GEN_DIR}/typed_types.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/server/dedicated_data.py"
      --split-sections --section-separator "$" "${WIN_DED_GEN_DIR}" ${WIN_GEN_C}
    DEPENDS ${WIN_GEN_C} "${CMAKE_SOURCE_DIR}/tools/server/dedicated_data.py"
    VERBATIM)
  list(APPEND WIN_TARGET_C ${WIN_TARGET_GEN_C})
endif()

# bss_native.c owns its objects and aliases g_banIPs_dvar with .set, which GAS
# refuses on a common symbol. Tentative definitions elsewhere still merge into them.
foreach(source ${WIN_TARGET_GEN_C})
  if(source MATCHES "/bss_native\\.c$")
    set_property(SOURCE ${source} APPEND PROPERTY COMPILE_OPTIONS -fno-common)
  endif()
endforeach()

# -mno-ms-bitfields: the engine headers describe GCC struct layout.
set(COD2_WIN_C_OPTIONS -w -msse -mno-ms-bitfields -fcommon -ffp-contract=off -fno-strict-aliasing -fwrapv
  -fno-strict-overflow -fno-delete-null-pointer-checks -fno-aggressive-loop-optimizations
  -include ${COD2_SRC_DIR}/headers/win32_compat.h ${COD2_WNO})

add_executable(${WIN_TARGET} ${WIN_TARGET_C})
target_include_directories(${WIN_TARGET} PRIVATE
  ${COD2_SRC_DIR}/PC/speex ${COD2_SRC_DIR} ${COD2_SRC_DIR}/headers ${COD2_SRC_DIR}/win32/shims
  ${COD2_SRC_DIR}/win32/sdl2/include ${CMAKE_SOURCE_DIR})
target_compile_options(${WIN_TARGET} PRIVATE -g "$<$<COMPILE_LANGUAGE:C>:${COD2_WIN_C_OPTIONS}>")
target_compile_definitions(${WIN_TARGET} PRIVATE _GNU_SOURCE=1 WIN32 _WIN32 SDL_DISABLE_IMMINTRIN_H
  COD2_WINDOWS_NATIVE=1)
target_compile_features(${WIN_TARGET} PRIVATE cxx_std_17)
target_compile_options(${WIN_TARGET} PRIVATE -ffunction-sections -fdata-sections)
# C symbols carry a leading '_' on 32-bit PE only.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(WIN_SYM "_")
  set(WIN_TRIPLE i686-w64-mingw32)
else()
  set(WIN_SYM "")
  set(WIN_TRIPLE x86_64-w64-mingw32)
endif()
# src/platform/macos_cpp_abi.cpp defines the old GCC string ABI for the reconstructed
# C, and a static libstdc++ brings the same names along.
target_link_options(${WIN_TARGET} PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--gc-sections
  -Wl,--allow-multiple-definition
  -Wl,--defsym,${WIN_SYM}__mh_execute_header=0x1000
  -Wl,--defsym,${WIN_SYM}g_entities_ptr=${WIN_SYM}g_entities
  -Wl,--defsym,${WIN_SYM}g_time_ptr=${WIN_SYM}imp_bgs
  -Wl,--defsym,${WIN_SYM}scr_const_ptr=${WIN_SYM}scr_const
  -Wl,--defsym,${WIN_SYM}sv_ptr=${WIN_SYM}sv
  -Wl,--defsym,${WIN_SYM}svs_ptr=${WIN_SYM}svs
  -Wl,--defsym,${WIN_SYM}r_limits_ptr=${WIN_SYM}vidConfig)
target_link_libraries(${WIN_TARGET} PRIVATE z ws2_32 winmm winpthread dbghelp advapi32)
set_target_properties(${WIN_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} SUFFIX ".exe")
set_source_files_properties(src/platform/macos_cpp_abi.cpp PROPERTIES COMPILE_OPTIONS -fvisibility=hidden)
set_source_files_properties(src/PC/qcommon/crash_handler.c PROPERTIES
  COMPILE_DEFINITIONS "COD2_GIT_HASH=\"${COD2_GIT_HASH}\"")

if(COD2_WIN32_CLIENT)
  # The official SDL2 MinGW package, at the version of the headers in src/win32/sdl2.
  set(COD2_SDL2_DIR "${CMAKE_BINARY_DIR}/sdl2" CACHE PATH "Unpacked SDL2 MinGW development package")
  execute_process(COMMAND sh "${CMAKE_SOURCE_DIR}/tools/windows/fetch-sdl2.sh" "${COD2_SDL2_DIR}"
    RESULT_VARIABLE fetch_result)
  if(fetch_result)
    message(FATAL_ERROR "Could not fetch SDL2 into ${COD2_SDL2_DIR}")
  endif()
  target_compile_definitions(${WIN_TARGET} PRIVATE W32_CLIENT)
  if(COD2_WIN32_D3D9)
    target_compile_definitions(${WIN_TARGET} PRIVATE COD2_WIN32_D3D9=1)
  endif()
  target_include_directories(${WIN_TARGET} PRIVATE ${COD2_SRC_DIR}/win32/sdl2/include/SDL2)
  target_link_directories(${WIN_TARGET} PRIVATE "${COD2_SDL2_DIR}/${WIN_TRIPLE}/lib")
  target_link_libraries(${WIN_TARGET} PRIVATE SDL2 opengl32 imm32 ole32 oleaut32 version uuid
    setupapi shell32 dinput8 dsound dxguid gdi32 user32 winmm kernel32)
  # HTTP downloads from servers that redirect to a web host (the 1.3 feature):
  # a static libcurl built from pinned source, on Windows' own TLS.
  set(COD2_CURL_DIR "${CMAKE_BINARY_DIR}/curl" CACHE PATH "Static libcurl build for this target")
  execute_process(COMMAND sh "${CMAKE_SOURCE_DIR}/tools/windows/fetch-curl.sh" "${COD2_CURL_DIR}"
      "${CMAKE_TOOLCHAIN_FILE}"
    RESULT_VARIABLE fetch_result)
  if(fetch_result)
    message(FATAL_ERROR "Could not build libcurl in ${COD2_CURL_DIR}; see its configure.log and build.log")
  endif()
  target_link_directories(${WIN_TARGET} PRIVATE "${COD2_CURL_DIR}/install/lib")
  target_link_libraries(${WIN_TARGET} PRIVATE curl ws2_32 bcrypt crypt32 secur32 advapi32 iphlpapi)
  if(COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
    # The CoD2x machine identity reads WMI (src/PC/qcommon/cod2x_identity.c), and
    # its demo upload uses the libcurl headers (src/PC/qcommon/cod2x_demo.c).
    target_link_libraries(${WIN_TARGET} PRIVATE wbemuuid)
    # CoD2x's HTTP (src/PC/qcommon/cod2x_http.c) includes the curl headers.
    target_include_directories(${WIN_TARGET} PRIVATE "${COD2_CURL_DIR}/install/include")
    target_compile_definitions(${WIN_TARGET} PRIVATE CURL_STATICLIB)
  endif()
else()
  # A server downloads nothing.
  target_compile_definitions(${WIN_TARGET} PRIVATE DEDICATED COD2_FEATURE_WWW_DOWNLOAD=0)
  if(COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
    # Except a CoD2x server: its update check, zPAM downloads, script HTTP and reports
    # use the same static libcurl as the client (src/PC/qcommon/cod2x_http.c).
    set(COD2_CURL_DIR "${CMAKE_BINARY_DIR}/curl" CACHE PATH "Static libcurl build for this target")
    execute_process(COMMAND sh "${CMAKE_SOURCE_DIR}/tools/windows/fetch-curl.sh" "${COD2_CURL_DIR}"
        "${CMAKE_TOOLCHAIN_FILE}"
      RESULT_VARIABLE fetch_result)
    if(fetch_result)
      message(FATAL_ERROR "Could not build libcurl in ${COD2_CURL_DIR}; see its configure.log and build.log")
    endif()
    target_include_directories(${WIN_TARGET} PRIVATE "${COD2_CURL_DIR}/install/include")
    target_compile_definitions(${WIN_TARGET} PRIVATE CURL_STATICLIB)
    target_link_directories(${WIN_TARGET} PRIVATE "${COD2_CURL_DIR}/install/lib")
    target_link_libraries(${WIN_TARGET} PRIVATE curl ws2_32 bcrypt crypt32 secur32 advapi32 iphlpapi)
  endif()
endif()

include(${CMAKE_SOURCE_DIR}/cmake/cod2x-layer.cmake)
cod2x_layer_order(${WIN_TARGET})
