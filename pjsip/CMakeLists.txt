# ##############################################################################
# Module options

include(CMakeDependentOption)

# Include TLS transport support
cmake_dependent_option(PJSIP_WITH_TLS
  "Enable TLS transport support" ON
  "PJLIB_WITH_SSL" OFF
)

# ##############################################################################
# pjsip

add_library(pjsip
  src/pjsip/sip_config.c
  src/pjsip/sip_multipart.c
  src/pjsip/sip_errno.c
  src/pjsip/sip_msg.c
  src/pjsip/sip_parser.c
  src/pjsip/sip_tel_uri.c
  src/pjsip/sip_uri.c
  src/pjsip/sip_endpoint.c
  src/pjsip/sip_util.c
  src/pjsip/sip_util_proxy.c
  src/pjsip/sip_resolve.c
  src/pjsip/sip_transport.c
  src/pjsip/sip_transport_loop.c
  src/pjsip/sip_transport_udp.c
  src/pjsip/sip_transport_tcp.c
  src/pjsip/sip_transport_tls.c
  src/pjsip/sip_auth_aka.c
  src/pjsip/sip_auth_client.c
  src/pjsip/sip_auth_msg.c
  src/pjsip/sip_auth_parser.c
  src/pjsip/sip_auth_server.c
  src/pjsip/sip_transaction.c
  src/pjsip/sip_util_statefull.c
  src/pjsip/sip_dialog.c
  src/pjsip/sip_ua_layer.c
)

target_include_directories(pjsip
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pjsip PRIVATE pjlib pjlib-util)

#
# pjsip - configuration
#

# definitions header
block(SCOPE_FOR VARIABLES)
  # G711 codec
  set(PJSIP_HAS_TLS_TRANSPORT ${PJSIP_WITH_TLS})

  # write configured header
  configure_file(
    include/pjsip/sip_autoconf.h.cm
    include/pjsip/sip_autoconf.h
  )

  # add generated file to headers file set
  target_sources(pjsip
    PUBLIC
      FILE_SET config_headers TYPE HEADERS
      BASE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/include"
      FILES
        "${CMAKE_CURRENT_BINARY_DIR}/include/pjsip/sip_autoconf.h"
  )
endblock()

# ##############################################################################
# pjsip-simple

add_library(pjsip-simple
  src/pjsip-simple/errno.c
  src/pjsip-simple/evsub.c
  src/pjsip-simple/evsub_msg.c
  src/pjsip-simple/iscomposing.c
  src/pjsip-simple/mwi.c
  src/pjsip-simple/pidf.c
  src/pjsip-simple/dialog_info.c
  src/pjsip-simple/presence.c
  src/pjsip-simple/dlg_event.c
  src/pjsip-simple/presence_body.c
  src/pjsip-simple/publishc.c
  src/pjsip-simple/rpid.c
  src/pjsip-simple/xpidf.c
)

target_include_directories(pjsip-simple
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pjsip-simple PRIVATE pjlib pjlib-util pjsip)

# ##############################################################################
# pjsip-ua

add_library(pjsip-ua
  src/pjsip-ua/sip_inv.c
  src/pjsip-ua/sip_reg.c
  src/pjsip-ua/sip_replaces.c
  src/pjsip-ua/sip_xfer.c
  src/pjsip-ua/sip_100rel.c
  src/pjsip-ua/sip_timer.c
  src/pjsip-ua/sip_siprec.c
)

target_include_directories(pjsip-ua
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pjsip-ua
  PRIVATE
    pjlib pjlib-util pjmedia pjsip pjsip-simple
)

# ##############################################################################
# pjsua-lib

add_library(pjsua-lib
  src/pjsua-lib/pjsua_acc.c
  src/pjsua-lib/pjsua_call.c
  src/pjsua-lib/pjsua_core.c
  src/pjsua-lib/pjsua_im.c
  src/pjsua-lib/pjsua_media.c
  src/pjsua-lib/pjsua_pres.c
  src/pjsua-lib/pjsua_dump.c
  src/pjsua-lib/pjsua_aud.c
  src/pjsua-lib/pjsua_txt.c
  src/pjsua-lib/pjsua_vid.c
)

target_include_directories(pjsua-lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pjsua-lib
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
    pjsip-ua
)

# ##############################################################################
# pjsua2

add_library(pjsua2
  src/pjsua2/account.cpp
  src/pjsua2/endpoint.cpp
  src/pjsua2/json.cpp
  src/pjsua2/persistent.cpp
  src/pjsua2/types.cpp
  src/pjsua2/siptypes.cpp
  src/pjsua2/call.cpp
  src/pjsua2/presence.cpp
  src/pjsua2/media.cpp
)

target_include_directories(pjsua2
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pjsua2
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
    pjsip-ua
    pjsua-lib
)
