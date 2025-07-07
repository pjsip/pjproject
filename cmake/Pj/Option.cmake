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
