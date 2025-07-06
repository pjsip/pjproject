# ##############################################################################
# Target

add_library(speex
  libspeex/bits.c
  libspeex/cb_search.c
  libspeex/exc_10_16_table.c
  libspeex/exc_10_32_table.c
  libspeex/exc_20_32_table.c
  libspeex/exc_5_256_table.c
  libspeex/exc_5_64_table.c
  libspeex/exc_8_128_table.c
  libspeex/fftwrap.c
  libspeex/filterbank.c
  libspeex/filters.c
  libspeex/gain_table.c
  libspeex/gain_table_lbr.c
  libspeex/hexc_10_32_table.c
  libspeex/hexc_table.c
  libspeex/high_lsp_tables.c
  libspeex/kiss_fft.c
  libspeex/kiss_fftr.c
  libspeex/lpc.c
  libspeex/lsp.c
  libspeex/lsp_tables_nb.c
  libspeex/ltp.c
  libspeex/mdf.c
  libspeex/modes.c
  libspeex/modes_wb.c
  libspeex/nb_celp.c
  libspeex/preprocess.c
  libspeex/quant_lsp.c
  libspeex/resample.c
  libspeex/sb_celp.c
  libspeex/smallft.c
  libspeex/speex.c
  libspeex/speex_callbacks.c
  libspeex/speex_header.c
  libspeex/scal.c
  libspeex/stereo.c
  libspeex/vbr.c
  libspeex/vq.c
  libspeex/window.c
)

# headers
target_sources(speex
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/speex"
      FILES
        include/speex/speex.h
        include/speex/speex_bits.h
        include/speex/speex_buffer.h
        include/speex/speex_callbacks.h
        include/speex/speex_config_types.h.in
        include/speex/speex_echo.h
        include/speex/speex_header.h
        include/speex/speex_jitter.h
        include/speex/speex_preprocess.h
        include/speex/speex_resampler.h
        include/speex/speex_stereo.h
        include/speex/speex_types.h
        include/speex/speexdsp_types.h

        "${CMAKE_CURRENT_SOURCE_DIR}/../build/speex/speex/speex_config_types.h"
)

# ##############################################################################
# Dependencies

# pjlib - for config & log overrides
target_link_libraries(speex PRIVATE pjlib)

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(speex PUBLIC "${MATH_LIBRARY}")

# ##############################################################################
# Configuration

target_include_directories(speex BEFORE
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/speex"
)

target_compile_definitions(speex PRIVATE HAVE_CONFIG_H=1)
