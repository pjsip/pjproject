# Microsoft Developer Studio Project File - Name="pjmedia" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjmedia - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjmedia.mak" CFG="pjmedia - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjmedia - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjmedia - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/pjproject/pjsip/build", RIAAAAAA"
# PROP Scc_LocalPath "..\..\pjsip\build"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjmedia - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjmedia_vc6_Release"
# PROP BASE Intermediate_Dir ".\output\pjmedia_vc6_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjmedia_vc6_Release"
# PROP Intermediate_Dir ".\output\pjmedia_vc6_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O2 /I "../../pjlib/src" /I "../src" /I "../../pjsdp/src" /I "../src/pjmedia/portaudio" /D "NDEBUG" /D "PA_NO_ASIO" /D "WIN32" /D "_MBCS" /D "_LIB" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjmedia_vc6s.lib"

!ELSEIF  "$(CFG)" == "pjmedia - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjmedia_vc6_Debug"
# PROP BASE Intermediate_Dir ".\output\pjmedia_vc6_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjmedia_vc6_Debug"
# PROP Intermediate_Dir ".\output\pjmedia_vc6_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../../pjlib/src" /I "../src" /I "../../pjsdp/src" /I "../src/pjmedia/portaudio" /D "_DEBUG" /D "PA_NO_ASIO" /D "WIN32" /D "_MBCS" /D "_LIB" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjmedia_vc6sd.lib"

!ENDIF 

# Begin Target

# Name "pjmedia - Win32 Release"
# Name "pjmedia - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\pjmedia\codec.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\dsound.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\g711.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\jbuf.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\mediamgr.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\nullsound.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\pasound.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\rtcp.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\rtp.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\sdp.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\session.c
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\stream.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\pjmedia\codec.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\config.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\jbuf.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\mediamgr.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\rtcp.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\rtp.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\sdp.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\session.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\sound.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\stream.h
# End Source File
# End Group
# Begin Group "PortAudio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\dsound_wrapper.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\dsound_wrapper.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_allocation.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_allocation.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_converters.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_converters.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_cpuload.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_cpuload.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_dither.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_dither.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_endianness.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_front.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_hostapi.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_process.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_process.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_skeleton.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_stream.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_stream.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_trace.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_trace.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_types.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_util.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_win_ds.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_win_hostapis.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_win_util.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_win_wmme.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_win_wmme.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_x86_plain_converters.c
# ADD CPP /W3
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\pa_x86_plain_converters.h
# End Source File
# Begin Source File

SOURCE=..\src\pjmedia\portaudio\portaudio.h
# End Source File
# End Group
# End Target
# End Project
