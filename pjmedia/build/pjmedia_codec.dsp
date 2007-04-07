# Microsoft Developer Studio Project File - Name="pjmedia_codec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjmedia_codec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_codec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia_codec.mak" CFG="pjmedia_codec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjmedia_codec - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjmedia_codec - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /I "../../third_party/speex/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjmedia-codec-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /ZI /Od /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /I "../../third_party/speex/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "pjmedia_codec - Win32 Release"
# Name "pjmedia_codec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\l16.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex_codec.c"

!IF  "$(CFG)" == "pjmedia_codec - Win32 Release"

!ELSEIF  "$(CFG)" == "pjmedia_codec - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\include\pjmedia-codec\config.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\gsm.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\ilbc.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\l16.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\speex.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\types.h"
# End Source File
# End Group
# Begin Group "iLBC Codec"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\anaFilter.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\anaFilter.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\constants.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\constants.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\createCB.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\createCB.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\doCPLC.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\doCPLC.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\enhancer.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\enhancer.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\filter.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\filter.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\FrameClassify.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\FrameClassify.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\gainquant.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\gainquant.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\getCBvec.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\getCBvec.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\helpfun.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\helpfun.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\hpInput.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\hpInput.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\hpOutput.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\hpOutput.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iCBConstruct.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iCBConstruct.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iCBSearch.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iCBSearch.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_decode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_decode.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_define.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_encode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_encode.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\iLBC_test.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\LPCdecode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\LPCdecode.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\LPCencode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\LPCencode.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\lsf.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\lsf.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\packing.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\packing.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\StateConstructW.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\StateConstructW.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\StateSearchW.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\StateSearchW.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\syntFilter.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\ilbc\syntFilter.h"
# End Source File
# End Group
# End Target
# End Project
