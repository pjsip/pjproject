# Intel IPP and its speech codec samples (USC), for ipp_codecs.c
#
# Defines the imported target `IPP::IPP`. As autoconf --enable-ipp:
#
#   IPP_ROOT          IPP location (--with-ipp), else $ENV{IPPROOT}, else
#                     the first /opt/intel/ipp/*/*
#   IPP_SAMPLES_ROOT  samples location (--with-ipp-samples), else
#                     $ENV{IPPSAMPLES}, else /opt/intel/ipp-samples
#   IPP_ARCH_SUFFIX   library name suffix (--with-ipp-arch), e.g. "64" or
#                     "em64t"; empty for IA32
#
# The samples must have been built: the library is libusc.a (IPP 5) or
# libspeech.a (IPP 6) under speech-codecs/{bin,_bin}/*gcc*/lib.

set(IPP_ARCH_SUFFIX "" CACHE STRING "Intel IPP library name suffix")

if(NOT IPP_ROOT)
  if(DEFINED ENV{IPPROOT})
    set(IPP_ROOT "$ENV{IPPROOT}")
  else()
    file(GLOB _ipp_roots LIST_DIRECTORIES TRUE "/opt/intel/ipp/*/*")
    if(_ipp_roots)
      list(GET _ipp_roots 0 IPP_ROOT)
    endif()
    unset(_ipp_roots)
  endif()
endif()

if(NOT IPP_SAMPLES_ROOT)
  if(DEFINED ENV{IPPSAMPLES})
    set(IPP_SAMPLES_ROOT "$ENV{IPPSAMPLES}")
  elseif(IS_DIRECTORY "/opt/intel/ipp-samples")
    set(IPP_SAMPLES_ROOT "/opt/intel/ipp-samples")
  endif()
endif()

# IPP libraries, and where they live
set(_ipp_names ippsc${IPP_ARCH_SUFFIX} ipps${IPP_ARCH_SUFFIX}
               ippcore${IPP_ARCH_SUFFIX})
set(_ipp_dirs)
if(IPP_ROOT)
  if(APPLE)
    list(APPEND _ipp_dirs "${IPP_ROOT}/Libraries" "${IPP_ROOT}/lib")
  else()
    set(_ipp_arch)
    foreach(_arch intel64 ia32 mic)
      if(IS_DIRECTORY "${IPP_ROOT}/lib/${_arch}")
        set(_ipp_arch ${_arch})
        break()
      endif()
    endforeach()
    if(_ipp_arch)
      # IPP 7, with the OpenMP runtime from the compiler beside it
      list(APPEND _ipp_dirs "${IPP_ROOT}/lib/${_ipp_arch}"
                            "${IPP_ROOT}/../compiler/lib/${_ipp_arch}")
      list(APPEND _ipp_names iomp5)
    else()
      # IPP 6 and older
      list(APPEND _ipp_dirs "${IPP_ROOT}/sharedlib")
      list(APPEND _ipp_names ippsr${IPP_ARCH_SUFFIX} guide)
    endif()
    unset(_ipp_arch)
    unset(_arch)
  endif()
endif()

find_path(IPP_INCLUDE_DIR
  NAMES ippcore.h
  HINTS "${IPP_ROOT}/include"
)
mark_as_advanced(IPP_INCLUDE_DIR)

set(_ipp_libs)
set(_ipp_required_vars)
foreach(_name IN LISTS _ipp_names)
  string(TOUPPER "IPP_LIBRARY_${_name}" _var)
  find_library(${_var} NAMES ${_name} HINTS ${_ipp_dirs})
  mark_as_advanced(${_var})
  list(APPEND _ipp_required_vars ${_var})
  list(APPEND _ipp_libs "${${_var}}")
endforeach()
unset(_name)
unset(_var)

# The speech codec samples: built library and USC headers
set(_ipp_usc_dirs)
if(IPP_SAMPLES_ROOT)
  file(GLOB _ipp_usc_dirs LIST_DIRECTORIES TRUE
    "${IPP_SAMPLES_ROOT}/speech-codecs/bin/*gcc*/lib"
    "${IPP_SAMPLES_ROOT}/speech-codecs/_bin/*gcc*/lib"
    "${IPP_SAMPLES_ROOT}/speech-codecs/_bin/*icc*/lib"
  )
endif()

find_path(IPP_USC_INCLUDE_DIR
  NAMES usc.h
  HINTS "${IPP_SAMPLES_ROOT}/speech-codecs/core/usc/include"
)
mark_as_advanced(IPP_USC_INCLUDE_DIR)

# Only the static library in the samples' build directory, which also keeps
# Apple's Speech.framework from matching
find_library(IPP_USC_LIBRARY
  NAMES libusc.a libspeech.a
  PATHS ${_ipp_usc_dirs}
  NO_DEFAULT_PATH
)
mark_as_advanced(IPP_USC_LIBRARY)
unset(_ipp_usc_dirs)

if(IPP_INCLUDE_DIR AND IPP_USC_INCLUDE_DIR AND IPP_USC_LIBRARY
   AND NOT "${_ipp_libs}" MATCHES "-NOTFOUND")
  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(IPP
    INCLUDE_DIRS "${IPP_INCLUDE_DIR}" "${IPP_USC_INCLUDE_DIR}"
    LIBRARIES "${IPP_USC_LIBRARY}" ${_ipp_libs}
    CODE [=[
      #include <ippcore.h>
      #include <usc.h>
      extern USC_Fxns USC_G729AFP_Fxns;
      int main(void) { ippStaticInit(); return 0; }
    ]=]
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IPP
  REQUIRED_VARS
    IPP_INCLUDE_DIR
    ${_ipp_required_vars}
    IPP_USC_INCLUDE_DIR
    IPP_USC_LIBRARY
    IPP_USABLE
)

if(IPP_FOUND)
  set(IPP_INCLUDE_DIRS "${IPP_INCLUDE_DIR}" "${IPP_USC_INCLUDE_DIR}")
  set(IPP_LIBRARIES "${IPP_USC_LIBRARY}" ${_ipp_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET IPP::IPP)
    add_library(IPP::IPP INTERFACE IMPORTED GLOBAL)
    set_target_properties(IPP::IPP PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${IPP_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${IPP_LIBRARIES}"
    )
  endif()
endif()

unset(_ipp_names)
unset(_ipp_dirs)
unset(_ipp_libs)
unset(_ipp_required_vars)
