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

target_include_directories(yuv
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(USING_GCC ON)
elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(USING_CLANG ON)
elseif(CCMAKE_XX_COMPILER_ID STREQUAL "MSVC")
  set(USING_MSVC ON)
endif()

if(USING_GCC OR USING_CLANG)
  target_compile_options(yuv
    PRIVATE
      -fomit-frame-pointer
      -fno-strict-aliasing
      -Wno-unknown-warning-option
      -Wno-pragmas
  )
  if (USING_CLANG)
    target_compile_options(yuv PRIVATE -Wno-memset-elt-size)
  endif()
elseif(USING_MSVC)
elseif(MSVC)
  target_compile_options(yuv PRIVATE /Oy)
endif()

# ##############################################################################
# Dependencies

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(yuv PRIVATE "${MATH_LIBRARY}")
