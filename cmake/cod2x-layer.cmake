# The CoD2x layer, every source with "cod2x" in its file name, is linked as one run
# between src/PC/qcommon/cod2x_layer_start.c and cod2x_layer_stop.c. Linkers lay out
# code in input order, so Cod2x_InLayer() can tell from an address whether a crash or
# an error came from CoD2x-layer code; only those reports may go to CoD2x. Stock
# builds keep their source order.
# Call it after the target's last target_sources().
function(cod2x_layer_order target)
  if(NOT COD2_FEATURE_CFLAGS MATCHES "(^| )-DCOD2_CODX=1( |$)")
    return()
  endif()
  get_target_property(sources ${target} SOURCES)
  set(start ${sources})
  list(FILTER start INCLUDE REGEX "/cod2x_layer_start\\.c$")
  set(stop ${sources})
  list(FILTER stop INCLUDE REGEX "/cod2x_layer_stop\\.c$")
  set(layer ${sources})
  list(FILTER layer INCLUDE REGEX "cod2x[^/]*\\.(c|m)$")
  list(FILTER layer EXCLUDE REGEX "/cod2x_layer_(start|stop)\\.c$")
  # The native app layer (links, freeze watch, setup) carries the name but is not
  # CoD2x's code: its faults must never be reported to CoD2x.
  list(FILTER layer EXCLUDE REGEX "/cod2x_native[^/]*$")
  list(REMOVE_ITEM sources ${start} ${layer} ${stop})
  set_property(TARGET ${target} PROPERTY SOURCES ${sources} ${start} ${layer} ${stop})
  # MinGW's linker sorts per-function sections (.text$name) by name; the layer's
  # code stays in plain .text, which keeps the input order.
  if(WIN32)
    set_property(SOURCE ${start} ${layer} ${stop} APPEND PROPERTY COMPILE_OPTIONS -fno-function-sections)
  endif()
endfunction()
