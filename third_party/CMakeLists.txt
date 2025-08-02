set(DEPS)

# G7221
set(DEP_G7221_HELP    "G7221 codec implementation provider")
set(DEP_G7221_SUBDIR  "g7221")
set(DEP_G7221_PACKAGE "G7221")
list(APPEND DEPS G7221)

# GSM
set(DEP_GSM_HELP    "GSM codec implementation provider")
set(DEP_GSM_SUBDIR  "gsm")
set(DEP_GSM_PACKAGE "GSM")
list(APPEND DEPS GSM)

# iLBC
set(DEP_iLBC_HELP   "iLBC codec implementation provider")
set(DEP_iLBC_SUBDIR "ilbc")
list(APPEND DEPS iLBC)

# Resample
set(DEP_Resample_HELP    "libresample implementation provider")
set(DEP_Resample_SUBDIR  "resample")
set(DEP_Resample_PACKAGE "Resample")
list(APPEND DEPS Resample)

# Speex
set(DEP_Speex_HELP    "Speex codec & resampling implementation provider")
set(DEP_Speex_SUBDIR  "speex")
set(DEP_Speex_PACKAGE "Speex")
list(APPEND DEPS Speex)

# SRTP
set(DEP_SRTP_HELP    "Secure RTP (sRTP) implementation provider")
set(DEP_SRTP_SUBDIR  "srtp")
set(DEP_SRTP_PACKAGE "SRTP")
list(APPEND DEPS SRTP)

# WebRTC
set(DEP_WebRTC_HELP  "WebRTC implementation provider")
set(DEP_WebRTC_SUBDIR "webrtc")
list(APPEND DEPS WebRTC)

# WebRTC_AEC3
set(DEP_WebRTC_AEC3_HELP   "WebRTC AEC3 implementation provider")
set(DEP_WebRTC_AEC3_SUBDIR "webrtc_aec3")
list(APPEND DEPS WebRTC_AEC3)

# YUV
set(DEP_YUV_HELP    "libyuv codec implementation provider")
set(DEP_YUV_SUBDIR  "yuv")
set(DEP_YUV_PACKAGE "YUV")
list(APPEND DEPS YUV)

# imports dependencies
set(install_subdirs)
foreach(dep IN LISTS DEPS)
  # construct providers list
  set(dep_providers)
  if(DEP_${dep}_SUBDIR)
    list(APPEND dep_providers "bundled")
  endif()
  if(DEP_${dep}_PACKAGE)
    list(APPEND dep_providers "system")
  endif()
  if(NOT dep_providers)
    message(FATAL_ERROR "No providers were set for ${dep}")
  endif()

  # add option
  string(TOUPPER "PJ_DEP_${dep}" dep_option_name)
  list(GET dep_providers 0 dep_option_default)

  pj_option(${dep_option_name} "${DEP_${dep}_HELP}"
    DEFAULT ${dep_option_default}
    ALLOWED_VALUES ${dep_providers}
  )

  # import dependency
  if(${dep_option_name} STREQUAL "bundled")
    add_subdirectory(${DEP_${dep}_SUBDIR} EXCLUDE_FROM_ALL)
    set(dep_target ${DEP_${dep}_SUBDIR})
    list(APPEND install_subdirs ${DEP_${dep}_SUBDIR})
  elseif(${dep_option_name} STREQUAL "system")
    find_package(${DEP_${dep}_PACKAGE} REQUIRED)
    if(${DEP_${dep}_PACKAGE}_FOUND)
      if(${DEP_${dep}_PACKAGE_TARGET})
        set(dep_target ${DEP_${dep}_PACKAGE_TARGET})
      else()
        set(dep_target ${DEP_${dep}_PACKAGE}::${DEP_${dep}_PACKAGE})
      endif()
    endif()
  endif()

  # set provider property on target
  if(dep_target)
    set_target_properties(${dep_target} PROPERTIES
      PJ_DEP_PROVIDER ${dep_option_name}
    )
    add_library(Pj::Dep::${dep} ALIAS ${dep_target})
  endif()

  # add alias
endforeach()

# install selected sub-diriectories
if(install_subdirs)
  # append `$ORIGIN/pjproject/third_party` to RPATH
  if(BUILD_SHARED_LIBS)
    if(APPLE)
      list(APPEND CMAKE_INSTALL_RPATH "@loader_path/pjproject/third_party")
    elseif(UNIX)
      list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/pjproject/third_party")
    endif()
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} PARENT_SCOPE)
  endif()

  foreach(subdir IN LISTS install_subdirs)
    # add installation config
    install(TARGETS ${subdir}
      EXPORT PjTargets
      LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pjproject/third_party"
        COMPONENT PjRuntime
        OPTIONAL
      ARCHIVE
        DESTINATION  "${CMAKE_INSTALL_BINDIR}/pjproject/third_party"
        COMPONENT PjDevelopment
        OPTIONAL
      FILE_SET HEADERS
        DESTINATION  "${CMAKE_INSTALL_INCLUDEDIR}/pjproject/third_party/${subdir}"
        COMPONENT PjDevelopment
        OPTIONAL
    )
  endforeach()
endif()
