# ##############################################################################
# Options

option(SRTP_WITH_OPENSSL "Build SRTP with OpenSSL support" ON)
set(OPENSSL_HAS_GCM FALSE)
if(SRTP_WITH_OPENSSL)
  # srtp only needs libcrypto (EVP and HMAC), not libssl
  pj_find_openssl(COMPONENTS Crypto ${PJ_FIND_QUIET})
  if(OPENSSL_FOUND)
    # GCM support, checked again when libcrypto changes. Earlier versions of
    # this probe kept their result in PJ_SRTP_OPENSSL_HAS_GCM or
    # OPENSSL_HAS_GCM, and never checked again.
    unset(PJ_SRTP_OPENSSL_HAS_GCM CACHE)
    include(Pj/CheckLibraryUsable)
    pj_check_library_usable(SRTP_OPENSSL_GCM
      CODE [=[
        #include <openssl/evp.h>

        int main(void) {
          EVP_aes_128_gcm();
          return 0;
        }
      ]=]
      LIBRARIES OpenSSL::Crypto
    )
    set(OPENSSL_HAS_GCM "${SRTP_OPENSSL_GCM_USABLE}")
  else()
    # this configure only: OpenSSL may be found by the next one
    set(SRTP_WITH_OPENSSL OFF)
  endif()
endif()

# This links OpenSSL whatever pjlib's SSL backend is, so PjConfig.cmake.in has
# to re-resolve it on its own; a global property to be visible outside this
# directory, and not in the cache, where it would outlive a switch to the
# system srtp.
if(SRTP_WITH_OPENSSL AND OPENSSL_HAS_GCM)
  set_property(GLOBAL PROPERTY PJ_SRTP_WITH_OPENSSL ON)
else()
  set_property(GLOBAL PROPERTY PJ_SRTP_WITH_OPENSSL OFF)
endif()

# ##############################################################################
# Target

add_library(pj_srtp
  # main
  srtp/srtp.c
  # ciphers
  crypto/cipher/cipher.c
  crypto/cipher/null_cipher.c
  crypto/cipher/cipher_test_cases.c
  # hashes
  crypto/hash/null_auth.c
  crypto/hash/auth.c
  crypto/hash/auth_test_cases.c
  # replay
  crypto/replay/rdb.c
  crypto/replay/rdbx.c
  # math
  crypto/math/datatypes.c
  # kernel
  crypto/kernel/crypto_kernel.c
  crypto/kernel/alloc.c
  crypto/kernel/key.c
  pjlib/srtp_err.c
  #crypto/ust/ust.c
)

if(SRTP_WITH_OPENSSL AND OPENSSL_HAS_GCM)
  target_sources(pj_srtp
    PRIVATE
      crypto/cipher/aes_icm_ossl.c
      crypto/cipher/aes_gcm_ossl.c
      crypto/hash/hmac_ossl.c
  )
else()
  target_sources(pj_srtp
    PRIVATE
      crypto/cipher/aes.c
      crypto/cipher/aes_icm.c
      crypto/hash/sha1.c
      crypto/hash/hmac.c
)
endif()

# headers
target_sources(pj_srtp
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
        crypto/include
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/srtp"
      FILES
        include/srtp.h
        include/srtp_priv.h
        include/stream_list_priv.h
        crypto/include/aes.h
        crypto/include/aes_gcm.h
        crypto/include/aes_icm.h
        crypto/include/aes_icm_ext.h
        crypto/include/alloc.h
        crypto/include/auth.h
        crypto/include/cipher.h
        crypto/include/cipher_priv.h
        crypto/include/cipher_types.h
        crypto/include/crypto_kernel.h
        crypto/include/crypto_types.h
        crypto/include/datatypes.h
        crypto/include/err.h
        crypto/include/hmac.h
        crypto/include/integers.h
        crypto/include/key.h
        crypto/include/null_auth.h
        crypto/include/null_cipher.h
        crypto/include/rdb.h
        crypto/include/rdbx.h
        crypto/include/sha1.h

        # custom config file
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/srtp/srtp_config.h"
)

# Warnings in the bundled code, which is kept as upstream has it
target_compile_options(pj_srtp
  PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wno-format>
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-shorten-64-to-32>
)

# ##############################################################################
# Dependencies

# pjlib - for log overrides
target_link_libraries(pj_srtp PRIVATE pjlib)

# OpenSSL
if(SRTP_WITH_OPENSSL AND OPENSSL_HAS_GCM)
  target_link_libraries(pj_srtp PRIVATE OpenSSL::Crypto)
  target_compile_definitions(pj_srtp PRIVATE OPENSSL=1 GCM=1)
endif()

# ##############################################################################
# Configuration

target_include_directories(pj_srtp BEFORE
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/srtp"
)

target_compile_definitions(pj_srtp PRIVATE HAVE_CONFIG_H=1)

# For the pkg-config file: the files of the dependencies found here
pj_pc_remember_imported_targets()
