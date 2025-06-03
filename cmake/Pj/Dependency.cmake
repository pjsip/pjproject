# provider keys
set(PJ_DEPENDENCY_PROVIDER_SUBDIR "vendored")
set(PJ_DEPENDENCY_PROVIDER_PACKAGE "system")

# valid target name regex pattern
set(_target_segment "[A-Za-z][A-Za-z0-9_.+-]*")
set(_target_pattern "^${_target_segment}(::${_target_segment})*$")

# dependency provider property
set(_provider_prop "PJ_DEPENDENCY_PROVIDER")

# dependency provider option prefix & suffix
if(NOT PJ_DEPENDENCY_PREFIX)
  set(PJ_DEPENDENCY_PREFIX "PJ_DEPENDENCY")
endif()
if(NOT PJ_DEPENDENCY_SUFFIX)
  set(PJ_DEPENDENCY_SUFFIX "PROVIDER")
endif()

# targets namespace
if(NOT PJ_DEPENDENCY_NAMESPACE)
  set(PJ_DEPENDENCY_NAMESPACE "Pj::Dep")
endif()

# Handles custom target name override for provider source properties
function(_pj_dependency_split_provider_args args out_source out_target)
  list(POP_FRONT args source target)
  if(source)
    if(args)
      message(FATAL_ERROR "Unexpected provider arguments were given: ${args}")
    endif()

    if(NOT target)
      set(target "${source}")
    endif()
    if(NOT target MATCHES "${_target_pattern}")
      message(FATAL_ERROR "Invalid target name: ${target}")
    endif()

    set(${out_source} "${source}" PARENT_SCOPE)
    set(${out_target} "${target}" PARENT_SCOPE)
  endif()
endfunction()

# Declares dependnecy option
function(pj_dependency_option name description out_var)
  #
  # argument handling
  #

  # parse options
  set(options REQUIRED)
  set(one_value_args PREFER)
  set(multi_value_args PROVIDERS)
  cmake_parse_arguments(PARSE_ARGV 1 arg
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
  )

  # validate name
  if(NOT name)
    message(FATAL_ERROR "Dependency name is required")
  endif()

  # validate providers
  if(NOT arg_PROVIDERS)
    message(FATAL_ERROR "Dependency '${name}' must have at least one provider")
  endif()
  if(arg_PREFER AND NOT arg_PREFER IN_LIST arg_PROVIDERS)
    message(FATAL_ERROR "Invalid preferred provider: ${arg_PREFER}")
  endif()

  #
  # provider option
  #

  # if description is empty, set default value
  if(NOT description)
    set(description "${name} dependency provider")
  endif()

  # set option requirement
  if(arg_REQUIRED)
    set(option_required "REQUIRED")
  endif()

  # build option key
  string(REPLACE "::" "_" normalized_name "${name}")
  string(TOUPPER
    "${PJ_DEPENDENCY_PREFIX}_${normalized_name}_${PJ_DEPENDENCY_SUFFIX}"
    option_var
  )

  # declare option
  include(Pj/Option)
  pj_option("${option_var}" "${arg_DESCRIPTION}"
    ${option_required}
    DEFAULT "${arg_PREFER}"
    ALLOWED_VALUES ${arg_PROVIDERS}
  )

  # set output value
  set(${out_var} "${${option_var}}" PARENT_SCOPE)
endfunction()

function(pj_dependency_export_target name src_target provider)
  if(NOT name OR NOT src_target)
    message(FATAL_ERROR "Missing required values")
  endif()

  set(dst_target "${PJ_DEPENDENCY_NAMESPACE}::${name}")
  if(TARGET "${dst_target}")
    message(FATAL_ERROR "Target ${dst_target} already exists")
  endif()

  add_library("${dst_target}" ALIAS "${src_target}")

  # set provider property on source target
  if(provider)
    set_target_properties("${src_target}" PROPERTIES
      "${_provider_prop}" "${provider}"
    )
  endif()
endfunction()

# Defines a dependency
function(pj_dependency name)
  #
  # argument handling
  #

  # parse options
  set(options REQUIRED)
  set(one_value_args DESCRIPTION PREFER)
  set(multi_value_args SUBDIR PACKAGE)
  cmake_parse_arguments(PARSE_ARGV 1 arg
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
  )

  # validate name
  if(NOT name)
    message(FATAL_ERROR "Dependency name is required")
  elseif(NOT name MATCHES "${_target_pattern}")
    message(FATAL_ERROR "Invalid name value: ${name}")
  endif()

  # normalize & validate subdirectory/subdirectory target
  _pj_dependency_split_provider_args("${arg_SUBDIR}" subdir subdir_target)
  if(subdir)
    list(APPEND providers "${PJ_DEPENDENCY_PROVIDER_SUBDIR}")
  endif()

  # normalize & validate package/package target
  _pj_dependency_split_provider_args("${arg_PACKAGE}" package package_target)
  if(package)
    list(APPEND providers "${PJ_DEPENDENCY_PROVIDER_PACKAGE}")

  endif()

  #
  # dependency importing
  #

  if(arg_REQUIRED)
    set(option_required "REQUIRED")
  endif()

  pj_dependency_option("${name}" "${arg_DESCRIPTION}" provider
    ${option_required}
    PREFER "${arg_PREFER}"
    PROVIDERS ${providers}
  )

  if(provider STREQUAL "${PJ_DEPENDENCY_PROVIDER_SUBDIR}")
    add_subdirectory("${subdir}" EXCLUDE_FROM_ALL)
    set(target "${subdir_target}")
  elseif(provider STREQUAL "${PJ_DEPENDENCY_PROVIDER_PACKAGE}")
    find_package("${package}" QUIET GLOBAL)
    set(target "${package_target}")
  endif()

  if(TARGET "${target}")
    pj_dependency_export_target("${name}" "${target}" "${provider}")
  elseif(arg_REQUIRED)
    message(FATAL_ERROR "Required dependency ${name} could not be satisfied")
  endif()

endfunction()

# Checks whether a dependency was imported from an external source or not
function(pj_dependency_is_external target out_result)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "No such target: ${target}")
  endif()

  get_target_property(_provider "${target}" "${_provider_prop}")
  if(_provider STREQUAL "${PJ_DEPENDENCY_PROVIDER_PACKAGE}")
    set(${out_result} TRUE PARENT_SCOPE)
  else()
    set(${out_result} OFF PARENT_SCOPE)
  endif()
endfunction()
