# ##############################################################################
# pjsua2 C# bindings
#
# SWIG turns pjsua2.i into pjsua2_wrap.cpp plus the C# sources. Any .NET
# project can use the result: compile the .cs files and ship the native
# library beside the app. Both land in this directory of the build tree:
#
#   cs/*.cs          the bindings, in namespace PJSUA2_CSHARP_NAMESPACE
#   lib/             the native library the bindings call:
#                      libpjsua2.so, libpjsua2.dylib or pjsua2.dll, which
#                      DllImport("pjsua2") loads; on Android also
#                      libc++_shared.so with ANDROID_STL=c++_shared
#                      libpjsua2.a on iOS, where the app links it statically
#                      (DllImport("__Internal")): the wrapper and every
#                      library it needs, in one archive
#   java/org/pjsip/  on Android, the Java side of the camera and Oboe
#                    backends, which the app compiles
#
# On Android and iOS they are also copied into the .NET MAUI project at
# PJSUA2_MAUI_DIR, where make's csharp/Makefile writes them.

find_package(SWIG 4.0 REQUIRED COMPONENTS csharp)
include(UseSWIG)

# The default is what the MAUI project uses
set(PJSUA2_CSHARP_NAMESPACE "libpjsua2.maui" CACHE STRING
  "Namespace of the generated pjsua2 C# bindings"
)

set(_pj_cs_dir "${CMAKE_CURRENT_BINARY_DIR}/cs")
set(_pj_lib_dir "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(_pj_java_dir "${CMAKE_CURRENT_BINARY_DIR}/java")

set(_pj_swig_i "${CMAKE_CURRENT_SOURCE_DIR}/../pjsua2.i")
set_property(SOURCE "${_pj_swig_i}" PROPERTY CPLUSPLUS ON)
set_property(SOURCE "${_pj_swig_i}" PROPERTY COMPILE_OPTIONS
  -namespace "${PJSUA2_CSHARP_NAMESPACE}"
)
if(IOS)
  set_property(SOURCE "${_pj_swig_i}" APPEND PROPERTY COMPILE_OPTIONS
    -dllimport __Internal
  )
endif()
set_property(SOURCE "${_pj_swig_i}" PROPERTY USE_SWIG_DEPENDENCIES TRUE)

if(IOS)
  set(_pj_type STATIC)
else()
  set(_pj_type SHARED)
endif()
swig_add_library(pjsua2_csharp
  TYPE ${_pj_type}
  LANGUAGE csharp
  OUTPUT_DIR "${_pj_cs_dir}"
  OUTFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
  SOURCES "${_pj_swig_i}"
)
unset(_pj_type)
unset(_pj_swig_i)

# Each library whose headers the wrapper includes: inline functions in them,
# e.g. pj::AccountNatConfig's constructor, call into pjnath directly
target_link_libraries(pjsua2_csharp PRIVATE
  pjsua2 pjsua-lib pjsip pjmedia pjnath pjlib-util pjlib
)

# SWIG parses the headers itself; see the same in ../CMakeLists.txt
set_property(TARGET pjsua2_csharp
  PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE
)
target_include_directories(pjsua2_csharp PRIVATE
  $<TARGET_PROPERTY:pjlib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjlib-util,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjnath,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjmedia,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsip,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua-lib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua2,INTERFACE_INCLUDE_DIRECTORIES>
)

if(NOT IOS)
  # pjsua2.dll on Windows, MinGW too: .NET adds no "lib" there
  set_target_properties(pjsua2_csharp PROPERTIES
    OUTPUT_NAME pjsua2
    LIBRARY_OUTPUT_DIRECTORY "$<1:${_pj_lib_dir}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${_pj_lib_dir}>"
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${_pj_lib_dir}>"
  )
  if(WIN32)
    set_target_properties(pjsua2_csharp PROPERTIES PREFIX "")
  endif()
  set(_pj_native_target pjsua2_csharp)
  set(_pj_native_files "$<TARGET_FILE:pjsua2_csharp>")
else()
  # The wrapper on its own; libpjsua2.a below adds what it links. UseSWIG
  # names a C# library .dylib on Apple, even a static one.
  set_target_properties(pjsua2_csharp PROPERTIES
    OUTPUT_NAME pjsua2_csharp_wrap
    SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )

  # Every static library built here that pjsua2_csharp links, directly or
  # not; libraries from outside the project are left to the app, as make
  set(_pj_seen "")
  set(_pj_todo pjsua2)
  set(_pj_archives "")
  set(_pj_archive_targets "")
  while(_pj_todo)
    list(POP_FRONT _pj_todo _pj_tgt)
    get_target_property(_pj_aliased ${_pj_tgt} ALIASED_TARGET)
    if(_pj_aliased)
      set(_pj_tgt "${_pj_aliased}")
    endif()
    if(_pj_tgt IN_LIST _pj_seen)
      continue()
    endif()
    list(APPEND _pj_seen "${_pj_tgt}")
    get_target_property(_pj_imported ${_pj_tgt} IMPORTED)
    if(_pj_imported)
      continue()
    endif()
    get_target_property(_pj_type ${_pj_tgt} TYPE)
    if(_pj_type STREQUAL "STATIC_LIBRARY")
      list(APPEND _pj_archives "$<TARGET_FILE:${_pj_tgt}>")
      list(APPEND _pj_archive_targets ${_pj_tgt})
    endif()
    set(_pj_items "")
    if(NOT _pj_type STREQUAL "INTERFACE_LIBRARY")
      get_target_property(_pj_items ${_pj_tgt} LINK_LIBRARIES)
    endif()
    get_target_property(_pj_iface ${_pj_tgt} INTERFACE_LINK_LIBRARIES)
    if(_pj_iface)
      list(APPEND _pj_items ${_pj_iface})
    endif()
    # e.g. $<$<BOOL:ON>:Pj::Dep::SRTP>, evaluated as cmake/Pj/PkgConfig.cmake
    # does for the .pc file
    _pj_pc_eval(_pj_items "${_pj_items}")
    foreach(_pj_item IN LISTS _pj_items)
      if(TARGET "${_pj_item}")
        list(APPEND _pj_todo "${_pj_item}")
      endif()
    endforeach()
  endwhile()

  # Not VERBATIM: the Xcode generator's library paths hold
  # ${EFFECTIVE_PLATFORM_NAME}, which the shell has to expand
  set(_pj_out "${_pj_lib_dir}/libpjsua2.a")
  add_custom_command(OUTPUT "${_pj_out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_pj_lib_dir}"
    COMMAND xcrun libtool -static -no_warning_for_no_symbols -o "${_pj_out}"
            "$<TARGET_FILE:pjsua2_csharp>" ${_pj_archives}
    DEPENDS pjsua2_csharp ${_pj_archive_targets}
    COMMENT "Combining the pjsua2 C# wrapper and its libraries into ${_pj_out}"
  )
  add_custom_target(pjsua2_csharp_lib ALL DEPENDS "${_pj_out}")
  set(_pj_native_target pjsua2_csharp_lib)
  set(_pj_native_files "${_pj_out}")

  unset(_pj_out)
  unset(_pj_seen)
  unset(_pj_todo)
  unset(_pj_archives)
  unset(_pj_archive_targets)
  unset(_pj_tgt)
  unset(_pj_aliased)
  unset(_pj_imported)
  unset(_pj_type)
  unset(_pj_items)
  unset(_pj_iface)
  unset(_pj_item)
endif()

# Android: the C++ runtime the library was linked against, and the Java
# helpers
set(_pj_helpers "")
if(ANDROID)
  if(ANDROID_STL STREQUAL "c++_shared")
    find_file(PJ_ANDROID_CXX_SHARED libc++_shared.so
      PATHS "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
      NO_DEFAULT_PATH
    )
    if(NOT PJ_ANDROID_CXX_SHARED)
      message(FATAL_ERROR "libc++_shared.so not found in the NDK sysroot")
    endif()
    add_custom_command(TARGET pjsua2_csharp POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${PJ_ANDROID_CXX_SHARED}" "${_pj_lib_dir}"
    )
    list(APPEND _pj_native_files "${_pj_lib_dir}/libc++_shared.so")
  endif()

  set(_pj_helper_srcs "")
  if(PJMEDIA_WITH_VIDEODEV_ANDROID)
    list(APPEND _pj_helper_srcs
      pjmedia/src/pjmedia-videodev/android/PjCamera.java
      pjmedia/src/pjmedia-videodev/android/PjCamera2.java
      pjmedia/src/pjmedia-videodev/android/PjCameraInfo.java
      pjmedia/src/pjmedia-videodev/android/PjCameraInfo2.java
    )
  endif()
  if(PJMEDIA_WITH_AUDIODEV_OBOE)
    list(APPEND _pj_helper_srcs
      pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java
    )
  endif()
  foreach(_pj_src IN LISTS _pj_helper_srcs)
    cmake_path(GET _pj_src FILENAME _pj_name)
    configure_file("${PROJECT_SOURCE_DIR}/${_pj_src}"
      "${_pj_java_dir}/org/pjsip/${_pj_name}" COPYONLY
    )
    list(APPEND _pj_helpers "${_pj_java_dir}/org/pjsip/${_pj_name}")
  endforeach()
  unset(_pj_helper_srcs)
  unset(_pj_src)
  unset(_pj_name)
endif()

# ##############################################################################
# The .NET MAUI project

if(ANDROID OR IOS)
  set(PJSUA2_MAUI_DIR
    "${PROJECT_SOURCE_DIR}/pjsip-apps/src/swig/csharp/pjsua2maui/pjsua2maui"
    CACHE PATH
    "The .NET MAUI project to copy the pjsua2 C# bindings into; empty for none"
  )
endif()

if(PJSUA2_MAUI_DIR AND (ANDROID OR IOS))
  # The directories pjsua2maui.csproj looks in
  if(ANDROID)
    set(_pj_platform_dir "${PJSUA2_MAUI_DIR}/Platforms/Android")
    set(_pj_arch "${ANDROID_ABI}")
  else()
    list(LENGTH CMAKE_OSX_ARCHITECTURES _pj_n_archs)
    if(_pj_n_archs GREATER 1)
      message(FATAL_ERROR
        "PJSUA2_MAUI_DIR needs a single CMAKE_OSX_ARCHITECTURES, as the MAUI "
        "project has a directory per architecture; set it empty to build the "
        "bindings without copying them there"
      )
    endif()
    unset(_pj_n_archs)
    if(CMAKE_OSX_ARCHITECTURES)
      set(_pj_arch "${CMAKE_OSX_ARCHITECTURES}")
    else()
      set(_pj_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator" AND _pj_arch STREQUAL "arm64")
      set(_pj_arch "iossimulator-arm64")
    endif()
    set(_pj_platform_dir "${PJSUA2_MAUI_DIR}/Platforms/iOS")
  endif()

  add_custom_target(pjsua2_csharp_maui ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${_pj_cs_dir}" "${_pj_platform_dir}/pjsua2"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_pj_platform_dir}/lib/${_pj_arch}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${_pj_native_files} "${_pj_platform_dir}/lib/${_pj_arch}"
    COMMENT "Copying the pjsua2 C# bindings into ${_pj_platform_dir}"
  )
  if(_pj_helpers)
    add_custom_command(TARGET pjsua2_csharp_maui POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory
              "${_pj_platform_dir}/org/pjsip"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              ${_pj_helpers} "${_pj_platform_dir}/org/pjsip"
    )
  endif()
  add_dependencies(pjsua2_csharp_maui ${_pj_native_target})

  unset(_pj_platform_dir)
  unset(_pj_arch)
endif()

# ##############################################################################
# Installation: the same layout, under the prefix

set(PJ_INSTALL_CSHARPDIR "${CMAKE_INSTALL_LIBDIR}/pjproject/csharp" CACHE PATH
  "Directory the pjsua2 C# bindings are installed to"
)

# With shared libraries, the native library finds them in the libdir, the
# bundled ones in pjproject/third_party there
if(BUILD_SHARED_LIBS AND NOT IOS AND NOT IS_ABSOLUTE "${PJ_INSTALL_CSHARPDIR}"
   AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  file(RELATIVE_PATH _pj_up "/x/${PJ_INSTALL_CSHARPDIR}/lib"
    "/x/${CMAKE_INSTALL_LIBDIR}"
  )
  if(APPLE)
    set_property(TARGET pjsua2_csharp APPEND PROPERTY INSTALL_RPATH
      "@loader_path/${_pj_up}"
      "@loader_path/${_pj_up}/pjproject/third_party"
    )
  elseif(UNIX)
    set_property(TARGET pjsua2_csharp APPEND PROPERTY INSTALL_RPATH
      "$ORIGIN/${_pj_up}"
      "$ORIGIN/${_pj_up}/pjproject/third_party"
    )
  endif()
  unset(_pj_up)
endif()

install(DIRECTORY "${_pj_cs_dir}/"
  DESTINATION "${PJ_INSTALL_CSHARPDIR}/cs"
  COMPONENT PjCSharp
)
if(IOS)
  install(FILES ${_pj_native_files}
    DESTINATION "${PJ_INSTALL_CSHARPDIR}/lib"
    COMPONENT PjCSharp
  )
else()
  install(TARGETS pjsua2_csharp
    LIBRARY DESTINATION "${PJ_INSTALL_CSHARPDIR}/lib" COMPONENT PjCSharp
    RUNTIME DESTINATION "${PJ_INSTALL_CSHARPDIR}/lib" COMPONENT PjCSharp
    ARCHIVE DESTINATION "${PJ_INSTALL_CSHARPDIR}/lib" COMPONENT PjCSharp
  )
  if(ANDROID_STL STREQUAL "c++_shared")
    install(FILES "${PJ_ANDROID_CXX_SHARED}"
      DESTINATION "${PJ_INSTALL_CSHARPDIR}/lib"
      COMPONENT PjCSharp
    )
  endif()
endif()
if(_pj_helpers)
  install(FILES ${_pj_helpers}
    DESTINATION "${PJ_INSTALL_CSHARPDIR}/java/org/pjsip"
    COMPONENT PjCSharp
  )
endif()

unset(_pj_cs_dir)
unset(_pj_lib_dir)
unset(_pj_java_dir)
unset(_pj_native_target)
unset(_pj_native_files)
unset(_pj_helpers)
