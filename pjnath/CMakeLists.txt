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
