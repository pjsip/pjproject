# Microsoft Developer Studio Project File - Name="pjlibpp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pjlibpp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pjlibpp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pjlibpp.mak" CFG="pjlibpp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pjlibpp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pjlibpp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/pjproject/pjlib/build", UIAAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pjlibpp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\output\pjlibpp-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir ".\output\pjlibpp-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\output\pjlibpp-i386-win32-vc6-release"
# PROP Intermediate_Dir ".\output\pjlibpp-i386-win32-vc6-release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../src" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PJ_WIN32" /D "PJ_M_I386" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlibp_vc6s.lib"

!ELSEIF  "$(CFG)" == "pjlibpp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\output\pjlibpp-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir ".\output\pjlibpp-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\output\pjlibpp-i386-win32-vc6-debug"
# PROP Intermediate_Dir ".\output\pjlibpp-i386-win32-vc6-debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../src" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PJ_WIN32" /D "PJ_M_I386" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib/pjlibp_vc6sd.lib"

!ENDIF 

# Begin Target

# Name "pjlibpp - Win32 Release"
# Name "pjlibpp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\src\pj++\compiletest.cpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\pj++.cpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\proactor.cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\src\pj++\hash.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\ioqueue.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\list.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\os.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pjlib++.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\pool.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\proactor.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\scanner.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\sock.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\string.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\timer.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\tree.hpp"
# End Source File
# Begin Source File

SOURCE="..\src\pj++\types.hpp"
# End Source File
# End Group
# End Target
# End Project
