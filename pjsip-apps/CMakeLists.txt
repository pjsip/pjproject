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

# embedded_ua uses SDES-SRTP and SIP over TLS unconditionally; it raises
# #error at compile time when either is unavailable.
if(PJMEDIA_WITH_SRTP AND NOT PJLIB_WITH_SSL STREQUAL "")
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
endif()
