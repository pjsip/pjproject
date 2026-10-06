# Install-time part of PkgConfig.cmake. Turns the library files marked in
# Libs.private into -L/-l flags, removes duplicates, and installs the .pc
# file.
#
# A shared library file becomes -l<name>, and its directory a -L flag
# unless the linker searches it anyway, so the file records no SDK paths.
# A Homebrew Cellar directory is replaced by the formula's opt/ link, which
# survives an upgrade. A static library stays a full path when a shared one
# of the same name is beside it, as -l would pick the shared one. A file
# whose name is not a library name stays as it is.
#
# Input: PJ_PC_INPUT, PJ_PC_DESTINATION, PJ_PC_IMPLICIT_DIRS (searched
# directories, matched exactly) and PJ_PC_SYSROOTS (matched with everything
# below them)

# Install scripts run with old policies; IN_LIST needs CMP0057
cmake_policy(VERSION 3.28)

# A configuration that was not generated, e.g. one outside
# CMAKE_CONFIGURATION_TYPES, should not fail the whole installation
if(NOT EXISTS "${PJ_PC_INPUT}")
  message(WARNING "No pkg-config file for configuration "
    "'${CMAKE_INSTALL_CONFIG_NAME}', libpjproject.pc not installed")
  return()
endif()

# A Homebrew Cellar directory as the formula's opt/ link, when that leads to
# the same place
function(_pj_pc_stable_dir out dir)
  set(${out} "${dir}" PARENT_SCOPE)
  if(dir MATCHES "^(.+)/Cellar/([^/]+)/[^/]+(/.*)?$")
    set(opt "${CMAKE_MATCH_1}/opt/${CMAKE_MATCH_2}${CMAKE_MATCH_3}")
    if(IS_DIRECTORY "${opt}")
      file(REAL_PATH "${opt}" real_opt)
      file(REAL_PATH "${dir}" real_dir)
      if(real_opt STREQUAL real_dir)
        set(${out} "${opt}" PARENT_SCOPE)
      endif()
    endif()
  endif()
endfunction()

function(_pj_pc_searched out dir)
  set(${out} FALSE PARENT_SCOPE)
  foreach(implicit IN LISTS PJ_PC_IMPLICIT_DIRS)
    if(dir STREQUAL implicit)
      set(${out} TRUE PARENT_SCOPE)
    endif()
  endforeach()
  foreach(sysroot IN LISTS PJ_PC_SYSROOTS)
    string(FIND "${dir}/" "${sysroot}/" pos)
    if(pos EQUAL 0)
      set(${out} TRUE PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

file(READ "${PJ_PC_INPUT}" content)
string(REGEX MATCH "\nLibs\\.private:([^\n]*)" line "${content}")
set(private "${CMAKE_MATCH_1}")

# Found with string(FIND), not a regex: a path may contain any character,
# e.g. Homebrew's openssl@3
set(begin "@PJ_PC_FILE@")
set(end "@PJ_PC_FILE_END@")
string(LENGTH "${begin}" begin_len)
string(LENGTH "${end}" end_len)
set(dirs "")
while(TRUE)
  string(FIND "${private}" "${begin}" first)
  if(first EQUAL -1)
    break()
  endif()
  math(EXPR path_start "${first} + ${begin_len}")
  string(SUBSTRING "${private}" ${path_start} -1 rest)
  string(FIND "${rest}" "${end}" path_len)
  if(path_len EQUAL -1)
    message(FATAL_ERROR "Unterminated library file in ${PJ_PC_INPUT}")
  endif()
  string(SUBSTRING "${rest}" 0 ${path_len} path)
  math(EXPR marker_len "${begin_len} + ${path_len} + ${end_len}")
  string(SUBSTRING "${private}" ${first} ${marker_len} marker)

  get_filename_component(dir "${path}" DIRECTORY)
  get_filename_component(name "${path}" NAME)
  _pj_pc_stable_dir(dir "${dir}")
  # One test per if(): a failed MATCHES clears CMAKE_MATCH_<n>
  set(lib "")
  set(static FALSE)
  if(name MATCHES "^lib(.+)\\.dll\\.a$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.a$")
    set(lib "${CMAKE_MATCH_1}")
    set(static TRUE)
  elseif(name MATCHES "^lib(.+)\\.(so|dylib|tbd)$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.so\\.[0-9.]+$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^(.+)\\.lib$")
    set(lib "${CMAKE_MATCH_1}")
  endif()
  if(static)
    file(GLOB shared "${dir}/lib${lib}.so" "${dir}/lib${lib}.so.*"
      "${dir}/lib${lib}.dylib" "${dir}/lib${lib}.tbd")
    if(shared)
      set(lib "")
    endif()
  endif()

  if(lib)
    set(flag "-l${lib}")
    _pj_pc_searched(searched "${dir}")
    if(NOT searched AND NOT dir IN_LIST dirs)
      list(APPEND dirs "${dir}")
    endif()
  else()
    set(flag "\"${dir}/${name}\"")
  endif()
  string(REPLACE "${marker}" "${flag}" private "${private}")
endwhile()

# Tokens, keeping a quoted path and an option with its argument together
string(REGEX MATCHALL "\"[^\"]*\"|[^ ]+" words "${private}")
set(tokens "")
set(pending "")
foreach(word IN LISTS words)
  if(pending)
    list(APPEND tokens "${pending} ${word}")
    set(pending "")
  elseif(word MATCHES "^-(framework|weak_framework|Xlinker)$")
    set(pending "${word}")
  else()
    list(APPEND tokens "${word}")
  endif()
endforeach()

# -L first, each once; everything else once, at its last place, as a static
# link needs a library after everything that uses it
set(search "")
set(others "")
foreach(token IN LISTS tokens)
  if(token MATCHES "^-L")
    if(NOT token IN_LIST search)
      list(APPEND search "${token}")
    endif()
  else()
    list(REMOVE_ITEM others "${token}")
    list(APPEND others "${token}")
  endif()
endforeach()
foreach(dir IN LISTS dirs)
  if(dir MATCHES " ")
    set(dir "\"${dir}\"")
  endif()
  if(NOT "-L${dir}" IN_LIST search)
    list(APPEND search "-L${dir}")
  endif()
endforeach()
list(JOIN search " " search)
list(JOIN others " " others)
string(STRIP "${search} ${others}" private)
string(REPLACE "${line}" "\nLibs.private: ${private}" content "${content}")

string(REGEX REPLACE "\\.in$" "" output "${PJ_PC_INPUT}")
file(WRITE "${output}" "${content}")
if(IS_ABSOLUTE "${PJ_PC_DESTINATION}")
  set(destination "${PJ_PC_DESTINATION}")
else()
  set(destination "${CMAKE_INSTALL_PREFIX}/${PJ_PC_DESTINATION}")
endif()
file(INSTALL "${output}" DESTINATION "${destination}"
  RENAME libpjproject.pc)
