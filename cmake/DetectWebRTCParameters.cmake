set(OPTS_TARGET webrtc_options)

if(TARGET ${OPTS_TARGET})
  return()
endif()

add_library(${OPTS_TARGET} INTERFACE)
add_library(WebRTC::Options ALIAS ${OPTS_TARGET})

# base compile definitions
target_compile_definitions(${OPTS_TARGET}
  INTERFACE
    WEBRTC_APM_DEBUG_DUMP=0
    WEBRTC_POSIX=1

    # os detecton
    $<$<BOOL:${IOS}>:WEBRTC_IOS=1>
    $<$<BOOL:${DARWIN}>:WEBRTC_MAC=1>
    $<$<BOOL:${ANDROID}>:WEBRTC_ANDROID=1>
    $<$<BOOL:${LINUX}>:WEBRTC_LINUX=1>
    $<$<BOOL:${WIN32}>:WEBRTC_WIN=1>
)

# ##############################################################################
# Architecture detection
# ##############################################################################

if(ANDROID)
  set(_arch "${CMAKE_ANDROID_ARCH}")
else()
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
endif()

if(_arch MATCHES "^(arm64|armv8|aarch64)")
  set(WEBRTC_ARCH "arm64")
  target_compile_definitions(${OPTS_TARGET} INTERFACE WEBRTC_ARCH_ARM64=1)
elseif(_arch MATCHES "^arm")
  set(WEBRTC_ARCH "arm")
  target_compile_definitions(${OPTS_TARGET} INTERFACE WEBRTC_ARCH_AR_V7=1)
elseif(_arch MATCHES "^(x86_64|amd64)")
  set(WEBRTC_ARCH "x86_64")
elseif(_arch MATCHES "^(x86|i[3-6]86)")
  set(WEBRTC_ARCH "x86")
elseif(_arch MATCHES "^mips64")
  set(WEBRTC_ARCH "mips64")
elseif(_arch MATCHES "^mips")
  set(WEBRTC_ARCH "mips")
else()
  message(WARNING "Unknown CPU '${_arch}', skipping CPU-specific options")
  return()
endif()

set_target_properties(${OPTS_TARGET} PROPERTIES
  WEBRTC_ARCH "${WEBRTC_ARCH}"
)

# ##############################################################################
# Apply settings
# ##############################################################################

# ISA
if(WEBRTC_ARCH STREQUAL "arm")
  if(ANDROID AND NOT CMAKE_ANDROID_ARM_MODE)
    target_compile_options(${OPTS_TARGET}
      INTERFACE
        -mthumb
        -march=armv7
    )
  endif()
endif()

# SIMD instruction set
if(WEBRTC_ARCH MATCHES "^(arm|arm64)$")
  if(WIN32 OR DARWIN OR LINUX)
    include(CheckCCompilerFlag)
    if(WEBRTC_ARCH STREQUAL "arm64")
      check_c_compiler_flag(-march=armv8-a+simd _has_neon)
    else()
      check_c_compiler_flag(-mfpu=neon _has_neon)
    endif()

    if(_has_neon AND _arch MATCHES "^arm.*gnueabihf")
      target_compile_options(${OPTS_TARGET}
        INTERFACE
          -mfloat-abi=hard
          -mfpu=neon
      )
    endif()
  elseif(ANDROID)
    if(CMAKE_ANDROID_ARM_NEON)
      target_compile_options(${OPTS_TARGET}
        INTERFACE
          -mfloat-abi=softfp
          -mfpu=neon
      )
      set(_has_neon ON)
    endif()
  else()
    set(_has_neon ON)
  endif()

  if(_has_neon)
    target_compile_definitions(${OPTS_TARGET} INTERFACE WEBRTC_HAS_NEON=1)
    set(WEBRTC_ARCH_SIMD "neon")
  endif()
elseif(WEBRTC_ARCH MATCHES "^(x86|x86_64)$")
  if(MINGW OR CYGWIN)
    target_compile_options(${OPTS_TARGET} INTERFACE -msse2)
  endif()
  target_compile_options(${OPTS_TARGET} INTERFACE -mfma)
  set(WEBRTC_ARCH_SIMD "sse2")
elseif(WEBRTC_ARCH STREQUAL "mips")
  target_compile_definitions(${OPTS_TARGET} INTERFACE MIPS_FPU_LE=1)
  set(WEBRTC_ARCH_SIMD "mips")
else()
  set(WEBRTC_ARCH_SIMD "generic")
endif()

set_target_properties(${OPTS_TARGET} PROPERTIES
  WEBRTC_ARCH_SIMD "${WEBRTC_ARCH_SIMD}"
)
