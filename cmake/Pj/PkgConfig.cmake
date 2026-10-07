# Generates libpjproject.pc from libpjproject.pc.in, the template the GNU build
# fills too.
#
# Libs lists the PJSIP libraries and Cflags the compile definitions consumers
# need. For `pkg-config --static`, Libs.private lists what those libraries
# link, and Requires.private the dependencies that have a pkg-config module
# of their own.
#
# Only targets this project builds are walked. Their link and compile
# properties hold a few generator expressions, all evaluated here, so nothing
# depends on how a dependency's own targets are written. A dependency target
# is looked up in _PJ_PC_DEPENDENCIES instead: it goes to Requires.private
# when pkg-config knows its module for the same library CMake found, and is
# otherwise linked through the library file CMake found.
#
# Everything is known at configure time, so the .pc is written then, and
# installed like any other file. Its prefix is relative to the file itself,
# so it follows `cmake --install --prefix` and a relocated installation.

# Dependency targets: "<target>|<pkg-config modules>|<library variables>|
# <libraries to link when neither is known>". The modules are alternatives.
# The variables are those the Find module caches its library files in, one
# per library; "a/b" takes the first that is set, as FindOpenSSL caches
# OPENSSL_SSL_LIBRARY, but SSL_EAY_RELEASE or SSL_EAY on Windows.
set(_PJ_PC_DEPENDENCIES
  "OpenSSL::SSL|libssl|OPENSSL_SSL_LIBRARY/SSL_EAY_RELEASE/SSL_EAY|ssl"
  "OpenSSL::Crypto|libcrypto|OPENSSL_CRYPTO_LIBRARY/LIB_EAY_RELEASE/LIB_EAY|crypto"
  "GnuTLS::GnuTLS|gnutls|GNUTLS_LIBRARY|gnutls"
  "MbedTLS::mbedtls|mbedtls||mbedtls"
  "MbedTLS::mbedx509|mbedx509||mbedx509"
  "MbedTLS::mbedcrypto|mbedcrypto||mbedcrypto"
  "UUID::UUID|uuid,libuuid|UUID_LIBRARY|uuid"
  "UPNP::UPNP|libupnp,upnp|UPNP_UPNP_LIBRARY,UPNP_IXML_LIBRARY|upnp,ixml"
  "ALSA::ALSA|alsa|ALSA_LIBRARY|asound"
  "SDL2::SDL2|sdl2||SDL2"
  "OpenGL::GL|gl|OPENGL_gl_LIBRARY|GL"
  "OpenGL::GLES2|glesv2|OPENGL_gles2_LIBRARY|GLESv2"
  "V4L2::V4L2|libv4l2,v4l2|V4L2_LIBRARY|v4l2"
  "FFMPEG::avutil|libavutil|FFMPEG_avutil_LIBRARY|avutil"
  "FFMPEG::swscale|libswscale|FFMPEG_swscale_LIBRARY|swscale"
  "FFMPEG::avcodec|libavcodec|FFMPEG_avcodec_LIBRARY|avcodec"
  "FFMPEG::avformat|libavformat|FFMPEG_avformat_LIBRARY|avformat"
  "FFMPEG::avdevice|libavdevice|FFMPEG_avdevice_LIBRARY|avdevice"
  "OPUS::OPUS|opus|OPUS_LIBRARY|opus"
  "VPX::VPX|vpx|VPX_LIBRARY|vpx"
  "OpenH264::OpenH264|openh264|OpenH264_LIBRARY|openh264"
  "Silk::Silk|SKP_SILK_SDK|Silk_LIBRARY|SKP_SILK_SDK"
  "BCG729::BCG729|bcg729,libbcg729|BCG729_LIBRARY|bcg729"
  "Lyra::Lyra|lyra|Lyra_LIBRARY|lyra"
  "OpenCoreAMRNB::OpenCoreAMRNB|opencore-amrnb|OpenCoreAMRNB_LIBRARY|opencore-amrnb"
  "OpenCoreAMRWB::OpenCoreAMRWB|opencore-amrwb|OpenCoreAMRWB_LIBRARY|opencore-amrwb"
  "VisualOnAMRWBEnc::VisualOnAMRWBEnc|vo-amrwbenc,libvo-amrwbenc|VisualOnAMRWBEnc_LIBRARY|vo-amrwbenc"
  "SpeexDSP::SpeexDSP|speexdsp,libspeexdsp|SpeexDSP_LIBRARY|speexdsp"
  "SampleRate::SampleRate|samplerate,libsamplerate|SampleRate_LIBRARY|samplerate"
  "Oboe::Oboe||Oboe_LIBRARY,Oboe_LIBRARY_OpenSLES,Oboe_LIBRARY_log|oboe"
  # PJ_DEP_<name>=system
  "SRTP::SRTP|libsrtp3,libsrtp2,srtp2|SRTP_LIBRARY|srtp2"
  "Speex::Speex|speex,libspeex|Speex_LIBRARY|speex"
  "GSM::GSM|gsm,libgsm|GSM_LIBRARY|gsm"
  "YUV::YUV|libyuv,yuv|YUV_LIBRARY|yuv"
  "Resample::Resample|resample,libresample|Resample_LIBRARY|resample"
)

# Options that take their argument as the next item, e.g. -framework;Foo
set(_PJ_PC_PAIRED_OPTIONS "-framework;-weak_framework;-Xlinker")

# Evaluates the generator expressions this project's targets use; their
# arguments are literal values by now. Any other is dropped with a warning,
# so that it cannot reach file(GENERATE) and fail the generate step.
function(_pj_pc_eval out value)
  set(v "${value}")
  while(v MATCHES "\\$<([^<>$:]*)(:([^<>$]*))?>")
    set(expr "${CMAKE_MATCH_0}")
    set(name "${CMAKE_MATCH_1}")
    set(arg "${CMAKE_MATCH_3}")
    string(TOUPPER "${arg}" uarg)
    if(name STREQUAL "LINK_ONLY" OR name STREQUAL "INSTALL_INTERFACE"
       OR name STREQUAL "1")
      set(r "${arg}")
    elseif(name STREQUAL "BUILD_INTERFACE" OR name STREQUAL "0")
      set(r "")
    elseif(name STREQUAL "BOOL")
      # As CMake's: false only for the false constants
      if(uarg MATCHES "^(|0|OFF|NO|FALSE|N|IGNORE|NOTFOUND|.*-NOTFOUND)$")
        set(r 0)
      else()
        set(r 1)
      endif()
    elseif(name STREQUAL "NOT" AND arg MATCHES "^[01]$")
      if(arg STREQUAL "1")
        set(r 0)
      else()
        set(r 1)
      endif()
    elseif((name STREQUAL "OR" OR name STREQUAL "AND")
           AND arg MATCHES "^[01](,[01])*$")
      if(name STREQUAL "OR" AND arg MATCHES "1")
        set(r 1)
      elseif(name STREQUAL "AND" AND NOT arg MATCHES "0")
        set(r 1)
      else()
        set(r 0)
      endif()
    elseif(name STREQUAL "STREQUAL" AND arg MATCHES "^([^,]*),([^,]*)$")
      if(CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2)
        set(r 1)
      else()
        set(r 0)
      endif()
    elseif(name STREQUAL "")
      # What remains of a condition already dropped below
      set(r "")
    elseif(name STREQUAL "UPPER_CASE")
      set(r "${uarg}")
    elseif(name STREQUAL "LOWER_CASE")
      string(TOLOWER "${arg}" r)
    else()
      message(AUTHOR_WARNING
        "pkg-config: ${expr} is not supported, and left out of "
        "libpjproject.pc. Add it to cmake/Pj/PkgConfig.cmake.")
      set(r "")
    endif()
    string(REPLACE "${expr}" "${r}" v "${v}")
  endwhile()
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# Joins an option with its argument, e.g. -framework;Foo, into one item
function(_pj_pc_pair_options out items)
  set(result "")
  set(pending "")
  foreach(item IN LISTS items)
    if(pending)
      list(APPEND result "${pending} ${item}")
      set(pending "")
    elseif(item IN_LIST _PJ_PC_PAIRED_OPTIONS)
      set(pending "${item}")
    else()
      list(APPEND result "${item}")
    endif()
  endforeach()
  set(${out} "${result}" PARENT_SCOPE)
endfunction()

# Whether the linker searches `dir` anyway: an implicit link directory itself,
# or anything inside a sysroot
function(_pj_pc_searched out dir)
  set(${out} FALSE PARENT_SCOPE)
  if(dir IN_LIST CMAKE_C_IMPLICIT_LINK_DIRECTORIES)
    set(${out} TRUE PARENT_SCOPE)
    return()
  endif()
  # CMAKE_OSX_SYSROOT may be an SDK name, e.g. iphoneos, rather than a path
  foreach(sysroot IN ITEMS "${CMAKE_OSX_SYSROOT}" "${CMAKE_SYSROOT}")
    if(IS_ABSOLUTE "${sysroot}")
      string(FIND "${dir}/" "${sysroot}/" pos)
      if(pos EQUAL 0)
        set(${out} TRUE PARENT_SCOPE)
      endif()
    endif()
  endforeach()
endfunction()

# A Homebrew Cellar directory as the formula's opt/ link, which survives an
# upgrade, when that leads to the same place
function(_pj_pc_stable_dir out dir)
  set(${out} "${dir}" PARENT_SCOPE)
  if(dir MATCHES "^(.+)/Cellar/([^/]+)/[^/]+(/.*)?$")
    set(opt "${CMAKE_MATCH_1}/opt/${CMAKE_MATCH_2}${CMAKE_MATCH_3}")
    if(IS_DIRECTORY "${opt}")
      file(REAL_PATH "${opt}" real_opt)
      file(REAL_PATH "${dir}" real_dir)
      if(real_opt STREQUAL real_dir)
        set(${out} "${opt}" PARENT_SCOPE)
      endif()
    endif()
  endif()
endfunction()

function(_pj_pc_quote out path)
  if(path MATCHES " ")
    set(${out} "\"${path}\"" PARENT_SCOPE)
  else()
    set(${out} "${path}" PARENT_SCOPE)
  endif()
endfunction()

# Whether `path` is a static library with a shared one of the same name
# beside it, which -l<name> would pick instead
function(_pj_pc_static_beside_shared out path)
  set(${out} FALSE PARENT_SCOPE)
  get_filename_component(dir "${path}" DIRECTORY)
  get_filename_component(name "${path}" NAME)
  if(name MATCHES "\\.dll\\.a$" OR NOT name MATCHES "^lib(.+)\\.a$")
    return()
  endif()
  set(lib "${CMAKE_MATCH_1}")
  file(GLOB shared "${dir}/lib${lib}.so" "${dir}/lib${lib}.so.*"
    "${dir}/lib${lib}.dylib" "${dir}/lib${lib}.tbd" "${dir}/lib${lib}.dll.a")
  if(shared)
    set(${out} TRUE PARENT_SCOPE)
  endif()
endfunction()

# The flags that link a library file: -L/-l, or -F/-framework for a
# framework. A static library stays a full path when a shared one of the same
# name is beside it, as -l would pick the shared one; so does a file whose
# name is not a library name.
function(_pj_pc_file_flags out path)
  get_filename_component(dir "${path}" DIRECTORY)
  get_filename_component(name "${path}" NAME)
  _pj_pc_stable_dir(dir "${dir}")
  _pj_pc_searched(searched "${dir}")

  if(path MATCHES "/([^/]+)\\.framework/?$")
    set(flags "-framework ${CMAKE_MATCH_1}")
    if(NOT searched AND NOT dir MATCHES "^(/System)?/Library/Frameworks$")
      _pj_pc_quote(qdir "${dir}")
      list(PREPEND flags "-F${qdir}")
    endif()
    set(${out} "${flags}" PARENT_SCOPE)
    return()
  endif()

  # One test per if(): a failed MATCHES clears CMAKE_MATCH_<n>
  set(lib "")
  if(name MATCHES "^lib(.+)\\.dll\\.a$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.a$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.(so|dylib|tbd)$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.so\\.[0-9.]+$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^(.+)\\.lib$")
    set(lib "${CMAKE_MATCH_1}")
  endif()
  _pj_pc_static_beside_shared(ambiguous "${path}")

  if(NOT lib OR ambiguous)
    set(${out} "\"${dir}/${name}\"" PARENT_SCOPE)
    return()
  endif()
  set(flags "-l${lib}")
  if(NOT searched)
    _pj_pc_quote(qdir "${dir}")
    list(PREPEND flags "-L${qdir}")
  endif()
  set(${out} "${flags}" PARENT_SCOPE)
endfunction()

# The pkg-config module among `modules` that describes the library at `path`,
# found by comparing its libdir: a module found elsewhere may describe
# another copy, e.g. the host's when cross-compiling, or MSYS2's in an MSVC
# build.
function(_pj_pc_find_module out modules path)
  set(${out} "" PARENT_SCOPE)
  if(NOT PKG_CONFIG_EXECUTABLE OR NOT modules)
    return()
  endif()
  get_filename_component(want "${path}" DIRECTORY)
  file(REAL_PATH "${want}" want)
  foreach(module IN LISTS modules)
    execute_process(
      COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=libdir "${module}"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE libdir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(NOT result EQUAL 0 OR NOT IS_DIRECTORY "${libdir}")
      continue()
    endif()
    file(REAL_PATH "${libdir}" libdir)
    if(NOT libdir STREQUAL want)
      continue()
    endif()
    set(${out} "${module}" PARENT_SCOPE)
    return()
  endforeach()
endfunction()

# Records how to link the dependency target `target`
function(_pj_pc_dependency target)
  if(target STREQUAL "Threads::Threads")
    # Part of the C library on Windows and Apple platforms
    if(UNIX AND NOT APPLE)
      set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE "-pthread")
    endif()
    return()
  endif()

  foreach(entry IN LISTS _PJ_PC_DEPENDENCIES)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 name)
    if(NOT name STREQUAL target)
      continue()
    endif()
    list(GET fields 1 modules)
    list(GET fields 2 variables)
    list(GET fields 3 names)
    string(REPLACE "," ";" modules "${modules}")
    string(REPLACE "," ";" variables "${variables}")
    string(REPLACE "," ";" names "${names}")

    set(paths "")
    foreach(alternatives IN LISTS variables)
      string(REPLACE "/" ";" alternatives "${alternatives}")
      foreach(variable IN LISTS alternatives)
        set(value "${${variable}}")
        if(NOT value STREQUAL "" AND NOT value MATCHES "-NOTFOUND$")
          list(APPEND paths "${value}")
          break()
        endif()
      endforeach()
    endforeach()
    # A module's -l would pick a shared library over a static one CMake
    # linked deliberately, e.g. with OPENSSL_USE_STATIC_LIBS
    set(static FALSE)
    foreach(path IN LISTS paths)
      _pj_pc_static_beside_shared(ambiguous "${path}")
      if(ambiguous)
        set(static TRUE)
      endif()
    endforeach()
    if(paths AND NOT static)
      list(GET paths 0 first)
      _pj_pc_find_module(module "${modules}" "${first}")
      if(module)
        set_property(GLOBAL APPEND PROPERTY _PJ_PC_REQUIRES "${module}")
        return()
      endif()
    endif()
    set(flags "")
    if(paths)
      foreach(path IN LISTS paths)
        _pj_pc_file_flags(file_flags "${path}")
        list(APPEND flags ${file_flags})
      endforeach()
    else()
      foreach(lib IN LISTS names)
        list(APPEND flags "-l${lib}")
      endforeach()
    endif()
    list(REVERSE flags)
    set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})
    return()
  endforeach()

  # Not in the table, but visible here: this project's own Find modules make
  # their interface targets (Apple frameworks, WASAPI, ...) global, and
  # third_party/CMakeLists.txt the PJ_DEP_<name>=system ones. An interface
  # target is read; a library is linked through its file, without reading
  # what it links.
  if(TARGET "${target}")
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
      _pj_pc_visit(${target})
      return()
    endif()
    foreach(property IN ITEMS IMPORTED_IMPLIB_RELEASE IMPORTED_IMPLIB
                              IMPORTED_LOCATION_RELEASE
                              IMPORTED_LOCATION_NOCONFIG IMPORTED_LOCATION)
      get_target_property(path ${target} ${property})
      if(path)
        _pj_pc_file_flags(flags "${path}")
        list(REVERSE flags)
        set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})
        return()
      endif()
    endforeach()
  endif()
  message(STATUS "pkg-config: no entry for ${target}, which "
    "libpjproject.pc leaves out")
endfunction()

# Records what `target`, a target this project builds, links, depth-first,
# then the target itself, so that the reversed result lists every library
# before what it needs, as a static link needs
function(_pj_pc_visit target)
  get_property(seen GLOBAL PROPERTY _PJ_PC_SEEN)
  if(target IN_LIST seen)
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_SEEN "${target}")
  get_property(pj_libs GLOBAL PROPERTY _PJ_PC_LIBS)

  get_target_property(type ${target} TYPE)
  get_target_property(imported ${target} IMPORTED)
  set(items "")
  if(NOT type STREQUAL "INTERFACE_LIBRARY")
    get_target_property(items ${target} LINK_LIBRARIES)
  endif()
  get_target_property(interface_items ${target} INTERFACE_LINK_LIBRARIES)
  if(interface_items)
    list(APPEND items ${interface_items})
  endif()
  # Evaluated as a whole, as an expression may hold a ";"
  _pj_pc_eval(items "${items}")
  _pj_pc_pair_options(items "${items}")

  set(flags "")
  foreach(item IN LISTS items)
    # "::@" marks the directory a cross-directory link was made from
    if(item STREQUAL "" OR item MATCHES "-NOTFOUND$" OR item MATCHES "^::@")
      continue()
    endif()
    if(TARGET "${item}")
      get_target_property(aliased ${item} ALIASED_TARGET)
      if(aliased)
        set(item "${aliased}")
      endif()
      get_target_property(item_imported ${item} IMPORTED)
      if(item_imported)
        _pj_pc_dependency("${item}")
      else()
        if(NOT imported AND NOT target IN_LIST pj_libs
           AND item IN_LIST pj_libs)
          # A bundled library linking PJSIP ones (srtp calls pj_log)
          set_property(GLOBAL APPEND PROPERTY _PJ_PC_THIRD_PARTY_NEEDS
            "${item}")
        endif()
        _pj_pc_visit("${item}")
      endif()
    elseif(item MATCHES "::")
      # A dependency's target, imported in another directory
      _pj_pc_dependency("${item}")
    elseif(item MATCHES "^-")
      list(APPEND flags "${item}")
    elseif(IS_ABSOLUTE "${item}")
      _pj_pc_file_flags(file_flags "${item}")
      list(APPEND flags ${file_flags})
    else()
      list(APPEND flags "-l${item}")
    endif()
  endforeach()
  # Reversed, as the whole list is reversed at the end
  list(REVERSE flags)
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})

  if(NOT imported AND NOT type STREQUAL "INTERFACE_LIBRARY"
     AND NOT target IN_LIST pj_libs)
    set_property(GLOBAL APPEND PROPERTY _PJ_PC_THIRD_PARTY "${target}")
  endif()
endfunction()

# pj_generate_pkgconfig(<pc.in> <destination> <library>...)
#
# The libraries are those to list in Libs, in link order.
function(pj_generate_pkgconfig template destination)
  set(pj_libs ${ARGN})
  find_package(PkgConfig QUIET)

  set(defs "")
  foreach(lib IN LISTS pj_libs)
    get_target_property(lib_defs ${lib} INTERFACE_COMPILE_DEFINITIONS)
    if(lib_defs)
      _pj_pc_eval(lib_defs "${lib_defs}")
      foreach(def IN LISTS lib_defs)
        if(def AND NOT def IN_LIST defs)
          list(APPEND defs "${def}")
        endif()
      endforeach()
    endif()
  endforeach()

  set_property(GLOBAL PROPERTY _PJ_PC_LIBS "${pj_libs}")
  set_property(GLOBAL PROPERTY _PJ_PC_SEEN "")
  set_property(GLOBAL PROPERTY _PJ_PC_PRIVATE "")
  set_property(GLOBAL PROPERTY _PJ_PC_REQUIRES "")
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY "")
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY_NEEDS "")
  foreach(lib IN LISTS pj_libs)
    _pj_pc_visit(${lib})
  endforeach()
  get_property(private GLOBAL PROPERTY _PJ_PC_PRIVATE)
  get_property(requires GLOBAL PROPERTY _PJ_PC_REQUIRES)
  get_property(third_party GLOBAL PROPERTY _PJ_PC_THIRD_PARTY)
  get_property(third_party_needs GLOBAL PROPERTY _PJ_PC_THIRD_PARTY_NEEDS)
  list(REVERSE private)
  list(REVERSE third_party)
  list(REMOVE_DUPLICATES requires)

  # pjlib has C++ sources, so a static link from C needs the C++ runtime
  set(cxx_runtime ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
  if(cxx_runtime AND CMAKE_C_IMPLICIT_LINK_LIBRARIES)
    list(REMOVE_ITEM cxx_runtime ${CMAKE_C_IMPLICIT_LINK_LIBRARIES})
  endif()
  foreach(lib IN LISTS cxx_runtime)
    if(IS_ABSOLUTE "${lib}")
      _pj_pc_file_flags(file_flags "${lib}")
      list(APPEND private ${file_flags})
    elseif(lib MATCHES "^-")
      list(APPEND private "${lib}")
    else()
      list(APPEND private "-l${lib}")
    endif()
  endforeach()

  # The bundled libraries, then the PJSIP ones they need, in Libs order
  set(bundled "")
  if(third_party)
    list(APPEND bundled "-L\${libdir}/pjproject/third_party")
    foreach(lib IN LISTS third_party)
      list(APPEND bundled "-l${lib}")
    endforeach()
    foreach(lib IN LISTS pj_libs)
      if(lib IN_LIST third_party_needs)
        list(APPEND bundled "-l${lib}")
      endif()
    endforeach()
  endif()

  # -L and -F first, each once; anything else once, at its last place, as a
  # static link needs a library after everything that uses it
  set(search "")
  set(others "")
  foreach(flag IN LISTS bundled private)
    if(flag MATCHES "^-[LF]")
      if(NOT flag IN_LIST search)
        list(APPEND search "${flag}")
      endif()
    else()
      list(REMOVE_ITEM others "${flag}")
      list(APPEND others "${flag}")
    endif()
  endforeach()
  list(JOIN search " " search)
  list(JOIN others " " others)
  string(STRIP "${search} ${others}" PJ_INSTALL_LDFLAGS_PRIVATE)

  set(PJ_INSTALL_LDFLAGS "-L\${libdir}")
  foreach(lib IN LISTS pj_libs)
    string(APPEND PJ_INSTALL_LDFLAGS " -l${lib}")
  endforeach()
  set(PJ_INSTALL_CFLAGS "-I\${includedir}")
  foreach(def IN LISTS defs)
    string(APPEND PJ_INSTALL_CFLAGS " -D${def}")
  endforeach()

  if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(LIBDIR "${CMAKE_INSTALL_LIBDIR}")
  else()
    file(RELATIVE_PATH up "/x/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "/x")
    string(REGEX REPLACE "/$" "" up "${up}")
    set(PREFIX "\${pcfiledir}/${up}")
    set(LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
  endif()
  if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
  else()
    set(INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
  endif()
  set(PJ_VERSION "${PROJECT_VERSION}")

  file(READ "${template}" content)
  string(CONFIGURE "${content}" content @ONLY)
  # The template, shared with the GNU build, has no Requires.private
  if(requires)
    list(JOIN requires ", " requires)
    string(REPLACE "\nLibs:" "\nRequires.private: ${requires}\nLibs:"
      content "${content}")
  endif()
  # Written as it is, as a second substitution pass could alter a path with
  # an "@" in it; and only when changed, so it is not rewritten every time
  set(output "${CMAKE_BINARY_DIR}/libpjproject.pc")
  set(previous "")
  if(EXISTS "${output}")
    file(READ "${output}" previous)
  endif()
  if(NOT content STREQUAL previous)
    file(WRITE "${output}" "${content}")
  endif()
  install(
    FILES "${output}"
    DESTINATION "${destination}"
    COMPONENT PjDevelopment
  )
endfunction()
