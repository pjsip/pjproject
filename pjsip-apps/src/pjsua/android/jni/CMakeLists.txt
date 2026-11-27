# ##############################################################################
# The Android pjsua sample's JNI library, as the Makefile here
#
# libpjsua.so holds the pjsua application, the Java callback glue and the SWIG
# wrapper of pjsua.i; MainActivity opens it with System.loadLibrary("pjsua").
# The build copies it, and the generated org.pjsip.pjsua classes, into the
# sample's source set, see PJSUA_ANDROID_DIR. The app then builds from there.

find_package(SWIG 4.0 REQUIRED COMPONENTS java)
include(UseSWIG)
include(Pj/Swig)

set(_pj_java_out "${CMAKE_CURRENT_BINARY_DIR}/java")
set(_pj_pjsua_dir "${PROJECT_SOURCE_DIR}/pjsip-apps/src/pjsua")

set_property(SOURCE pjsua.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE pjsua.i PROPERTY COMPILE_OPTIONS -package org.pjsip.pjsua)
set_property(SOURCE pjsua.i PROPERTY COMPILE_DEFINITIONS __ANDROID__)
set_property(SOURCE pjsua.i PROPERTY USE_SWIG_DEPENDENCIES TRUE)

swig_add_library(pjsuajni
  TYPE SHARED
  LANGUAGE java
  OUTPUT_DIR "${_pj_java_out}/org/pjsip/pjsua"
  OUTFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
  SOURCES
    pjsua.i
    pjsua_app_callback.cpp
    ${_pj_pjsua_dir}/pjsua_app.c
    ${_pj_pjsua_dir}/pjsua_app_cli.c
    ${_pj_pjsua_dir}/pjsua_app_common.c
    ${_pj_pjsua_dir}/pjsua_app_config.c
    ${_pj_pjsua_dir}/pjsua_app_legacy.c
)

set_target_properties(pjsuajni PROPERTIES
  # Fixed by the loadLibrary call in MainActivity
  OUTPUT_NAME pjsua
  # pjsua-lib is libpjsua too; keep this one apart
  LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
)

target_include_directories(pjsuajni PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${_pj_pjsua_dir}"
)

target_link_libraries(pjsuajni PRIVATE
  pjsua-lib pjsip-ua pjsip-simple pjsip pjmedia-codec pjmedia-audiodev
  pjmedia-videodev pjmedia pjnath pjlib-util pjlib android log
)

# As the pjsua2 wrapper, see ../../../swig/CMakeLists.txt
target_compile_options(pjsuajni PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-function>
)

pj_swig_include_pj_headers(pjsuajni)

pj_android_java_helpers(pjsuajni_helpers "${_pj_java_out}" _pj_helpers)
add_dependencies(pjsuajni pjsuajni_helpers)

# ##############################################################################
# Copy into the sample app

set(PJSUA_ANDROID_DIR "${_pj_pjsua_dir}/android/app/src/main" CACHE PATH
  "The Android source set to copy the pjsua sample's JNI library into; empty for none"
)

if(PJSUA_ANDROID_DIR)
  set(_pj_jni_dir "${PJSUA_ANDROID_DIR}/jniLibs/${ANDROID_ABI}")
  set(_pj_java_dir "${PJSUA_ANDROID_DIR}/java/org/pjsip")
  pj_android_cxx_shared(_pj_cxx_shared)

  add_custom_target(pjsuajni_android ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_pj_jni_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:pjsuajni>" ${_pj_cxx_shared} "${_pj_jni_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${_pj_java_out}/org/pjsip" "${_pj_java_dir}"
    COMMENT "Copying the pjsua sample's JNI library into ${PJSUA_ANDROID_DIR}"
    VERBATIM
  )
  add_dependencies(pjsuajni_android pjsuajni)

  unset(_pj_jni_dir)
  unset(_pj_java_dir)
  unset(_pj_cxx_shared)
endif()

unset(_pj_helpers)
unset(_pj_java_out)
unset(_pj_pjsua_dir)
