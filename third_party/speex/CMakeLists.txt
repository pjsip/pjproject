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

target_include_directories(speex
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../build/speex>
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(speex PRIVATE HAVE_CONFIG_H=1)

# ##############################################################################
# Dependencies

# pjlib - for config & log overrides
target_link_libraries(speex PRIVATE pjlib)
