ifeq ($(OS_NAME),palmos)
	export PALMOS_CYGWIN := /cygdrive/c/PalmOSCygwin
#	export PALMOS_SDK := /cygdrive/c/progra~1/PalmSource/PalmOS~1/sdk-5r4
#	export PALMOS_INCLUDES = \
#		$(CC_INC)$(PALMOS_SDK)/include \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Telephony \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Telephony/UI \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/SSL \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Sms \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Simulator \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Simulator/Locale \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Pdi \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/PalmOSGlue \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/Lz77 \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/INet \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/exglocal \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/CPMLib \
#		$(CC_INC)$(PALMOS_SDK)/include/Libraries/AddressSort \
#		$(CC_INC)$(PALMOS_SDK)/include/Extensions \
#		$(CC_INC)$(PALMOS_SDK)/include/Extensions/ExpansionMgr \
#		$(CC_INC)$(PALMOS_SDK)/include/Extensions/Bluetooth \
#		$(CC_INC)$(PALMOS_SDK)/include/Dynamic \
#		$(CC_INC)$(PALMOS_SDK)/include/Core \
#		$(CC_INC)$(PALMOS_SDK)/include/Core/UI \
#		$(CC_INC)$(PALMOS_SDK)/include/Core/System \
#		$(CC_INC)$(PALMOS_SDK)/include/Core/System/Unix \
#		$(CC_INC)$(PALMOS_SDK)/include/Core/Hardware \
#		\
#		$(CC_INC)$(PALMOS_CYGWIN)/lib/gcc-lib/m68k-palmos/2.95.3-kgpd/include \
#		$(CC_INC)$(PALMOS_CYGWIN)usr/m68k-palmos/include \
#		$(CC_INC)$(PALMOS_CYGWIN)/usr/share/prc-tools/include

	export PALMOS_SDK := /cygdrive/c/progra~1/PalmSource/PalmOS~1/sdk-6.1
	export PALMOS_INCLUDES := \
		$(CC_INC)$(PALMOS_SDK)/headers \
		$(CC_INC)$(PALMOS_SDK)/headers/posix \
		\
		$(CC_INC)$(PALMOS_CYGWIN)/lib/gcc-lib/m68k-palmos/2.95.3-kgpd/include \
		$(CC_INC)$(PALMOS_CYGWIN)usr/m68k-palmos/include \
		$(CC_INC)$(PALMOS_CYGWIN)/usr/share/prc-tools/include

	export CFLAGS += -Wall \
		$(subst /,$(HOST_PSEP),$(PALMOS_INCLUDES))
		#-I/cygdrive/c/PalmOSCygwin/usr/include
endif

ifeq ($(CC_NAME),vc)
	export CC_LDFLAGS += /link /LIBPATH:C:\Progra~1\Micros~2\vc98\lib
endif

ifeq ($(CC_NAME),gcc)
	export CFLAGS += 
endif
