# Install-time part of PkgConfig.cmake. Turns the library files marked in
# Libs.private into -L/-l flags, removes duplicates, and installs the .pc
# file.
#
# A file becomes -l<name>, and its directory a -L flag unless the linker
# searches it anyway (PJ_PC_SYSTEM_DIRS), so that the file does not record
# version-specific paths such as an SDK's or a package manager's. A file
# whose name is not a library name stays as it is.
#
# Input: PJ_PC_INPUT, PJ_PC_DESTINATION, PJ_PC_SYSTEM_DIRS

# Install scripts run with old policies; IN_LIST needs CMP0057
cmake_policy(VERSION 3.28)

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
  # One test per if(): a failed MATCHES clears CMAKE_MATCH_<n>
  set(lib "")
  if(name MATCHES "^lib(.+)\\.dll\\.a$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.(a|so|dylib|tbd)$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^lib(.+)\\.so\\.[0-9.]+$")
    set(lib "${CMAKE_MATCH_1}")
  elseif(name MATCHES "^(.+)\\.lib$")
    set(lib "${CMAKE_MATCH_1}")
  endif()
  if(lib)
    set(flag "-l${lib}")
    set(system FALSE)
    foreach(sys IN LISTS PJ_PC_SYSTEM_DIRS)
      string(FIND "${dir}/" "${sys}/" pos)
      if(pos EQUAL 0)
        set(system TRUE)
      endif()
    endforeach()
    if(NOT system AND NOT dir IN_LIST dirs)
      list(APPEND dirs "${dir}")
    endif()
  else()
    set(flag "\"${path}\"")
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
