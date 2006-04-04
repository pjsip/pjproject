
MACHINE_NAME = i386
OS_NAME = win32
CC_NAME = vc6-$(BUILD_MODE)
LIBEXT = .lib

!if "$(BUILD_MODE)" == "debug"
BUILD_FLAGS = /MTd /Od /Zi
!else
BUILD_FLAGS = /Ox /MD /DNDEBUG
!endif

PJLIB_LIB = ..\..\pjlib\lib\pjlib-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJLIB_UTIL_LIB = ..\..\pjlib-util\lib\pjlib-util-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_LIB = ..\..\pjmedia\lib\pjmedia-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJMEDIA_CODEC_LIB = ..\..\pjmedia\lib\pjmedia-codec-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_LIB = ..\..\pjsip\lib\pjsip-core-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_UA_LIB = ..\..\pjsip\lib\pjsip-ua-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSIP_SIMPLE_LIB = ..\..\pjsip\lib\pjsip-simple-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)
PJSUA_LIB_LIB = ..\..\pjsip\lib\pjsua-lib-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)$(LIBEXT)

LIBS = $(PJSUA_LIB_LIB) $(PJSIP_UA_LIB) $(PJSIP_SIMPLE_LIB) \
	  $(PJSIP_LIB) $(PJMEDIA_CODEC_LIB) $(PJMEDIA_LIB) \
	  $(PJLIB_UTIL_LIB) $(PJLIB_LIB)

CFLAGS 	= /DPJ_WIN32=1 /DPJ_M_I386=1 \
	  $(BUILD_FLAGS) \
	  -I..\..\pjsip\include \
	  -I..\..\pjlib\include -I..\..\pjlib-util\include \
	  -I..\..\pjmedia\include
LDFLAGS = $(BUILD_FLAGS) $(LIBS) \
	  ole32.lib user32.lib dsound.lib dxguid.lib netapi32.lib \
	  mswsock.lib ws2_32.lib 

SRCDIR = ..\src\samples
OBJDIR = .\output\samples-$(MACHINE_NAME)-$(OS_NAME)-$(CC_NAME)
BINDIR = ..\bin\samples


SAMPLES = $(BINDIR)\simpleua.exe $(BINDIR)\playfile.exe $(BINDIR)\playsine.exe \
	  $(BINDIR)\confsample.exe $(BINDIR)\sndinfo.exe \
	  $(BINDIR)\level.exe $(BINDIR)\recfile.exe  \
	  $(BINDIR)\resampleplay.exe $(BINDIR)\siprtp.exe


all: $(OBJDIR) $(SAMPLES)

$(SAMPLES): $(SRCDIR)\$(@B).c $(LIBS) $(SRCDIR)\util.h Samples-vc.mak
	cl -nologo -c $(SRCDIR)\$(@B).c /Fo$(OBJDIR)\$(@B).obj $(CFLAGS) 
	cl /nologo $(OBJDIR)\$(@B).obj /Fe$@ /Fm$(OBJDIR)\$(@B).map $(LDFLAGS)

$(OBJDIR):
	mkdir $(OBJDIR)


