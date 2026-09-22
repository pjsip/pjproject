# OpenGL ES video render backend (iOS and Android)
#
# Defines the imported target `OpenGLES::OpenGLES` when every library is
# found. FindOpenGL looks for the GL and GLES2 libraries, which neither
# platform provides in the shape it expects: on iOS the ES implementation is
# a framework, and on Android it lives in the NDK sysroot, where FindOpenGL
# fails to locate a GL library to go with it.
#
# The window the renderer draws into is part of the backend rather than
# incidental, so each platform's windowing library is included here:
#
#   iOS      a CAEAGLLayer inside a UIView  -> QuartzCore, UIKit
#   Android  an ANativeWindow               -> libandroid

if(IOS)
  set(_opengles_libs_wanted OpenGLES QuartzCore UIKit)
elseif(ANDROID)
  set(_opengles_libs_wanted GLESv2 EGL android)
else()
  set(_opengles_libs_wanted)
endif()

set(_opengles_libs)
set(_opengles_required_vars)
foreach(_opengles_lib IN LISTS _opengles_libs_wanted)
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
unset(_opengles_libs_wanted)

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
