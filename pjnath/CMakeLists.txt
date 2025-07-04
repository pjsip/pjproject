# ##############################################################################
# Options

# Include UPnP (Universal Plug and Play) support
option(PJNATH_WITH_UPNP "Enable UPnP support" ON)

# ##############################################################################
# Target

add_library(pjnath
  src/pjnath/errno.c
  src/pjnath/ice_session.c
  src/pjnath/ice_strans.c
  src/pjnath/nat_detect.c
  src/pjnath/stun_auth.c
  src/pjnath/stun_msg.c
  src/pjnath/stun_msg_dump.c
  src/pjnath/stun_session.c
  src/pjnath/stun_sock.c
  src/pjnath/stun_transaction.c
  src/pjnath/turn_session.c
  src/pjnath/turn_sock.c
  src/pjnath/upnp.c
)

target_include_directories(pjnath
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ##############################################################################
# Dependencies

target_link_libraries(pjnath PRIVATE pjlib pjlib-util)

# upnp
if(PJNATH_WITH_UPNP)
  find_package(UPNP)
  if(UPNP_FOUND)
    target_sources(pjnath PRIVATE src/pjnath/upnp.c)
    target_link_libraries(pjnath PRIVATE UPNP::UPNP)
    target_compile_definitions(pjnath PUBLIC PJNATH_HAS_UPNP=1)
  else()
    set(PJLIB_WITH_LIBUUID OFF "Enable UPnP support (not found)" FORCE)
  endif()
endif()

# ##############################################################################
# Client application

add_executable(pjturn-client EXCLUDE_FROM_ALL
  src/pjturn-client/client_main.c
)

target_link_libraries(pjturn-client PRIVATE pjlib pjlib-util pjnath)

# ##############################################################################
# Server application

add_executable(pjturn-srv EXCLUDE_FROM_ALL
  src/pjturn-srv/allocation.c
  src/pjturn-srv/auth.c
  src/pjturn-srv/listener_udp.c
  src/pjturn-srv/listener_tcp.c
  src/pjturn-srv/server.c
  src/pjturn-srv/main.c
)

target_link_libraries(pjturn-srv PRIVATE pjlib pjlib-util pjnath)

# ##############################################################################
# Test application

if(BUILD_TESTING)
  add_executable(pjnath-test
    src/pjnath-test/ice_test.c
    src/pjnath-test/stun.c
    src/pjnath-test/sess_auth.c
    src/pjnath-test/server.c
    src/pjnath-test/concur_test.c
    src/pjnath-test/stun_sock_test.c
    src/pjnath-test/turn_sock_test.c
    src/pjnath-test/test.c
    src/pjnath-test/${PJ_TEST_MAIN}
  )

  target_link_libraries(pjnath-test PRIVATE pjlib pjlib-util pjnath)

  add_test(NAME pjnath-test COMMAND pjnath-test)
endif()
