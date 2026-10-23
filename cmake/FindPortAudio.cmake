find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_PortAudio QUIET portaudio-2.0)
  set(PortAudio_VERSION ${PC_PortAudio_VERSION})
endif()

find_path(PortAudio_INCLUDE_DIR
  NAMES
    portaudio.h
  HINTS
    ${PC_PortAudio_INCLUDEDIR}
    ${PC_PortAudio_INCLUDE_DIRS}
)
mark_as_advanced(PortAudio_INCLUDE_DIR)

find_library(PortAudio_LIBRARY
  NAMES
    portaudio
  HINTS
    ${PC_PortAudio_LIBDIR}
    ${PC_PortAudio_LIBRARY_DIRS}
)
mark_as_advanced(PortAudio_LIBRARY)

if(PortAudio_INCLUDE_DIR AND PortAudio_LIBRARY)
  # A static libportaudio needs the host audio APIs it was built with
  set(_portaudio_link_libs)
  if(PortAudio_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    if(APPLE)
      foreach(_fw CoreAudio AudioToolbox AudioUnit CoreFoundation CoreServices)
        list(APPEND _portaudio_link_libs "-framework ${_fw}")
      endforeach()
    elseif(UNIX)
      list(REMOVE_ITEM PC_PortAudio_STATIC_LIBRARIES portaudio)
      list(APPEND _portaudio_link_libs ${PC_PortAudio_STATIC_LIBRARIES} m)
      find_package(Threads QUIET)
      if(TARGET Threads::Threads)
        list(APPEND _portaudio_link_libs Threads::Threads)
      endif()
    endif()
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(PortAudio
    INCLUDE_DIRS "${PortAudio_INCLUDE_DIR}"
    LIBRARIES "${PortAudio_LIBRARY}" ${_portaudio_link_libs}
    DEFINITIONS ${PC_PortAudio_CFLAGS_OTHER}
    CODE [=[
      #include <portaudio.h>
      int main(void) { return Pa_Initialize(); }
    ]=]
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio
  REQUIRED_VARS
    PortAudio_LIBRARY
    PortAudio_INCLUDE_DIR
    PortAudio_USABLE
  VERSION_VAR
    PortAudio_VERSION
)

if(PortAudio_FOUND)
  set(PortAudio_LIBRARIES ${PortAudio_LIBRARY})
  set(PortAudio_INCLUDE_DIRS ${PortAudio_INCLUDE_DIR})
  set(PortAudio_DEFINITIONS ${PC_PortAudio_CFLAGS_OTHER})

  if(NOT TARGET PortAudio::PortAudio)
    add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
    set_target_properties(PortAudio::PortAudio PROPERTIES
      IMPORTED_LOCATION "${PortAudio_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${PortAudio_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${PortAudio_DEFINITIONS}"
      INTERFACE_LINK_LIBRARIES "${_portaudio_link_libs}"
    )
  endif()
endif()
unset(_portaudio_link_libs)
