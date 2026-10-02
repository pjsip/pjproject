# Generates libpjproject.pc from libpjproject.pc.in, the template the GNU build
# fills too.
#
# Libs lists the PJSIP libraries, Libs.private everything they link, for
# `pkg-config --static`, and Cflags the include directory and the compile
# definitions consumers need. These are read off the targets' link and
# compile properties. Those hold a few simple generator expressions, which
# are evaluated here; anything else, and the file locations of imported
# libraries, is left to file(GENERATE). The prefix is relative to the .pc
# file itself, so the file follows `cmake --install --prefix` and a
# relocated installation.
#
# Needs the dependencies' imported targets to be visible from the top-level
# directory, which CMAKE_FIND_PACKAGE_TARGETS_GLOBAL makes them.

# Evaluates the generator expressions in `value` that the targets use, and
# leaves the others in place for file(GENERATE).
function(_pj_pc_eval out value)
  set(v "${value}")
  set(n 0)
  while(v MATCHES "\\$<([^<>$:]*)(:([^<>$]*))?>")
    set(expr "${CMAKE_MATCH_0}")
    set(name "${CMAKE_MATCH_1}")
    set(arg "${CMAKE_MATCH_3}")
    string(TOUPPER "${arg}" uarg)
    if(name STREQUAL "LINK_ONLY" OR name STREQUAL "INSTALL_INTERFACE"
       OR name STREQUAL "1")
      set(r "${arg}")
    elseif(name STREQUAL "BUILD_INTERFACE" OR name STREQUAL "0")
      set(r "")
    elseif(name STREQUAL "BOOL")
      if(uarg MATCHES "^(1|ON|YES|TRUE|Y|[1-9][0-9]*)$")
        set(r 1)
      else()
        set(r 0)
      endif()
    elseif(name STREQUAL "NOT")
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

# Records what `target` links, depth-first, then the target itself.
function(_pj_pc_visit target)
  get_target_property(aliased ${target} ALIASED_TARGET)
  if(aliased)
    set(target "${aliased}")
  endif()
  get_property(seen GLOBAL PROPERTY _PJ_PC_SEEN)
  if(target IN_LIST seen)
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_SEEN "${target}")
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
  if(imported)
    get_target_property(iopts ${target} INTERFACE_LINK_OPTIONS)
    if(iopts)
      list(APPEND items ${iopts})
    endif()
  endif()

  _pj_pc_eval(items "${items}")
  set(flags "")
  foreach(item IN LISTS items)
    # "::@" marks the directory a cross-directory link was made from
    if(item STREQUAL "" OR item MATCHES "-NOTFOUND$" OR item MATCHES "^::@")
      continue()
    elseif(TARGET "${item}")
      _pj_pc_visit("${item}")
    elseif(item MATCHES "^-" OR item MATCHES "\\$<")
      list(APPEND flags "${item}")
    elseif(item MATCHES "/([^/]+)\\.framework/?$")
      list(APPEND flags "-framework ${CMAKE_MATCH_1}")
    elseif(IS_ABSOLUTE "${item}")
      # Quoted, as the path may have spaces
      list(APPEND flags "\"${item}\"")
    else()
      list(APPEND flags "-l${item}")
    endif()
  endforeach()
  # Reversed, as the whole list is reversed at the end
  list(REVERSE flags)
  set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE ${flags})

  if(NOT type STREQUAL "INTERFACE_LIBRARY")
    if(imported)
      set_property(GLOBAL APPEND PROPERTY _PJ_PC_PRIVATE
        "\"$<TARGET_LINKER_FILE:${target}>\"")
    elseif(NOT target IN_LIST pj_libs)
      set_property(GLOBAL APPEND PROPERTY _PJ_PC_THIRD_PARTY "${target}")
    endif()
  endif()
endfunction()
# pj_generate_pkgconfig(<pc.in> <destination> <library>...)
#
# The libraries are those to list in Libs, in link order.
function(pj_generate_pkgconfig template destination)
  set(pj_libs ${ARGN})
  set(libs "")
  set(private "")
  set(third_party "")
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
  # dependencies, as a static link needs.
  set_property(GLOBAL PROPERTY _PJ_PC_LIBS "${pj_libs}")
  set_property(GLOBAL PROPERTY _PJ_PC_SEEN "")
  set_property(GLOBAL PROPERTY _PJ_PC_PRIVATE "")
  set_property(GLOBAL PROPERTY _PJ_PC_THIRD_PARTY "")
  foreach(lib IN LISTS pj_libs)
    _pj_pc_visit(${lib})
  endforeach()
  get_property(private GLOBAL PROPERTY _PJ_PC_PRIVATE)
  get_property(third_party GLOBAL PROPERTY _PJ_PC_THIRD_PARTY)
  # A library linked by several targets keeps its place after all of them
  list(REMOVE_DUPLICATES private)
  list(REVERSE private)
  list(REVERSE third_party)

  # pjlib has C++ sources, so a static link from C needs the C++ runtime
  set(cxx_runtime ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
  if(cxx_runtime AND CMAKE_C_IMPLICIT_LINK_LIBRARIES)
    list(REMOVE_ITEM cxx_runtime ${CMAKE_C_IMPLICIT_LINK_LIBRARIES})
  endif()
  list(REMOVE_DUPLICATES cxx_runtime)
  foreach(lib IN LISTS cxx_runtime)
    if(IS_ABSOLUTE "${lib}" OR lib MATCHES "^-")
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
    OUTPUT "${CMAKE_BINARY_DIR}/libpjproject-$<CONFIG>.pc"
    CONTENT "${content}"
  )
  install(
    FILES "${CMAKE_BINARY_DIR}/libpjproject-$<CONFIG>.pc"
    DESTINATION "${destination}"
    RENAME libpjproject.pc
    COMPONENT PjDevelopment
  )
endfunction()
