function(pj_canonicalize_arch arch out_arch)
  string(TOLOWER "${arch}" arch)

  if(arch MATCHES "^(arm64|armv8|aarch64)")
    set(canonical_arch "arm64")
  elseif(arch MATCHES "^arm7")
    set(canonical_arch "armv7")
  elseif(arch MATCHES "^arm4")
    set(canonical_arch "armv4")
  elseif(arch MATCHES "^arm")
    set(canonical_arch "arm")
  elseif(arch MATCHES "^(x86_64|amd64)")
    set(canonical_arch "x86_64")
  elseif(arch MATCHES "^(x86|i[3-6]86)")
    set(canonical_arch "i386")
  elseif(arch MATCHES "^(ppc|powerpc)")
    set(canonical_arch "powerpc")
  elseif(arch MATCHES "^mips64")
    set(canonical_arch "mips64")
  elseif(arch MATCHES "^mips")
    set(canonical_arch "mips")
  elseif(arch MATCHES "^ia64")
    set(canonical_arch "ia64")
  elseif(arch MATCHES "^m68k")
    set(canonical_arch "m68k")
  elseif(arch MATCHES "^alpha")
    set(canonical_arch "alpha")
  elseif(arch MATCHES "^sparc")
    set(canonical_arch "sparc")
  elseif(arch MATCHES "^nios")
    set(canonical_arch "nios")
  else()
    set(canonical_arch "${arch}")
  endif()

  set("${out_arch}" "${canonical_arch}" PARENT_SCOPE)
endfunction()

function(pj_detect_arch out_arch)
  # if a cached value exists, return it
  if(DEFINED CACHE{_pj_detected_arch})
    set("${out_arch}" "$CACHE{_pj_detected_arch}" PARENT_SCOPE)
    return()
  endif()

  # try getting the value from toolchain variables
  if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(GET CMAKE_OSX_ARCHITECTURES 0 arch)
  elseif(ANDROID AND CMAKE_ANDROID_ARCH)
    set(arch "${CMAKE_ANDROID_ARCH}")
  elseif(MSVC)
    if(MSVC_C_ARCHITECTURE_ID)
      set(arch "${MSVC_C_ARCHITECTURE_ID}")
    elseif(MSVC_CXX_ARCHITECTURE_ID)
      set(arch "${MSVC_CXX_ARCHITECTURE_ID}")
    endif()
  elseif(CMAKE_CROSSCOMPILING)
    if(CMAKE_C_COMPILER_TARGET)
      set(arch "${CMAKE_C_COMPILER_TARGET}")
    elseif(CMAKE_CXX_COMPILER_TARGET)
      set(arch "${CMAKE_CXX_COMPILER_TARGET}")
    endif()
  endif()

  # try getting the value from a compile check
  if(NOT arch)
    try_run(_run_result compileResult
      SOURCES
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/detect-arch.c"
      COMPILE_OUTPUT_VARIABLE
        compileOutput
    )

    string(REGEX REPLACE ".*arch:([a-zA-Z0-9_]+)" "\\1" arch "${compileOutput}")
  endif()

  # get value from cmake variable
  if(NOT arch)
    set(arch "${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  # if not value is detect, return "unknown"
  if(NOT arch)
    set("${out_arch}" "unknown" PARENT_SCOPE)
    return()
  endif()

  # canonicalize value
  pj_canonicalize_arch("${arch}" arch)

  # cache value
  set(_pj_detected_arch "${arch}" CACHE INTERNAL "Detected CPU architecture")

  # return canonical value
  set("${out_arch}" "${arch}" PARENT_SCOPE)
endfunction()

function(pj_detect_arch_simd_ext out_simd out_flags)
  # if a cached values exists, return them
  if(DEFINED CACHE{_pj_detected_simd_ext})
    set("${out_simd}" "$CACHE{_pj_detected_simd_ext}" PARENT_SCOPE)
    set("${out_flags}" "$CACHE{_pj_detected_simd_ext_flags}" PARENT_SCOPE)
    return()
  endif()

  pj_detect_arch(arch)
  if(arch MATCHES "^arm")
    set(simd_inst neon)
    set(simd_check_source [=[
      #if defined(_M_ARM64) || defined(_M_ARM64EC)
      #  include <arm64_neon.h>
      #else
      #  include <arm_neon.h>
      #endif

      int main() {
        return 0;
      }
    ]=])

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      if(arch STREQUAL "arm64")
        set(simd_flags "-march=armv8-a+simd")
      else()
        set(simd_flags "-mfpu=neon")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm.*gnueabihf")
          list(APPEND simd_flags "-mfloat-abi=hard")
        elseif(ANDROID)
          if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi")
            list(APPEND simd_flags "-march=armv7")
          endif()
          if(CMAKE_ANDROID_ARM_MODE)
            list(APPEND simd_flags "-mfloat-abi=softfp")
          endif()
          if(NOT CMAKE_ANDROID_ARM_MODE)
            list(APPEND simd_flags "-mthumb")
          endif()
        endif()
      endif()
    endif()
  elseif(arch MATCHES "^(i386|x86_64|x64)$")
    set(simd_inst sse2)
    set(simd_check_source [=[
      #include <immintrin.h>

      __m128i f(__m128i x, __m128i y) {
        return _mm_sad_epu8(x, y);
      }

      int main(void) {
        return 0;
      }
    ]=])

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND UNIX)
      if(CYGWIN OR MINGW)
        set(simd_flags "-msse2")
      else()
        set(simd_flags "-mfma")
      endif()
    elseif(MSVC AND NOT (arch STREQUAL "x86_64" OR arch STREQUAL "x64"))
      set(simd_flags "/arch:SSE2")
    endif()
  elseif(arch STREQUAL "mips")
    set(simd_inst mips)
  endif()

  set(_simd_supported TRUE)

  if(simd_check_source)
    include(CheckCSourceCompiles)
    include(CMakePushCheckState)

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS ${simd_flags})
    check_c_source_compiles("${simd_check_source}" _simd_supported
        FAIL_REGEX "not supported"
      )
    cmake_pop_check_state()
  endif()

  if(_simd_supported)
    # cache values
    set(_pj_detected_simd_ext "${simd_inst}" CACHE INTERNAL "SIMD extensions")
    set(_pj_detected_simd_ext_flags "${simd_flags}" CACHE INTERNAL "")

    set("${out_simd}" "${simd_inst}" PARENT_SCOPE)
    set("${out_flags}" "${simd_flags}" PARENT_SCOPE)
  else()
    set("${out_simd}" "" PARENT_SCOPE)
    set("${out_flags}" "" PARENT_SCOPE)
  endif()
endfunction()
