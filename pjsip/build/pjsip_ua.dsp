# Microsoft Developer Studio Project File - Name="pjsip_ua" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjsip_ua - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_ua.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjsip_ua.mak" CFG="pjsip_ua - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjsip_ua - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjsip_ua - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/pjproject/pjsip/build", RIAAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjsip_ua - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjsip_ua_vc6_Release"
# PROP BASE Intermediate_Dir ".\output\pjsip_ua_vc6_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjsip_ua_vc6_Release"
# PROP Intermediate_Dir ".\output\pjsip_ua_vc6_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /I "../src" /I "../../pjlib/src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjsip_ua_vc6s.lib"

!ELSEIF  "$(CFG)" == "pjsip_ua - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjsip_ua_vc6_Debug"
# PROP BASE Intermediate_Dir ".\output\pjsip_ua_vc6_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjsip_ua_vc6_Debug"
# PROP Intermediate_Dir ".\output\pjsip_ua_vc6_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../src" /I "../../pjlib/src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjsip_ua_vc6sd.lib"

!ENDIF 

# Begin Target

# Name "pjsip_ua - Win32 Release"
# Name "pjsip_ua - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pjsip-ua\sip_dialog.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsip-ua\sip_reg.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsip-ua\sip_ua.c"
# End Source File
# Begin Source File

SOURCE="..\src\pjsip-ua\sip_ua_private.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\pjsip_ua.h
# End Source File
# Begin Source File

SOURCE=..\src\pjsip_mod_ua\sip_dialog.h
# End Source File
# Begin Source File

SOURCE=..\src\pjsip_mod_ua\sip_reg.h
# End Source File
# Begin Source File

SOURCE=..\src\pjsip_mod_ua\sip_ua.h
# End Source File
# Begin Source File

SOURCE=..\src\pjsip_mod_ua\sip_ua_private.h
# End Source File
# End Group
# End Target
# End Project
