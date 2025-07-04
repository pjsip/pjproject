function(pj_option variable help_text)
  #
  # argument handling
  #

  # parse options
  set(options REQUIRED MULTI_VALUE)
  set(oneValueArgs DEFAULT)
  set(multiValueArgs ALLOWED_VALUES)
  cmake_parse_arguments(PARSE_ARGV 2 arg
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
  )

  #
  # option defintion
  #

  # setup doc string
  if(arg_ALLOWED_VALUES)
    # prepare choices
    string(REPLACE ";" "|" optChoices "${arg_ALLOWED_VALUES}")
    if(arg_REQUIRED)
      set(optChoices "<${optChoices}>")
    else()
      set(optChoices "[${optChoices}]")
    endif()

    # update help text
    if(help_text)
      set(help_text "${help_text}: ${optChoices}")
    else()
      set(help_text "${optChoices}")
    endif()
  endif()

  # define option cache variable
  set(${variable} "${arg_DEFAULT}" CACHE STRING "${help_text}")
  set_property(CACHE "${variable}" PROPERTY STRINGS ${arg_ALLOWED_VALUES})

  #
  # Option value validation
  #

  set(value "${${variable}}")

  if(value)
    if(arg_ALLOWED_VALUES)
      if(arg_MULTI_VALUE)
        foreach(v IN LISTS value)
          if(NOT v IN_LIST arg_ALLOWED_VALUES)
            message(FATAL_ERROR
              "Illegal value for ${variable}: \
               ${v} in (${value}), allowed: ${OPT_ALLOWED_VALUES}")
          endif()
        endforeach()
      else()
        if(NOT value IN_LIST arg_ALLOWED_VALUES)
          message(FATAL_ERROR
            "Illegal value for ${variable}: \
             ${value}, allowed: ${OPT_ALLOWED_VALUES}")
        endif()
      endif()
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
