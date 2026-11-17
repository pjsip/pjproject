# Resolves pjlib's find_package(MbedTLS CONFIG) to the Mbed TLS that Zephyr
# builds, which is an interface library named mbedTLS, not a CMake package.
foreach(_c mbedtls mbedx509 tfpsacrypto)
  if(NOT TARGET MbedTLS::${_c})
    add_library(MbedTLS::${_c} INTERFACE IMPORTED GLOBAL)
    set_target_properties(MbedTLS::${_c} PROPERTIES
      INTERFACE_LINK_LIBRARIES mbedTLS)
  endif()
endforeach()
set(MbedTLS_FOUND TRUE)
