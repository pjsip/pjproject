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
# ADD CPP /nologo /MD /W4 /O2 /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FD /c
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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /ZI /Od /I "../include" /I "../../pjlib/include" /I "../src/pjmedia-codec" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PJ_WIN32=1 /D PJ_M_I386=1 /D "HAVE_CONFIG_H" /FR /FD /GZ /c
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
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex_codec.c"
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

SOURCE="..\include\pjmedia-codec\speex.h"
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
# Begin Group "Speex Codec"

# PROP Default_Filter ""
# Begin Group "Speex Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\arch.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\cb_search.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\cb_search_arm4.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\cb_search_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\cb_search_sse.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\config.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\filters.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\filters_arm4.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\filters_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\filters_sse.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\fixed_arm4.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\fixed_arm5e.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\fixed_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\fixed_debug.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\fixed_generic.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lpc.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lpc_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lsp.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\ltp.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\ltp_arm4.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\ltp_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\ltp_sse.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\math_approx.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\misc.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\misc_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\modes.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\nb_celp.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\pseudofloat.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\quant_lsp.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\sb_celp.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\smallft.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_bits.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_callbacks.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_echo.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_header.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_jitter.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_preprocess.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_stereo.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_types.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\stack_alloc.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vbr.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vq.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vq_arm4.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vq_bfin.h"
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vq_sse.h"
# End Source File
# End Group
# Begin Group "Speex Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\bits.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\cb_search.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_10_16_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_10_32_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_20_32_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_5_256_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_5_64_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\exc_8_128_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\filters.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\gain_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\gain_table_lbr.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\hexc_10_32_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\hexc_table.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\high_lsp_tables.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lpc_spx.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lsp.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\lsp_tables_nb.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\ltp.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\math_approx.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\misc.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\modes.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\nb_celp.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\preprocess_spx.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\quant_lsp.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\sb_celp.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\smallft.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_callbacks.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\speex_header.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\stereo.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vbr.c"
# ADD CPP /W4
# End Source File
# Begin Source File

SOURCE="..\src\pjmedia-codec\speex\vq.c"
# ADD CPP /W4
# End Source File
# End Group
# End Group
# End Target
# End Project
