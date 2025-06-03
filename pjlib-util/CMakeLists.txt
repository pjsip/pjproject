# ##############################################################################
# Target

add_library(pjlib-util
  src/pjlib-util/base64.c
  src/pjlib-util/cli.c
  src/pjlib-util/cli_console.c
  src/pjlib-util/cli_telnet.c
  src/pjlib-util/crc32.c
  src/pjlib-util/errno.c
  src/pjlib-util/dns.c
  src/pjlib-util/dns_dump.c
  src/pjlib-util/dns_server.c
  src/pjlib-util/getopt.c
  src/pjlib-util/hmac_md5.c
  src/pjlib-util/hmac_sha1.c
  src/pjlib-util/http_client.c
  src/pjlib-util/json.c
  src/pjlib-util/md5.c
  src/pjlib-util/pcap.c
  src/pjlib-util/resolver.c
  src/pjlib-util/scanner.c
  src/pjlib-util/sha1.c
  src/pjlib-util/srv_resolver.c
  src/pjlib-util/string.c
  src/pjlib-util/stun_simple.c
  src/pjlib-util/stun_simple_client.c
  src/pjlib-util/xml.c
)

target_include_directories(pjlib-util
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ##############################################################################
# Dependencies

target_link_libraries(pjlib-util PUBLIC pjlib)
