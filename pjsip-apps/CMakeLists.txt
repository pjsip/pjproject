list(APPEND CMAKE_MESSAGE_CONTEXT "pjsip-apps")

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
  # in bin/samples, see "Output directories" in the top-level CMakeLists.txt
  set_target_properties(${_sample} PROPERTIES PJ_OUTPUT_SUBDIR samples)
endforeach()
unset(_pj_samples)

if(PJ_BUILD_PJSUA2)
  foreach(_sample pjsua2_demo pjsua2_hello_reg)
    add_executable(${_sample} src/samples/${_sample}.cpp)
    target_link_libraries(${_sample} PRIVATE ${_pj_app_libs} pjsua2)
    set_target_properties(${_sample} PROPERTIES PJ_OUTPUT_SUBDIR samples)
  endforeach()
endif() # PJ_BUILD_PJSUA2

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

# The published Oboe AAR has only a shared liboboe.so: the Java or C#
# bindings' native library then needs it at run time, and it is not copied or
# packaged with the bindings. (libc++_shared.so, which it also needs, is: a
# shared Oboe implies c++_shared, see pjmedia/CMakeLists.txt.)
if(ANDROID AND (PJ_BUILD_SWIG_JAVA OR PJ_BUILD_SWIG_CSHARP)
   AND PJMEDIA_WITH_AUDIODEV_OBOE AND Oboe_LIBRARY MATCHES "\\.so$")
  message(WARNING
    "The pjsua2 bindings' native library needs liboboe.so at run time "
    "(${Oboe_LIBRARY}), which is not packaged with it. Add it to the app (its "
    "jniLibs, or the MAUI project), or use a static Oboe built from source, "
    "as build/android/build-aar.sh does.")
endif()

# See PJ_BUILD_SWIG_JAVA in the top-level CMakeLists.txt
if(PJ_BUILD_SWIG_JAVA)
  add_subdirectory(src/swig)
endif()

# See PJ_BUILD_PJSUA_ANDROID in the top-level CMakeLists.txt
if(PJ_BUILD_PJSUA_ANDROID)
  add_subdirectory(src/pjsua/android/jni)
endif()

# See PJ_BUILD_SWIG_PYTHON in the top-level CMakeLists.txt
if(PJ_BUILD_SWIG_PYTHON)
  add_subdirectory(src/swig/python)
endif()

# See PJ_BUILD_SWIG_CSHARP in the top-level CMakeLists.txt
if(PJ_BUILD_SWIG_CSHARP)
  add_subdirectory(src/swig/csharp)
endif()
