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

# headers
target_sources(gsm
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        inc
      FILES
        inc/config.h
        inc/gsm.h
        inc/private.h
        inc/proto.h
        inc/toast.h
        inc/unproto.h
)

# ##############################################################################
# Configuration

target_include_directories(gsm BEFORE
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/gsm"
)

target_compile_definitions(gsm
  PRIVATE
    SASR=1
    WAV49=1
    NeedFunctionPrototypes=1
)
