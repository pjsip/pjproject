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

# headers
target_sources(ilbc
  PUBLIC
    FILE_SET HEADERS
      FILES
        anaFilter.h
        constants.h
        createCB.h
        doCPLC.h
        enhancer.h
        filter.h
        FrameClassify.h
        gainquant.h
        getCBvec.h
        helpfun.h
        hpInput.h
        hpOutput.h
        iCBConstruct.h
        iCBSearch.h
        iLBC_decode.h
        iLBC_define.h
        iLBC_encode.h
        LPCdecode.h
        LPCencode.h
        lsf.h
        packing.h
        StateConstructW.h
        StateSearchW.h
        syntFilter.h
)

# ##############################################################################
# Dependencies

if (NOT (WIN32 OR CYGWIN OR MIGW))
  # math library
  find_library(MATH_LIBRARY m REQUIRED)
  target_link_libraries(ilbc PRIVATE "${MATH_LIBRARY}")
endif()
