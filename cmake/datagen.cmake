# Local Stage 2 regeneration. All proprietary input and generated data stay out
# of git; COD2_X64=OFF never includes this module.
function(cod2_generate_typed_blobs output_var)
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  find_program(COD2_DATAGEN_CLANG NAMES clang)
  if(NOT COD2_DATAGEN_CLANG)
    message(FATAL_ERROR "Typed data generation needs clang with an i386 ELF assembler")
  endif()
  set(COD2_STABS_BINARY "$ENV{HOME}/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
      CACHE FILEPATH "User-supplied i386 Mac binary with STABS (type information only)")
  set(COD2_TYPED_DATA_DIR "${CMAKE_SOURCE_DIR}/build/x64_gen"
      CACHE PATH "Local generated architecture-neutral blobs")
  if(NOT EXISTS "${COD2_STABS_BINARY}")
    message(FATAL_ERROR "Typed data generation needs COD2_STABS_BINARY=${COD2_STABS_BINARY}")
  endif()
  file(GLOB DATAGEN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/datagen/*.py")
  file(GLOB_RECURSE DATAGEN_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/headers/*.h")
  set(typed_c
      "${COD2_TYPED_DATA_DIR}/data_native.c"
      "${COD2_TYPED_DATA_DIR}/literals_native.c"
      "${COD2_TYPED_DATA_DIR}/import_pointers_native.c"
      "${COD2_TYPED_DATA_DIR}/bss_native.c")
  add_custom_command(
    OUTPUT ${typed_c} "${COD2_TYPED_DATA_DIR}/typed_types.h" "${COD2_TYPED_DATA_DIR}/coverage.json"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/datagen/generate.py"
            --binary "${COD2_STABS_BINARY}" --output "${COD2_TYPED_DATA_DIR}"
            --clang "${COD2_DATAGEN_CLANG}"
    DEPENDS ${DATAGEN_SCRIPTS} ${DATAGEN_HEADERS} "${COD2_STABS_BINARY}"
            "${CMAKE_SOURCE_DIR}/src/blobs/data.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/literals.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/import_pointers.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/bss.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/data32.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/literals32.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/import_pointers_native.c"
    COMMENT "Generating typed data from the local STABS reference"
    VERBATIM)
  add_custom_target(cod2_datagen DEPENDS ${typed_c})
  set(${output_var} ${typed_c} PARENT_SCOPE)
endfunction()
