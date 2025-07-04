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
set(g7221_includes common encode decode)

foreach(g7221_include IN LISTS g7221_includes)
  if(NOT EXISTS "${g7221_interface_dir}/${g7221_include}")
    file(CREATE_LINK
      "${CMAKE_CURRENT_SOURCE_DIR}/${g7221_include}"
      "${g7221_interface_dir}/${g7221_include}"
      SYMBOLIC
      COPY_ON_ERROR
    )
  endif()
endforeach()

target_include_directories(g7221 PUBLIC "${g7221_interface_root}")

# ##############################################################################
# Dependencies

# pjlib - for config
target_link_libraries(g7221 PRIVATE pjlib)

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(g7221 PUBLIC "${MATH_LIBRARY}")
