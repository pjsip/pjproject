# ##############################################################################
# Target

add_library(resample src/resamplesubs.c)

# headers
target_sources(resample
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
      FILES
        include/resamplesubs.h
)

# HACK: allow inclusion using "third_party/resample/include/resamplesubs.h".
# This should be easily fixable by including "resamplesubs.h" instead of the
# full path.
file(CREATE_LINK
  "${CMAKE_CURRENT_SOURCE_DIR}/include/resamplesubs.h"
  "${CMAKE_CURRENT_BINARY_DIR}/include/third_party/resample/include/resamplesubs.h"
  SYMBOLIC
  COPY_ON_ERROR
)

target_include_directories(resample
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

# ##############################################################################
# Configuration

target_include_directories(resample BEFORE
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/resample"
)
