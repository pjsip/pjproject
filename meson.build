project('pjproject', ['c', 'cpp', 'objc'], version: '2.13.1')

# Use compiler
compiler = meson.get_compiler('c')

# Detect compile OS system: 'darwin', ...
os = build_machine.system()

# [+] - config.status: creating pjlib/include/pj/compat/os_auto.h
# [+] - config.status: creating pjlib/include/pj/compat/m_auto.h
# [_] - config.status: creating pjmedia/include/pjmedia/config_auto.h
# [-] - config.status: creating pjmedia/include/pjmedia-codec/config_auto.h
# [-] - config.status: creating pjsip/include/pjsip/sip_autoconf.h

# pjlib - os_auto.h
os_auto = configuration_data()
os_auto.set_quoted('PJ_OS_NAME', os)
if os == 'darwin'
    os_auto.set('PJ_DARWINOS', 1)
elif os == 'linux'
    os_auto.set('PJ_LINUX', 1)
endif
if compiler.has_header('arpa/inet.h')
    os_auto.set('PJ_HAS_ARPA_INET_H', 1)
endif
if compiler.has_header('assert.h')
  os_auto.set('PJ_HAS_ASSERT_H', 1)
endif
os_auto.set('PJ_HAS_CTYPE_H', 1)
if compiler.has_header('errno.h')
    os_auto.set('PJ_HAS_ERRNO_H', 1)
endif
os_auto.set('PJ_HAS_FCNTL_H', 1)
if compiler.has_header('limits.h')
  os_auto.set('PJ_HAS_LIMITS_H', 1)
endif
#undef PJ_HAS_LINUX_SOCKET_H */
if compiler.has_header('malloc.h')
  os_auto.set('PJ_HAS_MALLOC_H', 1)
endif
os_auto.set('PJ_HAS_NETDB_H', 1)
os_auto.set('PJ_HAS_NETINET_IN_SYSTM_H', 1)
os_auto.set('PJ_HAS_NETINET_IN_H', 1)
os_auto.set('PJ_HAS_NETINET_IP_H', 1)
if compiler.has_header('netinet/tcp.h')
  os_auto.set('PJ_HAS_NETINET_TCP_H', 1)
endif
if compiler.has_header('net_if.h')
  os_auto.set('PJ_HAS_NET_IF_H', 1)
endif
os_auto.set('PJ_HAS_IFADDRS_H', 1)
os_auto.set('PJ_HAS_INTTYPES_H', 1)
if compiler.has_header('semaphore.h')
  os_auto.set('PJ_HAS_SEMAPHORE_H', 1)
  os_auto.set('PJ_HAS_SEMAPHORE', 1) # TODO - on win32 always is on...
endif
if compiler.has_header('setjmp.h')
  os_auto.set('PJ_HAS_SETJMP_H', 1)
endif
if compiler.has_header('stdarg.h')
  os_auto.set('PJ_HAS_STDARG_H', 1)
endif
if compiler.has_header('stddef.h')
  os_auto.set('PJ_HAS_STDDEF_H', 1)
endif
if compiler.has_header('stdio.h')
  os_auto.set('PJ_HAS_STDIO_H', 1)
endif
if compiler.has_header('stdint.h')
  os_auto.set('PJ_HAS_STDINT_H', 1)
endif
if compiler.has_header('stdlib.h')
  os_auto.set('PJ_HAS_STDLIB_H', 1)
endif
if compiler.has_header('string.h')
  os_auto.set('PJ_HAS_STRING_H', 1)
endif
if compiler.has_header('sys/ioctl.h')
  os_auto.set('PJ_HAS_SYS_IOCTL_H', 1)
endif
if compiler.has_header('sys/select.h')
  os_auto.set('PJ_HAS_SYS_SELECT_H', 1)
endif
if compiler.has_header('sys/socket.h')
  os_auto.set('PJ_HAS_SYS_SOCKET_H', 1)
endif
if compiler.has_header('sys/time.h')
  os_auto.set('PJ_HAS_SYS_TIME_H', 1)
endif
if compiler.has_header('sys/timeb.h')
  os_auto.set('PJ_HAS_SYS_TIMEB_H', 1)
endif
#define PJ_HAS_SYS_TYPES_H 1
#define PJ_HAS_SYS_FILIO_H 1
#define PJ_HAS_SYS_SOCKIO_H 1
#define PJ_HAS_SYS_UTSNAME_H 1
if compiler.has_header('time.h')
  os_auto.set('PJ_HAS_TIME_H', 1)
endif
if compiler.has_header('unistd.h')
  os_auto.set('PJ_HAS_UNISTD_H', 1)
endif
if compiler.has_header('execinfo.h')
  os_auto.set('PJ_HAS_EXECINFO_H', 1)
endif
#undef PJ_HAS_MSWSOCK_H */
#undef PJ_HAS_WINSOCK_H */
#undef PJ_HAS_WINSOCK2_H */
#undef PJ_HAS_WS2TCPIP_H */
#define PJ_SOCK_HAS_IPV6_V6ONLY 1
#define PJ_SOCK_HAS_INET_ATON 1
#define PJ_SOCK_HAS_INET_PTON 1
#define PJ_SOCK_HAS_INET_NTOP 1
#define PJ_SOCK_HAS_GETADDRINFO 1
#define PJ_SOCK_HAS_SOCKETPAIR 1
#undef PJ_HAS_PTHREAD_MUTEXATTR_SETTYPE */
#undef PJ_PTHREAD_MUTEXATTR_T_HAS_RECURSIVE */
#define PJ_SOCKADDR_HAS_LEN 1
os_auto.set('PJ_HAS_SOCKLEN_T', 1)
#define PJ_SELECT_NEEDS_NFDS 0
#undef PJ_HAS_LINUX_EPOLL */
os_auto.set('PJ_HAS_ERRNO_VAR', 1)
os_auto.set('PJ_HAS_SO_ERROR', 1)
os_auto.set('PJ_BLOCKING_ERROR_VAL', 'EAGAIN')
os_auto.set('PJ_BLOCKING_CONNECT_ERROR_VAL', 'EINPROGRESS')
os_auto.set('PJ_HAS_THREADS', 1)
os_auto.set('PJ_HAS_HIGH_RES_TIMER', 1)
#define PJ_HAS_MALLOC 1
#   define PJ_OS_HAS_CHECK_STACK    0
#define PJ_HAS_LOCALTIME_R 1
#define PJ_NATIVE_STRING_IS_UNICODE 0
#define PJ_POOL_ALIGNMENT 8
os_auto.set('PJ_ATOMIC_VALUE_TYPE', 'long')
if os == 'darwin'
  os_auto.set('PJ_GETHOSTIP_DISABLE_LOCAL_RESOLUTION', 1)
  os_auto.set('PJ_SOCKADDR_USE_GETADDRINFO', 1)
endif
os_auto.set('PJ_EMULATE_RWMUTEX', 0)
os_auto.set('PJ_THREAD_SET_STACK_SIZE', 0)
os_auto.set('PJ_THREAD_ALLOCATE_STACK', 0)
os_auto.set('PJ_HAS_SSL_SOCK', 1)
os_auto.set('PJ_SSL_SOCK_IMP', 'PJ_SSL_SOCK_IMP_OPENSSL')
#undef PJ_HAS_PTHREAD_NP_H */
#define PJ_HAS_PTHREAD_SETNAME_NP 1
#undef PJ_HAS_PTHREAD_SET_NAME_NP */
#
configure_file(output : 'os_auto.h', configuration : os_auto)
run_command('/bin/cp', 'builddir/os_auto.h', 'pjlib/include/pj/compat/os_auto.h', check : true)

# pjlib - m_auto.h
m_auto = configuration_data()
m_auto.set_quoted('PJ_M_NAME', 'custom')
m_auto.set('PJ_HAS_FLOATING_POINT', 1)
m_auto.set('PJ_IS_BIG_ENDIAN', 0)
m_auto.set('PJ_IS_LITTLE_ENDIAN', 1)
m_auto.set('PJ_HAS_PENTIUM', 0) # TODO - ask developer about remove parameter...
configure_file(output : 'm_auto.h', configuration : m_auto)
run_command('/bin/cp', 'builddir/m_auto.h', 'pjlib/include/pj/compat/m_auto.h', check : true)

# pjmedia - config_auto.h

config_auto = configuration_data()
configure_file(output : 'pjmedia_config_auto.h', configuration : config_auto)
run_command('/bin/cp', 'builddir/pjmedia_config_auto.h', 'pjmedia/include/pjmedia/config_auto.h', check : true)

# pjmedia-codec - config_auto.h

config_auto2 = configuration_data()
configure_file(output : 'pjmedia_codec_config_auto2.h', configuration : config_auto2)
run_command('/bin/cp', 'builddir/pjmedia_codec_config_auto2.h', 'pjmedia/include/pjmedia-codec/config_auto.h', check : true)

# pjsip - sip_autoconf.h

sip_autoconf = configuration_data()
configure_file(output : 'pjsip_sip_autoconf.h', configuration : sip_autoconf)
run_command('/bin/cp', 'builddir/pjsip_sip_autoconf.h', 'pjsip/include/pjsip/sip_autoconf.h', check : true)

#

add_global_arguments('-fstack-protector-strong', language : 'c')
add_global_arguments('-Wformat', language : 'c')
add_global_arguments('-Werror=format-security', language : 'c')
add_global_arguments('-ggdb3', language : 'c')

add_global_arguments('-DPJ_AUTOCONF=1', language : ['c', 'cpp', 'objc'])

add_global_arguments('-DPJ_SOCK_HAS_GETADDRINFO=1', language : ['c', 'cpp'])
add_global_arguments('-DPJ_HAS_IPV6=0', language : ['c', 'cpp'])
add_global_arguments('-DPJ_SOCK_HAS_INET_PTON=0', language : ['c', 'cpp'])
add_global_arguments('-DPJ_GETHOSTIP_DISABLE_LOCAL_RESOLUTION=1', language : ['c', 'cpp'])

add_global_arguments('-DPJMEDIA_HAS_WEBRTC_AEC=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_WEBRTC_AEC3=0', language : ['c', 'cpp'])

add_global_arguments('-DPJMEDIA_AUDIO_DEV_HAS_WMME=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_AUDIO_DEV_HAS_WMME=0', language : ['c', 'cpp'])

add_global_arguments('-DPJMEDIA_HAS_LIBAVFORMAT=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_LIBAVCODEC=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_LIBSWSCALE=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_LIBAVUTIL=0', language : ['c', 'cpp'])

add_global_arguments('-DPJMEDIA_HAS_ILBC_CODEC=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_GSM_CODEC=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_L16_CODEC=0', language : ['c', 'cpp'])
add_global_arguments('-DPJMEDIA_HAS_G722_CODEC=0', language : ['c', 'cpp'])

system_dep = []

openssl_dep = dependency('openssl')

thread_dep = dependency('threads', required : true)

uuid_dep = dependency('uuid', required : true)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

srtp = get_option('srtp')
if srtp == 'system'
  srtp2_dep = dependency('libsrtp2', required : true)
  add_global_arguments('-DPJMEDIA_HAS_SRTP=1', language : ['c', 'cpp'])
  add_global_arguments('-DPJMEDIA_EXTERNAL_SRTP=2', language : ['c', 'cpp'])
elif srtp == 'external'
  # TODO - use https://github.com/cisco/libsrtp/blob/main/meson.build
elif srtp == 'bundled'
  # TODO - last chance use bundled version... 
endif

speex = get_option('speex')
if speex == 'system'
  speex_dep = dependency('speex', required : true)
  add_global_arguments('-DPJMEDIA_HAS_SPEEX_AEC=0', language : ['c', 'cpp'])
  add_global_arguments('-DPJMEDIA_HAS_SPEEX_CODEC=0', language : ['c', 'cpp'])
elif speex == 'external'
  # TODO - use https://gitlab.xiph.org/xiph/speex/-/blob/master/meson.build
elif speex == 'bundled'
  # TODO - last chance use bundled version...
endif

add_global_arguments('-DPJMEDIA_RESAMPLE_IMP=PJMEDIA_RESAMPLE_LIBRESAMPLE', language : ['c', 'cpp'])

add_global_arguments('-DRESAMPLE_HAS_SMALL_FILTER=1', language : ['c', 'cpp'])
add_global_arguments('-DRESAMPLE_HAS_LARGE_FILTER=1', language : ['c', 'cpp'])

# RESAMPLE

resample_inc = include_directories('third_party/resample/include')

configure_file(output : 'config.h',
  configuration : {
  }
)
resample = library('resample', [
	'third_party/resample/src/resamplesubs.c',
], include_directories : [
	resample_inc,
])
resample_dep = declare_dependency(link_with : [
	resample,
], include_directories : [
	resample_inc,
])

# PJLIB

pjlib_inc = include_directories('pjlib/include')
if os == 'linux'
    pjlib_sources = [
	'pjlib/src/pj/ioqueue_select.c',
	'pjlib/src/pj/file_access_unistd.c',
	'pjlib/src/pj/file_io_ansi.c',
	'pjlib/src/pj/os_error_unix.c',
	'pjlib/src/pj/os_time_unix.c',
	'pjlib/src/pj/os_timestamp_posix.c',
	'pjlib/src/pj/sock_qos_bsd.c',
	'pjlib/src/pj/guid_uuid.c',
	'pjlib/src/pj/addr_resolv_sock.c',
	'pjlib/src/pj/log_writer_stdout.c',
	'pjlib/src/pj/os_timestamp_common.c',
	'pjlib/src/pj/pool_policy_malloc.c',
	'pjlib/src/pj/sock_bsd.c',
	'pjlib/src/pj/sock_select.c',
	'pjlib/src/pj/activesock.c',
	'pjlib/src/pj/array.c',
	'pjlib/src/pj/config.c',
	'pjlib/src/pj/ctype.c',
	'pjlib/src/pj/errno.c',
	'pjlib/src/pj/except.c',
	'pjlib/src/pj/fifobuf.c',
	'pjlib/src/pj/guid.c',
	'pjlib/src/pj/hash.c',
	'pjlib/src/pj/ip_helper_generic.c',
	'pjlib/src/pj/list.c',
	'pjlib/src/pj/lock.c',
	'pjlib/src/pj/log.c',
	'pjlib/src/pj/os_time_common.c',
	'pjlib/src/pj/os_info.c',
	'pjlib/src/pj/pool.c',
	'pjlib/src/pj/pool_buf.c',
	'pjlib/src/pj/pool_caching.c',
	'pjlib/src/pj/pool_dbg.c',
	'pjlib/src/pj/rand.c',
	'pjlib/src/pj/rbtree.c',
	'pjlib/src/pj/sock_common.c',
	'pjlib/src/pj/sock_qos_common.c',
	'pjlib/src/pj/ssl_sock_common.c',
	'pjlib/src/pj/ssl_sock_ossl.c',
	'pjlib/src/pj/ssl_sock_gtls.c',
	'pjlib/src/pj/ssl_sock_dump.c',
	'pjlib/src/pj/ssl_sock_darwin.c',
	'pjlib/src/pj/string.c',
	'pjlib/src/pj/timer.c',
	'pjlib/src/pj/types.c',
	'pjlib/src/pj/os_core_unix.c',
    ]
elif os == 'darwin'
    pjlib_sources = [
	'pjlib/src/pj/ioqueue_select.c',
	'pjlib/src/pj/file_access_unistd.c',
	'pjlib/src/pj/file_io_ansi.c',
	'pjlib/src/pj/os_core_unix.c',
	'pjlib/src/pj/os_error_unix.c',
	'pjlib/src/pj/os_time_unix.c',
	'pjlib/src/pj/os_timestamp_posix.c',
	'pjlib/src/pj/os_core_darwin.m',
	'pjlib/src/pj/sock_qos_darwin.c',
	'pjlib/src/pj/sock_qos_bsd.c',
	'pjlib/src/pj/ssl_sock_apple.m',
	'pjlib/src/pj/guid_darwin.c',
	'pjlib/src/pj/addr_resolv_sock.c',
	'pjlib/src/pj/log_writer_stdout.c',
	'pjlib/src/pj/os_timestamp_common.c',
	'pjlib/src/pj/pool_policy_malloc.c',
	'pjlib/src/pj/sock_bsd.c',
	'pjlib/src/pj/sock_select.c',
	'pjlib/src/pj/activesock.c',
	'pjlib/src/pj/array.c',
	'pjlib/src/pj/config.c',
	'pjlib/src/pj/ctype.c',
	'pjlib/src/pj/errno.c',
	'pjlib/src/pj/except.c',
	'pjlib/src/pj/fifobuf.c',
	'pjlib/src/pj/guid.c',
	'pjlib/src/pj/hash.c',
	'pjlib/src/pj/ip_helper_generic.c',
	'pjlib/src/pj/list.c',
	'pjlib/src/pj/lock.c',
	'pjlib/src/pj/log.c',
	'pjlib/src/pj/os_time_common.c',
	'pjlib/src/pj/os_info.c',
	'pjlib/src/pj/pool.c',
	'pjlib/src/pj/pool_buf.c',
	'pjlib/src/pj/pool_caching.c',
	'pjlib/src/pj/pool_dbg.c',
	'pjlib/src/pj/rand.c',
	'pjlib/src/pj/rbtree.c',
	'pjlib/src/pj/sock_common.c',
	'pjlib/src/pj/sock_qos_common.c',
	'pjlib/src/pj/ssl_sock_common.c',
	'pjlib/src/pj/ssl_sock_ossl.c',
	'pjlib/src/pj/ssl_sock_gtls.c',
	'pjlib/src/pj/ssl_sock_dump.c',
	'pjlib/src/pj/ssl_sock_darwin.c',
	'pjlib/src/pj/string.c',
	'pjlib/src/pj/timer.c',
	'pjlib/src/pj/types.c',
    ]
    system_dep += dependency('CoreFoundation')
    system_dep += dependency('AppKit')
    
endif
pjlib = library('pjlib', pjlib_sources, include_directories: [
	pjlib_inc,
], dependencies: [
	system_dep,
	openssl_dep,
	thread_dep,
	uuid_dep,
	m_dep,
])

if os == 'linux'
    pjlib_test_sources = [
	'pjlib/src/pjlib-test/main.c',
	'pjlib/src/pjlib-test/activesock.c',
	'pjlib/src/pjlib-test/atomic.c',
	'pjlib/src/pjlib-test/echo_clt.c',
	'pjlib/src/pjlib-test/errno.c',
	'pjlib/src/pjlib-test/exception.c',
	'pjlib/src/pjlib-test/fifobuf.c',
	'pjlib/src/pjlib-test/file.c',
	'pjlib/src/pjlib-test/hash_test.c',
	'pjlib/src/pjlib-test/ioq_perf.c',
	'pjlib/src/pjlib-test/ioq_udp.c',
	'pjlib/src/pjlib-test/ioq_stress_test.c',
	'pjlib/src/pjlib-test/ioq_unreg.c',
	'pjlib/src/pjlib-test/ioq_tcp.c',
	'pjlib/src/pjlib-test/list.c',
	'pjlib/src/pjlib-test/mutex.c',
	'pjlib/src/pjlib-test/os.c',
	'pjlib/src/pjlib-test/pool.c',
	'pjlib/src/pjlib-test/pool_perf.c',
	'pjlib/src/pjlib-test/rand.c',
	'pjlib/src/pjlib-test/rbtree.c',
	'pjlib/src/pjlib-test/select.c',
	'pjlib/src/pjlib-test/sleep.c',
	'pjlib/src/pjlib-test/sock.c',
	'pjlib/src/pjlib-test/sock_perf.c',
	'pjlib/src/pjlib-test/ssl_sock.c',
	'pjlib/src/pjlib-test/string.c',
	'pjlib/src/pjlib-test/test.c',
	'pjlib/src/pjlib-test/thread.c',
	'pjlib/src/pjlib-test/timer.c',
	'pjlib/src/pjlib-test/timestamp.c',
	'pjlib/src/pjlib-test/udp_echo_srv_sync.c',
	'pjlib/src/pjlib-test/udp_echo_srv_ioqueue.c',
	'pjlib/src/pjlib-test/util.c',
    ]
elif os == 'darwin'
    pjlib_test_sources = [
	'pjlib/src/pjlib-test/main.c',
	'pjlib/src/pjlib-test/activesock.c',
	'pjlib/src/pjlib-test/atomic.c',
	'pjlib/src/pjlib-test/echo_clt.c',
	'pjlib/src/pjlib-test/errno.c',
	'pjlib/src/pjlib-test/exception.c',
	'pjlib/src/pjlib-test/fifobuf.c',
	'pjlib/src/pjlib-test/file.c',
	'pjlib/src/pjlib-test/hash_test.c',
	'pjlib/src/pjlib-test/ioq_perf.c',
	'pjlib/src/pjlib-test/ioq_udp.c',
	'pjlib/src/pjlib-test/ioq_stress_test.c',
	'pjlib/src/pjlib-test/ioq_unreg.c',
	'pjlib/src/pjlib-test/ioq_tcp.c',
	'pjlib/src/pjlib-test/list.c',
	'pjlib/src/pjlib-test/mutex.c',
	'pjlib/src/pjlib-test/os.c',
	'pjlib/src/pjlib-test/pool.c',
	'pjlib/src/pjlib-test/pool_perf.c',
	'pjlib/src/pjlib-test/rand.c',
	'pjlib/src/pjlib-test/rbtree.c',
	'pjlib/src/pjlib-test/select.c',
	'pjlib/src/pjlib-test/sleep.c',
	'pjlib/src/pjlib-test/sock.c',
	'pjlib/src/pjlib-test/sock_perf.c',
	'pjlib/src/pjlib-test/ssl_sock.c',
	'pjlib/src/pjlib-test/string.c',
	'pjlib/src/pjlib-test/test.c',
	'pjlib/src/pjlib-test/thread.c',
	'pjlib/src/pjlib-test/timer.c',
	'pjlib/src/pjlib-test/timestamp.c',
	'pjlib/src/pjlib-test/udp_echo_srv_sync.c',
	'pjlib/src/pjlib-test/udp_echo_srv_ioqueue.c',
	'pjlib/src/pjlib-test/util.c',
    ]
endif
test('PJLIB Test', executable('pjlib-test', pjlib_test_sources,
    include_directories: [
	pjlib_inc,
    ], link_with: [
	pjlib,
    ]),
    timeout: 0,
    workdir: join_paths(meson.current_source_dir(), 'pjmedia/build')
)

# PJLIB_UTIL

pjlib_util_inc = include_directories('pjlib-util/include')

pjlib_util = library('pjlib-util', [
	'pjlib-util/src/pjlib-util/base64.c',
	'pjlib-util/src/pjlib-util/cli.c',
	'pjlib-util/src/pjlib-util/cli_console.c',
	'pjlib-util/src/pjlib-util/cli_telnet.c',
	'pjlib-util/src/pjlib-util/crc32.c',
	'pjlib-util/src/pjlib-util/errno.c',
	'pjlib-util/src/pjlib-util/dns.c',
	'pjlib-util/src/pjlib-util/dns_dump.c',
	'pjlib-util/src/pjlib-util/dns_server.c',
	'pjlib-util/src/pjlib-util/getopt.c',
	'pjlib-util/src/pjlib-util/hmac_md5.c',
	'pjlib-util/src/pjlib-util/hmac_sha1.c',
	'pjlib-util/src/pjlib-util/http_client.c',
	'pjlib-util/src/pjlib-util/json.c',
	'pjlib-util/src/pjlib-util/md5.c',
	'pjlib-util/src/pjlib-util/pcap.c',
	'pjlib-util/src/pjlib-util/resolver.c',
	'pjlib-util/src/pjlib-util/scanner.c',
	'pjlib-util/src/pjlib-util/sha1.c',
	'pjlib-util/src/pjlib-util/srv_resolver.c',
	'pjlib-util/src/pjlib-util/string.c',
	'pjlib-util/src/pjlib-util/stun_simple.c',
	'pjlib-util/src/pjlib-util/stun_simple_client.c',
	'pjlib-util/src/pjlib-util/xml.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
], link_with: [
	pjlib,
])

test('Util test', executable('pjlib-util-test', [
	'pjlib-util/src/pjlib-util-test/main.c',
	'pjlib-util/src/pjlib-util-test/xml.c',
	'pjlib-util/src/pjlib-util-test/encryption.c',
	'pjlib-util/src/pjlib-util-test/stun.c',
	'pjlib-util/src/pjlib-util-test/resolver_test.c',
	'pjlib-util/src/pjlib-util-test/test.c',
	'pjlib-util/src/pjlib-util-test/json_test.c',
	'pjlib-util/src/pjlib-util-test/http_client.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
], link_with: [
	pjlib,
	pjlib_util,
]), timeout: 0, workdir: join_paths(meson.current_source_dir(), 'pjlib-util/build') )

# PJNATH

pjnath_inc = include_directories('pjnath/include')

pjnath = library('pjnath', [
	'pjnath/src/pjnath/errno.c',
	'pjnath/src/pjnath/ice_session.c',
	'pjnath/src/pjnath/ice_strans.c',
	'pjnath/src/pjnath/nat_detect.c',
	'pjnath/src/pjnath/stun_auth.c',
	'pjnath/src/pjnath/stun_msg.c',
	'pjnath/src/pjnath/stun_msg_dump.c',
	'pjnath/src/pjnath/stun_session.c',
	'pjnath/src/pjnath/stun_sock.c',
	'pjnath/src/pjnath/stun_transaction.c',
	'pjnath/src/pjnath/turn_session.c',
	'pjnath/src/pjnath/turn_sock.c',
	'pjnath/src/pjnath/upnp.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjnath_inc,
], link_with: [
	pjlib,
	pjlib_util,
])

# PJNATH_TEST

test('PJNATH Test', executable('pjnath-test', [
	'pjnath/src/pjnath-test/main.c',
	'pjnath/src/pjnath-test/ice_test.c',
	'pjnath/src/pjnath-test/stun.c',
	'pjnath/src/pjnath-test/sess_auth.c',
	'pjnath/src/pjnath-test/server.c',
	'pjnath/src/pjnath-test/concur_test.c',
	'pjnath/src/pjnath-test/stun_sock_test.c',
	'pjnath/src/pjnath-test/turn_sock_test.c',
	'pjnath/src/pjnath-test/test.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjnath_inc,
], link_with: [
	pjlib,
	pjlib_util,
	pjnath,
]), timeout: 0, workdir: join_paths(meson.current_source_dir(), 'pjnath/build') )

# PJTURN_CLIENT

executable('pjturn-client', [
	'pjnath/src/pjturn-client/client_main.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjnath_inc,
], link_with: [
	pjlib,
	pjlib_util,
	pjnath,
])

# PJTURN_SRV

executable('pjturn-srv', [
	'pjnath/src/pjturn-srv/allocation.c',
	'pjnath/src/pjturn-srv/auth.c',
	'pjnath/src/pjturn-srv/listener_udp.c',
	'pjnath/src/pjturn-srv/listener_tcp.c',
	'pjnath/src/pjturn-srv/server.c',
	'pjnath/src/pjturn-srv/main.c',
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjnath_inc,
], link_with: [
	pjlib,
	pjlib_util,
	pjnath,
])

# G7221_CODEC

third_party_inc = include_directories('third_party')
g7221_inc = include_directories('third_party/g7221', 'third_party/g7221/common')

libg7221codec = library('libg7221codec', [
	'third_party/g7221/common/common.c',
	'third_party/g7221/common/huff_tab.c',
	'third_party/g7221/common/tables.c',
	'third_party/g7221/common/basic_op.c',
	'third_party/g7221/decode/coef2sam.c',
	'third_party/g7221/decode/dct4_s.c',
	'third_party/g7221/decode/decoder.c',
	'third_party/g7221/encode/dct4_a.c',
	'third_party/g7221/encode/sam2coef.c',
	'third_party/g7221/encode/encoder.c',
], include_directories: [
	pjlib_inc,
	third_party_inc,
	g7221_inc,
])

# YUV

yuv_inc = include_directories('third_party/yuv/include')

libyuv = library('libyuv', [
	'third_party/yuv/source/compare.cc',
	'third_party/yuv/source/compare_common.cc',
	'third_party/yuv/source/compare_gcc.cc',
	'third_party/yuv/source/compare_neon64.cc',
	'third_party/yuv/source/compare_neon.cc',
	'third_party/yuv/source/compare_win.cc',
	'third_party/yuv/source/convert_argb.cc',
	'third_party/yuv/source/convert.cc',
	'third_party/yuv/source/convert_from_argb.cc',
	'third_party/yuv/source/convert_from.cc',
	'third_party/yuv/source/convert_jpeg.cc',
	'third_party/yuv/source/convert_to_argb.cc',
	'third_party/yuv/source/convert_to_i420.cc',
	'third_party/yuv/source/cpu_id.cc',
	'third_party/yuv/source/mjpeg_decoder.cc',
	'third_party/yuv/source/mjpeg_validate.cc',
	'third_party/yuv/source/planar_functions.cc',
	'third_party/yuv/source/rotate_any.cc',
	'third_party/yuv/source/rotate_argb.cc',
	'third_party/yuv/source/rotate.cc',
	'third_party/yuv/source/rotate_common.cc',
	'third_party/yuv/source/rotate_gcc.cc',
	'third_party/yuv/source/rotate_dspr2.cc',
	'third_party/yuv/source/rotate_neon64.cc',
	'third_party/yuv/source/rotate_neon.cc',
	'third_party/yuv/source/rotate_win.cc',
	'third_party/yuv/source/row_any.cc',
	'third_party/yuv/source/row_common.cc',
	'third_party/yuv/source/row_gcc.cc',
	'third_party/yuv/source/row_dspr2.cc',
	'third_party/yuv/source/row_neon64.cc',
	'third_party/yuv/source/row_neon.cc',
	'third_party/yuv/source/row_win.cc',
	'third_party/yuv/source/scale_any.cc',
	'third_party/yuv/source/scale_argb.cc',
	'third_party/yuv/source/scale.cc',
	'third_party/yuv/source/scale_common.cc',
	'third_party/yuv/source/scale_gcc.cc',
	'third_party/yuv/source/scale_dspr2.cc',
	'third_party/yuv/source/scale_neon64.cc',
	'third_party/yuv/source/scale_neon.cc',
	'third_party/yuv/source/scale_win.cc',
	'third_party/yuv/source/video_common.cc',
], include_directories: [
	yuv_inc,
])

# PJMEDIA

pjmedia_inc = include_directories('pjmedia/include')

pjmedia = library('pjmedia', [
	'pjmedia/src/pjmedia/alaw_ulaw.c',
	'pjmedia/src/pjmedia/alaw_ulaw_table.c',
	'pjmedia/src/pjmedia/avi_player.c',
	'pjmedia/src/pjmedia/bidirectional.c',
	'pjmedia/src/pjmedia/clock_thread.c',
	'pjmedia/src/pjmedia/codec.c',
	'pjmedia/src/pjmedia/conference.c',
	'pjmedia/src/pjmedia/conf_switch.c',
	'pjmedia/src/pjmedia/converter.c',
	'pjmedia/src/pjmedia/converter_libswscale.c',
	'pjmedia/src/pjmedia/converter_libyuv.c',
	'pjmedia/src/pjmedia/delaybuf.c',
	'pjmedia/src/pjmedia/echo_common.c',
	'pjmedia/src/pjmedia/echo_port.c',
	'pjmedia/src/pjmedia/echo_suppress.c',
	'pjmedia/src/pjmedia/echo_webrtc.c',
	'pjmedia/src/pjmedia/echo_webrtc_aec3.cpp',
	'pjmedia/src/pjmedia/endpoint.c',
	'pjmedia/src/pjmedia/errno.c',
	'pjmedia/src/pjmedia/event.c',
	'pjmedia/src/pjmedia/format.c',
	'pjmedia/src/pjmedia/ffmpeg_util.c',
	'pjmedia/src/pjmedia/g711.c',
	'pjmedia/src/pjmedia/jbuf.c',
	'pjmedia/src/pjmedia/master_port.c',
	'pjmedia/src/pjmedia/mem_capture.c',
	'pjmedia/src/pjmedia/mem_player.c',
#	'pjmedia/src/pjmedia/nack_buffer.c',
	'pjmedia/src/pjmedia/null_port.c',
	'pjmedia/src/pjmedia/plc_common.c',
	'pjmedia/src/pjmedia/port.c',
	'pjmedia/src/pjmedia/splitcomb.c',
	'pjmedia/src/pjmedia/resample_resample.c',
	'pjmedia/src/pjmedia/resample_libsamplerate.c',
	'pjmedia/src/pjmedia/resample_speex.c',
	'pjmedia/src/pjmedia/resample_port.c',
	'pjmedia/src/pjmedia/rtcp.c',
	'pjmedia/src/pjmedia/rtcp_xr.c',
	'pjmedia/src/pjmedia/rtcp_fb.c',
	'pjmedia/src/pjmedia/rtp.c',
	'pjmedia/src/pjmedia/sdp.c',
	'pjmedia/src/pjmedia/sdp_cmp.c',
	'pjmedia/src/pjmedia/sdp_neg.c',
	'pjmedia/src/pjmedia/session.c',
	'pjmedia/src/pjmedia/silencedet.c',
	'pjmedia/src/pjmedia/sound_legacy.c',
	'pjmedia/src/pjmedia/sound_port.c',
	'pjmedia/src/pjmedia/stereo_port.c',
	'pjmedia/src/pjmedia/stream_common.c',
	'pjmedia/src/pjmedia/stream.c',
	'pjmedia/src/pjmedia/stream_info.c',
	'pjmedia/src/pjmedia/tonegen.c',
	'pjmedia/src/pjmedia/transport_adapter_sample.c',
	'pjmedia/src/pjmedia/transport_ice.c',
	'pjmedia/src/pjmedia/transport_loop.c',
	'pjmedia/src/pjmedia/transport_srtp.c',
	'pjmedia/src/pjmedia/transport_udp.c',
	'pjmedia/src/pjmedia/types.c',
	'pjmedia/src/pjmedia/vid_codec.c',
	'pjmedia/src/pjmedia/vid_codec_util.c',
	'pjmedia/src/pjmedia/vid_port.c',
	'pjmedia/src/pjmedia/vid_stream.c',
	'pjmedia/src/pjmedia/vid_stream_info.c',
	'pjmedia/src/pjmedia/vid_conf.c',
	'pjmedia/src/pjmedia/wav_player.c',
	'pjmedia/src/pjmedia/wav_playlist.c',
	'pjmedia/src/pjmedia/wav_writer.c',
	'pjmedia/src/pjmedia/wave.c',
	'pjmedia/src/pjmedia/wsola.c',
	'pjmedia/src/pjmedia/audiodev.c',
	'pjmedia/src/pjmedia/videodev.c',
], include_directories: [
	yuv_inc,
	pjlib_inc,
	pjlib_util_inc,
	pjmedia_inc,
	pjnath_inc,
], link_with: [
	pjlib,
	pjlib_util,
	pjnath,
], dependencies: [
	openssl_dep,
	srtp2_dep,
	speex_dep,
	resample_dep,
])

# PJMEDIA_CODEC

pjmedia_codec = library('pjmedia-codec', [
	'pjmedia/src/pjmedia-codec/audio_codecs.c',
	'pjmedia/src/pjmedia-codec/ffmpeg_vid_codecs.c',
	'pjmedia/src/pjmedia-codec/openh264.cpp',
	'pjmedia/src/pjmedia-codec/h263_packetizer.c',
	'pjmedia/src/pjmedia-codec/h264_packetizer.c',
	'pjmedia/src/pjmedia-codec/vpx_packetizer.c',
	'pjmedia/src/pjmedia-codec/ipp_codecs.c',
	'pjmedia/src/pjmedia-codec/silk.c',
	'pjmedia/src/pjmedia-codec/speex_codec.c',
	'pjmedia/src/pjmedia-codec/opus.c',
#	'pjmedia/src/pjmedia-codec/opus_util.c',
	'pjmedia/src/pjmedia-codec/g722.c',
	'pjmedia/src/pjmedia-codec/g722/g722_enc.c',
	'pjmedia/src/pjmedia-codec/g722/g722_dec.c',
	'pjmedia/src/pjmedia-codec/g7221.c',
	'pjmedia/src/pjmedia-codec/bcg729.c',
	'pjmedia/src/pjmedia-codec/and_aud_mediacodec.cpp',
	'pjmedia/src/pjmedia-codec/and_vid_mediacodec.cpp',
	'pjmedia/src/pjmedia-codec/g7221_sdp_match.c',
	'pjmedia/src/pjmedia-codec/amr_sdp_match.c',
	'pjmedia/src/pjmedia-codec/passthrough.c',
	'pjmedia/src/pjmedia-codec/vpx.c',
], link_with: [
	pjlib,
	pjmedia,
], include_directories: [
	pjlib_inc,
	pjmedia_inc,
])

# PJMEDIA_VIDEODEV

library('pjmedia-videodev', [
	'pjmedia/src/pjmedia-videodev/sdl_dev.c',
	'pjmedia/src/pjmedia-videodev/videodev.c',
	'pjmedia/src/pjmedia-videodev/errno.c',
	'pjmedia/src/pjmedia-videodev/avi_dev.c',
	'pjmedia/src/pjmedia-videodev/ffmpeg_dev.c',
	'pjmedia/src/pjmedia-videodev/colorbar_dev.c',
	'pjmedia/src/pjmedia-videodev/v4l2_dev.c',
	'pjmedia/src/pjmedia-videodev/opengl_dev.c',
	'pjmedia/src/pjmedia-videodev/util.c',
], include_directories: [
	pjlib_inc,
	pjmedia_inc,
])

# PJMEDIA_AUDIODEV

pjmedia_audiodev = library('pjmedia-audiodev', [
	'pjmedia/src/pjmedia-audiodev/audiodev.c',
	'pjmedia/src/pjmedia-audiodev/audiotest.c',
	'pjmedia/src/pjmedia-audiodev/errno.c',
	'pjmedia/src/pjmedia-audiodev/legacy_dev.c',
	'pjmedia/src/pjmedia-audiodev/null_dev.c',
	'pjmedia/src/pjmedia-audiodev/pa_dev.c',
	'pjmedia/src/pjmedia-audiodev/wmme_dev.c',
	'pjmedia/src/pjmedia-audiodev/alsa_dev.c',
	'pjmedia/src/pjmedia-audiodev/bb10_dev.c',
	'pjmedia/src/pjmedia-audiodev/bdimad_dev.c',
	'pjmedia/src/pjmedia-audiodev/android_jni_dev.c',
	'pjmedia/src/pjmedia-audiodev/opensl_dev.c',
	'pjmedia/src/pjmedia-audiodev/oboe_dev.cpp',
], link_with: [
	pjlib,
	pjmedia,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjmedia_inc,
])

# PJSDP

pjsdp = library('pjsdp', [
	'pjmedia/src/pjmedia/errno.c',
	'pjmedia/src/pjmedia/sdp.c',
	'pjmedia/src/pjmedia/sdp_cmp.c',
	'pjmedia/src/pjmedia/sdp_neg.c',
], link_with: [
	pjlib,
	pjlib_util,
	pjmedia,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjmedia_inc,
])

# PJMEDIA_TEST

#test('PJMEDIA Test', executable('pjmedia-test', [
#	'pjmedia/src/test/codec_vectors.c',
#	'pjmedia/src/test/jbuf_test.c',
#	'pjmedia/src/test/main.c',
#	'pjmedia/src/test/mips_test.c',
#	'pjmedia/src/test/vid_codec_test.c',
#	'pjmedia/src/test/vid_dev_test.c',
#	'pjmedia/src/test/vid_port_test.c',
#	'pjmedia/src/test/rtp_test.c',
#	'pjmedia/src/test/test.c',
#	'pjmedia/src/test/sdp_neg_test.c',
##	'pjmedia/src/test/nack_buffer_test.c',
#], link_with: [
#	pjlib,
#	pjlib_util,
#	pjmedia,
#	pjmedia_audiodev,
#], include_directories: [
#	pjlib_inc,
#	pjlib_util_inc,
#	pjmedia_inc,
#	pjnath_inc,
#]), timeout: 0, workdir: join_paths(meson.current_source_dir(), 'pjmedia/build') )

# PJSIP

pjsip_inc = include_directories('pjsip/include')

pjsip = library('pjsip', [
	'pjsip/src/pjsip/sip_config.c',
	'pjsip/src/pjsip/sip_multipart.c',
	'pjsip/src/pjsip/sip_errno.c',
	'pjsip/src/pjsip/sip_msg.c',
	'pjsip/src/pjsip/sip_parser.c',
	'pjsip/src/pjsip/sip_tel_uri.c',
	'pjsip/src/pjsip/sip_uri.c',
	'pjsip/src/pjsip/sip_endpoint.c',
	'pjsip/src/pjsip/sip_util.c',
	'pjsip/src/pjsip/sip_util_proxy.c',
	'pjsip/src/pjsip/sip_resolve.c',
	'pjsip/src/pjsip/sip_transport.c',
	'pjsip/src/pjsip/sip_transport_loop.c',
	'pjsip/src/pjsip/sip_transport_udp.c',
	'pjsip/src/pjsip/sip_transport_tcp.c',
	'pjsip/src/pjsip/sip_transport_tls.c',
	'pjsip/src/pjsip/sip_auth_aka.c',
	'pjsip/src/pjsip/sip_auth_client.c',
	'pjsip/src/pjsip/sip_auth_msg.c',
	'pjsip/src/pjsip/sip_auth_parser.c',
	'pjsip/src/pjsip/sip_auth_server.c',
	'pjsip/src/pjsip/sip_transaction.c',
	'pjsip/src/pjsip/sip_util_statefull.c',
	'pjsip/src/pjsip/sip_dialog.c',
	'pjsip/src/pjsip/sip_ua_layer.c',
], link_with: [
	pjlib,
	pjlib_util,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjsip_inc,
], dependencies: [
	openssl_dep,
])

# PJSIP_SIMPLE

pjsip_simple = library('pjsip-simple', [
	'pjsip/src/pjsip-simple/errno.c',
	'pjsip/src/pjsip-simple/evsub.c',
	'pjsip/src/pjsip-simple/evsub_msg.c',
	'pjsip/src/pjsip-simple/iscomposing.c',
	'pjsip/src/pjsip-simple/mwi.c',
	'pjsip/src/pjsip-simple/pidf.c',
	'pjsip/src/pjsip-simple/presence.c',
	'pjsip/src/pjsip-simple/presence_body.c',
	'pjsip/src/pjsip-simple/publishc.c',
	'pjsip/src/pjsip-simple/rpid.c',
	'pjsip/src/pjsip-simple/xpidf.c',
], link_with: [
	pjlib,
	pjlib_util,
	pjsip,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjsip_inc,
])

# PJSIP_UA

pjsip_ua = library('pjsip-ua', [
	'pjsip/src/pjsip-ua/sip_inv.c',
	'pjsip/src/pjsip-ua/sip_reg.c',
	'pjsip/src/pjsip-ua/sip_replaces.c',
	'pjsip/src/pjsip-ua/sip_xfer.c',
	'pjsip/src/pjsip-ua/sip_100rel.c',
	'pjsip/src/pjsip-ua/sip_timer.c',
], link_with: [
	pjlib,
	pjlib_util,
	pjsdp,
	pjsip,
	pjsip_simple,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjsip_inc,
	pjmedia_inc,
])

# PJSUA_LIB

pjsua_lib = library('pjsua-lib', [
	'pjsip/src/pjsua-lib/pjsua_acc.c',
	'pjsip/src/pjsua-lib/pjsua_call.c',
	'pjsip/src/pjsua-lib/pjsua_core.c',
	'pjsip/src/pjsua-lib/pjsua_im.c',
	'pjsip/src/pjsua-lib/pjsua_media.c',
	'pjsip/src/pjsua-lib/pjsua_pres.c',
	'pjsip/src/pjsua-lib/pjsua_dump.c',
	'pjsip/src/pjsua-lib/pjsua_aud.c',
	'pjsip/src/pjsua-lib/pjsua_vid.c',
], link_with: [
	pjlib,
	pjlib_util,
	pjmedia,
	pjsip,
	pjsip_simple,
	pjnath,
	pjmedia_codec,
	pjmedia_audiodev,
	pjsip_ua,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjsip_inc,
	pjmedia_inc,
	pjnath_inc,
])

# TEST

test('PJSIP Test', executable('pjsip-test', [
	'pjsip/src/test/main.c',
	'pjsip/src/test/dlg_core_test.c',
	'pjsip/src/test/dns_test.c',
	'pjsip/src/test/msg_err_test.c',
	'pjsip/src/test/msg_logger.c',
	'pjsip/src/test/msg_test.c',
	'pjsip/src/test/multipart_test.c',
	'pjsip/src/test/regc_test.c',
	'pjsip/src/test/test.c',
	'pjsip/src/test/transport_loop_test.c',
	'pjsip/src/test/transport_tcp_test.c',
	'pjsip/src/test/transport_test.c',
	'pjsip/src/test/transport_udp_test.c',
	'pjsip/src/test/tsx_basic_test.c',
	'pjsip/src/test/tsx_bench.c',
	'pjsip/src/test/tsx_uac_test.c',
	'pjsip/src/test/tsx_uas_test.c',
	'pjsip/src/test/txdata_test.c',
	'pjsip/src/test/uri_test.c',
	'pjsip/src/test/inv_offer_answer_test.c',
], dependencies: [
	m_dep,
], link_with: [
	pjlib,
	pjlib_util,
	pjsdp,
	pjsip,
	pjsip_ua,
], include_directories: [
	pjlib_inc,
	pjlib_util_inc,
	pjsip_inc,
	pjmedia_inc,
]), timeout: 0, workdir: join_paths(meson.current_source_dir(), 'pjsip/build') )

#

pjproject_dep = declare_dependency(link_with : [
	pjlib,
	pjlib_util,
	pjsip,
	pjsip_ua,
	pjsip_simple,
	pjmedia,
	pjmedia_codec,
	pjmedia_audiodev,
	pjnath,
	pjsua_lib,
], include_directories : [
	pjlib_inc,
	pjlib_util_inc,
	pjmedia_inc,
	pjnath_inc,
	pjsip_inc,
])

# PJSUA-APPS

executable('auddemo', [
	'pjsip-apps/src/samples/auddemo.c',
], dependencies: [ pjproject_dep ])

executable('aviplay', [
	'pjsip-apps/src/samples/aviplay.c',
], dependencies: [ pjproject_dep ])

executable('aectest', [
	'pjsip-apps/src/samples/aectest.c',
], dependencies: [ pjproject_dep ])

executable('clidemo', [
	'pjsip-apps/src/samples/clidemo.c',
], dependencies: [ pjproject_dep ])

executable('confsample', [
	'pjsip-apps/src/samples/confsample.c',
], dependencies: [ pjproject_dep ])

executable('encdec', [
	'pjsip-apps/src/samples/encdec.c',
], dependencies: [ pjproject_dep ])

executable('httpdemo', [
	'pjsip-apps/src/samples/httpdemo.c',
], dependencies: [ pjproject_dep ])

executable('icedemo', [
	'pjsip-apps/src/samples/icedemo.c',
], dependencies: [ pjproject_dep ])

executable('jbsim', [
	'pjsip-apps/src/samples/jbsim.c',
], dependencies: [ pjproject_dep ])

executable('latency', [
	'pjsip-apps/src/samples/latency.c',
], dependencies: [ pjproject_dep ])

executable('level', [
	'pjsip-apps/src/samples/level.c',
], dependencies: [ pjproject_dep ])

executable('mix', [
	'pjsip-apps/src/samples/mix.c',
], dependencies: [ pjproject_dep ])

executable('pjsip-perf', [
	'pjsip-apps/src/samples/pjsip-perf.c',
], dependencies: [ pjproject_dep ])

executable('pcaputil', [
	'pjsip-apps/src/samples/pcaputil.c',
], dependencies: [ pjproject_dep ])

executable('playfile', [
	'pjsip-apps/src/samples/playfile.c',
], dependencies: [ pjproject_dep ])

executable('playsine', [
	'pjsip-apps/src/samples/playsine.c',
], dependencies: [ pjproject_dep ])

executable('recfile', [
	'pjsip-apps/src/samples/recfile.c',
], dependencies: [ pjproject_dep ])

executable('resampleplay', [
	'pjsip-apps/src/samples/resampleplay.c',
], dependencies: [ pjproject_dep ])

executable('simpleua', [
	'pjsip-apps/src/samples/simpleua.c',
], dependencies: [ pjproject_dep ])

executable('simple_pjsua', [
	'pjsip-apps/src/samples/simple_pjsua.c',
], dependencies: [ pjproject_dep ])

executable('sipecho', [
	'pjsip-apps/src/samples/sipecho.c',
], dependencies: [ pjproject_dep ])

executable('siprtp', [
	'pjsip-apps/src/samples/siprtp.c',
], dependencies: [ pjproject_dep ])

executable('sipstateless', [
	'pjsip-apps/src/samples/sipstateless.c',
], dependencies: [ pjproject_dep ])

executable('stateful_proxy', [
	'pjsip-apps/src/samples/stateful_proxy.c',
], dependencies: [ pjproject_dep ])

executable('stateless_proxy', [
	'pjsip-apps/src/samples/stateless_proxy.c',
], dependencies: [ pjproject_dep ])

executable('stereotest', [
	'pjsip-apps/src/samples/stereotest.c',
], dependencies: [ pjproject_dep ])

executable('streamutil', [
	'pjsip-apps/src/samples/streamutil.c',
], dependencies: [ pjproject_dep ])

executable('strerror', [
	'pjsip-apps/src/samples/strerror.c',
], dependencies: [ pjproject_dep ])

executable('tonegen', [
	'pjsip-apps/src/samples/tonegen.c',
], dependencies: [ pjproject_dep ])

executable('vid_codec_test', [
	'pjsip-apps/src/samples/vid_codec_test.c',
], dependencies: [ pjproject_dep ])

executable('vid_streamutil', [
	'pjsip-apps/src/samples/vid_streamutil.c',
], dependencies: [ pjproject_dep ])

#

executable('pjsua', [
	'pjsip-apps/src/pjsua/main.c',
	'pjsip-apps/src/pjsua/pjsua_app.c',
	'pjsip-apps/src/pjsua/pjsua_app_cli.c',
	'pjsip-apps/src/pjsua/pjsua_app_common.c',
	'pjsip-apps/src/pjsua/pjsua_app_legacy.c',
	'pjsip-apps/src/pjsua/pjsua_app_config.c',
], dependencies: [ pjproject_dep ])

#

executable('pjsystest', [
	'pjsip-apps/src/pjsystest/systest.c',
	'pjsip-apps/src/pjsystest/main_console.c',
], dependencies: [ pjproject_dep ])
