#
# PJMEDIA OS specific configuration for RTEMS OS target.
#

export PJMEDIA_OBJS += nullsound.o
export SOUND_OBJS = $(NULLSOUND_OBJS)

