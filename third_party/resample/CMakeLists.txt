# ##############################################################################
# Target

add_library(resample src/resamplesubs.c)

if(BUILD_SHARED_LIBS AND WIN32)
  target_sources(resample PRIVATE src/libresample_dll.c)
endif()

target_include_directories(resample
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../build/resample>
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# HACK: allow inclusion using "third_party/resample/include/resamplesubs.h".
# This should be easily fixable by including "resamplesubs.h" instead of the
# full path.
file(INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/include/resamplesubs.h"
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include/third_party/resample/include"
)

target_include_directories(resample
  INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)
