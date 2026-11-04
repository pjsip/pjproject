include_guard(GLOBAL)

# pj_package_roots(<package> <out-var>)
#
# The roots find_package() searches first for <package>: <package>_ROOT and
# <PACKAGE>_ROOT, as variables, then from the environment.
function(pj_package_roots package out)
  string(TOUPPER "${package}" _upper)
  set(_roots ${${package}_ROOT} ${${_upper}_ROOT})
  foreach(_name IN ITEMS ${package}_ROOT ${_upper}_ROOT)
    if(NOT "$ENV{${_name}}" STREQUAL "")
      file(TO_CMAKE_PATH "$ENV{${_name}}" _env_roots)
      list(APPEND _roots ${_env_roots})
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _roots)
  set(${out} "${_roots}" PARENT_SCOPE)
endfunction()

# pj_package_root_given(<package> <out-var>)
#
# TRUE when a root is given for <package>, i.e. the user named where
# find_package() should look.
function(pj_package_root_given package out)
  pj_package_roots(${package} _roots)
  if(_roots)
    set(${out} TRUE PARENT_SCOPE)
  else()
    set(${out} FALSE PARENT_SCOPE)
  endif()
endfunction()

# pj_pkg_search_module(<prefix> <module>...)
#
# pkg_search_module(<prefix> QUIET <module>...) for a Find module, after
# find_package(PkgConfig). When a root is given for the package being found,
# only a .pc file in the root counts, and only for <prefix>_VERSION,
# _CFLAGS_OTHER and _STATIC_LIBRARIES; the directories are left empty, so
# find_path()/find_library() take the files from the root rather than from
# where a .pc file (perhaps copied there) points.
macro(pj_pkg_search_module prefix)
  if(PKG_CONFIG_FOUND)
    pj_package_roots(${CMAKE_FIND_PACKAGE_NAME} _pj_pc_roots)
    if(_pj_pc_roots)
      _pj_pkg_search_module_in_roots(${prefix} "${_pj_pc_roots}" ${ARGN})
    else()
      pkg_search_module(${prefix} QUIET ${ARGN})
    endif()
    unset(_pj_pc_roots)
  endif()
endmacro()

function(_pj_pkg_search_module_in_roots prefix roots)
  set(_dirs)
  foreach(_root IN LISTS roots)
    list(APPEND _dirs "${_root}/lib/pkgconfig" "${_root}/share/pkgconfig")
  endforeach()
  if(CMAKE_HOST_WIN32)
    set(_sep ";")
  else()
    set(_sep ":")
  endif()
  list(JOIN _dirs "${_sep}" _path)

  # pkg_search_module() keeps its result per prefix, so a prefix of its own
  # for each set of roots makes a changed root search again
  string(MD5 _hash "${_path};${ARGN}")
  set(_pc "_PJ_PC_${_hash}")

  # the roots go first; the usual search path stays for what a .pc requires
  set(_old_path "$ENV{PKG_CONFIG_PATH}")
  if(NOT _old_path STREQUAL "")
    string(APPEND _path "${_sep}${_old_path}")
  endif()
  set(ENV{PKG_CONFIG_PATH} "${_path}")
  pkg_search_module(${_pc} QUIET NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
    ${ARGN})
  # only a .pc file from a root counts, not one found past the roots
  set(_in_root FALSE)
  if(${_pc}_FOUND)
    pkg_get_variable(_pcfiledir ${${_pc}_MODULE_NAME} pcfiledir)
    file(TO_CMAKE_PATH "${_pcfiledir}" _pcfiledir)
    foreach(_dir IN LISTS _dirs)
      if(CMAKE_HOST_WIN32)
        string(TOLOWER "${_dir}" _dir)
        string(TOLOWER "${_pcfiledir}" _pcfiledir)
      endif()
      if(_pcfiledir STREQUAL _dir)
        set(_in_root TRUE)
      endif()
    endforeach()
  endif()
  set(ENV{PKG_CONFIG_PATH} "${_old_path}")

  foreach(_var IN ITEMS VERSION CFLAGS_OTHER STATIC_LIBRARIES)
    if(_in_root)
      set(${prefix}_${_var} "${${_pc}_${_var}}" PARENT_SCOPE)
    else()
      set(${prefix}_${_var} "" PARENT_SCOPE)
    endif()
  endforeach()
  set(${prefix}_FOUND ${_in_root} PARENT_SCOPE)
  foreach(_var IN ITEMS INCLUDEDIR INCLUDE_DIRS LIBDIR LIBRARY_DIRS)
    set(${prefix}_${_var} "" PARENT_SCOPE)
  endforeach()
endfunction()

# pj_warn_outside_package_roots()
#
# A root is searched first, not only, so whatever an incomplete root lacks is
# taken from the system. For each <X>_ROOT (or OPENSSL_ROOT_DIR) given, warn
# about any <X>_..._INCLUDE_DIR or <X>_..._LIBRARY found outside it.
function(pj_warn_outside_package_roots)
  # Only the packages found by this run: the cache may still hold what an
  # earlier one found, e.g. before PJ_DEP_<X> was set to bundled
  get_property(_found GLOBAL PROPERTY PACKAGES_FOUND)
  string(TOUPPER "${_found}" _found)
  get_cmake_property(_vars CACHE_VARIABLES)
  foreach(_root_var IN LISTS _vars)
    # Test the name before reading the value: CMake warns when some of its
    # own variables are read
    if(NOT _root_var MATCHES "^([A-Za-z0-9][A-Za-z0-9_]*)_ROOT(_DIR)?$")
      continue()
    endif()
    set(_pkg "${CMAKE_MATCH_1}")
    if(CMAKE_MATCH_2 AND NOT _pkg STREQUAL "OPENSSL")
      continue()
    endif()
    get_property(_type CACHE "${_root_var}" PROPERTY TYPE)
    if(_type STREQUAL "INTERNAL" OR "${${_root_var}}" STREQUAL "")
      continue()
    endif()
    string(TOUPPER "${_pkg}" _upper)
    if(NOT _upper IN_LIST _found)
      continue()
    endif()
    set(_roots "")
    foreach(_root IN LISTS ${_root_var})
      file(REAL_PATH "${_root}" _root)
      list(APPEND _roots "${_root}")
    endforeach()
    foreach(_var IN LISTS _vars)
      if(NOT _var MATCHES "^(${_pkg}|${_upper})_([A-Za-z0-9_]+_)?(INCLUDE_DIR|LIBRARY)$")
        continue()
      endif()
      if(NOT EXISTS "${${_var}}")
        continue()
      endif()
      file(REAL_PATH "${${_var}}" _path)
      if(CMAKE_HOST_WIN32)
        string(TOLOWER "${_path}" _path)
      endif()
      set(_inside FALSE)
      foreach(_root IN LISTS _roots)
        if(CMAKE_HOST_WIN32)
          string(TOLOWER "${_root}" _root)
        endif()
        string(FIND "${_path}/" "${_root}/" _pos)
        if(_pos EQUAL 0)
          set(_inside TRUE)
        endif()
      endforeach()
      if(NOT _inside)
        message(WARNING
          "${_root_var} is set, but ${_var} was found outside it: "
          "${${_var}}")
      endif()
    endforeach()
  endforeach()
endfunction()
