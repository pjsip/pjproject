function(pj_get_macro_value header key out_value)
  # parse arguments
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "TYPE;PATTERN;PATTERN_GROUP" "")

  # validate access
  if(NOT IS_READABLE "${header}")
    message(WARNING "Could not get read access to: ${header}")
    set("${out_value}" "" PARENT_SCOPE)
    return()
  endif()

  set(value_group 1)

  # use default type if not set
  if(NOT DEFINED arg_TYPE OR arg_TYPE STREQUAL "")
    set(arg_TYPE "number")
  endif()

  # get matching pattern
  set(value_group 1)
  if(arg_TYPE STREQUAL "string")
    set(value_pattern [["([^"]*)"]])
  elseif(arg_TYPE STREQUAL "pattern")
    if(arg_PATTERN STREQUAL "")
      message(FATAL_ERROR "Regex pattern is required")
    endif()
    set(value_pattern "${arg_PATTERN}")
    if(NOT arg_PATTERN_GROUP STREQUAL "")
      set(value_group "${arg_PATTERN_GROUP}")
    endif()
  elseif(arg_TYPE STREQUAL "number")
    set(value_pattern "([0-9]+)")
  else()
    message(FATAL_ERROR "Invalid value type: ${arg_TYPE}")
  endif()

  # build macro pattern
  set(macro_pattern "#define[ \t]+${key}[ \t]+${value_pattern}")

  # read macro line
  file(STRINGS "${header}" macro_line REGEX "${macro_pattern}" LIMIT_COUNT 1)

  # extract macro value
  string(REGEX REPLACE "${macro_pattern}" "\\${value_group}" macro_value
    "${macro_line}"
  )

  # set output value
  set(${out_value} "${macro_value}" PARENT_SCOPE)
endfunction()
