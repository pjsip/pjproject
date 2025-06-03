cmake_minimum_required(VERSION 3.29)

project(pjproject
  VERSION 2.15
  LANGUAGES C CXX
)

# project modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ##############################################################################
# Options

include(Pj/Option)

#
# PJLIB options
#

#
# PJNATH options
#


#
# PJMEDIA options
#

# Include Speex support (codec, resampling, aec)
pj_option(PJ_WITH_SPEEX
  DEFAULT "bundled"
  ALLOWED_VALUES bundled system
)


# ##############################################################################
# Sub-directories

# base library
add_subdirectory(pjlib)

# bundled third_party


# include(Dependencies)
add_subdirectory(third_party)

# third-party libraries
# add_subdirectory(third_party)

# project components
add_subdirectory(pjlib-util)
add_subdirectory(pjnath)
add_subdirectory(pjmedia)
add_subdirectory(pjsip)
add_subdirectory(pjsip-apps)
