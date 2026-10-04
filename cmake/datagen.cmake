# COD2_X64=OFF never includes this module. Private inputs remain local; only the
# five production sources are tracked in build/lp64_gen.
function(cod2_generate_typed_blobs output_var)
  option(COD2_REGENERATE_TYPED_DATA "Require private inputs and regenerate typed data" OFF)
  option(COD2_UPDATE_TYPED_SNAPSHOT "Refresh the committed typed-data snapshot" OFF)
  set(COD2_STABS_BINARY "$ENV{HOME}/Projects/cod2-native-refs/macbin/cod2mp_mac_1.3_i386"
      CACHE FILEPATH "User-supplied i386 Mac binary with STABS (type information only)")
  set(COD2_VALUES_BINARY "$ENV{HOME}/Games/CoD2-mac-bin/Call of Duty 2.app/Contents/Call of Duty 2 Multiplayer.app/Contents/MacOS/Call of Duty 2 Multiplayer"
      CACHE FILEPATH "User-supplied Steam i386 Mac binary for verified LP64 scalar values")
  set(COD2_TYPED_DATA_DIR "${CMAKE_BINARY_DIR}/x64_gen"
      CACHE PATH "Local generated architecture-neutral blobs")
  set(snapshot_dir "${CMAKE_SOURCE_DIR}/build/lp64_gen")
  set(typed_names data_native.c literals_native.c import_pointers_native.c bss_native.c)
  set(snapshot_files)
  foreach(name ${typed_names} typed_types.h)
    list(APPEND snapshot_files "${snapshot_dir}/${name}")
    if(NOT EXISTS "${snapshot_dir}/${name}")
      message(FATAL_ERROR "Missing committed typed-data snapshot: ${snapshot_dir}/${name}")
    endif()
  endforeach()
  if(NOT COD2_REGENERATE_TYPED_DATA AND NOT COD2_UPDATE_TYPED_SNAPSHOT AND
     (NOT EXISTS "${COD2_STABS_BINARY}" OR NOT EXISTS "${COD2_VALUES_BINARY}"))
    message(STATUS "Private typed-data inputs unavailable; compiling committed build/lp64_gen snapshot")
    set(typed_c)
    foreach(name ${typed_names})
      list(APPEND typed_c "${snapshot_dir}/${name}")
    endforeach()
    # Both source selections describe the same payload; normalize only its debug
    # paths so snapshot and regenerated objects can be compared byte for byte.
    set_source_files_properties(${typed_c} PROPERTIES COMPILE_OPTIONS
      "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.;-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.;-fdebug-prefix-map=${snapshot_dir}=build/lp64_gen")
    add_custom_target(cod2_datagen DEPENDS ${snapshot_files})
    set(${output_var} ${typed_c} PARENT_SCOPE)
    return()
  endif()
  foreach(input COD2_STABS_BINARY COD2_VALUES_BINARY)
    if(NOT EXISTS "${${input}}")
      message(FATAL_ERROR "Typed data regeneration needs ${input}=${${input}}")
    endif()
  endforeach()
  get_filename_component(typed_dir "${COD2_TYPED_DATA_DIR}" REALPATH)
  if(typed_dir STREQUAL snapshot_dir)
    message(FATAL_ERROR "COD2_TYPED_DATA_DIR must not be the committed snapshot directory")
  endif()
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
  find_program(COD2_DATAGEN_CLANG NAMES clang)
  if(NOT COD2_DATAGEN_CLANG)
    message(FATAL_ERROR "Typed data generation needs clang with an i386 ELF assembler")
  endif()
  set(snapshot_args)
  if(COD2_UPDATE_TYPED_SNAPSHOT)
    list(APPEND snapshot_args --update)
  endif()
  file(GLOB DATAGEN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/datagen/*.py")
  file(GLOB_RECURSE DATAGEN_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/headers/*.h")
  set(typed_c)
  foreach(name ${typed_names})
    list(APPEND typed_c "${COD2_TYPED_DATA_DIR}/${name}")
  endforeach()
  set(verified "${COD2_TYPED_DATA_DIR}/snapshot-verified.stamp")
  add_custom_command(
    OUTPUT ${typed_c} "${COD2_TYPED_DATA_DIR}/typed_types.h" "${verified}"
    BYPRODUCTS "${COD2_TYPED_DATA_DIR}/coverage.json"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/datagen/generate.py"
            --binary "${COD2_STABS_BINARY}" --output "${COD2_TYPED_DATA_DIR}"
            --values-binary "${COD2_VALUES_BINARY}"
            --clang "${COD2_DATAGEN_CLANG}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/datagen/snapshot.py"
            "${COD2_TYPED_DATA_DIR}" "${snapshot_dir}" ${snapshot_args}
    COMMAND "${CMAKE_COMMAND}" -E touch "${verified}"
    DEPENDS ${DATAGEN_SCRIPTS} ${DATAGEN_HEADERS} ${snapshot_files}
            "${COD2_STABS_BINARY}" "${COD2_VALUES_BINARY}"
            "${CMAKE_SOURCE_DIR}/src/blobs/data.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/literals.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/import_pointers.S"
            "${CMAKE_SOURCE_DIR}/src/blobs/bss.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/data32.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/literals32.c"
            "${CMAKE_SOURCE_DIR}/build/native_gen/import_pointers_native.c"
    COMMENT "Regenerating typed data and verifying the committed LP64 snapshot"
    VERBATIM)
  set_source_files_properties(${typed_c} PROPERTIES OBJECT_DEPENDS "${verified}"
    COMPILE_OPTIONS "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.;-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.;-fdebug-prefix-map=${COD2_TYPED_DATA_DIR}=build/lp64_gen")
  add_custom_target(cod2_datagen DEPENDS ${typed_c} "${verified}")
  set(${output_var} ${typed_c} PARENT_SCOPE)
endfunction()
