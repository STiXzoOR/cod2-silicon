# Native Apple platform. Typed engine data/import slots are supplied by WS2.
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum native macOS version" FORCE)
option(COD2_MACOS_RELEASE "Require a portable pinned-SDL release build" OFF)
enable_language(CXX OBJC)
find_library(COD2_AUDIO_FRAMEWORK AudioToolbox REQUIRED)
find_library(COD2_COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
find_library(COD2_GAMECONTROLLER_FRAMEWORK GameController REQUIRED)
find_library(COD2_FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
if(COD2_MACOS_RELEASE)
  set(COD2_MACOS_SDL_PREFIX "" CACHE PATH "Pinned SDL install prefix for release bundles")
  if(NOT COD2_MACOS_SDL_PREFIX)
    message(FATAL_ERROR "Release bundles require COD2_MACOS_SDL_PREFIX; use scripts/package-release.sh")
  endif()
  set(CMAKE_SKIP_BUILD_RPATH TRUE)
  set(COD2_MACOS_APP_FRAMEWORK_ARGS --frameworks "${COD2_MACOS_SDL_PREFIX}")
endif()
file(GLOB MACOS_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platform/*.[cm]" "${CMAKE_SOURCE_DIR}/src/platform/*.cpp")
set_property(SOURCE ${MACOS_PLATFORM_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
  -Werror=unguarded-availability -Werror=unguarded-availability-new)
find_library(COD2_OPENGL_FRAMEWORK OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_library(COD2_CURL_LIBRARY curl REQUIRED)

file(GLOB_RECURSE MACOS_PC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/PC/*.c")
file(GLOB_RECURSE MACOS_MAC_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/Mac/*.c")
file(GLOB_RECURSE MACOS_STUBS_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/stubs/*.c")
file(GLOB MACOS_ROOT_C CONFIGURE_DEPENDS "${COD2_SRC_DIR}/*.c")
set(MACOS_C ${MACOS_PC_C} ${MACOS_MAC_C} ${MACOS_STUBS_C} ${MACOS_ROOT_C})
# Use the SDK zlib, as in the native Linux build. No 32-bit data generators or ASM.
list(FILTER MACOS_C EXCLUDE REGEX "/PC/zlib/(inflate|infblock|infcodes|inffast|inftrees|infutil|adler32|zutil)\\.c$")
list(FILTER MACOS_C EXCLUDE REGEX "/(data|import_pointers|literals)\\.c$")
# Native replacements own these platform APIs; legacy stubs never intercept SDK calls.
list(FILTER MACOS_C EXCLUDE REGEX "/Mac/Tools/(MacDisplay|MacThreads|CCircularBuffer|CAudioRecorder|MacMSS[^/]*)\\.c$")
list(FILTER MACOS_C EXCLUDE REGEX "/stubs/(agl_stubs|audio_stubs|cpp_compat|cpp_trampoline|fx_override)\\.c$")
# The real CoreFoundation and IOKit must win: these stubs return 0 and would
# shadow the frameworks for every call made from this binary.
list(FILTER MACOS_C EXCLUDE REGEX "/stubs/(iokit_stubs|corefoundation_stubs)\\.c$")
list(APPEND MACOS_C src/unix/linux_common.c src/unix/linux_net.c src/unix/sysdiff_statehash.c
  src/platform/macos_system.c src/platform/macos_threads.c src/platform/macos_ring.c
  src/platform/macos_cpp_abi.cpp src/platform/macos_abs_symbols.c)
list(FILTER MACOS_C EXCLUDE REGEX "/Mac/Main/(mac_play_dsound|mac_record_dsound)\\.c$")
list(FILTER MACOS_C EXCLUDE REGEX "/PC/groupvoice/record\\.c$")
set(MACOS_DED_C ${MACOS_C})
list(FILTER MACOS_DED_C EXCLUDE REGEX "/(Mac/DirectX_9|PC/gfx_d3d|PC/cgame|PC/cgame_mp|PC/ui|PC/ui_mp|PC/EffectsCore|PC/client_mp|PC/groupvoice)/")
# Retain the CPU effect templates, lengths and server visibility implementation.
foreach(unit FxUtil Fxexport FxScheduler FxScheduler_load_obj FxTemplate FxSystem
    GenericParser2 FxChannel FxCurve FxCurve_load_obj FxMemMgr)
  list(APPEND MACOS_DED_C "${COD2_SRC_DIR}/PC/EffectsCore/${unit}.c")
endforeach()
list(FILTER MACOS_DED_C EXCLUDE REGEX "/PC/(snd|win32/cinematics)\\.c$")
list(FILTER MACOS_DED_C EXCLUDE REGEX "/Mac/Main/(mac_input|mac_decode|mac_sound)\\.c$")
# A dedicated build never loads the client sound driver or group voice output.
list(FILTER MACOS_DED_C EXCLUDE REGEX "/PC/win32/(snd_driver|win_voice)\\.c$")
list(FILTER MACOS_DED_C EXCLUDE REGEX "/PC/qcommon/cod2x_(features|demo|url|pose)\\.c$")

# Typed LP64 data generated from the Mac binary's STABS (WS2, tools/datagen).
include(${CMAKE_SOURCE_DIR}/cmake/datagen.cmake)
cod2_generate_typed_blobs(MACOS_GEN_C)
list(APPEND MACOS_C ${MACOS_GEN_C})
list(APPEND MACOS_DED_C ${MACOS_GEN_C})

foreach(target cod2_macos cod2_macos_ded)
  if(target STREQUAL "cod2_macos_ded")
    add_executable(${target} ${MACOS_DED_C})
  else()
    add_executable(${target} ${MACOS_C})
  endif()
  target_include_directories(${target} PRIVATE
    ${COD2_SRC_DIR}/PC/speex ${COD2_SRC_DIR} ${COD2_SRC_DIR}/headers ${CMAKE_SOURCE_DIR})
  # Respect CMAKE_BUILD_TYPE optimization; Debug/unspecified still default to O0.
  target_compile_options(${target} PRIVATE -g -fcommon -ffp-contract=off
    -fno-strict-aliasing ${COD2_WNO}
    -Wshorten-64-to-32 -Wpointer-to-int-cast -Wint-to-pointer-cast
    -Wint-conversion -Wincompatible-pointer-types -Wvoid-pointer-to-int-cast
    -Wno-typedef-redefinition -Wno-duplicate-decl-specifier
    -ferror-limit=0)
  target_link_libraries(${target} PRIVATE ZLIB::ZLIB c++ "-framework IOKit" "-framework CoreFoundation")
  target_compile_features(${target} PRIVATE cxx_std_17)
  # ld64 equivalents of the MinGW --defsym seam aliases in CMakeLists.txt.
  target_link_options(${target} PRIVATE
    "LINKER:-alias,_g_entities,_g_entities_ptr"
    "LINKER:-alias,_imp_bgs,_g_time_ptr"
    "LINKER:-alias,_scr_const,_scr_const_ptr"
    "LINKER:-alias,_sv,_sv_ptr"
    "LINKER:-alias,_svs,_svs_ptr")
endforeach()
# vidConfig lives in the renderer, which the dedicated server does not build.
target_link_options(cod2_macos PRIVATE "LINKER:-alias,_vidConfig,_r_limits_ptr")
target_sources(cod2_macos PRIVATE src/unix/linux_input.c src/platform/macos_display.c src/platform/macos_window.m
  src/platform/macos_rawmouse.m src/platform/macos_audio.c src/platform/macos_voice.c
  src/platform/macos_jpeg.c)
set_property(SOURCE src/platform/macos_rawmouse.m APPEND PROPERTY COMPILE_OPTIONS -fobjc-arc)
# The renderer renders into the exact r_mode framebuffer; presentation scales it.
set_source_files_properties(src/PC/gfx_d3d/rb_state.c src/PC/gfx_d3d/r_screenshot.c PROPERTIES
  COMPILE_DEFINITIONS "glDrawBuffer=MacGL_DrawBuffer;glReadBuffer=MacGL_ReadBuffer")
target_link_libraries(cod2_macos PRIVATE SDL2::SDL2 ${COD2_OPENGL_FRAMEWORK} ${COD2_CURL_LIBRARY}
  ${COD2_AUDIO_FRAMEWORK} ${COD2_COREAUDIO_FRAMEWORK}
  ${COD2_GAMECONTROLLER_FRAMEWORK} ${COD2_FOUNDATION_FRAMEWORK}
  "-framework ImageIO" "-framework CoreGraphics" "-framework AppKit")
if(COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_library(COD2_APPKIT_FRAMEWORK AppKit REQUIRED)
  target_sources(cod2_macos PRIVATE src/platform/cod2x_native.c
    src/platform/cod2x_native_mouse.c src/platform/cod2x_native_macos.m
    src/platform/cod2x_native_shaders.m)
  set_property(SOURCE src/platform/cod2x_native_macos.m src/platform/cod2x_native_shaders.m
    APPEND PROPERTY COMPILE_OPTIONS -fobjc-arc)
  target_link_libraries(cod2_macos PRIVATE ${COD2_APPKIT_FRAMEWORK})
  set(COD2_MACOS_APP "${CMAKE_BINARY_DIR}/CoD2 Silicon.app" CACHE PATH "Native app output path")
  file(GLOB COD2_LAUNCHER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/launcher/*")
  add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/launcher/CoD2Launcher"
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/build-launcher.sh" "${CMAKE_BINARY_DIR}/launcher"
    DEPENDS ${COD2_LAUNCHER_SOURCES} "${CMAKE_SOURCE_DIR}/scripts/build-launcher.sh"
      "${CMAKE_SOURCE_DIR}/src/platform/cod2x_native_setup.h"
      "${CMAKE_SOURCE_DIR}/src/platform/cod2x_native_shaders.m"
      "${CMAKE_SOURCE_DIR}/src/PC/qcommon/cod2x_url.c"
    COMMENT "Build Swift 6 native launcher (macOS 13)"
    VERBATIM)
  add_custom_target(cod2_launcher DEPENDS "${CMAKE_BINARY_DIR}/launcher/CoD2Launcher")
  add_custom_target(cod2_macos_app
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/cod2x/make_macos_app.py"
      "$<TARGET_FILE:cod2_macos>" "${COD2_MACOS_APP}" --replace --launcher "${CMAKE_BINARY_DIR}/launcher/CoD2Launcher" ${COD2_MACOS_APP_FRAMEWORK_ARGS}
    DEPENDS cod2_macos cod2_launcher
    COMMENT "Build and ad-hoc sign CoD2 Silicon.app"
    VERBATIM)
endif()
target_compile_definitions(cod2_macos_ded PRIVATE DEDICATED)
target_link_options(cod2_macos_ded PRIVATE -Wl,-dead_strip)
set_source_files_properties(src/PC/qcommon/crash_handler.c PROPERTIES
  COMPILE_DEFINITIONS "COD2_GIT_HASH=\"${COD2_GIT_HASH}\"")
