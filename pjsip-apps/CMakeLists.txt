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
