# Generates libpjproject.pc from libpjproject.pc.in, the template the GNU build
# fills too.
#
# Libs lists the PJSIP libraries, Libs.private everything they link, for
# `pkg-config --static`, and Cflags the include directory and the compile
# definitions consumers need. These are read off the targets' link and
# compile properties. Those hold a few simple generator expressions, which
# are evaluated here; anything else, and the file locations of imported
# libraries, is left to file(GENERATE). Library files are turned into -L/-l
# flags at install time (PkgConfigInstall.cmake), as their per-configuration
# locations are only known then. The prefix is relative to the .pc file
# itself, so the file follows `cmake --install --prefix` and a relocated
# installation.
#
# Needs the dependencies' imported targets to be visible from the top-level
# directory, which CMAKE_FIND_PACKAGE_TARGETS_GLOBAL makes them.

# Marks a library file, for PkgConfigInstall.cmake to turn into -L/-l
set(_PJ_PC_FILE_BEGIN "@PJ_PC_FILE@")
set(_PJ_PC_FILE_END "@PJ_PC_FILE_END@")

# Evaluates the generator expressions in `value` that the targets use, and
# leaves the others in place for file(GENERATE), along with any expression
# whose argument contains one of them.
function(_pj_pc_eval out value)
  set(v "${value}")
  set(n 0)
  while(v MATCHES "\\$<([^<>$:]*)(:([^<>$]*))?>")
    set(expr "${CMAKE_MATCH_0}")
    set(name "${CMAKE_MATCH_1}")
    set(arg "${CMAKE_MATCH_3}")
    string(TOUPPER "${arg}" uarg)
    set(r "")
    set(keep FALSE)
    if(arg MATCHES "@PJ_PC_GENEX_[0-9]+@")
      set(keep TRUE)
    elseif(name STREQUAL "LINK_ONLY" OR name STREQUAL "INSTALL_INTERFACE"
           OR name STREQUAL "1")
      set(r "${arg}")
    elseif(name STREQUAL "BUILD_INTERFACE" OR name STREQUAL "0")
      set(r "")
    elseif(name STREQUAL "BOOL")
      # As CMake's: false only for the false constants
      if(uarg MATCHES "^(|0|OFF|NO|FALSE|N|IGNORE|NOTFOUND|.*-NOTFOUND)$")
        set(r 0)
      else()
        set(r 1)
      endif()
    elseif(name STREQUAL "NOT" AND arg MATCHES "^[01]$")
      if(arg STREQUAL "1")
        set(r 0)
      else()
        set(r 1)
      endif()
    elseif(name STREQUAL "STREQUAL" AND arg MATCHES "^([^,]*),([^,]*)$")
      if(CMAKE_MATCH_1 STREQUAL CMAKE_MATCH_2)
        set(r 1)
      else()
        set(r 0)
      endif()
    else()
      set(keep TRUE)
    endif()
    if(keep)
      # Hide it from the loop, to be restored below
      set(r "@PJ_PC_GENEX_${n}@")
      set(_genex_${n} "${expr}")
      math(EXPR n "${n} + 1")
    endif()
    string(REPLACE "${expr}" "${r}" v "${v}")
  endwhile()
  while(v MATCHES "@PJ_PC_GENEX_([0-9]+)@")
    string(REPLACE "${CMAKE_MATCH_0}" "${_genex_${CMAKE_MATCH_1}}" v "${v}")
  endwhile()
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# Splits a list as list() does, but not at a ";" inside a generator
# expression, which becomes a space there instead.
function(_pj_pc_split out value)
  set(result "")
  set(current "")
  set(depth 0)
  foreach(part IN LISTS value)
    if(depth GREATER 0)
      # Flags in a .pc file are separated by spaces
      string(APPEND current " ${part}")
    else()
      set(current "${part}")
    endif()
    string(REGEX MATCHALL "\\$<" opens "${part}")
    string(REGEX MATCHALL ">" closes "${part}")
    list(LENGTH opens o)
    list(LENGTH closes c)
    math(EXPR depth "${depth} + ${o} - ${c}")
    if(depth LESS_EQUAL 0)
      set(depth 0)
      list(APPEND result "${current}")
    endif()
  endforeach()
  if(depth GREATER 0)
    list(APPEND result "${current}")
  endif()
  set(${out} "${result}" PARENT_SCOPE)
endfunction()

# The flag, or marked library file, that links `target`; empty for an
# interface library.
function(_pj_pc_target_file out target)
  get_target_property(type ${target} TYPE)
  get_target_property(imported ${target} IMPORTED)
  if(type STREQUAL "INTERFACE_LIBRARY" OR NOT imported)
    set(${out} "" PARENT_SCOPE)
  else()
    set(${out}
      "${_PJ_PC_FILE_BEGIN}$<TARGET_LINKER_FILE:${target}>${_PJ_PC_FILE_END}"
      PARENT_SCOPE)
  endif()
endfunction()

# Turns link items, or link options when `options` is set, into flags in
# `out`, and visits the targets among them.
function(_pj_pc_items out items options)
  set(flags "")
  set(pending "")
  foreach(item IN LISTS items)
    # "::@" marks the directory a cross-directory link was made from
    if(item STREQUAL "" OR item MATCHES "-NOTFOUND$" OR item MATCHES "^::@")
      continue()
    endif()
    # An option taking the next item as its argument, e.g. -framework;Foo
    if(pending)
      list(APPEND flags "${pending} ${item}")
      set(pending "")
      continue()
    elseif(item MATCHES "^-(framework|weak_framework|Xlinker)$")
      set(pending "${item}")
      continue()
    endif()

    if(item MATCHES "\\$<")
      # An expression left for file(GENERATE). Targets in it are replaced
      # with their files, and what they link is collected regardless of
      # the condition.
      string(REGEX MATCHALL "[A-Za-z0-9_.+-]+(::[A-Za-z0-9_.+-]+)*"
        words "${item}")
      foreach(word IN LISTS words)
        if(TARGET "${word}")
          get_target_property(aliased ${word} ALIASED_TARGET)
          set(target "${word}")
          if(aliased)
            set(target "${aliased}")
          endif()
          _pj_pc_visit(${target} NO_SELF)
          _pj_pc_target_file(file ${target})
          string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" re "${word}")
          string(REGEX REPLACE "(^|[:;>])${re}($|[;>])" "\\1${file}\\2"
            item "${item}")
        endif()
      endforeach()
      list(APPEND flags "${item}")
    elseif(options)
      if(item MATCHES "^SHELL:(.*)$")
        list(APPEND flags "${CMAKE_MATCH_1}")
      elseif(item MATCHES "^LINKER:(.*)$")
        string(REPLACE "," "${CMAKE_C_LINKER_WRAPPER_FLAG_SEP}" args
          "${CMAKE_MATCH_1}")
        if(CMAKE_C_LINKER_WRAPPER_FLAG)
          list(APPEND flags "${CMAKE_C_LINKER_WRAPPER_FLAG}${args}")
        else()
          list(APPEND flags "${args}")
        endif()
      else()
        list(APPEND flags "${item}")
      endif()
    elseif(TARGET "${item}")
      _pj_pc_visit("${item}")
    elseif(item MATCHES "^-")
      list(APPEND flags "${item}")
    elseif(item MATCHES "/([^/]+)\\.framework/?$")
      list(APPEND flags "-framework ${CMAKE_MATCH_1}")
    elseif(IS_ABSOLUTE "${item}")
      list(APPEND flags "${_PJ_PC_FILE_BEGIN}${item}${_PJ_PC_FILE_END}")
    else()
      list(APPEND flags "-l${item}")
    endif()
  endforeach()
  set(${out} "${flags}" PARENT_SCOPE)
endfunction()

# Records what `target` links, depth-first, then the target itself unless
# NO_SELF is given.
function(_pj_pc_visit target)
  get_target_property(aliased ${target} ALIASED_TARGET)
  if(aliased)
    set(target "${aliased}")
  endif()
  get_property(seen GLOBAL PROPERTY _PJ_PC_SEEN)
  if(target IN_LIST seen)
    return()
  endif()
  if(NOT ARGV1 STREQUAL "NO_SELF")
    set_property(GLOBAL APPEND PROPERTY _PJ_PC_SEEN "${target}")
  endif()
  get_property(pj_libs GLOBAL PROPERTY _PJ_PC_LIBS)

  get_target_property(imported ${target} IMPORTED)
  get_target_property(type ${target} TYPE)
  set(items "")
  if(NOT imported AND NOT type STREQUAL "INTERFACE_LIBRARY")
    get_target_property(items ${target} LINK_LIBRARIES)
  endif()
  get_target_property(ilibs ${target} INTERFACE_LINK_LIBRARIES)
  if(ilibs)
    list(APPEND items ${ilibs})
  endif()
  _pj_pc_eval(items "${items}")
  _pj_pc_split(items "${items}")
  _pj_pc_items(flags "${items}" FALSE)

  if(imported)
    get_target_property(opts ${target} INTERFACE_LINK_OPTIONS)
    if(opts)
      _pj_pc_eval(opts "${opts}")
      _pj_pc_split(opts "${opts}")
      _pj_pc_items(opt_flags "${opts}" TRUE)
      list(APPEND flags ${opt_flags})
    endif()
  endif()

  # Reversed, as the whole list is reversed at the end
  list(REVERSE flags)
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})

  if(ARGV1 STREQUAL "NO_SELF")
    return()
  endif()
  if(imported)
    _pj_pc_target_file(file ${target})
    if(file)
      set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE "${file}")
    endif()
  elseif(NOT type STREQUAL "INTERFACE_LIBRARY"
         AND NOT target IN_LIST pj_libs)
    set_property(GLOBAL APPEND PROPERTY _PJ_PC_THIRD_PARTY "${target}")
  endif()
endfunction()

# pj_generate_pkgconfig(<pc.in> <destination> <library>...)
#
# The libraries are those to list in Libs, in link order.
function(pj_generate_pkgconfig template destination)
  set(pj_libs ${ARGN})
  set(libs "")
  set(cflags "")

  foreach(lib IN LISTS pj_libs)
    string(APPEND libs " -l$<TARGET_LINKER_FILE_BASE_NAME:${lib}>")
    get_target_property(defs ${lib} INTERFACE_COMPILE_DEFINITIONS)
    if(defs)
      _pj_pc_eval(defs "${defs}")
      foreach(def IN LISTS defs)
        if(def AND NOT " ${cflags} " MATCHES " -D${def} ")
          string(APPEND cflags " -D${def}")
        endif()
      endforeach()
    endif()
  endforeach()

  # Everything the libraries link. The walk records each target after what
  # it links, so the reversed result lists every library before its
  # dependencies, as a static link needs. Duplicates are removed at
  # install time, after the library files have become flags.
  set_property(GLOBAL PROPERTY _PJ_PC_LIBS "${pj_libs}")
  set_property(GLOBAL PROPERTY _PJ_PC_SEEN "")
  set_property(GLOBAL PROPERTY _PJ_PC_PRIVATE "")
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY "")
  foreach(lib IN LISTS pj_libs)
    _pj_pc_visit(${lib})
  endforeach()
  get_property(private GLOBAL PROPERTY _PJ_PC_PRIVATE)
  get_property(third_party GLOBAL PROPERTY _PJ_PC_THIRD_PARTY)
  list(REVERSE private)
  list(REVERSE third_party)

  # pjlib has C++ sources, so a static link from C needs the C++ runtime
  set(cxx_runtime ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
  if(cxx_runtime AND CMAKE_C_IMPLICIT_LINK_LIBRARIES)
    list(REMOVE_ITEM cxx_runtime ${CMAKE_C_IMPLICIT_LINK_LIBRARIES})
  endif()
  foreach(lib IN LISTS cxx_runtime)
    if(IS_ABSOLUTE "${lib}")
      list(APPEND private "${_PJ_PC_FILE_BEGIN}${lib}${_PJ_PC_FILE_END}")
    elseif(lib MATCHES "^-")
      list(APPEND private "${lib}")
    else()
      list(APPEND private "-l${lib}")
    endif()
  endforeach()

  set(tp_flags "")
  if(third_party)
    set(tp_flags "-L\${libdir}/pjproject/third_party")
    foreach(lib IN LISTS third_party)
      string(APPEND tp_flags " -l$<TARGET_LINKER_FILE_BASE_NAME:${lib}>")
    endforeach()
  endif()
  list(JOIN private " " private)

  if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(LIBDIR "${CMAKE_INSTALL_LIBDIR}")
  else()
    file(RELATIVE_PATH up "/x/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "/x")
    string(REGEX REPLACE "/$" "" up "${up}")
    set(PREFIX "\${pcfiledir}/${up}")
    set(LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
  endif()
  if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
  else()
    set(INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
  endif()
  set(PJ_VERSION "${PROJECT_VERSION}")
  set(PJ_INSTALL_LDFLAGS "-L\${libdir}${libs}")
  string(STRIP "${tp_flags} ${private}" PJ_INSTALL_LDFLAGS_PRIVATE)
  set(PJ_INSTALL_CFLAGS "-I\${includedir}${cflags}")

  file(READ "${template}" content)
  string(CONFIGURE "${content}" content @ONLY)
  file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/pkgconfig/libpjproject-$<CONFIG>.pc.in"
    CONTENT "${content}"
  )

  # Directories the linker searches anyway need no -L
  set(system_dirs ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
  if(CMAKE_OSX_SYSROOT)
    list(APPEND system_dirs "${CMAKE_OSX_SYSROOT}")
  endif()
  if(CMAKE_SYSROOT)
    list(APPEND system_dirs "${CMAKE_SYSROOT}")
  endif()
  install(CODE "
    set(PJ_PC_INPUT \"${CMAKE_BINARY_DIR}/pkgconfig/libpjproject-\${CMAKE_INSTALL_CONFIG_NAME}.pc.in\")
    set(PJ_PC_DESTINATION \"${destination}\")
    set(PJ_PC_SYSTEM_DIRS \"${system_dirs}\")
    include(\"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PkgConfigInstall.cmake\")
    "
    COMPONENT PjDevelopment
  )
endfunction()
