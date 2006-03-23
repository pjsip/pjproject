#
# OS specific configuration for Darwin/MacOS target. 
#

#
# PJMEDIA_OBJS specified here are object files to be included in PJMEDIA
# (the library) for this specific operating system. Object files common 
# to all operating systems should go in Makefile instead.
#
export PJMEDIA_OBJS += $(PA_DIR)/pa_mac_hostapis.o \
		       $(PA_DIR)/pa_unix_util.o \
		       $(PA_DIR)/pa_mac_core.o

#		       $(PA_DIR)/pa_mac_alt.o
#		       $(PA_DIR)/ringbuffer.o

export OS_CFLAGS +=  $(CC_DEF)PA_USE_COREAUDIO=1 $(CC_DEF)PA_BIG_ENDIAN=1
