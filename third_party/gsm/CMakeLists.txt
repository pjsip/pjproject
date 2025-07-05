# ##############################################################################
# Target

add_library(gsm
  src/add.c
  src/code.c
  src/decode.c
  src/gsm_create.c
  src/gsm_decode.c
  src/gsm_destroy.c
  src/gsm_encode.c
  src/gsm_explode.c
  src/gsm_implode.c
  src/gsm_option.c
  src/long_term.c
  src/lpc.c
  src/preprocess.c
  src/rpe.c
  src/short_term.c
  src/table.c
)

target_include_directories(gsm
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../build/gsm>
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:inc>
)

target_compile_definitions(gsm
  PRIVATE
    SASR=1
    WAV49=1
    NeedFunctionPrototypes=1
)
