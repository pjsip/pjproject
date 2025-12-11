macro(_ffmpeg_set_found component status)
  if("${status}")
    set("FFMPEG_${component}_FOUND" 1 PARENT_SCOPE)
  else()
    if("${ARGV2}")
      set("FFMPEG_${component}_NOT_FOUND_MESSAGE" "${ARGV2}" PARENT_SCOPE)
    else()
      set("FFMPEG_${component}_NOT_FOUND_MESSAGE"
        "Could not find ${message}" PARENT_SCOPE)
    endif()
  endif()
endmacro()

function(_ffmpeg_find_component component)
  #
  # argument handling
  #

  # parse options
  set(options)
  set(one_value_args HEADER LIBRARY)
  set(multi_value_args NEEDS)
  cmake_parse_arguments(PARSE_ARGV 1 arg
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
  )

  # validate component
  if(NOT component)
    message(FATAL_ERROR "Component name is required")
  endif()

  # setup default values
  if(NOT arg_HEADER)
    set(arg_HEADER "lib${component}/${component}.h")
  endif()
  if(NOT arg_LIBRARY)
    set(arg_LIBRARY "${component}")
  endif()

  #
  # find component
  #

  find_path("FFMPEG_${component}_INCLUDE_DIR"
    NAMES
      "${arg_HEADER}"
    HINTS
      "${FFMPEG_ROOT}/include"
    PATH_SUFFIXES
      ffmpeg
  )
  mark_as_advanced("FFMPEG_${component}_INCLUDE_DIR")

  if(NOT FFMPEG_${component}_INCLUDE_DIR)
    _ffmpeg_set_found(${component} FALSE
    "Could not find header '${arg_HEADER}' for ${component}."
    )
    return()
  endif()

  find_library("FFMPEG_${component}_LIBRARY"
    NAMES
      "${arg_LIBRARY}"
    HINTS
      "${FFMPEG_ROOT}/lib"
      "${FFMPEG_ROOT}/bin"
  )
  mark_as_advanced("FFMPEG_${component}_LIBRARY")

  if(NOT FFMPEG_${component}_LIBRARY)
    _ffmpeg_set_found(${component} FALSE
      "Could not find library '${arg_LIBRARY}' for ${component}."
    )
    return()
  endif()

  foreach(dep IN LISTS arg_NEEDS)
    if(NOT TARGET "FFMPEG::${dep}")
      _ffmpeg_set_found(${component} FALSE
        "Could not find dependency '${dep}' for ${component}."
      )
      return()
    endif()
  endforeach()

  _ffmpeg_set_found("${component}" TRUE)

  #
  # add target
  #

  if(NOT TARGET "FFMPEG::${component}")
    add_library("FFMPEG::${component}" UNKNOWN IMPORTED)
    # Build the list of dependency targets with FFMPEG:: prefix
    set(_dep_targets)
    foreach(_dep IN LISTS arg_NEEDS)
      list(APPEND _dep_targets "FFMPEG::${_dep}")
    endforeach()
    set_target_properties("FFMPEG::${component}" PROPERTIES
      IMPORTED_LOCATION "${FFMPEG_${component}_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_${component}_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${_dep_targets}"
    )
    unset(_dep_targets)
  endif()


  # try to find version
  cmake_path(APPEND version_header
    "${FFMPEG_${component}_INCLUDE_DIR}"
    "lib${component}"
    "version.h"
  )

  if(IS_READABLE "${version_header}")
    include(Pj/GetMacroValue)
    foreach(label IN ITEMS major minor micro)
      string(TOUPPER "LIB${component}_VERSION_${label}" macro)
      pj_get_macro_value("${version_header}" "${macro}" "${label}")
    endforeach()

    if (NOT major STREQUAL "" AND
        NOT minor STREQUAL "" AND
        NOT micro STREQUAL "")
      set("FFMPEG_${component}_VERSION"
        "${major}.${minor}.${micro}" PARENT_SCOPE
      )
    endif()
  endif()
endfunction()

_ffmpeg_find_component(avutil)
_ffmpeg_find_component(avresample NEEDS avutil)
_ffmpeg_find_component(swresample NEEDS avutil)
_ffmpeg_find_component(swscale NEEDS avutil)
_ffmpeg_find_component(avcodec NEEDS avutil)
_ffmpeg_find_component(avformat NEEDS avutil avcodec)
_ffmpeg_find_component(avfilter NEEDS avutil)
_ffmpeg_find_component(avdevice NEEDS avutil avformat)

set(FFMPEG_VERSION FFMPEG_VERSION-NOTFOUND)
if(TARGET FFMPEG::avutil)
  cmake_path(APPEND _ffmpeg_version_header
    "${FFMPEG_avutil_INCLUDE_DIR}"
    "libavutil"
    "ffversion.h"
  )

  if(IS_READABLE "${_ffmpeg_version_header}")
    include(Pj/GetMacroValue)
    pj_get_macro_value("${_ffmpeg_version_header}" FFMPEG_VERSION
      _ffmpeg_version_value
      PATTERN [["n?([^"]*)"]]
    )

    if(NOT _ffmpeg_version_value STREQUAL "")
      set(FFMPEG_VERSION "${_ffmpeg_version_value}")
    endif()

    unset(_ffmpeg_version_value)
  endif()

  unset(_ffmpeg_version_header)
endif()

set(FFMPEG_INCLUDE_DIRS)
set(FFMPEG_LIBRARIES)
set(_ffmpeg_required_vars)
foreach(_ffmpeg_component IN LISTS FFMPEG_FIND_COMPONENTS)
  if(TARGET "FFMPEG::${_ffmpeg_component}")
    # component variables
    set(FFMPEG_${_ffmpeg_component}_INCLUDE_DIRS
        "${FFMPEG_${_ffmpeg_component}_INCLUDE_DIR}")
    set(FFMPEG_${_ffmpeg_component}_LIBRARIES
        "${FFMPEG_${_ffmpeg_component}_LIBRARY}")
    # ffmpeg variables
    list(APPEND FFMPEG_INCLUDE_DIRS
         "${FFMPEG_${_ffmpeg_component}_INCLUDE_DIRS}")
    list(APPEND FFMPEG_LIBRARIES
         "${FFMPEG_${_ffmpeg_component}_LIBRARIES}")
    # requirement check
    if (FFMEG_FIND_REQUIRED_${_ffmpeg_component})
      list(APPEND _ffmpeg_required_vars
           "FFMPEG_${_ffmpeg_required_vars}_INCLUDE_DIRS")
      list(APPEND _ffmpeg_required_vars
           "FFMPEG_${_ffmpeg_required_vars}_LIBRARIES")
    endif ()
  endif()
endforeach()
unset(_ffmpeg_component)

if (FFMPEG_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMPEG
  REQUIRED_VARS
    FFMPEG_INCLUDE_DIRS
    FFMPEG_LIBRARIES
    ${_ffmpeg_required_vars}
  VERSION_VAR
    FFMPEG_VERSION
  HANDLE_COMPONENTS
)
unset(_ffmpeg_required_vars)
