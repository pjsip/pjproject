# Run by the uninstall target: remove every file `cmake --install` recorded in
# install_manifest.txt, then the directories that leaves empty below the
# install prefix (the deepest directory common to all the files), as make
# uninstall removes lib/ and include/ when empty.
#
#   cmake -DPJ_INSTALL_MANIFEST=<build>/install_manifest.txt -P Uninstall.cmake

if(NOT EXISTS "${PJ_INSTALL_MANIFEST}")
  message(FATAL_ERROR "Nothing to uninstall: ${PJ_INSTALL_MANIFEST} not found")
endif()

file(STRINGS "${PJ_INSTALL_MANIFEST}" _files)
set(_dirs)
set(_prefix)
foreach(_file IN LISTS _files)
  set(_file "$ENV{DESTDIR}${_file}")
  if(EXISTS "${_file}" OR IS_SYMLINK "${_file}")
    message(STATUS "Uninstalling: ${_file}")
    file(REMOVE "${_file}")
  endif()
  cmake_path(GET _file PARENT_PATH _dir)
  list(APPEND _dirs "${_dir}")
  if(NOT DEFINED _prefix OR _prefix STREQUAL "")
    set(_prefix "${_dir}")
  endif()
  while(NOT _dir STREQUAL _prefix)
    cmake_path(IS_PREFIX _prefix "${_dir}" NORMALIZE _under)
    if(_under)
      break()
    endif()
    cmake_path(GET _prefix PARENT_PATH _prefix)
  endwhile()
endforeach()

# Deepest first, so a parent is tried after its children
list(REMOVE_DUPLICATES _dirs)
list(SORT _dirs ORDER DESCENDING)
foreach(_dir IN LISTS _dirs)
  while(IS_DIRECTORY "${_dir}" AND NOT _dir STREQUAL _prefix)
    cmake_path(IS_PREFIX _prefix "${_dir}" NORMALIZE _under)
    file(GLOB _left "${_dir}/*" "${_dir}/.*")
    if(NOT _under OR _left)
      break()
    endif()
    file(REMOVE_RECURSE "${_dir}")
    cmake_path(GET _dir PARENT_PATH _dir)
  endwhile()
endforeach()
