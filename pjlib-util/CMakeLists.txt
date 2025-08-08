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

# headers
target_sources(pjlib-util
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjlib-util.h
      include/pjlib-util/base64.h
      include/pjlib-util/cli.h
      include/pjlib-util/cli_console.h
      include/pjlib-util/cli_imp.h
      include/pjlib-util/cli_telnet.h
      include/pjlib-util/config.h
      include/pjlib-util/crc32.h
      include/pjlib-util/dns.h
      include/pjlib-util/dns_server.h
      include/pjlib-util/errno.h
      include/pjlib-util/getopt.h
      include/pjlib-util/hmac_md5.h
      include/pjlib-util/hmac_sha1.h
      include/pjlib-util/http_client.h
      include/pjlib-util/json.h
      include/pjlib-util/md5.h
      include/pjlib-util/pcap.h
      include/pjlib-util/resolver.h
      include/pjlib-util/scanner.h
      include/pjlib-util/scanner_cis_bitwise.h
      include/pjlib-util/scanner_cis_uint.h
      include/pjlib-util/sha1.h
      include/pjlib-util/srv_resolver.h
      include/pjlib-util/string.h
      include/pjlib-util/stun_simple.h
      include/pjlib-util/types.h
      include/pjlib-util/xml.h
)

# ##############################################################################
# Dependencies

target_link_libraries(pjlib-util PUBLIC pjlib)

# ##############################################################################
# Test application

if(BUILD_TESTING)
  add_executable(pjlib-util-test
    src/pjlib-util-test/xml.c
    src/pjlib-util-test/encryption.c
    src/pjlib-util-test/stun.c
    src/pjlib-util-test/resolver_test.c
    src/pjlib-util-test/test.c
    src/pjlib-util-test/json_test.c
    src/pjlib-util-test/http_client.c
    src/pjlib-util-test/${PJ_TEST_MAIN}
  )

  target_link_libraries(pjlib-util-test PRIVATE pjlib pjlib-util)

  add_test(NAME pjlib-util-test COMMAND pjlib-util-test)
endif()
