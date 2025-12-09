# ##############################################################################
# Options

include(Pj/Option)
include(CMakeDependentOption)

# Floating point support
option(PJLIB_WITH_FLOATING_POINT "Enable floating point support" ON)

# I/O queue backend
pj_option(PJLIB_WITH_IOQUEUE "I/O Queue implementation backend"
  DEFAULT select
  ALLOWED_VALUES select kqueue epoll iocp
)

# Use `libuuid` for `UUID` handling implementation
cmake_dependent_option(PJLIB_WITH_LIBUUID
  "Enable libuuid (Linux only)" ON
  "LINUX" OFF
)

# Enable SSL support with the selected implementation backend
pj_option(PJLIB_WITH_SSL "Enable SSL support"
  DEFAULT openssl
  ALLOWED_VALUES openssl gnutls mbedtls darwin apple schannel
)

# ##############################################################################
# Target

add_library(pjlib
  src/pj/activesock.c
  src/pj/addr_resolv_sock.c
  src/pj/array.c
  src/pj/atomic_queue.cpp
  src/pj/atomic_slist.c
  src/pj/config.c
  src/pj/ctype.c
  src/pj/errno.c
  src/pj/except.c
  src/pj/fifobuf.c
  src/pj/guid.c
  src/pj/hash.c
  src/pj/ioqueue_dummy.c
  src/pj/ioqueue_epoll.c
  src/pj/ioqueue_kqueue.c
  src/pj/ioqueue_select.c
  src/pj/ip_helper_generic.c
  src/pj/list.c
  src/pj/lock.c
  src/pj/log.c
  src/pj/log_writer_stdout.c
  src/pj/os_info.c
  src/pj/os_time_common.c
  src/pj/os_timestamp_common.c
  src/pj/pool.c
  src/pj/pool_buf.c
  src/pj/pool_caching.c
  src/pj/pool_dbg.c
  src/pj/pool_policy_malloc.c
  src/pj/rand.c
  src/pj/rbtree.c
  src/pj/sock_bsd.c
  src/pj/sock_common.c
  src/pj/sock_qos_common.c
  src/pj/sock_qos_bsd.c
  src/pj/sock_qos_darwin.c
  src/pj/sock_select.c
  src/pj/ssl_sock_common.c
  src/pj/ssl_sock_darwin.c
  src/pj/ssl_sock_dump.c
  src/pj/ssl_sock_gtls.c
  src/pj/ssl_sock_mbedtls.c
  src/pj/ssl_sock_ossl.c
  src/pj/string.c
  src/pj/timer.c
  src/pj/types.c
  src/pj/unittest.c
)

# Ensure generated headers are found before source headers
target_include_directories(pjlib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

# headers
target_sources(pjlib
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
      FILES
        include/pjlib.h
        include/pj/activesock.h
        include/pj/addr_resolv.h
        include/pj/argparse.h
        include/pj/array.h
        include/pj/assert.h
        include/pj/atomic_queue.h
        include/pj/atomic_slist.h
        include/pj/compat/assert.h
        include/pj/compat/cc_armcc.h
        include/pj/compat/cc_clang.h
        include/pj/compat/cc_codew.h
        include/pj/compat/cc_gcc.h
        include/pj/compat/cc_gcce.h
        include/pj/compat/cc_msvc.h
        include/pj/compat/cc_mwcc.h
        include/pj/compat/ctype.h
        include/pj/compat/errno.h
        include/pj/compat/high_precision.h
        include/pj/compat/limits.h
        include/pj/compat/m_alpha.h
        include/pj/compat/m_armv4.h
        include/pj/compat/m_i386.h
        include/pj/compat/m_m68k.h
        include/pj/compat/m_powerpc.h
        include/pj/compat/m_sparc.h
        include/pj/compat/m_x86_64.h
        include/pj/compat/malloc.h
        include/pj/compat/os_darwinos.h
        include/pj/compat/os_linux.h
        include/pj/compat/os_palmos.h
        include/pj/compat/os_rtems.h
        include/pj/compat/os_sunos.h
        include/pj/compat/os_symbian.h
        include/pj/compat/os_win32.h
        include/pj/compat/os_win32_wince.h
        include/pj/compat/os_winphone8.h
        include/pj/compat/os_winuwp.h
        include/pj/compat/rand.h
        include/pj/compat/setjmp.h
        include/pj/compat/size_t.h
        include/pj/compat/socket.h
        include/pj/compat/stdarg.h
        include/pj/compat/stdfileio.h
        include/pj/compat/string.h
        include/pj/compat/time.h
        include/pj/config.h
        include/pj/ctype.h
        include/pj/doxygen.h
        include/pj/errno.h
        include/pj/except.h
        include/pj/fifobuf.h
        include/pj/file_access.h
        include/pj/file_io.h
        include/pj/guid.h
        include/pj/hash.h
        include/pj/ioqueue.h
        include/pj/ip_helper.h
        include/pj/limits.h
        include/pj/list.h
        include/pj/list_i.h
        include/pj/lock.h
        include/pj/log.h
        include/pj/math.h
        include/pj/os.h
        include/pj/pool.h
        include/pj/pool_alt.h
        include/pj/pool_buf.h
        include/pj/pool_i.h
        include/pj/rand.h
        include/pj/rbtree.h
        include/pj/sock.h
        include/pj/sock_qos.h
        include/pj/sock_select.h
        include/pj/ssl_sock.h
        include/pj/string.h
        include/pj/string_i.h
        include/pj/timer.h
        include/pj/types.h
        include/pj/unicode.h
        include/pj/unittest.h
)

# system APIs
if(WIN32 OR CYGWIN OR MINGW)
  target_sources(pjlib
    PRIVATE
      src/pj/file_io_win32.c
      src/pj/guid_win32.c
      src/pj/os_core_win32.c
      src/pj/os_error_win32.c
      src/pj/os_time_win32.c
      src/pj/os_timestamp_win32.c
      src/pj/sock_qos_bsd.c
      src/pj/unicode_win32.c
  )
else()
  target_sources(pjlib
    PRIVATE
      src/pj/file_access_unistd.c
      src/pj/file_io_ansi.c
      src/pj/os_core_unix.c
      src/pj/os_error_unix.c
      src/pj/os_time_unix.c
      src/pj/os_timestamp_posix.c
  )
endif()

# Apple-specific
if(APPLE)
  target_sources(pjlib
    PRIVATE
      src/pj/os_core_darwin.m
      src/pj/ssl_sock_apple.m
      $<$<BOOL:${IOS}>:src/pj/os_info_iphone.m>
  )
endif()

# UUID
if(ANDROID)
  target_sources(pjlib PRIVATE src/pj/guid_android.c)
elseif(APPLE)
  target_sources(pjlib PRIVATE src/pj/guid_darwin.c)
elseif(BSD)
  target_sources(pjlib PRIVATE src/pj/guid_bsd.c)
elseif(WIN32 OR CYGWIN OR MINGW)
  target_sources(pjlib PRIVATE src/pj/file_access_win32.c)
elseif(NOT PJLIB_WITH_LIBUUID)
  target_sources(pjlib PRIVATE src/pj/guid_simple.c)
endif()

# ##############################################################################
# Dependencies

# win32
include(CheckIncludeFile)
if(WIN32 OR CYGWIN OR MINGW)
  check_include_file(winsock.h PJ_HAS_WINSOCK_H)
  check_include_file(winsock2.h PJ_HAS_WINSOCK2_H)
  check_include_file(mswsock.h PJ_HAS_MSWSOCK_H)
  check_include_file(ws2tcpip.h PJ_HAS_WS2TCPIP_H)

  target_link_libraries(pjlib PRIVATE ole32 winmm)
  if(PJ_HAS_WINSOCK2_H)
    target_link_libraries(pjlib PRIVATE ws2_32)
  elseif(PJ_HAS_WINSOCK_H)
    target_link_libraries(pjlib PRIVATE wsock32)
  endif()
endif()

# apple
if(APPLE)
  find_library(CoreFoundation_LIBRARY CoreFoundation)
  if (CoreFoundation_LIBRARY)
    target_link_libraries(pjlib PRIVATE "${CoreFoundation_LIBRARY}")
  endif()
endif()

# math library
if(NOT (WIN32 OR CYGWIN OR MINGW))
  find_library(MATH_LIBRARY m)
  if(MATH_LIBRARY)
    target_link_libraries(pjlib PUBLIC m)
  elseif(PJLIB_WITH_FLOATING_POINT)
    pj_force_set(PJLIB_WITH_FLOATING_POINT OFF "math library not found")
  endif()
endif()

# threading library
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(pjlib PRIVATE Threads::Threads)
endif()

# uuid
if(PJLIB_WITH_LIBUUID)
  find_package(UUID)
  if(UUID_FOUND)
    target_link_libraries(pjlib PRIVATE UUID::UUID)
    target_sources(pjlib PRIVATE src/pj/guid_uuid.c)
  else()
    pj_force_set(PJLIB_WITH_LIBUUID OFF "libuuid not found")
    target_sources(pjlib PRIVATE src/pj/guid_simple.c)
  endif()
endif()

# ssl
add_library(pjlib-ssl INTERFACE) # a target to expose SSL deps
if(PJLIB_WITH_SSL STREQUAL "openssl")
  find_package(OpenSSL COMPONENTS Crypto SSL CONFIG)
  if(NOT OPENSSL_FOUND)
    find_package(OpenSSL COMPONENTS Crypto SSL)
  endif()
  if(OPENSSL_FOUND)
    target_link_libraries(pjlib-ssl INTERFACE OpenSSL::Crypto OpenSSL::SSL)
  else()
    pj_force_set(PJLIB_WITH_SSL "" "OpenSSL not found")
  endif()
elseif(PJLIB_WITH_SSL STREQUAL "gnutls")
  find_package(GnuTLS REQUIRED)
  target_link_libraries(pjlib-ssl INTERFACE GnuTLS::GnuTLS)
elseif(PJLIB_WITH_SSL STREQUAL "mbedtls")
  find_package(MbedTLS CONFIG REQUIRED)
  target_link_libraries(pjlib-ssl
    INTERFACE
      MbedTLS::mbedtls
      MbedTLS::mbedcrypto
      MbedTLS::mbedx509
  )
elseif(PJLIB_WITH_SSL STREQUAL "darwin")
  message(FATAL_ERROR "TODO")
elseif(PJLIB_WITH_SSL STREQUAL "darwin")
  message(FATAL_ERROR "TODO")
elseif(PJLIB_WITH_SSL STREQUAL "apple")
  message(FATAL_ERROR "TODO")
elseif(PJLIB_WITH_SSL STREQUAL "schannel")
  message(FATAL_ERROR "TODO")
endif()
target_link_libraries(pjlib PRIVATE pjlib-ssl)

# ##############################################################################
# Configuration

# Do not attempt to guess default configuration
target_compile_definitions(pjlib PUBLIC PJ_AUTOCONF=1)

# architecture
block(SCOPE_FOR VARIABLES)
  include(Pj/DetectArch)
  pj_detect_arch(arch)

  # architecture name
  set(PJ_M_NAME "${arch}")

  # architecture macros
  target_compile_definitions(pjlib PUBLIC
    $<$<STREQUAL:${arch},"i386">:PJ_M_I386=1>
    $<$<STREQUAL:${arch},"x86_64">:PJ_M_X86_64=1>
    $<$<STREQUAL:${arch},"ia64">:PJ_M_IA64=1>
    $<$<STREQUAL:${arch},"m68k">:PJ_M_M68K=1>
    $<$<STREQUAL:${arch},"alpha">:PJ_M_ALPHA=1>
    $<$<STREQUAL:${arch},"mips64">:PJ_M_MIPS=1>
    $<$<STREQUAL:${arch},"mips">:PJ_M_MIPS=1>
    $<$<STREQUAL:${arch},"sparc">:PJ_M_SPARC=1>
    $<$<STREQUAL:${arch},"arm64">:PJ_M_ARM64=1>
    $<$<STREQUAL:${arch},"armv7">:PJ_M_ARMV7=1>
    $<$<STREQUAL:${arch},"armv4">:PJ_M_ARMV4=1>
    $<$<STREQUAL:${arch},"powerpc">:PJ_M_POWERPC=1>
    $<$<STREQUAL:${arch},"nios2">:PJ_M_NIOS2=1>
  )

  # endianness detection
  include(TestBigEndian)
  test_big_endian(WORDS_BIGENDIAN)

  # floating point support
  if(PJLIB_WITH_FLOATING_POINT)
    set(PJ_HAS_FLOATING_POINT 1)
  else()
    set(PJ_HAS_FLOATING_POINT 0)
  endif()

  # write configured header
  configure_file(
    include/pj/compat/m_auto.h.cm
    include/pj/compat/m_auto.h
    @ONLY
  )
endblock()

# operating system
block(SCOPE_FOR VARIABLES)
  #
  # os detection
  #

  string(TOLOWER ${CMAKE_SYSTEM_NAME} PJ_OS_NAME)

  if(ANDROID)
    set(PJ_ANDROID 1)
  elseif(WIN32 OR CYGWIN OR MINGW)
    set(PJ_WIN32 1)
    set(PJ_WIN32_WINNT 0x0501)
    set(WIN32_LEAN_AND_MEAN 1)
    set(NOMINMAX 1)

    if(CMAKE_SYSTEM_NAME MATCHES "_64-w64-mingw"
      OR CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "AMD64")
      set(PJ_WIN64 1)
    endif()
  elseif(APPLE)
    set(PJ_DARWINOS 1)
  elseif(LINUX)
    set(PJ_LINUX 1)
  elseif(BSD)
    set(PJ_BSD 1)
  else()
    # manual system name checks
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _system_name_lower)
    if(PJ_OS_NAME MATCHES "rtems")
      set(PJ_RTEMS 1)
    elseif(PJ_OS_NAME STREQUAL "sunos|solaris")
      set(PJ_SUNOS 1)
    endif()
  endif()

  set(PJ_HAS_ERRNO_VAR 1)
  set(PJ_HAS_HIGH_RES_TIMER 1)
  set(PJ_HAS_MALLOC 1)
  set(PJ_NATIVE_STRING_IS_UNICODE 0)
  set(PJ_ATOMIC_VALUE_TYPE "long")

  #
  # headers
  #

  check_include_file(arpa/inet.h PJ_HAS_ARPA_INET_H)
  check_include_file(assert.h PJ_HAS_ASSERT_H)
  check_include_file(ctype.h PJ_HAS_CTYPE_H)

  if(WIN32 OR CYGWIN OR MINGW)
    set(PJ_HAS_ERRNO_H 0)
  else()
    check_include_file(errno.h PJ_HAS_ERRNO_H)
  endif()

  check_include_file(fcntl.h PJ_HAS_FCNTL_H)
  check_include_file(ifaddrs.h PJ_HAS_IFADDRS_H)
  check_include_file(limits.h PJ_HAS_LIMITS_H)
  check_include_file(linux/socket.h PJ_HAS_LINUX_SOCKET_H)
  check_include_file(malloc.h PJ_HAS_MALLOC_H)
  check_include_file(netdb.h PJ_HAS_NETDB_H)
  check_include_file(netinet/in_systm.h PJ_HAS_NETINET_IN_SYSTM_H)
  check_include_file(netinet/in.h PJ_HAS_NETINET_IN_H)
  check_include_file(netinet/ip.h PJ_HAS_NETINET_IP_H)
  check_include_file(netinet/tcp.h PJ_HAS_NETINET_TCP_H)
  check_include_file(net/if.h PJ_HAS_NET_IF_H)
  check_include_file(semaphore.h PJ_HAS_SEMAPHORE_H)
  check_include_file(setjmp.h PJ_HAS_SETJMP_H)
  check_include_file(stdarg.h PJ_HAS_STDARG_H)
  check_include_file(stddef.h PJ_HAS_STDDEF_H)
  check_include_file(stdio.h PJ_HAS_STDIO_H)
  check_include_file(stdint.h PJ_HAS_STDINT_H)
  check_include_file(stdlib.h PJ_HAS_STDLIB_H)
  check_include_file(string.h PJ_HAS_STRING_H)
  check_include_file(inttypes.h PJ_HAS_INTTYPES_H)
  check_include_file(sys/ioctl.h PJ_HAS_SYS_IOCTL_H)
  check_include_file(sys/select.h PJ_HAS_SYS_SELECT_H)
  check_include_file(sys/socket.h PJ_HAS_SYS_SOCKET_H)
  check_include_file(sys/time.h PJ_HAS_SYS_TIME_H)
  check_include_file(sys/timeb.h PJ_HAS_SYS_TIMEB_H)
  check_include_file(sys/types.h PJ_HAS_SYS_TYPES_H)
  check_include_file(sys/filio.h PJ_HAS_SYS_FILIO_H)
  check_include_file(sys/sockio.h PJ_HAS_SYS_SOCKIO_H)
  check_include_file(sys/utsname.h PJ_HAS_SYS_UTSNAME_H)
  check_include_file(time.h PJ_HAS_TIME_H)
  check_include_file(unistd.h PJ_HAS_UNISTD_H)
  check_include_file(execinfo.h PJ_HAS_EXECINFO_H)

  if(ANDROID)
    set(PJ_HAS_ANDROID_ALARM_H 1)
  endif()

  #
  # functions
  #

  include(CheckSymbolExists)
  check_symbol_exists(localtime_r time.h PJ_HAS_LOCALTIME_R)
  check_symbol_exists(getifaddrs ifaddrs.h PJ_HAS_IFADDRS_H)
  check_symbol_exists(inet_aton arpa/inet.h PJ_SOCK_HAS_INET_ATON)
  check_symbol_exists(inet_pton arpa/inet.h PJ_SOCK_HAS_INET_PTON)
  check_symbol_exists(inet_ntop arpa/inet.h PJ_SOCK_HAS_INET_NTOP)
  check_symbol_exists(getaddrinfo netdb.h PJ_SOCK_HAS_GETADDRINFO)
  check_symbol_exists(socketpair sys/socket.h PJ_SOCK_HAS_SOCKETPAIR)

  #
  # compilation checks
  #

  include(CheckSourceCompiles)

  # determine if sockaddr_in has sin_len member
  check_source_compiles(C [=[
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>

    int main(void) {
        struct sockaddr_in a;
        a.sin_len=0;
        return 0;
    }
  ]=] PJ_SOCKADDR_HAS_LEN)

  # determine if socklen_t is available
  check_source_compiles(C [=[
    #include <sys/types.h>
    #include <sys/socket.h>

    int main(void) {
        socklen_t xxx = 0;
        return 0;
    }
  ]=] PJ_HAS_SOCKLEN_T)

  # determine if IPV6_V6ONLY is available
  check_source_compiles(C [=[
    #include <sys/socket.h>
    #include <netinet/in.h>

    int main(void) {
        int opt = IPV6_V6ONLY;
        return 0;
    }
  ]=] PJ_SOCK_HAS_IPV6_V6ONLY)

  # determine if SO_ERROR is available
  if(WIN32 OR CYGWIN OR MINGW)
    set(PJ_HAS_SO_ERROR 1)
  else()
    check_source_compiles(C [=[
      #include <sys/types.h>
      #include <sys/socket.h>
      #include <netinet/in.h>
      #include <arpa/inet.h>

      int main(void) {
          int i=SO_ERROR;
          return 0;
      }
    ]=] PJ_HAS_SO_ERROR)
  endif()

  #
  # pthread capabilities
  #

  if(UNIX)
    check_include_file(pthread.h PJ_HAS_PTHREAD_H)
    check_include_file(pthread_np.h PJ_HAS_PTHREAD_NP_H)
  endif()

  if(PJ_HAS_PTHREAD_H)
    # determine if RW-mutex is available
    check_source_compiles(C [=[
      #include <pthread.h>

      int main(void) {
          pthread_rwlock_t *x;
          return 0;
      }
    ]=] _has_pthread_rwlock)

    # if rwmutex is not detected, check again but this time with
    # _POSIX_READER_WRITER_LOCKS defined (newlib needs this)
    if(NOT _has_pthread_rwlock)
      check_source_compiles(C [=[
        #include <pthread.h>

        int main(void) {
            pthread_rwlock_t *x;
            return 0;
        }
      ]=] _has_pthread_rwlock)
    endif()

    if(_has_pthread_rwlock)
      set(PJ_EMULATE_RWMUTEX 0)
    else()
      set(PJ_EMULATE_RWMUTEX 1)
    endif()

    unset(_has_pthread_rwlock)

    # do we have pthread_mutexattr_settype()?
    check_source_compiles(C [=[
      #include <pthread.h>

      int main(void) {
          pthread_mutexattr_settype(0, PTHREAD_MUTEX_FAST_NP);
          return 0;
      }
    ]=] PJ_HAS_PTHREAD_MUTEXATTR_SETTYPE)

    # does pthread_mutexattr_t has "recursive" member?
    check_source_compiles(C [=[
      #include <pthread.h>

      int main(void) {
          pthread_mutexattr_t attr;
          attr.recursive=1;
          return 0;
      }
    ]=] PJ_PTHREAD_MUTEXATTR_T_HAS_RECURSIVE)
  endif()


  if(PJ_HAS_PTHREAD_NP_H)
    check_symbol_exists(pthread_setname_np pthread_np.h
      PJ_HAS_PTHREAD_SETNAME_NP
    )
    check_symbol_exists(pthread_set_name_np pthread_np.h
      PJ_HAS_PTHREAD_SET_NAME_NP
    )
  endif()

  #
  # manual checks
  #

  # Determine if select() requires nfds to be filled up with
  # correct value (max_fd+1). If zero, nfds will be filled up with
  # PJ_FD_SETSIZE
  if(PJ_RTEMS)
    set(PJ_SELECT_NEEDS_NFDS 1)
  else()
    set(PJ_SELECT_NEEDS_NFDS 0)
  endif()

  # Determine if pj_thread_create() should enforce thread stack size when
  # creating thread. Default is zero, to allow OS to allocate appropriate
  # thread's stack size.
  if(PJ_RTEMS)
    set(PJ_THREAD_SET_STACK_SIZE 1)
  else()
    set(PJ_THREAD_SET_STACK_SIZE 0)
  endif()

  # Determine if pj_thread_create() should allocate thread's stack from
  # the pool. Default is zero, to let OS allocate thread's stack.
  if(PJ_RTEMS)
    set(PJ_THREAD_ALLOCATE_STACK 1)
  else()
    set(PJ_THREAD_ALLOCATE_STACK 0)
  endif()

  # This value specifies the value set in errno by the OS when a non-blocking
  # socket recv() can not return immediate data.
  if(WIN32 OR CYGWIN OR MINGW)
    set(PJ_BLOCKING_ERROR_VAL WSAEWOULDBLOCK)
  else()
    set(PJ_BLOCKING_ERROR_VAL EAGAIN)
  endif()

  # This value specifies the value set in errno by the OS when a non-blocking
  # socket connect() can not get connected immediately.
  if(WIN32 OR CYGWIN OR MINGW)
    set(PJ_BLOCKING_CONNECT_ERROR_VAL WSAEWOULDBLOCK)
  else()
    set(PJ_BLOCKING_CONNECT_ERROR_VAL EINPROGRESS)
  endif()

  #
  # configuration checks
  #

  # i/o queue implementation
  if(NOT PJLIB_WITH_IOQUEUE STREQUAL "")
    string(TOUPPER "PJ_IOQUEUE_IMP_${PJLIB_WITH_IOQUEUE}" PJ_IOQUEUE_IMP)
  else()
    set(PJ_IOQUEUE_IMP "PJ_IOQUEUE_IMP_NONE")
  endif()

  # ssl
  if(NOT PJLIB_WITH_SSL STREQUAL "")
    set(PJ_HAS_SSL_SOCK 1)
    string(TOUPPER "PJ_SSL_SOCK_IMP_${PJLIB_WITH_SSL}" PJ_SSL_SOCK_IMP)
  else()
    set(PJ_HAS_SSL_SOCK 0)
    set(PJ_HAS_SSL_SOCK_IMP "PJ_SSL_SOCK_IMP_NONE")
  endif()

  # write configured header
  configure_file(
    include/pj/compat/os_auto.h.cm
    include/pj/compat/os_auto.h
    @ONLY
  )
endblock()

# generate a dummy `pj/config_site.h` if it does not exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/pj/config_site.h")
  file(TOUCH "${CMAKE_CURRENT_SOURCE_DIR}/include/pj/config_site.h")
endif()

# add generated files includes
target_sources(pjlib
  PUBLIC
    FILE_SET config_headers TYPE HEADERS
      BASE_DIRS
        "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
      FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/pj/config_site.h"
        "${CMAKE_CURRENT_BINARY_DIR}/include/pj/compat/m_auto.h"
        "${CMAKE_CURRENT_BINARY_DIR}/include/pj/compat/os_auto.h"
)

# ##############################################################################
# pjlib test application

if(BUILD_TESTING)
  add_executable(pjlib-test
    src/pjlib-test/activesock.c
    src/pjlib-test/atomic.c
    src/pjlib-test/atomic_slist.c
    src/pjlib-test/echo_clt.c
    src/pjlib-test/errno.c
    src/pjlib-test/exception.c
    src/pjlib-test/fifobuf.c
    src/pjlib-test/file.c
    src/pjlib-test/hash_test.c
    src/pjlib-test/ioq_perf.c
    src/pjlib-test/ioq_udp.c
    src/pjlib-test/ioq_stress_test.c
    src/pjlib-test/ioq_unreg.c
    src/pjlib-test/ioq_tcp.c
    src/pjlib-test/ioq_iocp_unreg_test.c
    src/pjlib-test/list.c
    src/pjlib-test/mutex.c
    src/pjlib-test/os.c
    src/pjlib-test/pool.c
    src/pjlib-test/pool_perf.c
    src/pjlib-test/rand.c
    src/pjlib-test/rbtree.c
    src/pjlib-test/select.c
    src/pjlib-test/sleep.c
    src/pjlib-test/sock.c
    src/pjlib-test/sock_perf.c
    src/pjlib-test/ssl_sock.c
    src/pjlib-test/string.c
    src/pjlib-test/test.c
    src/pjlib-test/thread.c
    src/pjlib-test/timer.c
    src/pjlib-test/timestamp.c
    src/pjlib-test/udp_echo_srv_sync.c
    src/pjlib-test/udp_echo_srv_ioqueue.c
    src/pjlib-test/unittest_test.c
    src/pjlib-test/util.c
    src/pjlib-test/${PJ_TEST_MAIN}
  )

  target_link_libraries(pjlib-test PRIVATE pjlib pjlib-ssl)

  add_test(NAME pjlib-test COMMAND pjlib-test)
endif()

# ##############################################################################
# pjlib++

add_library(pjlib++ INTERFACE)

# headers
target_sources(pjlib++
  INTERFACE
    FILE_SET HEADERS
      BASE_DIRS
        include/
      FILES
        include/pjlib++.hpp
        include/pj++/file.hpp
        include/pj++/hash.hpp
        include/pj++/list.hpp
        include/pj++/lock.hpp
        include/pj++/os.hpp
        include/pj++/pool.hpp
        include/pj++/proactor.hpp
        include/pj++/scanner.hpp
        include/pj++/sock.hpp
        include/pj++/string.hpp
        include/pj++/timer.hpp
        include/pj++/tree.hpp
        include/pj++/types.hpp
)

# dependencies
target_link_libraries(pjlib++ INTERFACE pjlib)

# ##############################################################################
# pjlib++ test application

if(BUILD_TESTING AND FALSE) # does not compile
  add_executable(pjlib++-test
    src/pjlib++-test/main.cpp
  )

  target_link_libraries(pjlib++-test PRIVATE pjlib++)

  add_test(NAME pjlib++-test COMMAND pjlib++-test)
endif()
