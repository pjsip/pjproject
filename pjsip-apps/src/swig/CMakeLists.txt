# ##############################################################################
# pjsua2 JNI bindings
#
# SWIG turns pjsua2.i into pjsua2_wrap.cpp plus the org.pjsip.pjsua2 Java
# sources. The shared library is what System.loadLibrary("pjsua2") opens --
# see the jniclasscode pragma in pjsua2.i -- so its name is fixed. Gradle
# compiles the Java beside it and packages both into the AAR.
#
# This covers Android only. The desktop Java, Python and C# bindings are still
# built by pjsip-apps/src/swig/*/Makefile; they need a JDK probe, javac steps
# and sample runners that have no CMake consumer asking for them.

find_package(SWIG 4.0 REQUIRED COMPONENTS java)
include(UseSWIG)

# Where the generated .java is written.
#
# It is identical for every ABI, but CMAKE_CURRENT_BINARY_DIR is not: Gradle
# configures one build directory per ABI, and putting four copies of the same
# classes on the source path is a duplicate class error. So Gradle overrides
# this with a single shared directory, while a standalone CMake build keeps
# everything under its own build tree.
set(PJSUA2_JAVA_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/java"
  CACHE PATH "Directory the generated pjsua2 Java sources are written to"
)

# __ANDROID__ selects the UTF-8 std::string typemaps in and_string.i. SWIG
# does not see the compiler's built-in macros, so it has to be told.
set_property(SOURCE pjsua2.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE pjsua2.i PROPERTY COMPILE_DEFINITIONS __ANDROID__)
set_property(SOURCE pjsua2.i PROPERTY COMPILE_OPTIONS
  -package org.pjsip.pjsua2
  -doxygen
)

swig_add_library(pjsua2jni
  TYPE SHARED
  LANGUAGE java
  OUTPUT_DIR "${PJSUA2_JAVA_OUTPUT_DIR}/org/pjsip/pjsua2"
  OUTFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
  SOURCES pjsua2.i
)

set_target_properties(pjsua2jni PROPERTIES
  # Fixed by the loadLibrary call in pjsua2.i.
  OUTPUT_NAME pjsua2

  # The C++ library is called pjsua2 as well. It is normally static, so the
  # two do not collide, but with BUILD_SHARED_LIBS they would both want
  # libpjsua2.so. Keep this one in its own directory either way.
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# pjsua2 carries the rest of the stack transitively.
target_link_libraries(pjsua2jni PRIVATE pjsua2)

# SWIG parses the headers itself, so it needs its own -I list. Two things are
# needed for that:
#
#  - Linking does not populate INCLUDE_DIRECTORIES: usage requirements reach
#    the compiler without ever being written to that property. So name each
#    library's interface directories here. This also picks up the generated
#    headers (os_auto.h, config_auto.h, sip_autoconf.h), which live in the
#    build tree rather than beside the sources.
#  - UseSWIG ignores the target's INCLUDE_DIRECTORIES unless it is told to
#    forward them, which is what SWIG_USE_TARGET_INCLUDE_DIRECTORIES does.
#    Without it SWIG runs with no -I at all and cannot find pjsua2/types.hpp.
set_property(TARGET pjsua2jni PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE)
target_include_directories(pjsua2jni PRIVATE
  $<TARGET_PROPERTY:pjlib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjlib-util,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjnath,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjmedia,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsip,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua-lib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua2,INTERFACE_INCLUDE_DIRECTORIES>
)

# ##############################################################################
# Java helper classes
#
# The Android capture and audio device backends call up into Java. These
# classes belong to the binding rather than to any sample app, so they ship
# beside the generated sources -- the android_helpers target in the SWIG
# Makefile does the same. configure_file(COPYONLY) rather than a build rule so
# that editing one of them re-runs CMake and refreshes the copy.

set(_pjsua2_helpers)
if(PJMEDIA_WITH_VIDEODEV_ANDROID)
  list(APPEND _pjsua2_helpers
    pjmedia/src/pjmedia-videodev/android/PjCamera.java
    pjmedia/src/pjmedia-videodev/android/PjCamera2.java
    pjmedia/src/pjmedia-videodev/android/PjCameraInfo.java
    pjmedia/src/pjmedia-videodev/android/PjCameraInfo2.java
  )
endif()
if(PJMEDIA_WITH_AUDIODEV_OBOE)
  list(APPEND _pjsua2_helpers
    pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java
  )
endif()

foreach(_helper IN LISTS _pjsua2_helpers)
  cmake_path(GET _helper FILENAME _helper_name)
  configure_file(
    "${PROJECT_SOURCE_DIR}/${_helper}"
    # package org.pjsip, one level above the generated bindings
    "${PJSUA2_JAVA_OUTPUT_DIR}/org/pjsip/${_helper_name}"
    COPYONLY
  )
endforeach()
unset(_helper)
unset(_helper_name)
unset(_pjsua2_helpers)
