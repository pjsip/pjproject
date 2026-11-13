include_guard(GLOBAL)

# What the pjsua2 SWIG bindings (Java, Python, C#) have in common.

# pj_swig_include_pj_headers(<target>)
#
# SWIG parses the pjsua2 headers itself, so <target> needs every library's
# include directories in its own INCLUDE_DIRECTORIES:
#  - Linking does not populate INCLUDE_DIRECTORIES: usage requirements reach
#    the compiler without ever being written to that property. So name each
#    library's interface directories here. This also picks up the generated
#    headers (os_auto.h, config_auto.h, sip_autoconf.h), which live in the
#    build tree rather than beside the sources.
#  - UseSWIG ignores the target's INCLUDE_DIRECTORIES unless it is told to
#    forward them, which is what SWIG_USE_TARGET_INCLUDE_DIRECTORIES does.
#    Without it SWIG runs with no -I at all and cannot find pjsua2/types.hpp.
function(pj_swig_include_pj_headers target)
  set_property(TARGET ${target} PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE)
  foreach(lib IN ITEMS pjlib pjlib-util pjnath pjmedia pjsip pjsua-lib pjsua2)
    target_include_directories(${target} PRIVATE
      $<TARGET_PROPERTY:${lib},INTERFACE_INCLUDE_DIRECTORIES>
    )
  endforeach()
endfunction()

# pj_swig_install_rpath(<target> <install dir>)
#
# With shared libraries, the installed <target> in <install dir> finds them in
# the libdir and the bundled ones in pjproject/third_party there. Both paths
# relative to the prefix; anything else is left to the user.
function(pj_swig_install_rpath target install_dir)
  if(NOT BUILD_SHARED_LIBS OR IS_ABSOLUTE "${install_dir}"
     OR IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    return()
  endif()
  file(RELATIVE_PATH up "/x/${install_dir}" "/x/${CMAKE_INSTALL_LIBDIR}")
  # CMake 4 ends it with a slash
  string(REGEX REPLACE "/$" "" up "${up}")
  if(APPLE)
    set(origin "@loader_path")
  elseif(UNIX)
    set(origin "$ORIGIN")
  else()
    return()
  endif()
  set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH
    "${origin}/${up}"
    "${origin}/${up}/pjproject/third_party"
  )
endfunction()

# pj_android_java_helpers(<dir> <out-var>)
#
# The Android capture and audio device backends call up into Java. Copy the
# classes the enabled backends need into <dir>/org/pjsip, package org.pjsip,
# and set <out-var> to the copies. configure_file(COPYONLY) rather than a
# build rule so that editing one of them re-runs CMake and refreshes the copy.
function(pj_android_java_helpers dir out)
  set(sources "")
  if(PJMEDIA_WITH_VIDEODEV_ANDROID)
    list(APPEND sources
      pjmedia/src/pjmedia-videodev/android/PjCamera.java
      pjmedia/src/pjmedia-videodev/android/PjCamera2.java
      pjmedia/src/pjmedia-videodev/android/PjCameraInfo.java
      pjmedia/src/pjmedia-videodev/android/PjCameraInfo2.java
    )
  endif()
  if(PJMEDIA_WITH_AUDIODEV_OBOE)
    list(APPEND sources
      pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java
    )
  endif()
  set(copies "")
  foreach(source IN LISTS sources)
    cmake_path(GET source FILENAME name)
    configure_file("${PROJECT_SOURCE_DIR}/${source}"
      "${dir}/org/pjsip/${name}" COPYONLY
    )
    list(APPEND copies "${dir}/org/pjsip/${name}")
  endforeach()
  set(${out} "${copies}" PARENT_SCOPE)
endfunction()
