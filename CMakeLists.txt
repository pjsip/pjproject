cmake_minimum_required(VERSION 3.28...4.0)

# Read version from version.mak
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.mak" _version_lines)
foreach(_line IN LISTS _version_lines)
  if(_line MATCHES "^export PJ_VERSION_MAJOR[ \t]*:=[ \t]*([0-9]+)")
    set(_pj_ver_major "${CMAKE_MATCH_1}")
  elseif(_line MATCHES "^export PJ_VERSION_MINOR[ \t]*:=[ \t]*([0-9]+)")
    set(_pj_ver_minor "${CMAKE_MATCH_1}")
  elseif(_line MATCHES "^export PJ_VERSION_REV[ \t]*:=[ \t]*([0-9]+)")
    set(_pj_ver_rev "${CMAKE_MATCH_1}")
  endif()
endforeach()
# CMake VERSION field only accepts numeric components
if(_pj_ver_rev)
  set(_pj_cmake_version "${_pj_ver_major}.${_pj_ver_minor}.${_pj_ver_rev}")
else()
  set(_pj_cmake_version "${_pj_ver_major}.${_pj_ver_minor}")
endif()

project(pjproject
  VERSION ${_pj_cmake_version}
  DESCRIPTION "Multimedia communication library"
  HOMEPAGE_URL "http://www.pjsip.org"
  LANGUAGES C CXX
)

# project modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# experimental warning notice
option(PJ_SKIP_EXPERIMENTAL_NOTICE OFF)
if (NOT PJ_SKIP_EXPERIMENTAL_NOTICE)
  message(WARNING
    " ****************************************************\n"
    " * Building `pjproject` with CMake is experimental  *\n"
    " *--------------------------------------------------*\n"
    " * Tested platforms:                                *\n"
    " * - Linux x86_64                                   *\n"
    " * - macOS                                          *\n"
    " * - Android (arm64-v8a, armeabi-v7a, x86_64, x86)  *\n"
    " ****************************************************\n"
    "Set `PJ_SKIP_EXPERIMENTAL_NOTICE=ON` to disable this warning"
  )
endif()

# ##############################################################################
# Testing support

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
endif()

if(BUILD_TESTING)
  if(CMAKE_SYSTEM_NAME MATCHES "rtems")
    set(PJ_TEST_MAIN "main_rtems.c")
  else()
    set(PJ_TEST_MAIN "main.c")
  endif()
endif()

# ##############################################################################
# Installation preparation

include(GNUInstallDirs)

# add `$ORIGIN` to targets' RPATH
if(BUILD_SHARED_LIBS)
  if(APPLE)
    list(APPEND CMAKE_INSTALL_RPATH
      "@loader_path"
      "@executable_path/../${CMAKE_INSTALL_LIBDIR}"
    )
  elseif(UNIX)
    list(APPEND CMAKE_INSTALL_RPATH
      "$ORIGIN"
      "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
  endif()
endif()

# ##############################################################################
# Sub-directories

# base library
add_subdirectory(pjlib)

# third-party libraries
add_subdirectory(third_party)

# remaning libraries
add_subdirectory(pjlib-util)
add_subdirectory(pjnath)
add_subdirectory(pjmedia)
add_subdirectory(pjsip)
# The sample applications are command line executables. iOS has no place to
# run one, and CMake would make each a bundle needing a destination and a
# signing identity, so the iOS build stops at the libraries -- which is what
# an iOS consumer links into an app of their own anyway.
if(NOT IOS)
  add_subdirectory(pjsip-apps)
endif()

# The iOS sample applications are Xcode projects rather than CMake targets,
# and they link the static libraries from beside their own sources under the
# names the GNU makefiles produce. Mirror what pjsip-apps/build/Makefile does
# for $(PJSUA_EXE) on iOS, so the sample projects can be opened and built
# after a CMake build too. CMake's own library names differ from the GNU ones
# for a few targets, hence the mapping rather than a plain copy.
if(IOS)
  # On by default so the Xcode sample projects work straight after a build,
  # which is the whole point of them. It writes into the source tree, so it
  # turns itself off when that tree is not writable rather than failing an
  # otherwise valid out-of-source build.
  option(PJ_IOS_SAMPLE_LIBS
    "Link the built libraries into the iOS sample app projects" ON
  )

  if(PJ_IOS_SAMPLE_LIBS)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E touch
              "${PROJECT_SOURCE_DIR}/pjsip-apps/src/.pj_write_probe"
      RESULT_VARIABLE _pj_src_writable
      ERROR_QUIET
    )
    if(_pj_src_writable EQUAL 0)
      file(REMOVE "${PROJECT_SOURCE_DIR}/pjsip-apps/src/.pj_write_probe")
    else()
      set(PJ_IOS_SAMPLE_LIBS OFF)
      message(STATUS
        "[!] the source tree is not writable, so the iOS sample projects will "
        "not be given the built libraries. The libraries themselves are "
        "unaffected."
      )
    endif()
    unset(_pj_src_writable)
  endif()
endif()

if(IOS AND PJ_IOS_SAMPLE_LIBS)
  set(_pj_ios_sample_dirs
    "${PROJECT_SOURCE_DIR}/pjsip-apps/src/pjsua/ios"
    "${PROJECT_SOURCE_DIR}/pjsip-apps/src/pjsua/ios-swift/Libraries"
    "${PROJECT_SOURCE_DIR}/pjsip-apps/src/pjsua2/ios-swift-pjsua2/Libraries"
  )

  # <cmake target>|<file name the Xcode projects link>
  set(_pj_ios_sample_libs
    pjlib|libpj.a
    pjlib-util|libpjlib-util.a
    pjnath|libpjnath.a
    pjmedia|libpjmedia.a
    pjmedia-codec|libpjmedia-codec.a
    pjmedia-audiodev|libpjmedia-audiodev.a
    pjmedia-videodev|libpjmedia-videodev.a
    pjsip|libpjsip.a
    pjsip-simple|libpjsip-simple.a
    pjsip-ua|libpjsip-ua.a
    pjsua-lib|libpjsua.a
    pjsua2|libpjsua2.a
    Pj::Dep::G7221|libg7221codec.a
    Pj::Dep::GSM|libgsmcodec.a
    Pj::Dep::iLBC|libilbccodec.a
    Pj::Dep::Resample|libresample.a
    Pj::Dep::Speex|libspeex.a
    Pj::Dep::SRTP|libsrtp.a
    Pj::Dep::WebRTC|libwebrtc.a
    Pj::Dep::YUV|libyuv.a
  )

  # The Xcode projects include the pjproject headers straight from the source
  # tree, where the autotools build also writes the five headers it generates.
  # CMake generates them under the build directory instead, so pj/config.h
  # fails on a missing pj/compat/os_auto.h. Copy rather than symlink: a later
  # ./aconfigure writes to these exact paths and would otherwise write through
  # the link into the build directory.
  #
  # Note that CMake puts the source include directory ahead of the binary one,
  # so a generated header sitting in the source tree -- left by this step, or
  # by an autotools build -- shadows the one the current build generated. Turn
  # this off for a build that must not be influenced by whatever configured
  # the tree last.
  cmake_dependent_option(PJ_IOS_SAMPLE_HEADERS
    "Copy the generated headers into the source tree for the iOS samples" ON
    "PJ_IOS_SAMPLE_LIBS" OFF
  )

  set(_pj_ios_generated_headers
    pjlib/include/pj/compat/os_auto.h
    pjlib/include/pj/compat/m_auto.h
    pjmedia/include/pjmedia/config_auto.h
    pjmedia/include/pjmedia-codec/config_auto.h
    pjsip/include/pjsip/sip_autoconf.h
  )

  # The Xcode projects link a fixed list of archives, including ones this
  # configuration may not build -- WebRTC has no Apple support here, and any
  # codec can be switched off. The matching feature is compiled out of the
  # headers too, so nothing references those symbols and an empty archive
  # satisfies the link. Without it the projects fail with "library not found".
  set(_pj_ios_placeholder_src "${PROJECT_BINARY_DIR}/pj_ios_placeholder.c")
  file(WRITE "${_pj_ios_placeholder_src}"
    "/* Stands in for a library this configuration does not build.\n"
    " * Its feature is compiled out, so nothing refers to its symbols. */\n"
    "int pj_ios_placeholder(void);\n"
    "int pj_ios_placeholder(void) { return 0; }\n"
  )
  add_library(pj-ios-placeholder STATIC "${_pj_ios_placeholder_src}")

  add_custom_target(pj-ios-sample-libs ALL
    COMMENT "Linking libraries into the iOS sample app projects"
  )
  add_dependencies(pj-ios-sample-libs pj-ios-placeholder)

  if(PJ_IOS_SAMPLE_HEADERS)
    foreach(_pj_hdr IN LISTS _pj_ios_generated_headers)
      add_custom_command(TARGET pj-ios-sample-libs POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${PROJECT_BINARY_DIR}/${_pj_hdr}" "${PROJECT_SOURCE_DIR}/${_pj_hdr}"
      )
    endforeach()
  endif()
  foreach(_dir IN LISTS _pj_ios_sample_dirs)
    add_custom_command(TARGET pj-ios-sample-libs PRE_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dir}"
    )
  endforeach()

  foreach(_entry IN LISTS _pj_ios_sample_libs)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _pj_tgt)
    list(GET _parts 1 _pj_name)

    # The third-party entries are the provider-agnostic Pj::Dep:: aliases, so
    # a system provider is staged as readily as a bundled one. Only a library
    # this configuration genuinely does not have falls back to the
    # placeholder, and it says so rather than leaving the sample to fail with
    # unresolved symbols later.
    if(TARGET ${_pj_tgt})
      if(NOT _pj_tgt MATCHES "^Pj::")
        add_dependencies(pj-ios-sample-libs ${_pj_tgt})
      endif()
      set(_pj_src "$<TARGET_FILE:${_pj_tgt}>")
    else()
      set(_pj_src "$<TARGET_FILE:pj-ios-placeholder>")
      message(STATUS
        "[!] ${_pj_name} is not built by this configuration; the iOS sample "
        "projects get an empty archive in its place"
      )
    endif()

    foreach(_dir IN LISTS _pj_ios_sample_dirs)
      add_custom_command(TARGET pj-ios-sample-libs POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E create_symlink
                "${_pj_src}" "${_dir}/${_pj_name}"
      )
    endforeach()
  endforeach()

  unset(_pj_ios_sample_dirs)
  unset(_pj_ios_sample_libs)
  unset(_pj_ios_generated_headers)
  unset(_pj_hdr)
  unset(_pj_src)
  unset(_pj_ios_placeholder_src)
  unset(_pj_tgt)
  unset(_pj_name)
  unset(_parts)
endif()

# ##############################################################################
# Installation

set(install_targets
  pjlib
  pjlib-ssl
  pjlib++
  pjlib-util
  pjnath
  pjmedia
  pjmedia-codec
  pjmedia-audiodev
  pjmedia-videodev
  pjsip
  pjsip-simple
  pjsip-ua
  pjsua-lib
  pjsua2
)

if(NOT IOS)
  list(APPEND install_targets pjsua)
endif()

# set `VERSION` and `SO_VERSION`
foreach(target IN LISTS install_targets)
  set_target_properties(${target} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )
endforeach()

# install targets
install(
  TARGETS ${install_targets}
  EXPORT PjTargets

  FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT PjDevelopment
  FILE_SET config_headers
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT PjDevelopment
  RUNTIME
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT PjRuntime
  LIBRARY
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT PjRuntime
    NAMELINK_COMPONENT PjDevelopment
  ARCHIVE
    DESTINATION  "${CMAKE_INSTALL_BINDIR}"
    COMPONENT PjDevelopment
)

# ##############################################################################
# CMAKe Package config

include(CMakePackageConfigHelpers)

set(PJ_CMAKE_INSTALL_DIR         "${CMAKE_INSTALL_LIBDIR}/cmake/Pj")
set(PJ_CMAKE_TARGETS_FILE        "PjTargets.cmake")
set(PJ_CMAKE_CONFIG_FILE         "PjConfig.cmake")
set(PJ_CMAKE_CONFIG_VERSION_FILE "PjConfigVersion.cmake")

# install exports
install(
  EXPORT      PjTargets
  NAMESPACE   Pj::
  FILE        "${PJ_CMAKE_TARGETS_FILE}"
  DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
  COMPONENT   PjDevelopment
)

# APPLE is empty rather than 0 off Apple platforms, and PjConfig.cmake.in
# substitutes it into an if() condition, where an empty operand beside AND is
# a hard error.
if(APPLE)
  set(PJ_APPLE 1)
else()
  set(PJ_APPLE 0)
endif()

# write package config
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Pj/${PJ_CMAKE_CONFIG_FILE}.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_FILE}"

  INSTALL_DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
)

# write package config version
write_basic_package_version_file(
  "${PJ_CMAKE_CONFIG_VERSION_FILE}"

  COMPATIBILITY SameMajorVersion
)

# collect third-party dependency provider info for PjDeps.cmake
set(PJ_BUNDLED_DEPS "")
set(PJ_SYSTEM_DEPS "")

set(_pj_all_deps G7221 GSM iLBC Resample Speex SRTP YUV)
if(NOT (APPLE OR MINGW OR CYGWIN))
  list(APPEND _pj_all_deps WebRTC WebRTC_AEC3)
endif()

foreach(_dep IN LISTS _pj_all_deps)
  string(TOUPPER "PJ_DEP_${_dep}" _dep_option)
  if(DEFINED ${_dep_option})
    if(${_dep_option} STREQUAL "bundled")
      list(APPEND PJ_BUNDLED_DEPS "${_dep}")
    elseif(${_dep_option} STREQUAL "system")
      list(APPEND PJ_SYSTEM_DEPS "${_dep}")
    endif()
  endif()
endforeach()
unset(_pj_all_deps)

list(JOIN PJ_BUNDLED_DEPS ";" PJ_BUNDLED_DEPS_STR)
list(JOIN PJ_SYSTEM_DEPS ";" PJ_SYSTEM_DEPS_STR)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Pj/PjDeps.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/PjDeps.cmake"
  @ONLY
)

# Install custom Find modules needed by enabled dependencies.
# CMake built-in modules (Threads, OpenSSL, GnuTLS, ALSA, OpenGL, SDL2)
# do not need to be installed.
set(_pj_find_modules)

# Helper: append a Find module if the feature is enabled and the file exists
macro(_pj_collect_find_module _condition _package)
  if(${_condition})
    set(_fm "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Find${_package}.cmake")
    if(EXISTS "${_fm}")
      list(APPEND _pj_find_modules "${_fm}")
    endif()
    unset(_fm)
  endif()
endmacro()

# System third-party deps (bundled deps use Pj::Dep:: aliases instead)
set(_pj_pkg_G7221    "G7221")
set(_pj_pkg_GSM      "GSM")
set(_pj_pkg_Resample "Resample")
set(_pj_pkg_Speex    "Speex")
set(_pj_pkg_SRTP     "SRTP")
set(_pj_pkg_YUV      "YUV")
foreach(_dep IN LISTS PJ_SYSTEM_DEPS)
  if(DEFINED _pj_pkg_${_dep})
    _pj_collect_find_module(TRUE ${_pj_pkg_${_dep}})
  endif()
endforeach()

# Platform libs and optional features with custom Find modules
_pj_collect_find_module(PJLIB_WITH_LIBUUID               UUID)
_pj_collect_find_module(PJNATH_WITH_UPNP                 UPNP)
_pj_collect_find_module(PJMEDIA_WITH_AUDIODEV_COREAUDIO   CoreAudio)
_pj_collect_find_module(PJMEDIA_WITH_AUDIODEV_OBOE        Oboe)
_pj_collect_find_module(PJMEDIA_WITH_VIDEODEV_V4L2        V4L2)
# PJ_OPENGL_PACKAGE is the name PjConfig.cmake.in calls find_dependency with,
# so keying off it keeps the two from drifting apart. Only the OpenGLES module
# is ours; FindOpenGL is a built-in and needs no installing.
if(PJ_OPENGL_PACKAGE STREQUAL "OpenGLES")
  _pj_collect_find_module(PJMEDIA_WITH_VIDEODEV_OPENGL    OpenGLES)
endif()
_pj_collect_find_module(PJMEDIA_WITH_VIDEODEV_DARWIN      AVFoundation)
_pj_collect_find_module(PJMEDIA_WITH_VIDEODEV_METAL       Metal)
_pj_collect_find_module(PJMEDIA_WITH_VID_TOOLBOX_CODEC    VideoToolbox)
_pj_collect_find_module(APPLE                             AppleFoundation)
_pj_collect_find_module(PJMEDIA_WITH_ILBC_CODEC           AudioToolbox)
if(PJMEDIA_WITH_RESAMPLE STREQUAL "libsamplerate")
  _pj_collect_find_module(TRUE SampleRate)
endif()
_pj_collect_find_module(PJMEDIA_WITH_SPEEX_AEC            SpeexDSP)
_pj_collect_find_module(PJMEDIA_WITH_FFMPEG               FFMPEG)
_pj_collect_find_module(PJMEDIA_WITH_OPENCORE_AMRNB_CODEC OpenCoreAMRNB)
_pj_collect_find_module(PJMEDIA_WITH_OPENCORE_AMRWB_CODEC OpenCoreAMRWB)
_pj_collect_find_module(PJMEDIA_WITH_OPENCORE_AMRWB_CODEC VisualOnAMRWBEnc)
_pj_collect_find_module(PJMEDIA_WITH_SILK_CODEC           Silk)
_pj_collect_find_module(PJMEDIA_WITH_OPUS_CODEC           OPUS)
_pj_collect_find_module(PJMEDIA_WITH_BCG729_CODEC         BCG729)
_pj_collect_find_module(PJMEDIA_WITH_LYRA_CODEC           Lyra)
_pj_collect_find_module(PJMEDIA_WITH_VPX_CODEC            VPX)
_pj_collect_find_module(PJMEDIA_WITH_OPEN_H264_CODEC      OpenH264)
_pj_collect_find_module(PJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC MediaNDK)

if(_pj_find_modules)
  list(REMOVE_DUPLICATES _pj_find_modules)
  install(FILES ${_pj_find_modules}
    DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
    COMPONENT PjDevelopment
  )
endif()

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_FILE}"
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_VERSION_FILE}"
  "${CMAKE_CURRENT_BINARY_DIR}/PjDeps.cmake"

  DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
  COMPONENT PjDevelopment
)

# ##############################################################################
# pkg-config file

set(PREFIX "${CMAKE_INSTALL_PREFIX}") # this should be done at install time
set(LIBDIR "${PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(INCLUDEDIR "${PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PJ_VERSION "${PROJECT_VERSION}")
set(PJ_INSTALL_LDFLAGS "")
set(PJ_INSTALL_LDFLAGS "")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libpjproject.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libpjproject.pc"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/libpjproject.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
