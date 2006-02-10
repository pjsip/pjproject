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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W4 /GX /O2 /I "../include" /I "../../pjlib/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-vc6.lib"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../include" /I "../../pjlib/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\pjmedia-codec-vc6d.lib"

!ENDIF 

# Begin Target

# Name "pjmedia_codec - Win32 Release"
# Name "pjmedia_codec - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\pjmedia-codec.c"
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

SOURCE="..\include\pjmedia-codec.h"
# End Source File
# Begin Source File

SOURCE="..\include\pjmedia-codec\types.h"
# End Source File
# End Group
# Begin Group "GSM 06.10 Library"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\add.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\code.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\config.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\debug.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\decode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_create.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_decode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_destroy.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_encode.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_option.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\gsm_print.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\long_term.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\lpc.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\preprocess.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\private.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\proto.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\rpe.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\short_term.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\table.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\toast.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\gsm\unproto.h"
# End Source File
# End Group
# End Target
# End Project
