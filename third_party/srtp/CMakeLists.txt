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

target_include_directories(srtp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../build/srtp>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(srtp PRIVATE HAVE_CONFIG_H=1)

# ##############################################################################
# Dependencies
# ##############################################################################

# pjlib - for log overrides
target_link_libraries(srtp PRIVATE pjlib)

# openssl
if(NOT BUILD_SHARED_LIBS)
  set(OPENSSL_USE_STATIC_LIBS ON)
endif()

find_package(OpenSSL COMPONENTS Crypto SSL)
if(OPENSSL_FOUND)
  set(CMAKE_REQUIRED_INCLUDES "${OPENSSL_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${OPENSSL_LIBRARIES}")
  include(CheckCSourceCompiles)
  check_c_source_compiles([=[
    #include <openssl/evp.h>

    int main(void) {
      EVP_CIPHER_CTX *ctx;
      EVP_aes_128_gcm();
      return 0;
    }
  ]=] OPENSSL_SUPPORTS_GCM)
endif()
if(OPENSSL_SUPPORTS_GCM)
  target_sources(srtp
    PRIVATE
      # ciphers
      crypto/cipher/aes_icm_ossl.c
      crypto/cipher/aes_gcm_ossl.c
      # hashes
      crypto/hash/hmac_ossl.c
  )
  target_compile_definitions(srtp
    PRIVATE
      OPENSSL
      GCM
  )
  target_link_libraries(srtp
    PRIVATE
      OpenSSL::Crypto
      OpenSSL::SSL
  )
else()
  target_sources(srtp
    PRIVATE
      # ciphers
      crypto/cipher/aes.c
      crypto/cipher/aes_icm.c
      # hashes
      crypto/hash/sha1.c
      crypto/hash/hmac.c
  )
endif()
