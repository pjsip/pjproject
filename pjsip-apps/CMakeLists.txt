include(CMakeDependentOption)

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

target_link_libraries(pjsua
  PRIVATE
    pjlib
    pjlib-util
    pjnath
    pjmedia
    pjmedia-codec
    pjmedia-audiodev
    pjmedia-videodev
    pjsip
    pjsip-simple
    pjsua-lib
)

# ##############################################################################
# samples

add_executable(aidemo src/samples/aidemo.c)
target_link_libraries(aidemo
  PRIVATE
    pjlib
    pjlib-util
    pjnath
    pjmedia
    pjmedia-codec
    pjmedia-audiodev
    pjmedia-videodev
)

add_executable(embedded_ua src/samples/embedded_ua.c)
target_link_libraries(embedded_ua
  PRIVATE
    pjlib
    pjlib-util
    pjnath
    pjmedia
    pjmedia-codec
    pjmedia-audiodev
    pjsip
    pjsip-ua
)

# ##############################################################################
# SWIG bindings

# Off by default: SWIG is a build dependency nothing else here needs, and most
# people configuring this project only want the libraries. Gradle turns it on.
cmake_dependent_option(PJ_BUILD_SWIG_JAVA
  "Build the pjsua2 JNI bindings with SWIG" OFF
  "ANDROID" OFF
)

if(PJ_BUILD_SWIG_JAVA)
  add_subdirectory(src/swig)
endif()
