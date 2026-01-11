# ##############################################################################
# Options

option(SRTP_WITH_OPENSSL "Build SRTP with OpenSSL support" ON)
if(SRTP_WITH_OPENSSL)
  find_package(OpenSSL COMPONENTS Crypto SSL CONFIG)
  if(NOT OPENSSL_FOUND)
    find_package(OpenSSL COMPONENTS Crypto SSL)
  endif()
  if(OPENSSL_FOUND)
    # check GCM support
    include(CheckCSourceCompiles)
    include(CMakePushCheckState)

    cmake_push_check_state(RESET)
      set(CMAKE_REQUIRED_INCLUDES "${OPENSSL_INCLUDE_DIR}")
      set(CMAKE_REQUIRED_LIBRARIES "${OPENSSL_CRYPTO_LIBRARIES}")
      check_c_source_compiles([=[
        #include <openssl/evp.h>

        int main(void) {
          EVP_CIPHER_CTX *ctx;
          EVP_aes_128_gcm();
          return 0;
        }
      ]=] OPENSSL_HAS_GCM)
    cmake_pop_check_state()
  else()
    set(SRTP_WITH_OPENSSL OFF CACHE BOOL "OpenSSL not found" FORCE)
  endif()
endif()

# ##############################################################################
# Target

add_library(srtp
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
  target_sources(srtp
    PRIVATE
      crypto/cipher/aes_icm_ossl.c
      crypto/cipher/aes_gcm_ossl.c
      crypto/hash/hmac_ossl.c
  )
else()
  target_sources(srtp
    PRIVATE
      crypto/cipher/aes.c
      crypto/cipher/aes_icm.c
      crypto/hash/sha1.c
      crypto/hash/hmac.c
)
endif()

# headers
target_sources(srtp
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

# ##############################################################################
# Dependencies

# pjlib - for log overrides
target_link_libraries(srtp PRIVATE pjlib)

# OpenSSL
if(SRTP_WITH_OPENSSL AND OPENSSL_HAS_GCM)
  target_link_libraries(srtp PRIVATE OpenSSL::Crypto OpenSSL::SSL)
  target_compile_definitions(srtp PRIVATE OPENSSL=1 GCM=1)
endif()

# ##############################################################################
# Configuration

target_include_directories(srtp BEFORE
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/srtp"
)

target_compile_definitions(srtp PRIVATE HAVE_CONFIG_H=1)
