function(pj_option variable help_text)
  # parse options
  set(options REQUIRED)
  set(oneValueArgs DEFAULT)
  set(multiValueArgs ALLOWED_VALUES)
  cmake_parse_arguments(PARSE_ARGV 2 arg
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
  )

  # setup doc string
  if(arg_ALLOWED_VALUES)
    string(REPLACE ";" "|" optChoices "${arg_ALLOWED_VALUES}")
    if(arg_REQUIRED)
      set(optChoices "<${optChoices}>")
    else()
      set(optChoices "[${optChoices}]")
    endif()

    if(help_text)
      set(help_text "${help_text}: ${optChoices}")
    else()
      set(help_text "${optChoices}")
    endif()
  endif()

  # define option cache variable
  set(${variable} "${arg_DEFAULT}" CACHE STRING "${help_text}")
  set_property(CACHE "${variable}" PROPERTY STRINGS ${arg_ALLOWED_VALUES})

  # validate option value
  set(value "${${variable}}")
  if(value)
    if(arg_ALLOWED_VALUES AND NOT value IN_LIST arg_ALLOWED_VALUES)
      message(FATAL_ERROR "Illegal value for ${variable}: \
                           ${value}, allowed: ${arg_ALLOWED_VALUES}")
    endif()
  elseif(arg_REQUIRED)
    message(FATAL_ERROR "Option '${variable}' is required")
  endif()
endfunction()

function(pj_force_set variable value)
  # If not a cache variable, set a new internal value
  if(NOT DEFINED CACHE{${variable}})
    set(${variable} "${value}" CACHE INTERNAL "(forced)")
    return()
  endif()

  # remember the value before the first forcing, for pj_retry_on_new_root()
  if(NOT DEFINED CACHE{_PJ_${variable}_UNFORCED})
    set(_PJ_${variable}_UNFORCED "${${variable}}" CACHE INTERNAL "")
  endif()

  # update value
  set_property(CACHE ${variable} PROPERTY VALUE "${value}")

  # update help string
  get_property(docstr CACHE ${variable} PROPERTY HELPSTRING)
  if(NOT docstr MATCHES "\\(forced\\)$")
    set_property(CACHE ${variable} PROPERTY HELPSTRING "${docstr} (forced)")
  endif()

  # show warning
  if(ARGC GREATER 2)
    message(STATUS "[!] ${ARGV2}. setting ${variable} to ${value}")
  endif()
endfunction()

# pj_retry_on_new_root(<option> <package>)
#
# When <package>_ROOT or <PACKAGE>_ROOT (variable or environment) differs from
# the previous configure, drop the cached search results of <package> and
# restore <option> if pj_force_set() turned it off, so find_package() searches
# the new root, like re-running configure with --with-<package>=DIR.
function(pj_retry_on_new_root option package)
  string(TOUPPER "${package}" _upper)
  set(_root "${${package}_ROOT}|${${_upper}_ROOT}|$ENV{${package}_ROOT}|$ENV{${_upper}_ROOT}")

  if(NOT DEFINED CACHE{_PJ_${package}_ROOT})
    set(_PJ_${package}_ROOT "${_root}" CACHE INTERNAL "")
    return()
  endif()
  if(_root STREQUAL "$CACHE{_PJ_${package}_ROOT}")
    return()
  endif()
  set(_PJ_${package}_ROOT "${_root}" CACHE INTERNAL "")

  get_cmake_property(_vars CACHE_VARIABLES)
  foreach(_var IN LISTS _vars)
    if(_var MATCHES "^(${package}|${_upper})_" AND NOT _var MATCHES "_ROOT$")
      get_property(_type CACHE ${_var} PROPERTY TYPE)
      if(_type MATCHES "^(FILE)?PATH$")
        unset(${_var} CACHE)
      endif()
    endif()
  endforeach()
  unset(${package}_USABLE CACHE)

  if(DEFINED CACHE{_PJ_${option}_UNFORCED})
    set_property(CACHE ${option} PROPERTY VALUE "$CACHE{_PJ_${option}_UNFORCED}")
    get_property(_doc CACHE ${option} PROPERTY HELPSTRING)
    string(REGEX REPLACE " ?\\(forced\\)$" "" _doc "${_doc}")
    set_property(CACHE ${option} PROPERTY HELPSTRING "${_doc}")
    unset(_PJ_${option}_UNFORCED CACHE)
    message(STATUS "${package} root changed, retrying ${option}")
  endif()
endfunction()
