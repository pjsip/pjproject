# Build configurations:
#
# MACHINE_NAME values: 
#	- i386 (generic x86)
#	- m68k
#
# OS_NAME values:
#	- win32 (generic windows)
#	- linux
#
# CC_NAME values:
#	- gcc
#	- msvc
#
# HOST_NAME values:
#	- win32 (Windows command line)
#	- mingw (Windows, mingw)
#

#
# PalmOS 6 cross-compile, cygwin
#
#export MACHINE_NAME := m68k
#export OS_NAME := palmos
#export CC_NAME := gcc
#export HOST_NAME := mingw

#
# Win32, mingw
#
#export MACHINE_NAME := i386
#export OS_NAME := win32
#export CC_NAME := gcc
#export HOST_NAME := mingw

#
# Linux i386, gcc
#
export MACHINE_NAME := i386
export OS_NAME := linux
export CC_NAME := gcc
export HOST_NAME := unix

#
# Linux KERNEL i386, gcc
#
#export MACHINE_NAME := i386
#export OS_NAME := linux-kernel
#export CC_NAME := gcc
#export HOST_NAME := unix
#export PJPROJECT_DIR := /usr/src/pjproject-0.3
##export KERNEL_DIR = /usr/src/linux
#export KERNEL_DIR = /usr/src/uml/linux
#export KERNEL_ARCH = ARCH=um

