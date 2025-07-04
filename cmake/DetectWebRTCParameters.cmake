set(OPTS_TARGET webrtc_options)

if(TARGET ${OPTS_TARGET})
  return()
endif()

add_library(${OPTS_TARGET} INTERFACE)
add_library(WebRTC::Options ALIAS ${OPTS_TARGET})

#
# Configuration
#

include(Pj/DetectArch)
pj_detect_arch(arch)
pj_detect_arch_simd_ext(simd_inst simd_flags)

target_compile_options(${OPTS_TARGET} INTERFACE ${simd_flags})

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

    # architecture
    $<$<STREQUAL:${arch},arm64>:WEBRTC_ARCH_ARM64=1>
    $<$<STREQUAL:${arch},mips>:MIPS_FPU_LE=1>
    $<$<STREQUAL:${simd_inst},neon>:WEBRTC_HAS_NEON=1>
)

set_target_properties(${OPTS_TARGET} PROPERTIES
  WEBRTC_ARCH "${arch}"
  WEBRTC_ARCH_SIMD "${simd_inst}"
)
