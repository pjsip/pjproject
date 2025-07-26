# ##############################################################################
# Target

add_library(g7221
  common/common.c
  common/huff_tab.c
  common/tables.c
  common/basic_op.c
  decode/coef2sam.c
  decode/dct4_s.c
  decode/decoder.c
  encode/dct4_a.c
  encode/sam2coef.c
  encode/encoder.c
)

target_include_directories(g7221 PRIVATE common)

# HACK: Allow inclusion using `g7221` prefix. This is done
# by linking headers to the binary tree under that prefix,
# then adding the destination to include directories.

set(g7221_interface_root "${CMAKE_CURRENT_BINARY_DIR}/include")
set(g7221_interface_dir "${g7221_interface_root}/g7221")

# create directory if it does not exist
if(NOT EXISTS "${g7221_interface_dir}")
  file(MAKE_DIRECTORY "${g7221_interface_dir}")
endif()

# link files
foreach(g7221_include IN ITEMS common encode decode)
  if(NOT EXISTS "${g7221_interface_dir}/${g7221_include}")
    file(CREATE_LINK
      "${CMAKE_CURRENT_SOURCE_DIR}/${g7221_include}"
      "${g7221_interface_dir}/${g7221_include}"
      SYMBOLIC
    )
  endif()
endforeach()

target_sources(g7221
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        "${g7221_interface_root}"
      FILES
        "${g7221_interface_dir}/common/basic_op.h"
        "${g7221_interface_dir}/common/basic_op_i.h"
        "${g7221_interface_dir}/common/config.h"
        "${g7221_interface_dir}/common/count.h"
        "${g7221_interface_dir}/common/defs.h"
        "${g7221_interface_dir}/common/huff_def.h"
        "${g7221_interface_dir}/common/huff_tab.h"
        "${g7221_interface_dir}/common/tables.h"
        "${g7221_interface_dir}/common/typedef.h"
        "${g7221_interface_dir}/decode/dct4_s.h"
        "${g7221_interface_dir}/encode/dct4_a.h"
)

# ##############################################################################
# Dependencies

# pjlib - for config
target_link_libraries(g7221 PRIVATE pjlib)

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(g7221 PRIVATE "${MATH_LIBRARY}")
