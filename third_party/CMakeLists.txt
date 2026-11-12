list(APPEND CMAKE_MESSAGE_CONTEXT "third_party")

set(DEPS)

# G7221
set(DEP_G7221_HELP    "G7221 codec implementation provider")
set(DEP_G7221_SUBDIR  "g7221")
list(APPEND DEPS G7221)

# GSM
set(DEP_GSM_HELP    "GSM codec implementation provider")
set(DEP_GSM_SUBDIR  "gsm")
set(DEP_GSM_PACKAGE "GSM")
list(APPEND DEPS GSM)

# iLBC
set(DEP_iLBC_HELP   "iLBC codec implementation provider")
set(DEP_iLBC_SUBDIR "ilbc")
list(APPEND DEPS iLBC)

# Resample
set(DEP_Resample_HELP    "libresample implementation provider")
set(DEP_Resample_SUBDIR  "resample")
set(DEP_Resample_PACKAGE "Resample")
list(APPEND DEPS Resample)

# Speex
set(DEP_Speex_HELP    "Speex codec & resampling implementation provider")
set(DEP_Speex_SUBDIR  "speex")
set(DEP_Speex_PACKAGE "Speex")
list(APPEND DEPS Speex)

# SRTP
set(DEP_SRTP_HELP    "Secure RTP (sRTP) implementation provider")
set(DEP_SRTP_SUBDIR  "srtp")
set(DEP_SRTP_PACKAGE "SRTP")
list(APPEND DEPS SRTP)

# A provider asked for by name that cannot be used stops the configure, see
# pj_dep_fail_if_asked(); auto and none leave the library out
include(Pj/Option)

# Both WebRTC libraries pick their SIMD sources and flags for a single
# architecture, so an Apple build for several at once (e.g. arm64;x86_64)
# cannot build them.
list(LENGTH CMAKE_OSX_ARCHITECTURES _pj_osx_arch_count)
string(CONCAT _pj_multi_arch_reason
  "it cannot be built for several architectures at once "
  "(CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}), only for one")

# WebRTC is C, except the bundled sources' x86 CPU feature detection
# (cpu_features.cc); pjmedia's code that calls it is C
include(Pj/DetectArch)
pj_detect_arch(_pj_arch)
pj_dep_choice(PJ_DEP_WEBRTC WebRTC _pj_choice)
if(_pj_osx_arch_count GREATER 1)
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC WebRTC "bundled;system"
    "${_pj_multi_arch_reason}")
  message(STATUS
    "[!] WebRTC cannot be built for several architectures at once. leaving it out")
elseif(PJ_WITH_CXX OR NOT _pj_arch MATCHES "^(i386|x86_64|x64)$")
  set(DEP_WebRTC_HELP  "WebRTC implementation provider")
  set(DEP_WebRTC_SUBDIR "webrtc")
  set(DEP_WebRTC_PACKAGE "WebRTC")
  list(APPEND DEPS WebRTC)
# x86 without C++: only an installed WebRTC, asked for or picked by a root
elseif(_pj_choice STREQUAL "system")
  set(DEP_WebRTC_HELP  "WebRTC implementation provider")
  set(DEP_WebRTC_PACKAGE "WebRTC")
  list(APPEND DEPS WebRTC)
else()
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC "" "bundled"
    "the bundled WebRTC needs C++ on x86 (PJ_WITH_CXX)")
  message(STATUS "[!] WebRTC needs C++ on x86. leaving it out")
endif()
unset(_pj_arch)

# WebRTC_AEC3 (C++). Opt-in, as autoconf --enable-libwebrtc-aec3, and only
# imported when enabled, so an unused library is neither built nor exported.
if(PJ_WITH_CXX)
  option(PJMEDIA_WITH_WEBRTC_AEC3
    "Enable WebRTC Acoustic Echo Cancellation 3 (WebRTC AEC3) support" OFF)
endif()
pj_dep_choice(PJ_DEP_WEBRTC_AEC3 WebRTC_AEC3 _pj_choice)
if(NOT PJMEDIA_WITH_WEBRTC_AEC3)
  # a provider alone does not enable it, as --with-external-webrtc-aec3
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC_AEC3 "" "bundled;system"
    "WebRTC AEC3 is off; enable it with -DPJMEDIA_WITH_WEBRTC_AEC3=ON")
elseif(NOT PJ_WITH_CXX)
  # pjmedia's AEC3 code is C++ too, so no provider can be used
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC_AEC3 WebRTC_AEC3 "bundled;system"
    "WebRTC AEC3 needs C++ (PJ_WITH_CXX)")
elseif(_pj_osx_arch_count GREATER 1)
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC_AEC3 WebRTC_AEC3 "bundled;system"
    "${_pj_multi_arch_reason}")
  message(STATUS "[!] WebRTC AEC3 cannot be built for several "
    "architectures at once. leaving it out")
  set_property(GLOBAL PROPERTY PJ_WEBRTC_AEC3_LEFT_OUT TRUE)
# The bundled sources take MSVC's or POSIX threading paths, and neither
# compiles with MinGW (autoconf fails the same way); a system one may.
elseif(MINGW AND _pj_choice MATCHES "^(auto|bundled)$")
  pj_dep_fail_if_asked(PJ_DEP_WEBRTC_AEC3 "" "bundled"
    "the bundled WebRTC AEC3 cannot be built with MinGW")
  message(STATUS "[!] The bundled WebRTC AEC3 cannot be built with MinGW. "
    "leaving it out")
  set_property(GLOBAL PROPERTY PJ_WEBRTC_AEC3_LEFT_OUT TRUE)
else()
  set(DEP_WebRTC_AEC3_HELP   "WebRTC AEC3 implementation provider")
  set(DEP_WebRTC_AEC3_SUBDIR "webrtc_aec3")
  set(DEP_WebRTC_AEC3_PACKAGE "WebRTC_AEC3")
  list(APPEND DEPS WebRTC_AEC3)
endif()

unset(_pj_choice)
unset(_pj_osx_arch_count)
unset(_pj_multi_arch_reason)

# YUV (C++)
if(PJ_WITH_CXX)
  set(DEP_YUV_HELP    "libyuv codec implementation provider")
  set(DEP_YUV_SUBDIR  "yuv")
  set(DEP_YUV_PACKAGE "YUV")
  list(APPEND DEPS YUV)
else()
  # its link test is C++ too, so an installed one cannot be checked either
  pj_dep_fail_if_asked(PJ_DEP_YUV YUV "bundled;system"
    "libyuv needs C++ (PJ_WITH_CXX)")
endif()

# imports dependencies
set(install_subdirs)
foreach(dep IN LISTS DEPS)
  unset(dep_target)

  # construct providers list
  set(dep_providers)
  if(DEP_${dep}_SUBDIR)
    list(APPEND dep_providers "bundled")
  endif()
  if(DEP_${dep}_PACKAGE)
    list(APPEND dep_providers "system")
  endif()
  if(NOT dep_providers)
    message(FATAL_ERROR "No providers were set for ${dep}")
  endif()
  # "none" leaves the dependency out entirely; the features that need it
  # test for the Pj::Dep:: target and switch themselves off.
  list(APPEND dep_providers "none")

  # add option
  string(TOUPPER "PJ_DEP_${dep}" dep_option_name)
  # A build folder from before "auto" stored the default it had written; a
  # value still equal to it was not chosen by the user
  if(DEFINED CACHE{_PJ_${dep_option_name}_DEFAULT})
    if("$CACHE{${dep_option_name}}" STREQUAL
       "$CACHE{_PJ_${dep_option_name}_DEFAULT}")
      set_property(CACHE ${dep_option_name} PROPERTY VALUE auto)
    endif()
    unset(_PJ_${dep_option_name}_DEFAULT CACHE)
  endif()
  pj_option(${dep_option_name} "${DEP_${dep}_HELP}"
    DEFAULT auto
    ALLOWED_VALUES auto ${dep_providers}
  )

  # auto: the installed library when a root is given for it, as that names
  # the library wanted; the bundled one otherwise. Decided on every configure,
  # so adding or removing a root is followed, while a value the user set is
  # never changed.
  set(dep_provider "${${dep_option_name}}")
  if(dep_provider STREQUAL "auto")
    list(GET dep_providers 0 dep_provider)
    if(DEP_${dep}_PACKAGE)
      pj_package_root_given(${DEP_${dep}_PACKAGE} _root_given)
      if(_root_given)
        set(dep_provider "system")
        message(STATUS "${dep}: the installed library, as its root is given "
          "(${dep_option_name}=auto)")
      endif()
    endif()
  endif()

  # import dependency
  if(dep_provider STREQUAL "bundled")
    add_subdirectory(${DEP_${dep}_SUBDIR} EXCLUDE_FROM_ALL)
    # A target name of our own, so that a project building pjproject inside
    # it (add_subdirectory(), FetchContent) can have its own srtp or yuv. The
    # library file and the installed Pj::<name> keep the plain name.
    set(dep_target pj_${DEP_${dep}_SUBDIR})
    set_target_properties(${dep_target} PROPERTIES
      OUTPUT_NAME ${DEP_${dep}_SUBDIR}
      EXPORT_NAME ${DEP_${dep}_SUBDIR}
    )
    list(APPEND install_subdirs ${DEP_${dep}_SUBDIR})
  elseif(dep_provider STREQUAL "system")
    pj_retry_on_new_root(${dep_option_name} ${DEP_${dep}_PACKAGE})
    find_package(${DEP_${dep}_PACKAGE} REQUIRED)
    if(${DEP_${dep}_PACKAGE}_FOUND)
      if(${DEP_${dep}_PACKAGE_TARGET})
        set(dep_target ${DEP_${dep}_PACKAGE_TARGET})
      else()
        set(dep_target ${DEP_${dep}_PACKAGE}::${DEP_${dep}_PACKAGE})
      endif()
      # Promote the IMPORTED target to GLOBAL so the `Pj::Dep::${dep}`
      # ALIAS created below is visible across subdirectories. An ALIAS
      # that backs a non-GLOBAL IMPORTED target is scope-local, which
      # breaks `if(TARGET Pj::Dep::${dep})` checks in sibling modules
      # (e.g. pjmedia checking for `Pj::Dep::Speex`).
      get_target_property(_is_imported ${dep_target} IMPORTED)
      if(_is_imported)
        set_target_properties(${dep_target} PROPERTIES IMPORTED_GLOBAL TRUE)
      endif()
      unset(_is_imported)
    endif()
  endif()

  # set provider property on target
  if(dep_target)
    set_target_properties(${dep_target} PROPERTIES
      PJ_DEP_PROVIDER ${dep_provider}
    )
    add_library(Pj::Dep::${dep} ALIAS ${dep_target})
  endif()

  # add alias
endforeach()

# install selected sub-diriectories
if(install_subdirs)
  # append `$ORIGIN/pjproject/third_party` to RPATH, and the same folder seen
  # from the bin directory for the executables
  if(BUILD_SHARED_LIBS)
    if(APPLE)
      list(APPEND CMAKE_INSTALL_RPATH
        "@loader_path/pjproject/third_party"
        "@executable_path/../${CMAKE_INSTALL_LIBDIR}/pjproject/third_party"
      )
    elseif(UNIX)
      list(APPEND CMAKE_INSTALL_RPATH
        "$ORIGIN/pjproject/third_party"
        "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/pjproject/third_party"
      )
    endif()
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} PARENT_SCOPE)
  endif()

endif()

# Called from the top-level CMakeLists.txt for the libraries pjmedia links:
# one it does not link is never built (EXCLUDE_FROM_ALL), so exporting it
# would break find_package(Pj) for consumers.
function(pj_install_bundled_dep target)
  get_target_property(name ${target} EXPORT_NAME)
  install(TARGETS ${target}
    EXPORT PjTargets
    LIBRARY
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pjproject/third_party"
      COMPONENT PjRuntime
      OPTIONAL
    ARCHIVE
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pjproject/third_party"
      COMPONENT PjDevelopment
      OPTIONAL
    FILE_SET HEADERS
      DESTINATION  "${CMAKE_INSTALL_INCLUDEDIR}/pjproject/third_party/${name}"
      COMPONENT PjDevelopment
      OPTIONAL
  )
endfunction()
