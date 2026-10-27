list(APPEND CMAKE_MESSAGE_CONTEXT "pjsip-apps")

include(CMakeDependentOption)

# The applications, see PJ_BUILD_APPS in the top-level CMakeLists.txt
if(PJ_BUILD_APPS)

# ##############################################################################
# pjsua

add_executable(pjsua
  src/pjsua/main.c
  src/pjsua/pjsua_app.c
  src/pjsua/pjsua_app_cli.c
  src/pjsua/pjsua_app_common.c
  src/pjsua/pjsua_app_config.c
  src/pjsua/pjsua_app_legacy.c
)

# Every application links all the libraries, as make's APP_LDLIBS
set(_pj_app_libs
  pjlib
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
)

target_link_libraries(pjsua PRIVATE ${_pj_app_libs})

# ##############################################################################
# pjsystest

add_executable(pjsystest
  src/pjsystest/systest.c
  src/pjsystest/main_console.c
)
target_link_libraries(pjsystest PRIVATE ${_pj_app_libs})

# ##############################################################################
# samples, as build/Samples.mak

set(_pj_samples
  aidemo auddemo aviplay aectest clidemo confsample embedded_ua encdec
  httpdemo icedemo jbsim latency level mix pjsip-perf pcaputil playfile
  playsine recfile resampleplay simpleua simple_pjsua sipecho siprtp
  sipstateless stateful_proxy stateless_proxy stereotest streamutil strerror
  tonegen vaddemo vid_codec_test vid_streamutil
)
foreach(_sample IN LISTS _pj_samples)
  add_executable(${_sample} src/samples/${_sample}.c)
  target_link_libraries(${_sample} PRIVATE ${_pj_app_libs})
  set_target_properties(${_sample} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/samples"
  )
endforeach()
unset(_pj_samples)

if(PJ_WITH_CXX)
  foreach(_sample pjsua2_demo pjsua2_hello_reg)
    add_executable(${_sample} src/samples/${_sample}.cpp)
    target_link_libraries(${_sample} PRIVATE ${_pj_app_libs} pjsua2)
    set_target_properties(${_sample} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/samples"
    )
  endforeach()
endif() # PJ_WITH_CXX

# ##############################################################################
# pjsua-test: the Python tests in tests/pjsua, as make pjsua-test. Not in
# ctest: they take minutes and some need SIPp.

set(_pj_pjsua_test_dir "${PROJECT_SOURCE_DIR}/tests/pjsua")

# mod_media_playrec.py runs tools/cmp_wav, so it is built there, and only for
# pjsua-test, as make does
add_executable(cmp_wav EXCLUDE_FROM_ALL "${_pj_pjsua_test_dir}/tools/cmp_wav.c")
target_link_libraries(cmp_wav PRIVATE ${_pj_app_libs})
set_target_properties(cmp_wav PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "$<1:${_pj_pjsua_test_dir}/tools>"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_custom_target(pjsua-test
    COMMAND "${Python3_EXECUTABLE}" runall.py -t 2 --exe "$<TARGET_FILE:pjsua>"
    WORKING_DIRECTORY "${_pj_pjsua_test_dir}"
    DEPENDS pjsua cmp_wav
    USES_TERMINAL
    VERBATIM
  )
endif()
unset(_pj_pjsua_test_dir)

unset(_pj_app_libs)

endif() # PJ_BUILD_APPS

# ##############################################################################
# SWIG bindings

# Off by default: SWIG is a build dependency nothing else here needs, and most
# people configuring this project only want the libraries. Gradle turns it on.
cmake_dependent_option(PJ_BUILD_SWIG_JAVA
  "Build the pjsua2 JNI bindings with SWIG" OFF
  "ANDROID;PJ_WITH_CXX" OFF
)

if(PJ_BUILD_SWIG_JAVA)
  add_subdirectory(src/swig)
endif()
