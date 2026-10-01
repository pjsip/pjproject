# WASAPI is part of the Windows SDK. A compile check is used rather than
# find_path()/find_library(): with the Visual Studio generators the SDK
# directories are known to MSBuild only, so those never see them.
# A C check, as PjConfig.cmake runs this in consumers that may not enable C++.
include(CheckIncludeFile)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_QUIET ${WASAPI_FIND_QUIETLY})
check_include_file("audioclient.h" WASAPI_HAS_AUDIOCLIENT_H)
cmake_pop_check_state()

# The desktop backend (wasapi_dev_win.cpp) needs COM only, and loads avrt.dll
# at run time. The UWP backend (wasapi_dev.cpp) needs the Media Foundation
# libraries.
if(CMAKE_SYSTEM_NAME MATCHES "WindowsStore|WindowsPhone")
  set(_wasapi_libs ksuser mfplat mfuuid wmcodecdspuuid)
else()
  set(_wasapi_libs ole32)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WASAPI
  REQUIRED_VARS
    WASAPI_HAS_AUDIOCLIENT_H
)

if(WASAPI_FOUND)
  set(WASAPI_LIBRARIES ${_wasapi_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET WASAPI::WASAPI)
    add_library(WASAPI::WASAPI INTERFACE IMPORTED GLOBAL)
    set_target_properties(WASAPI::WASAPI PROPERTIES
      INTERFACE_LINK_LIBRARIES "${WASAPI_LIBRARIES}"
    )
  endif()
endif()

unset(_wasapi_libs)
