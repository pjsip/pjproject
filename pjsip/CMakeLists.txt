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

# headers
target_sources(pjsip
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjsip.h
      include/pjsip/print_util.h
      include/pjsip/sip_auth.h
      include/pjsip/sip_auth_aka.h
      include/pjsip/sip_auth_msg.h
      include/pjsip/sip_auth_parser.h
      include/pjsip/sip_autoconf.h.cm
      include/pjsip/sip_autoconf.h.in
      include/pjsip/sip_config.h
      include/pjsip/sip_dialog.h
      include/pjsip/sip_endpoint.h
      include/pjsip/sip_errno.h
      include/pjsip/sip_event.h
      include/pjsip/sip_module.h
      include/pjsip/sip_msg.h
      include/pjsip/sip_multipart.h
      include/pjsip/sip_parser.h
      include/pjsip/sip_private.h
      include/pjsip/sip_resolve.h
      include/pjsip/sip_tel_uri.h
      include/pjsip/sip_transaction.h
      include/pjsip/sip_transport.h
      include/pjsip/sip_transport_loop.h
      include/pjsip/sip_transport_tcp.h
      include/pjsip/sip_transport_tls.h
      include/pjsip/sip_transport_udp.h
      include/pjsip/sip_types.h
      include/pjsip/sip_ua_layer.h
      include/pjsip/sip_uri.h
      include/pjsip/sip_util.h
)

# dependencies
target_link_libraries(pjsip
  PRIVATE
    pjlib
    pjlib-ssl
    pjlib-util
)

# configuration
block(SCOPE_FOR VARIABLES)
  # G711 codec
  set(PJSIP_HAS_TLS_TRANSPORT ${PJSIP_WITH_TLS})

  # write configured header
  configure_file(
    include/pjsip/sip_autoconf.h.cm
    include/pjsip/sip_autoconf.h
  )
endblock()

# add generated file to headers file set
target_sources(pjsip
  PUBLIC
    FILE_SET config_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/include"
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/include/pjsip/sip_autoconf.h"
)

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

# headers
target_sources(pjsip-simple
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjsip_simple.h
      include/pjsip-simple/dialog_info.h
      include/pjsip-simple/dlg_event.h
      include/pjsip-simple/errno.h
      include/pjsip-simple/evsub.h
      include/pjsip-simple/evsub_msg.h
      include/pjsip-simple/iscomposing.h
      include/pjsip-simple/mwi.h
      include/pjsip-simple/pidf.h
      include/pjsip-simple/presence.h
      include/pjsip-simple/publish.h
      include/pjsip-simple/rpid.h
      include/pjsip-simple/types.h
      include/pjsip-simple/xpidf.h
)

# dependencies
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

# headers
target_sources(pjsip-ua
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjsip_ua.h
      include/pjsip-ua/sip_100rel.h
      include/pjsip-ua/sip_inv.h
      include/pjsip-ua/sip_regc.h
      include/pjsip-ua/sip_replaces.h
      include/pjsip-ua/sip_siprec.h
      include/pjsip-ua/sip_timer.h
      include/pjsip-ua/sip_xfer.h
)

# dependencies
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

# headers
target_sources(pjsua-lib
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjsua.h
      include/pjsua-lib/pjsua.h
      include/pjsua-lib/pjsua_internal.h
)

# dependencies
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

# headers
target_sources(pjsua2
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
      FILES
        include/pjsua2.hpp
        include/pjsua2/account.hpp
        include/pjsua2/call.hpp
        include/pjsua2/config.hpp
        include/pjsua2/doxygen.hpp
        include/pjsua2/endpoint.hpp
        include/pjsua2/json.hpp
        include/pjsua2/media.hpp
        include/pjsua2/persistent.hpp
        include/pjsua2/presence.hpp
        include/pjsua2/siptypes.hpp
        include/pjsua2/types.hpp

)

# dependencies
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

# ##############################################################################
# Test application

if(BUILD_TESTING)
  # pjsip test application
  add_executable(pjsip-test
    src/test/dlg_core_test.c
    src/test/dns_test.c
    src/test/msg_err_test.c
    src/test/msg_logger.c
    src/test/msg_test.c
    src/test/multipart_test.c
    src/test/regc_test.c
    src/test/test.c
    src/test/transport_loop_test.c
    src/test/transport_tcp_test.c
    src/test/transport_test.c
    src/test/transport_udp_test.c
    src/test/tsx_basic_test.c
    src/test/tsx_bench.c
    src/test/tsx_uac_test.c
    src/test/tsx_uas_test.c
    src/test/txdata_test.c
    src/test/uri_test.c
    src/test/inv_offer_answer_test.c
    src/test/auth_async_test.c
    src/test/${PJ_TEST_MAIN}
  )

  target_link_libraries(pjsip-test
    PRIVATE
      pjlib
      pjlib-util
      pjmedia
      pjsip
      pjsip-simple
      pjsip-ua
  )

  add_test(NAME pjsip-test COMMAND pjsip-test)

  # pjsua test application
  add_executable(pjsua2-test
    src/pjsua2-test/main.cpp
    src/pjsua2-test/instant_messaging.cpp
  )

  target_link_libraries(pjsua2-test
    PRIVATE
      pjlib
      pjlib-util
      pjnath
      pjmedia
      pjmedia-audiodev
      pjmedia-videodev
      pjsip
      pjsua2
  )

  add_test(NAME pjsua2-test COMMAND pjsua2-test)
endif()
