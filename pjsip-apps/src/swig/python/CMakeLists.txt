# ##############################################################################
# pjsua2 Python module
#
# SWIG turns pjsua2.i into pjsua2_wrap.cpp and pjsua2.py, and the extension
# module _pjsua2 that pjsua2.py imports is built from the former: what the
# Makefile and setup.py beside this file build, without make's build.mak.
# Both land in this directory of the build tree, so the module can be used
# without installing it:
#
#   PYTHONPATH=<build>/pjsip-apps/src/swig/python python3 test.py
#
# Wheels are still made with setup.py (make wheel).

find_package(SWIG 4.0 REQUIRED COMPONENTS python)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
include(UseSWIG)

set(_pj_swig_i "${CMAKE_CURRENT_SOURCE_DIR}/../pjsua2.i")
set_property(SOURCE "${_pj_swig_i}" PROPERTY CPLUSPLUS ON)
# -threads releases the GIL around every call into the library, as the
# Makefile does: a callback from a pjsip worker thread needs it
set_property(SOURCE "${_pj_swig_i}" PROPERTY COMPILE_OPTIONS -threads -doxygen)
# Regenerate when a header pjsua2.i includes changes
set_property(SOURCE "${_pj_swig_i}" PROPERTY USE_SWIG_DEPENDENCIES TRUE)

swig_add_library(pjsua2_python
  TYPE MODULE
  LANGUAGE python
  OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}"
  OUTFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
  SOURCES "${_pj_swig_i}"
)
unset(_pj_swig_i)

# _pjsua2.cpython-312-darwin.so and the like, as setup.py names it
if(WIN32)
  set(_pj_suffix ".pyd")
else()
  set(_pj_suffix "${CMAKE_SHARED_MODULE_SUFFIX}")
endif()
if(Python3_SOABI)
  set(_pj_suffix ".${Python3_SOABI}${_pj_suffix}")
endif()
set_target_properties(pjsua2_python PROPERTIES
  OUTPUT_NAME pjsua2
  PREFIX _
  SUFFIX "${_pj_suffix}"
  # beside pjsua2.py, without a per-configuration subdirectory
  LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
)
unset(_pj_suffix)

target_link_libraries(pjsua2_python PRIVATE pjsua2 Python3::Module)

# SWIG parses the headers itself; see the same in ../CMakeLists.txt
set_property(TARGET pjsua2_python
  PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE
)
target_include_directories(pjsua2_python PRIVATE
  $<TARGET_PROPERTY:pjlib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjlib-util,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjnath,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjmedia,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsip,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua-lib,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:pjsua2,INTERFACE_INCLUDE_DIRECTORIES>
)

# ##############################################################################
# Installation
#
# Into the prefix, as the rest of the project; make installs into the user's
# site directory instead (setup.py install --user). Point this at
# Python3_SITEARCH to install into the interpreter itself.

if(WIN32)
  set(_pj_pythondir "Lib/site-packages")
else()
  set(_pj_pythondir
    "${CMAKE_INSTALL_LIBDIR}/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages"
  )
endif()
set(PJ_INSTALL_PYTHONDIR "${_pj_pythondir}" CACHE PATH
  "Directory the pjsua2 Python module is installed to"
)
unset(_pj_pythondir)

# With shared libraries, the module finds them in the libdir
if(BUILD_SHARED_LIBS AND NOT IS_ABSOLUTE "${PJ_INSTALL_PYTHONDIR}"
   AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  file(RELATIVE_PATH _pj_up "/x/${PJ_INSTALL_PYTHONDIR}"
    "/x/${CMAKE_INSTALL_LIBDIR}"
  )
  if(APPLE)
    set_property(TARGET pjsua2_python APPEND PROPERTY
      INSTALL_RPATH "@loader_path/${_pj_up}"
    )
  elseif(UNIX)
    set_property(TARGET pjsua2_python APPEND PROPERTY
      INSTALL_RPATH "$ORIGIN/${_pj_up}"
    )
  endif()
  unset(_pj_up)
endif()

install(TARGETS pjsua2_python
  LIBRARY DESTINATION "${PJ_INSTALL_PYTHONDIR}" COMPONENT PjPython
  RUNTIME DESTINATION "${PJ_INSTALL_PYTHONDIR}" COMPONENT PjPython
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pjsua2.py"
  DESTINATION "${PJ_INSTALL_PYTHONDIR}"
  COMPONENT PjPython
)

# ##############################################################################
# Test: test.py, run against the module in the build tree

if(BUILD_TESTING)
  add_test(NAME pjsua2-python
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )
  set_tests_properties(pjsua2-python PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()
