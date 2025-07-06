# ##############################################################################
# Target

add_library(yuv
  source/compare.cc
  source/compare_common.cc
  source/compare_gcc.cc
  source/compare_neon64.cc
  source/compare_neon.cc
  source/compare_win.cc
  source/convert_argb.cc
  source/convert.cc
  source/convert_from_argb.cc
  source/convert_from.cc
  source/convert_jpeg.cc
  source/convert_to_argb.cc
  source/convert_to_i420.cc
  source/cpu_id.cc
  source/mjpeg_decoder.cc
  source/mjpeg_validate.cc
  source/planar_functions.cc
  source/rotate_any.cc
  source/rotate_argb.cc
  source/rotate.cc
  source/rotate_common.cc
  source/rotate_gcc.cc
  source/rotate_dspr2.cc
  source/rotate_neon64.cc
  source/rotate_neon.cc
  source/rotate_win.cc
  source/row_any.cc
  source/row_common.cc
  source/row_gcc.cc
  source/row_dspr2.cc
  source/row_neon64.cc
  source/row_neon.cc
  source/row_win.cc
  source/scale_any.cc
  source/scale_argb.cc
  source/scale.cc
  source/scale_common.cc
  source/scale_gcc.cc
  source/scale_dspr2.cc
  source/scale_neon64.cc
  source/scale_neon.cc
  source/scale_win.cc
  source/video_common.cc
)

# headers
target_sources(yuv
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/libyuv.h
      include/libyuv/basic_types.h
      include/libyuv/compare.h
      include/libyuv/compare_row.h
      include/libyuv/convert.h
      include/libyuv/convert_argb.h
      include/libyuv/convert_from.h
      include/libyuv/convert_from_argb.h
      include/libyuv/cpu_id.h
      include/libyuv/macros_msa.h
      include/libyuv/mjpeg_decoder.h
      include/libyuv/planar_functions.h
      include/libyuv/rotate.h
      include/libyuv/rotate_argb.h
      include/libyuv/rotate_row.h
      include/libyuv/row.h
      include/libyuv/scale.h
      include/libyuv/scale_argb.h
      include/libyuv/scale_row.h
      include/libyuv/version.h
      include/libyuv/video_common.h
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(yuv
    PRIVATE
      -fomit-frame-pointer
      -fno-strict-aliasing
      -Wno-memset-elt-size
      -Wno-pragmas
      -Wno-unknown-warning-option
  )
elseif(MSVC)
  target_compile_options(yuv PRIVATE /Oy)
endif()

# ##############################################################################
# Dependencies

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(yuv PRIVATE "${MATH_LIBRARY}")
