# Microsoft Developer Studio Project File - Name="pjlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlib.mak" CFG="pjlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/pjproject/pjlib/build", UIAAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjlib_vc6_Release"
# PROP BASE Intermediate_Dir "output\pjlib_vc6_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "\.output\pjlib_vc6_Release"
# PROP Intermediate_Dir ".\output\pjlib_vc6_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /Zi /O2 /I "../src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib_vc6s.lib"

!ELSEIF  "$(CFG)" == "pjlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjlib_vc6_Debug"
# PROP BASE Intermediate_Dir ".\output\pjlib_vc6_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjlib_vc6_Debug"
# PROP Intermediate_Dir ".\output\pjlib_vc6_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "../src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlib_vc6sd.lib"

!ENDIF 

# Begin Target

# Name "pjlib - Win32 Release"
# Name "pjlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\pj\array.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\config.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\except.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\fifobuf.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\guid.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\hash.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_select.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue_winnt.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\list.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\log.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\log_stdout.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\md5.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\os_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_caching.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_dbg_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_policy.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\rbtree.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\scanner.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\string.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\stun.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\stun_client.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\timer.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\types.c
# End Source File
# Begin Source File

SOURCE=..\src\pj\xml.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\pj\array.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\compat.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\config.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\except.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\fifobuf.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\fwd.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\guid.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\hash.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\ioqueue.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\list.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\log.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\md5.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\os.h
# End Source File
# Begin Source File

SOURCE=..\src\pjlib.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\rbtree.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\scanner.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\string.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\stun.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\timer.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\types.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\xml.h
# End Source File
# End Group
# Begin Group "Inline Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\pj\array_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\list_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\pool_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\scanner_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\sock_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\string_i.h
# End Source File
# Begin Source File

SOURCE=..\src\pj\timer_i.h
# End Source File
# End Group
# End Target
# End Project
