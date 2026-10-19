find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Lyra QUIET lyra)
  set(Lyra_VERSION ${PC_Lyra_VERSION})
endif()

find_path(Lyra_INCLUDE_DIR
  NAMES
    "lyra_decoder.h"
  HINTS
    ${PC_Lyra_INCLUDEDIR}
    ${PC_Lyra_INCLUDE_DIRS}
)
mark_as_advanced(Lyra_INCLUDE_DIR)

find_library(Lyra_LIBRARY
  NAMES
    lyra
  HINTS
    ${PC_Lyra_LIBDIR}
    ${PC_Lyra_LIBRARY_DIRS}
)
mark_as_advanced(Lyra_LIBRARY)

# Headers of Lyra's dependencies, laid out as for autoconf --with-lyra=DIR
# (DIR/include/<bazel repo>), or installed on the system
set(_lyra_dep_hints "${Lyra_INCLUDE_DIR}" "${Lyra_INCLUDE_DIR}/..")
find_path(Lyra_ABSL_INCLUDE_DIR
  NAMES "absl/types/span.h"
  HINTS ${_lyra_dep_hints}
  PATH_SUFFIXES include/com_google_absl
)
find_path(Lyra_FILESYSTEM_INCLUDE_DIR
  NAMES "include/ghc/filesystem.hpp"
  HINTS ${_lyra_dep_hints}
  PATH_SUFFIXES include/gulrak_filesystem
)
find_path(Lyra_GLOG_INCLUDE_DIR
  NAMES "glog/logging.h"
  HINTS ${_lyra_dep_hints}
  PATH_SUFFIXES include/com_google_glog/src
)
find_path(Lyra_MODEL_PATH
  NAMES "lyra_config.binarypb"
  HINTS ${_lyra_dep_hints}
  PATH_SUFFIXES model_coeffs
  NO_DEFAULT_PATH
)
mark_as_advanced(
  Lyra_ABSL_INCLUDE_DIR
  Lyra_FILESYSTEM_INCLUDE_DIR
  Lyra_GLOG_INCLUDE_DIR
  Lyra_MODEL_PATH
)
unset(_lyra_dep_hints)

# glog sources used without its generated export header
set(_lyra_definitions
  "GLOG_DEPRECATED=__attribute__((deprecated))"
  "GLOG_EXPORT=__attribute__((visibility(\"default\")))"
  "GLOG_NO_EXPORT=__attribute__((visibility(\"default\")))"
)

if(Lyra_INCLUDE_DIR AND Lyra_LIBRARY AND Lyra_ABSL_INCLUDE_DIR
   AND Lyra_FILESYSTEM_INCLUDE_DIR AND Lyra_GLOG_INCLUDE_DIR)
  set(_lyra_check_definitions)
  foreach(_def IN LISTS _lyra_definitions)
    list(APPEND _lyra_check_definitions "-D${_def}")
  endforeach()

  set(_lyra_check_libs "${Lyra_LIBRARY}")
  if(Lyra_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    # A static liblyra bundles abseil and TFLite, which need these
    if(APPLE)
      list(APPEND _lyra_check_libs "-framework CoreFoundation")
    elseif(UNIX)
      list(APPEND _lyra_check_libs m ${CMAKE_DL_LIBS})
      if(TARGET Threads::Threads)
        list(APPEND _lyra_check_libs Threads::Threads)
      endif()
    endif()
  endif()

  # Lyra needs C++17
  set(_lyra_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
  if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 17)
    set(CMAKE_CXX_STANDARD 17)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(Lyra
    LANGUAGE CXX
    CODE "
#include \"lyra_decoder.h\"
int main() {
  std::unique_ptr<chromemedia::codec::LyraDecoder> dec =
    chromemedia::codec::LyraDecoder::Create(8000, 1, \"\");
  return 0;
}
"
    INCLUDE_DIRS
      ${Lyra_INCLUDE_DIR}
      ${Lyra_ABSL_INCLUDE_DIR}
      ${Lyra_FILESYSTEM_INCLUDE_DIR}
      ${Lyra_GLOG_INCLUDE_DIR}
    LIBRARIES ${_lyra_check_libs}
    DEFINITIONS ${_lyra_check_definitions}
  )

  if(_lyra_saved_cxx_standard)
    set(CMAKE_CXX_STANDARD "${_lyra_saved_cxx_standard}")
  else()
    unset(CMAKE_CXX_STANDARD)
  endif()
  unset(_lyra_saved_cxx_standard)
  unset(_lyra_check_definitions)
  unset(_lyra_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lyra
  REQUIRED_VARS
    Lyra_LIBRARY
    Lyra_INCLUDE_DIR
    Lyra_ABSL_INCLUDE_DIR
    Lyra_FILESYSTEM_INCLUDE_DIR
    Lyra_GLOG_INCLUDE_DIR
    Lyra_USABLE
  VERSION_VAR
    Lyra_VERSION
)

if(Lyra_FOUND)
  set(Lyra_LIBRARIES ${Lyra_LIBRARY})
  set(Lyra_INCLUDE_DIRS
    ${Lyra_INCLUDE_DIR}
    ${Lyra_ABSL_INCLUDE_DIR}
    ${Lyra_FILESYSTEM_INCLUDE_DIR}
    ${Lyra_GLOG_INCLUDE_DIR}
  )
  set(Lyra_DEFINITIONS ${PC_Lyra_CFLAGS_OTHER})

  if(NOT TARGET Lyra::Lyra)
    add_library(Lyra::Lyra UNKNOWN IMPORTED)
    set_target_properties(Lyra::Lyra PROPERTIES
      IMPORTED_LOCATION "${Lyra_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Lyra_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${_lyra_definitions}"
      INTERFACE_COMPILE_OPTIONS "${Lyra_DEFINITIONS}"
      INTERFACE_COMPILE_FEATURES cxx_std_17
    )
    if(Lyra_MODEL_PATH)
      # default model directory, as autoconf: DIR/model_coeffs
      set_target_properties(Lyra::Lyra PROPERTIES
        LYRA_MODEL_PATH "${Lyra_MODEL_PATH}"
      )
    endif()
  endif()
endif()
unset(_lyra_definitions)
