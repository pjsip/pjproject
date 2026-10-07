# Generates libpjproject.pc from libpjproject.pc.in, the template the GNU build
# fills too.
#
# Libs lists the PJSIP libraries and Cflags the compile definitions consumers
# need. Libs.private lists, for `pkg-config --static`, what those libraries
# link. Like the GNU build's, the file has no Requires.private, so using it
# does not depend on the dependencies' own .pc files being installed.
#
# Only targets this project builds are walked. Their link and compile
# properties hold a few generator expressions, all evaluated here, so nothing
# depends on how a dependency's own targets are written. A dependency target
# is linked through its library file instead, which
# pj_pc_remember_imported_targets() records in the directory that found it,
# where the target is visible: when pkg-config has a module for that same
# library, the module's own `--static --libs` flags are used, otherwise the
# file as -L/-l.
#
# Everything is known at configure time, so the .pc is written then, and
# installed like any other file. Its prefix is relative to the file itself,
# so it follows `cmake --install --prefix` and a relocated installation.

# Dependency targets: "<target>|<pkg-config modules>|<libraries to link when
# no file is known>". The modules are alternatives.
set(_PJ_PC_DEPENDENCIES
  "OpenSSL::SSL|libssl|ssl"
  "OpenSSL::Crypto|libcrypto|crypto"
  "GnuTLS::GnuTLS|gnutls|gnutls"
  "MbedTLS::mbedtls|mbedtls|mbedtls"
  "MbedTLS::mbedx509|mbedx509|mbedx509"
  "MbedTLS::mbedcrypto|mbedcrypto|mbedcrypto"
  "UUID::UUID|uuid,libuuid|uuid"
  "UPNP::UPNP|libupnp,upnp|upnp,ixml"
  "ALSA::ALSA|alsa|asound"
  "SDL2::SDL2|sdl2|SDL2"
  "OpenGL::GL|gl|GL"
  "OpenGL::GLES2|glesv2|GLESv2"
  "V4L2::V4L2|libv4l2,v4l2|v4l2"
  "FFMPEG::avutil|libavutil|avutil"
  "FFMPEG::swscale|libswscale|swscale"
  "FFMPEG::avcodec|libavcodec|avcodec"
  "FFMPEG::avformat|libavformat|avformat"
  "FFMPEG::avdevice|libavdevice|avdevice"
  "OPUS::OPUS|opus|opus"
  "VPX::VPX|vpx|vpx"
  "OpenH264::OpenH264|openh264|openh264"
  "Silk::Silk|SKP_SILK_SDK|SKP_SILK_SDK"
  "BCG729::BCG729|bcg729,libbcg729|bcg729"
  "Lyra::Lyra|lyra|lyra"
  "OpenCoreAMRNB::OpenCoreAMRNB|opencore-amrnb|opencore-amrnb"
  "OpenCoreAMRWB::OpenCoreAMRWB|opencore-amrwb|opencore-amrwb"
  "VisualOnAMRWBEnc::VisualOnAMRWBEnc|vo-amrwbenc,libvo-amrwbenc|vo-amrwbenc"
  "SpeexDSP::SpeexDSP|speexdsp,libspeexdsp|speexdsp"
  "SampleRate::SampleRate|samplerate,libsamplerate|samplerate"
  "Oboe::Oboe||oboe"
  # PJ_DEP_<name>=system
  "SRTP::SRTP|libsrtp3,libsrtp2,srtp2|srtp2"
  "Speex::Speex|speex,libspeex|speex"
  "GSM::GSM|gsm,libgsm|gsm"
  "YUV::YUV|libyuv,yuv|yuv"
  "Resample::Resample|resample,libresample|resample"
)

# Options that take their argument as the next item, e.g. -framework;Foo
set(_PJ_PC_PAIRED_OPTIONS "-framework;-weak_framework;-Xlinker")

# The library files of the imported library target `target`: its file, and
# the absolute paths it lists in INTERFACE_LINK_LIBRARIES (e.g. libixml for
# UPNP::UPNP). Nothing else of that list is read: a dependency's targets may
# use any generator expression.
function(_pj_pc_target_files out target)
  set(files "")
  get_target_property(configs ${target} IMPORTED_CONFIGURATIONS)
  set(suffixes _RELEASE _NOCONFIG "")
  if(configs)
    foreach(config IN LISTS configs)
      string(TOUPPER "_${config}" config)
      list(APPEND suffixes "${config}")
    endforeach()
  endif()
  foreach(suffix IN LISTS suffixes)
    foreach(property IN ITEMS IMPORTED_IMPLIB IMPORTED_LOCATION)
      get_target_property(path ${target} ${property}${suffix})
      if(path)
        list(APPEND files "${path}")
        break()
      endif()
    endforeach()
    if(files)
      break()
    endif()
  endforeach()
  get_target_property(items ${target} INTERFACE_LINK_LIBRARIES)
  foreach(item IN LISTS items)
    if(IS_ABSOLUTE "${item}" AND NOT item MATCHES "\\$<")
      list(APPEND files "${item}")
    endif()
  endforeach()
  set(${out} "${files}" PARENT_SCOPE)
endfunction()

# Records the library files of the imported library targets found in the
# current directory, while they are visible. To be called at the end of each
# directory that finds dependencies of the PJSIP libraries.
function(pj_pc_remember_imported_targets)
  get_directory_property(targets IMPORTED_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "INTERFACE_LIBRARY")
      _pj_pc_target_files(files ${target})
      set_property(GLOBAL PROPERTY "_PJ_PC_FILES_${target}" "${files}")
    endif()
  endforeach()
endfunction()

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
# or anything inside a sysroot or an Apple SDK. CMAKE_OSX_SYSROOT may be
# empty, or an SDK name such as iphoneos, rather than a path.
function(_pj_pc_searched out dir)
  set(${out} FALSE PARENT_SCOPE)
  if(dir IN_LIST CMAKE_C_IMPLICIT_LINK_DIRECTORIES
     OR dir MATCHES "/SDKs/[^/]+\\.sdk(/|$)")
    set(${out} TRUE PARENT_SCOPE)
    return()
  endif()
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

# A -L or -F directory flag, or nothing when the linker searches it anyway
function(_pj_pc_dir_flag out option dir)
  _pj_pc_stable_dir(dir "${dir}")
  _pj_pc_searched(searched "${dir}")
  if(searched)
    set(${out} "" PARENT_SCOPE)
  else()
    _pj_pc_quote(dir "${dir}")
    set(${out} "${option}${dir}" PARENT_SCOPE)
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

  if(path MATCHES "/([^/]+)\\.framework/?$")
    _pj_pc_dir_flag(flags "-F" "${dir}")
    list(APPEND flags "-framework ${CMAKE_MATCH_1}")
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
    _pj_pc_stable_dir(dir "${dir}")
    set(${out} "\"${dir}/${name}\"" PARENT_SCOPE)
    return()
  endif()
  _pj_pc_dir_flag(flags "-L" "${dir}")
  list(APPEND flags "-l${lib}")
  set(${out} "${flags}" PARENT_SCOPE)
endfunction()

# The `--static --libs` flags of the pkg-config module among `modules` that
# describes the library at `path`, found by comparing its libdir: a module
# found elsewhere may describe another copy, e.g. the host's when
# cross-compiling, or MSYS2's in an MSVC build. Results are cached, keyed on
# the modules, the path and pkg-config's settings, to keep pkg-config out of
# every reconfigure.
function(_pj_pc_module_flags out found modules path)
  set(${found} FALSE PARENT_SCOPE)
  set(${out} "" PARENT_SCOPE)
  if(NOT PKG_CONFIG_EXECUTABLE OR NOT modules)
    return()
  endif()
  set(key "${modules}|${path}|${PKG_CONFIG_EXECUTABLE}")
  foreach(var IN ITEMS PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR)
    string(APPEND key "|$ENV{${var}}")
  endforeach()
  string(MD5 key "${key}")
  set(cache "_PJ_PC_MODULE_${key}")
  if(NOT DEFINED CACHE{${cache}})
    set(result "NONE")
    get_filename_component(want "${path}" DIRECTORY)
    file(REAL_PATH "${want}" want)
    foreach(module IN LISTS modules)
      execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=libdir "${module}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE libdir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(NOT status EQUAL 0 OR NOT IS_DIRECTORY "${libdir}")
        continue()
      endif()
      file(REAL_PATH "${libdir}" libdir)
      if(NOT libdir STREQUAL want)
        continue()
      endif()
      execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" --static --libs "${module}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE libs
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(status EQUAL 0)
        separate_arguments(libs UNIX_COMMAND "${libs}")
        set(result "FOUND;${libs}")
        break()
      endif()
    endforeach()
    set(${cache} "${result}" CACHE INTERNAL "pkg-config result")
  endif()

  set(result "${${cache}}")
  list(POP_FRONT result state)
  if(NOT state STREQUAL "FOUND")
    return()
  endif()
  set(flags "")
  foreach(flag IN LISTS result)
    if(flag MATCHES "^-([LF])(.+)$")
      _pj_pc_dir_flag(flag "-${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    else()
      _pj_pc_quote(flag "${flag}")
    endif()
    if(NOT flag STREQUAL "")
      list(APPEND flags "${flag}")
    endif()
  endforeach()
  _pj_pc_pair_options(flags "${flags}")
  set(${found} TRUE PARENT_SCOPE)
  set(${out} "${flags}" PARENT_SCOPE)
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

  # The library files: recorded where the target was found, or read here
  # when the target is global
  get_property(recorded GLOBAL PROPERTY "_PJ_PC_FILES_${target}" SET)
  if(recorded)
    get_property(paths GLOBAL PROPERTY "_PJ_PC_FILES_${target}")
  elseif(TARGET "${target}")
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
      # This project's own Find modules make their interface targets (Apple
      # frameworks, WASAPI, ...) global; they are read like its own
      _pj_pc_visit(${target})
      return()
    endif()
    _pj_pc_target_files(paths ${target})
  else()
    set(paths "")
  endif()

  set(modules "")
  set(names "")
  foreach(entry IN LISTS _PJ_PC_DEPENDENCIES)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 name)
    if(name STREQUAL target)
      list(GET fields 1 modules)
      list(GET fields 2 names)
      string(REPLACE "," ";" modules "${modules}")
      string(REPLACE "," ";" names "${names}")
      break()
    endif()
  endforeach()

  if(NOT paths AND NOT names)
    message(AUTHOR_WARNING
      "pkg-config: no library file or entry for ${target}, which "
      "libpjproject.pc leaves out. Add it to cmake/Pj/PkgConfig.cmake.")
    return()
  endif()

  set(flags "")
  if(paths)
    # A module's -l would pick a shared library over a static one CMake
    # linked deliberately, e.g. with OPENSSL_USE_STATIC_LIBS
    set(static FALSE)
    foreach(path IN LISTS paths)
      _pj_pc_static_beside_shared(ambiguous "${path}")
      if(ambiguous)
        set(static TRUE)
      endif()
    endforeach()
    set(found FALSE)
    if(NOT static)
      list(GET paths 0 first)
      _pj_pc_module_flags(flags found "${modules}" "${first}")
    endif()
    if(NOT found)
      foreach(path IN LISTS paths)
        _pj_pc_file_flags(file_flags "${path}")
        list(APPEND flags ${file_flags})
      endforeach()
    endif()
  else()
    foreach(lib IN LISTS names)
      list(APPEND flags "-l${lib}")
    endforeach()
  endif()
  list(REVERSE flags)
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})
endfunction()

# Records what `target`, a target this project builds or an interface target
# of its Find modules, links, depth-first, then the target itself, so that the
# reversed result lists every library before what it needs, as a static link
# needs
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
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY "")
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY_NEEDS "")
  foreach(lib IN LISTS pj_libs)
    _pj_pc_visit(${lib})
  endforeach()
  get_property(private GLOBAL PROPERTY _PJ_PC_PRIVATE)
  get_property(third_party GLOBAL PROPERTY _PJ_PC_THIRD_PARTY)
  get_property(third_party_needs GLOBAL PROPERTY _PJ_PC_THIRD_PARTY_NEEDS)
  list(REVERSE private)
  list(REVERSE third_party)

  # With PJ_WITH_CXX, a static link from C needs the C++ runtime
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
  # Written as it is, as a second substitution pass could alter a path with
  # an "@" in it; and only when changed, so it is not rewritten every time.
  # In this project's binary directory, not the top-level one of a project
  # that includes it with add_subdirectory().
  set(output "${PROJECT_BINARY_DIR}/libpjproject.pc")
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
