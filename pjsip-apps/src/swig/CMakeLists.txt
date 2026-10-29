# ##############################################################################
# pjsua2 Java bindings
#
# SWIG turns pjsua2.i into pjsua2_wrap.cpp plus the org.pjsip.pjsua2 Java
# sources. The shared library is what System.loadLibrary("pjsua2") opens --
# see the jniclasscode pragma in pjsua2.i -- so its name is fixed.
#
# On Android, Gradle compiles the Java beside it and packages both into the
# AAR. Elsewhere this builds what java/Makefile does, in this directory of the
# build tree:
#
#   libpjsua2.so, libpjsua2.jnilib or pjsua2.dll   the JNI library
#   pjsua2.jar                                      the bindings
#   pjsua2_java_samples.jar                         test.java and sample.java
#
#   java -cp pjsua2.jar:pjsua2_java_samples.jar -Djava.library.path=. test
#
# python/CMakeLists.txt builds the Python module, csharp/CMakeLists.txt the C#
# bindings.

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

set_property(SOURCE pjsua2.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE pjsua2.i PROPERTY COMPILE_OPTIONS
  -package org.pjsip.pjsua2
  -doxygen
)
set_property(SOURCE pjsua2.i PROPERTY USE_SWIG_DEPENDENCIES TRUE)
# __ANDROID__ selects the UTF-8 std::string typemaps in and_string.i. SWIG
# does not see the compiler's built-in macros, so it has to be told.
if(ANDROID)
  set_property(SOURCE pjsua2.i PROPERTY COMPILE_DEFINITIONS __ANDROID__)
endif()

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
  # libpjsua2.so. Keep this one in its own directory either way, without a
  # per-configuration subdirectory.
  LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
)

# Each library whose headers the wrapper includes, as the C# bindings
target_link_libraries(pjsua2jni PRIVATE
  pjsua2 pjsua-lib pjsip pjmedia pjnath pjlib-util pjlib
)

# The NDK has jni.h in its sysroot; a desktop build needs the JDK's
if(NOT ANDROID)
  find_package(JNI REQUIRED COMPONENTS JVM)
  target_include_directories(pjsua2jni PRIVATE ${JNI_INCLUDE_DIRS})
endif()

# Export the JNI entry points and nothing else; see pjsua2jni.map. Only the
# GNU-style linkers use a version script, which on Android is all of them.
if(ANDROID)
  set(_pjsua2jni_map "${CMAKE_CURRENT_SOURCE_DIR}/pjsua2jni.map")
  target_link_options(pjsua2jni PRIVATE
    "LINKER:--version-script=${_pjsua2jni_map}"
  )
  # A version script is not a source file, so relink when it changes.
  set_property(TARGET pjsua2jni APPEND PROPERTY LINK_DEPENDS "${_pjsua2jni_map}")
  unset(_pjsua2jni_map)
endif()

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

if(ANDROID)
  return()
endif()

# ##############################################################################
# pjsua2.jar
#
# Which classes SWIG writes is only known once it has run, so the list of
# sources javac compiles is a file made after each run.

find_package(Java 1.8 REQUIRED COMPONENTS Development Runtime)
include(UseJava)

set(_pj_java_pkg_dir "${PJSUA2_JAVA_OUTPUT_DIR}/org/pjsip/pjsua2")
set(_pj_java_list "${CMAKE_CURRENT_BINARY_DIR}/pjsua2_java_sources.txt")
set(_pj_java_list_script "${CMAKE_CURRENT_BINARY_DIR}/pjsua2_java_sources.cmake")
file(WRITE "${_pj_java_list_script}" [[
file(GLOB _sources "${DIR}/*.java")
list(JOIN _sources "\"\n\"" _sources)
file(WRITE "${OUT}" "\"${_sources}\"\n")
]])

# pjsua2JNI.java is one of the files the SWIG step names as its output
add_custom_command(
  OUTPUT "${_pj_java_list}"
  COMMAND "${CMAKE_COMMAND}" -DDIR=${_pj_java_pkg_dir} -DOUT=${_pj_java_list}
          -P "${_pj_java_list_script}"
  DEPENDS "${_pj_java_pkg_dir}/pjsua2JNI.java" "${_pj_java_list_script}"
  VERBATIM
)

add_jar(pjsua2_jar
  SOURCES "@${_pj_java_list}"
  OUTPUT_NAME pjsua2
)
add_dependencies(pjsua2_jar pjsua2jni)

# make compiles MyApp.java, the Android sample's model, with the bindings;
# sample.java is built on it
add_jar(pjsua2_java_samples
  SOURCES
    java/test.java
    java/sample.java
    java/android/app/src/main/java/org/pjsip/pjsua2/app/MyApp.java
  INCLUDE_JARS pjsua2_jar
)

unset(_pj_java_pkg_dir)
unset(_pj_java_list)
unset(_pj_java_list_script)

# ##############################################################################
# Installation
#
# The jar and the JNI library together, as the C# bindings; make installs
# nothing. Run with -Djava.library.path pointing there.

set(PJ_INSTALL_JAVADIR "${CMAKE_INSTALL_LIBDIR}/pjproject/java" CACHE PATH
  "Directory the pjsua2 Java bindings are installed to"
)

# With shared libraries, the JNI library finds them in the libdir
if(BUILD_SHARED_LIBS AND NOT IS_ABSOLUTE "${PJ_INSTALL_JAVADIR}"
   AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  file(RELATIVE_PATH _pj_up "/x/${PJ_INSTALL_JAVADIR}"
    "/x/${CMAKE_INSTALL_LIBDIR}"
  )
  # CMake 4 ends it with a slash
  string(REGEX REPLACE "/$" "" _pj_up "${_pj_up}")
  if(APPLE)
    set_property(TARGET pjsua2jni APPEND PROPERTY INSTALL_RPATH
      "@loader_path/${_pj_up}"
      "@loader_path/${_pj_up}/pjproject/third_party"
    )
  elseif(UNIX)
    set_property(TARGET pjsua2jni APPEND PROPERTY INSTALL_RPATH
      "$ORIGIN/${_pj_up}"
      "$ORIGIN/${_pj_up}/pjproject/third_party"
    )
  endif()
  unset(_pj_up)
endif()

install(TARGETS pjsua2jni
  LIBRARY DESTINATION "${PJ_INSTALL_JAVADIR}" COMPONENT PjJava
  RUNTIME DESTINATION "${PJ_INSTALL_JAVADIR}" COMPONENT PjJava
)
install_jar(pjsua2_jar DESTINATION "${PJ_INSTALL_JAVADIR}" COMPONENT PjJava)

# ##############################################################################
# Test: test.java, run against the build tree, as make test

if(BUILD_TESTING)
  if(WIN32)
    set(_pj_sep "$<SEMICOLON>")
  else()
    set(_pj_sep ":")
  endif()
  add_test(NAME pjsua2-java
    COMMAND "${Java_JAVA_EXECUTABLE}"
      -cp "$<TARGET_PROPERTY:pjsua2_jar,JAR_FILE>${_pj_sep}$<TARGET_PROPERTY:pjsua2_java_samples,JAR_FILE>"
      "-Djava.library.path=$<TARGET_FILE_DIR:pjsua2jni>"
      test
  )
  set_tests_properties(pjsua2-java PROPERTIES
    FAIL_REGULAR_EXPRESSION "Failed to load native library pjsua2"
  )
  unset(_pj_sep)
endif()
