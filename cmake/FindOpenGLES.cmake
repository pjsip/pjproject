# OpenGL ES video render backend (iOS)
#
# Defines the imported target `OpenGLES::OpenGLES` when every framework is
# found. FindOpenGL looks for the GL and GLES2 libraries, which do not exist
# on iOS, where the ES implementation is a framework instead.
#
# The renderer draws into a CAEAGLLayer inside a UIView, so QuartzCore and
# UIKit are part of the backend rather than incidental.

set(_opengles_frameworks OpenGLES QuartzCore UIKit)

set(_opengles_libs)
foreach(_opengles_lib IN LISTS _opengles_frameworks)
  string(TOUPPER "OpenGLES_LIBRARY_${_opengles_lib}" _opengles_lib_var)
  list(APPEND _opengles_required_vars ${_opengles_lib_var})

  find_library(${_opengles_lib_var} "${_opengles_lib}")
  mark_as_advanced(${_opengles_lib_var})
  if(${_opengles_lib_var})
    list(APPEND _opengles_libs "${${_opengles_lib_var}}")
  endif()
endforeach()
unset(_opengles_lib)
unset(_opengles_lib_var)
unset(_opengles_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGLES
  REQUIRED_VARS
    ${_opengles_required_vars}
)

if(OpenGLES_FOUND)
  set(OpenGLES_LIBRARIES ${_opengles_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET OpenGLES::OpenGLES)
    add_library(OpenGLES::OpenGLES INTERFACE IMPORTED GLOBAL)
    set_target_properties(OpenGLES::OpenGLES PROPERTIES
      INTERFACE_LINK_LIBRARIES "${OpenGLES_LIBRARIES}"
    )
  endif()
endif()

unset(_opengles_libs)
unset(_opengles_required_vars)
