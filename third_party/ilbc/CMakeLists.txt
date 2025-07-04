# ##############################################################################
# Target

add_library(ilbc
  FrameClassify.c
  LPCdecode.c
  LPCencode.c
  StateConstructW.c
  StateSearchW.c
  anaFilter.c
  constants.c
  createCB.c
  doCPLC.c
  enhancer.c
  filter.c
  gainquant.c
  getCBvec.c
  helpfun.c
  hpInput.c
  hpOutput.c
  iCBConstruct.c
  iCBSearch.c
  iLBC_decode.c
  iLBC_encode.c
  lsf.c
  packing.c
  syntFilter.c
)

target_include_directories(ilbc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:.>
)

# ##############################################################################
# Dependencies

# math library
find_library(MATH_LIBRARY m REQUIRED)
target_link_libraries(ilbc PRIVATE "${MATH_LIBRARY}")
