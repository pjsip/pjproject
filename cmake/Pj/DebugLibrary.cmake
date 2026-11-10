include_guard(GLOBAL)

# pj_import_debug_library(<target> <variable>)
#
# MSVC builds Debug and Release code against different C runtimes, so a library
# installed in both builds must be linked per configuration: a Release static
# library in a Debug program mixes the runtimes (LNK4098). <variable> is the
# cache entry holding the library the imported <target> links. Look for its
# Debug build where vcpkg puts it, <prefix>/debug/lib under the same name, or
# beside it with a "d" suffix, and import it for the Debug configuration. The
# result is cached as <variable>_DEBUG, which can also be set by hand, and is
# looked for again when <variable> changes unless it was. Only on MSVC;
# elsewhere the one library serves every configuration.
function(pj_import_debug_library target variable)
  if(NOT MSVC OR NOT ${variable})
    return()
  endif()
  if(DEFINED CACHE{_PJ_${variable}_DEBUG_FOR}
     AND NOT "$CACHE{_PJ_${variable}_DEBUG_FOR}" STREQUAL "${${variable}}"
     AND "${${variable}_DEBUG}" STREQUAL "$CACHE{_PJ_${variable}_DEBUG_FOUND}")
    unset(${variable}_DEBUG CACHE)
  endif()
  if(NOT ${variable}_DEBUG)
    cmake_path(GET ${variable} PARENT_PATH dir)
    cmake_path(GET dir PARENT_PATH prefix)
    cmake_path(GET ${variable} STEM name)
    cmake_path(GET ${variable} EXTENSION ext)
    set(found "${variable}_DEBUG-NOTFOUND")
    foreach(candidate "${prefix}/debug/lib/${name}${ext}" "${dir}/${name}d${ext}")
      if(EXISTS "${candidate}" AND NOT candidate STREQUAL ${variable})
        set(found "${candidate}")
        break()
      endif()
    endforeach()
    set(${variable}_DEBUG "${found}" CACHE FILEPATH
      "Debug build of ${variable}" FORCE)
    mark_as_advanced(${variable}_DEBUG)
    # what it was derived from and found, to tell a hand-set value apart
    set(_PJ_${variable}_DEBUG_FOR "${${variable}}" CACHE INTERNAL "")
    set(_PJ_${variable}_DEBUG_FOUND "${found}" CACHE INTERNAL "")
  endif()
  if(${variable}_DEBUG)
    set_target_properties(${target} PROPERTIES
      IMPORTED_LOCATION_DEBUG "${${variable}_DEBUG}"
    )
  endif()
endfunction()
