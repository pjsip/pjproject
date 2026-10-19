include_guard(GLOBAL)

include(CheckSourceCompiles)
include(CMakePushCheckState)

# pj_check_library_usable(<package>
#   <CODE <source> | FUNCTION <name>>
#   [LANGUAGE <C|CXX>]
#   [INCLUDE_DIRS <dir>...]
#   [LIBRARIES <lib>...]
#   [DEFINITIONS <flag>...]
# )
#
# Compile and link <source> against a library a Find module has located, to
# reject one that is present but unusable (e.g. built for another target).
# FUNCTION <name> instead links a call to that C function without its header,
# like autoconf's AC_CHECK_LIB.
# Sets the cache variable <package>_USABLE; pass it to
# find_package_handle_standard_args() as a REQUIRED_VARS entry. The check is
# re-run whenever its inputs change. If LANGUAGE (default C) is not enabled in
# the calling project, the check is skipped and the library assumed usable.
function(pj_check_library_usable package)
  cmake_parse_arguments(PARSE_ARGV 1 arg
    "" "CODE;FUNCTION;LANGUAGE" "INCLUDE_DIRS;LIBRARIES;DEFINITIONS")
  if(arg_FUNCTION AND NOT arg_CODE)
    set(arg_CODE "
#ifdef __cplusplus
extern \"C\"
#endif
char ${arg_FUNCTION}(void);
int main(void) { return ${arg_FUNCTION}(); }
")
  elseif(NOT arg_CODE OR arg_FUNCTION)
    message(FATAL_ERROR
      "pj_check_library_usable: exactly one of CODE or FUNCTION is required")
  endif()
  if(NOT arg_LANGUAGE)
    set(arg_LANGUAGE C)
  endif()

  get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  if(NOT arg_LANGUAGE IN_LIST _languages)
    set(${package}_USABLE TRUE PARENT_SCOPE)
    return()
  endif()

  string(SHA1 _inputs "${arg_LANGUAGE}|${arg_INCLUDE_DIRS}|${arg_LIBRARIES}|${arg_DEFINITIONS}|${arg_CODE}")
  if(NOT _inputs STREQUAL "$CACHE{_PJ_${package}_USABLE_INPUTS}")
    unset(${package}_USABLE CACHE)
    set(_PJ_${package}_USABLE_INPUTS "${_inputs}" CACHE INTERNAL "")
  endif()

  cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES ${arg_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES ${arg_LIBRARIES})
    set(CMAKE_REQUIRED_DEFINITIONS ${arg_DEFINITIONS})
    set(CMAKE_REQUIRED_QUIET ${${package}_FIND_QUIETLY})
    check_source_compiles(${arg_LANGUAGE} "${arg_CODE}" ${package}_USABLE)
  cmake_pop_check_state()
endfunction()
