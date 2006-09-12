

Getting Started: Building and Using PJSIP and PJMEDIA
[Last Update: 12/Sept/2006]

   This article describes how to get, build, and use the open source PJSIP and
   PJMEDIA SIP and media stack. You can get the online (and HTML) version of
   this file in http://www.pjsip.org/using.htm


If you're so impatient..

   If you just want to get going quickly (and maybe read this document later),
   this is what you can do to build the libraries:

   Building with GNU tools
          Just do:

   $ ./configure
   $ make dep && make clean && make

   Building with Microsoft Visual Studio
          Just follow the following steps:

         1. Open pjsip-apps/build/pjsip_apps.dsw workspace,
         2. Create an empty pjlib/include/pj/config_site.h, and
         3. build the pjsua application.

   Building for Windows Mobile
          Just follow the following steps:

         1. Open pjsip-apps/build/wince-evc4/wince_demos.vcw EVC4 workspace,
         2. Create an empty pjlib/include/pj/config_site.h, and
         3. build the pjsua_wince application.

   With  all  the  build systems, the output libraries will be put in lib
   directory under each projects, and the output binaries will be put in bin
   directory under each projects.


Table of Contents:
     _________________________________________________________________

   1. Getting the Source Distribution

     1.1 Getting the Release tarball

     1.2 Getting from Subversion trunk

     1.3 Source Directories Layout

   2. Build Preparation

     2.1 config_site.h file

     2.2 Disk Space Requirements

   3. Building Linux, *nix, *BSD, and MacOS X Targets with GNU Build Systems

     3.1 Supported Targets

     3.2 Requirements

     3.3 Running configure

     3.4 Running make

     3.5 Cross Compilation

   4. Building for Windows Targets with Microsoft Visual Studio

     4.1 Requirements

     4.2 Building the Projects

     4.3 Debugging the Sample Application

   5. Building for Windows Mobile Targets (Windows CE/WinCE/PDA/SmartPhone)

     5.1 Requirements

     5.2 Building the Projects

   6. Using PJPROJECT with Your Applications


   Appendix I: Common Problems/Frequently Asked Question (FAQ)

     I.1 fatal error C1083: Cannot open include file: 'pj/config_site.h':
   No such file or directory


1. Getting the Source Code Distribution
     _________________________________________________________________

   Since all libraries are released under Open Source license, all source code
   are available for your scrutinizing pleasure.

   All libraries (PJLIB, PJLIB-UTIL, PJSIP, PJMEDIA, and PJMEDIA-CODEC) are
   currently distributed under a single source tree, collectively named as
   PJPROJECT or just PJ libraries. These libraries can be obtained by either
   downloading the release tarball or getting them from the Subversion trunk.


1.1 Getting the Release tarball
     _________________________________________________________________

   Getting the released tarball is a convenient way to obtain stable version of
   PJPROJECT. The tarball may not contain the latest features or bug-fixes, but
   normally it is considered more stable as it will be tested more rigorously
   before it is released.

   You    can    get    the    latest    released    tarball   from   the
   http://www.pjsip.org/download.htm.


1.2 Getting from Subversion trunk
     _________________________________________________________________

   You can always get the most up-to-date version of the sources from the
   Subversion trunk. However, please bear in mind that the sources in the
   Subversion trunk may not be the most stable one. In fact, it may not even
   compile for some particular targets, because of the time lag in the updating
   process for all targets. Please consult the mailing list if you encounter
   such problems.

   Using Subversion also has benefits of keeping your source up to date with
   the main PJ source tree and to track your changes made to your local copy,
   if any.


What is Subversion

   Subversion is Open Source version control system similar to CVS. Subversion
   homepage is in http://subversion.tigris.org/


Getting Subversion Client

   Before you can download the PJ source files from pjsip.org SVN tree, you
   need  to  install  a Subversion client. You can download binaries from
   http://subversion.tigris.org/  and  follow the instructions there.
   Subversion clients are available for Windows, Linux, MacOS X, and many more
   platforms.


Getting the Source for The First Time

   Once Subversion client is installed, you can use these commands to initially
   retrieve the latest sources from the Subversion trunk:

   $ svn co http://svn.pjproject.net/repos/pjproject/trunk pjproject
   $ cd pjproject


Keeping Your Local Copy Up-to-Date

   Once you have your local copy of the sources, you will want to keep your
   local copy up to date by periodically synchronizing your source with the
   latest revision from the Subversion trunk. The mailing list provides best
   source of information about the availability of new updates in the trunk.

   You can use these commands to synchronize your copy with the main trunk:

   $ cd pjproject
   $ svn update


Tracking Local and Remote Changes

   In general, it is not recommended to keep your local changes (to the library
   source codes) for a long time, because the longer you keep your changes, the
   more chances that your source will be out-of-sync with the main PJ source
   tree (the trunk), because the trunk may be updated to support new features
   or to fix some bugs.

   The  best way to resolve this is to send your modification back to the
   author, so that he can change the copy in the SVN trunk.

   To see what files have been changed locally:

   $ cd pjproject
   $ svn status

   The above command only compares local file against the original local copy,
   so it doesn't require Internet connection to perform the check.

   To see what files have been changed both locally and remotely:

   $ cd pjproject
   $ svn status -u

   Note that svn status -u requires Internet connection to the SVN tree.


1.3 Source Directories Layout
     _________________________________________________________________

Top-Level Directory Layout

   The top-level directories (denoted as $PJ here) in the source distribution
   contains the sources of individual libraries:

   $PJ/build
          Contains makefiles that are common for all projects.

   $PJ/pjlib
          Contains PJLIB header and source files.

   $PJ/pjlib-util
          Contains PJLIB-UTIL header and source files.

   $PJ/pjmedia
          Contains PJMEDIA and PJMEDIA-CODEC header and source files.

   $PJ/pjsip
          Contains PJSIP header and source files.

   $PJ/pjsip-apps
          Contains source code for PJSUA and samples applications.


Individual Directory Inside Each Project

   The directories inside each project (for example, inside pjlib, pjmedia, or
   pjsip) further contains some sub-directories below:

   bin
          Contains binaries produced by the build process. The contents of this
          directory will not get synchronized with the SVN trunk.

   build
          Contains build scripts/makefiles, project files, project workspace,
          etc. to build the project. In particular, it contains one Makefile
          file  to  build the project with GNU build systems, and a *.dsw
          workspace file to build the library with Microsoft Visual Studio 6 or
          later.

   build/output
          The build/output directory contains the object files and other files
          generated by the build process.

   build/wince-evc4
          This directory contains the project/workspace files to build Windows
          CE/WinCE version of the project using Microsoft Embedded Visual C++
          4.

   docs
          Contains Doxygen configuration file (doxygen.cfg) to generate online
          documentation from the source files. The output documentation will be
          put in this directory as well (for example, docs/html directory for
          the HTML files).

   include
          Contains the header files for the project.

   lib
          Contains libraries produced by the build process.

   src
          Contains the source files of the project.


2. Build Preparation
     _________________________________________________________________

2.1 config_site.h file
     _________________________________________________________________

   Before you can compile and use the libraries, you need to create your
   config_site.h MANUALLY.

   (Sorry to write in red background, but this question comes out quite often
   so I thought it's worth to put some punctuation)

Q: What is config_site.h File

   The pjlib/include/pj/config_site.h contains your local customizations to the
   libraries.

Q: Why do we need config_site.h file

   You should put your customization in this file instead of modifying PJ's
   files,  because  if you modify PJ's files, then you will prevent those
   modified files from being updated next time you synchronize your local copy
   to the SVN trunk. Or even worse, you may accidently overwrite your local
   modification with the fresh copy from the SVN.

   Putting your local customization to the config_site.h solves this problem,
   because this file is not included in the version control.

Q: What customizations can be put in config_site.h file

   You  can  put  your  #define macros in this file. You can find list of
   configuration macros that you can override by scanning:
     * pjlib/config.h file
     * pjmedia/config.h file
     * pjsip/sip_config.h file

   You    can    also    see    a    sample    config_site.h    file   in
   pjlib/include/config_site_sample.h.

Q: How to create config_site.h file

   The simplest way is just to create an empty file.

   Another way to create your config_site.h is to write something like this:

   // Uncomment to get minimum footprint (suitable for 1-2 concurrent calls
   only)
   //#define PJ_CONFIG_MINIMAL_SIZE
   // Uncomment to get maximum performance
   //#define PJ_CONFIG_MAXIMUM_SPEED
   #include <pj/config_site_sample.h>


2.2 Disk Space Requirements
     _________________________________________________________________

   PJ will need
   currently about 50-60 MB of disk space to store the source files, and
     * approximately 30-50 MB of additional space for building each target

   (For example, Visual Studio Debug and Release are considered to be separate
   targets, so you'll need twice the capacity to build both of them)


3. Building Linux, *nix, *BSD, and MacOS X Targets with GNU Build Systems
     _________________________________________________________________

3.1 Supported Targets
     _________________________________________________________________

   The  new,  autoconf  based  GNU  build system can be used to build the
   libraries/applications for the following targets:
     * Linux/uC-Linux (i386, Opteron, Itanium, MIPS, PowerPC, etc.),
     * MacOS X (PowerPC),
     * mingw (i386),
     * *BSD (i386, Opteron, etc.),
     * RTEMS (ARM, powerpc),
     * etc.


3.2 Requirements
     _________________________________________________________________

   To use PJ's GNU build system, you would need the typical GNU tools such as:
     * GNU Make (other make will not work),
     * binutils,
     * gcc, and
     * sh compatible shell (for autoconf to work)

   On Windows, mingw will work, but cygwin currently doesn't. As usual, your
   mileage may vary.


3.3 Running configure
     _________________________________________________________________

Using Default Settings

   Just  run  configure  without any options to let the script detect the
   appropriate settings for the host:

   $ cd pjproject
   $ ./configure
   ...

   Notes:
          The  default settings build the library in "release" mode, with
          default CFLAGS set to "-O2 -DNDEBUG".

    Features Customization

   With the new autoconf based build system, most configuration/customization
   can  be  specified  as  configure  arguments.  You can get the list of
   customizable features by running ./configure --help:

   $ cd pjproject
   $ ./configure --help
   ...
   Optional Features:
   --disable-floating-point   Disable floating point where possible
   --disable-sound            Exclude sound (i.e. use null sound)
   --disable-small-filter     Exclude small filter in resampling
   --disable-large-filter     Exclude large filter in resampling
   --disable-g711-plc         Exclude G.711 Annex A PLC
   --disable-speex-aec        Exclude Speex Acoustic Echo Canceller/AEC
   --disable-g711-codec       Exclude G.711 codecs from the build
   --disable-l16-codec        Exclude Linear/L16 codec family from the build
   --disable-gsm-codec        Exclude GSM codec in the build
   --disable-speex-codec      Exclude Speex codecs in the build
   --disable-ilbc-codec       Exclude iLBC codec in the build
   ...                       

    Debug Version and Other Customizations

   The configure script accepts standard customization such as the CFLAGS,
   LDFLAGS, etc.

   For example, to build the libraries/application in debug mode:

   $ ./configure CFLAGS="-g"
   ...


  3.4 Cross Compilation
     _________________________________________________________________

   (.. to be completed)

   $ ./configure --target=powerpc-linux-unknown
   ...


  3.5 Running make
     _________________________________________________________________

   Once the configure script completes successfully, start the build process by
   invoking these commands:

   $ cd pjproject
   $ make dep
   $ make

   Note:
          You may need to call gmake instead of make for your host to invoke
          GNU make instead of the native make.

   Description of all make targets supported by the Makefile's:

   all
          The default (or first) make target to build the libraries/binaries.

   dep, depend
          Build dependencies rule from the source files.

   clean
          Clean the object files, but keep the output library/binary files
          intact.

   distclean, realclean
          Clean  all  generated  files  (object, libraries, binaries, and
          dependency files).


   Note:
          You can run make in the top-level PJ directory or in build directory
          under each project to build only the particular project.


4. Building for Windows Targets with Microsoft Visual Studio
     _________________________________________________________________

  4.1 Requirements
     _________________________________________________________________

   In order to build the projects using Microsoft Visual Studio, you need to
   have one of the following:

     * Microsoft Visual Studio 6,
     * Microsoft Visual Studio .NET 2002,
     * Microsoft Visual Studio .NET 2003,
     * Microsoft Visual Studio Express 2005 with Platform SDK and DirectX SDK,

   For the host, you need:
     * Windows NT, 2000, XP, 2003, or later (it may work on Windows 95 or 98,
       but this has not been tested),
     * Sufficient amount of RAM for the build process,


  4.2 Building the Projects
     _________________________________________________________________

   Follow the steps below to build the libraries/application using Visual
   Studio:
    1. Open Visual Studio 6 workspace file pjsip-apps/build/pjsip_apps.dsw. If
       you're using later version of Visual Studio, it should convert the
       workspace file and project files into the new formats.
    2. Set pjsua as Active Project.
    3. Select Debug or Release build as appropriate.
    4. Build the project. This will build pjsua application and all libraries
       needed by pjsua.
    5. After  successful  build,  the pjsua application will be placed in
       pjsip-apps/bin directory, and the libraries in lib directory under each
       projects.

   To build the samples:
    1. (Still using the same workspace)
    2. Set samples project as Active Project
    3. Select Debug or Release build as appropriate.
    4. Build the project. This will build all sample applications and all
       libraries needed.
    5. After  successful build, the sample applications will be placed in
       pjsip-apps/bin/samples directory, and the libraries in lib directory
       under each projects.

  4.3 Debugging the Sample Application
     _________________________________________________________________

   The sample applications are build using Samples.mak makefile, therefore it
   is  difficult  to  setup  debugging session in Visual Studio for these
   applications. To solve this issue, the pjsip_apps workspace contain one
   project  called  sample_debug  which  can  be used to debug the sample
   application.

   To setup debugging using sample_debug project:
    1. (Still using pjsip_apps workspace)
    2. Set sample_debug project as Active Project
    3. Edit debug.c file inside this project.
    4. Modify the #include line to include the particular sample application
       you want to debug
    5. Select Debug build.
    6. Build and debug the project.


5. Building for Windows Mobile Targets (Windows CE/WinCE/PDA/SmartPhone)
     _________________________________________________________________

   PJ supports building SIP and media stacks and applications for Windows
   Mobile targets. A very simple WinCE SIP user agent (with media) application
   is provided just as proof of concept that the port works.

  5.1 Requirements
     _________________________________________________________________

   You will need the following to build SIP and media components for Windows
   Mobile:
     * Microsoft Embedded Visual C++ 4 with appropriate SDKs, or
     * Microsoft Visual Studio 2005 for Windows Mobile with appropriate SDKs.

   Note that VS2005 is not directly supported (as I don't have the tools), but
   it is reported to work (and I assumed that VS2005 for Windows Mobile can
   import EVC4 workspace file).

  5.2 Building the Projects
     _________________________________________________________________

   The Windows Mobile port is included in the main source distribution. Please
   follow  the  following  steps  to build the WinCE libraries and sample
   application:
    1. Open pjsip-apps/build/wince-evc4/wince_demos.vcw workspace file. If
       you're using later version of EVC4 this may cause the workspace file to
       be converted to the current version of your Visual Studio.
    2. Select pjsua_wince project as the Active Project.
    3. Select the appropriate SDK (for example Pocket PC 2003 SDK or SmartPhone
       2003 SDK)
    4. Select the appropriate configuration (for example, Win32 (WCE Emulator
       Debug) if you plan to debug the program in emulator)
    5. Select the appropriate device (Emulator or the actual Device).
    6. Build the project. This will build the sample WinCE application and all
       libraries (SIP, Media, etc.) needed by this application.

   Notes
          If your config_site.h includes config_site_sample.h file, then
          there are certain configuration in config_site_sample.h that get
          activated for Windows CE targets.


6. Using PJPROJECT with Your Applications
     _________________________________________________________________

   Regardless if you use Visual Studio or GNU build systems or other tools, in
   order to build your application to use PJSIP and PJMEDIA SIP and media
   stack, you need to configure your build tools as follows:
    1. Put these include directories in your include search path:
          + pjlib/include
          + pjlib-util/include
          + pjmedia/include
          + pjsip/include
    2. Put these library directories in your library search path:
          + pjlib/lib
          + pjlib-util/lib
          + pjmedia/lib
          + pjsip/lib
    3. Include the relevant PJ header files in your application source file.
       For example, using these would include ALL APIs exported by PJ:

      #include <pjlib.h>
      #include <pjlib-util.h>
      #include <pjsip.h>
      #include <pjsip_ua.h>
      #include <pjsip_simple.h>
      #include <pjsua.h>
      #include <pjmedia.h>
      #include <pjmedia-codec.h>
       (Note: the documentation of the relevant libraries should say which
       header files should be included to get the declaration of the APIs).
    4. Declare the OS macros.
          + For Windows applications built with Visual Studio, you need to
            declare PJ_WIN32=1 macro in your project settings (declaring the
            macro in your source file may not be sufficient).
          + For Windows Mobile applications build with Visual C++, you need to
            declare PJ_WIN32_WINCE=1 macro in your project settings.
          + For GNU build system/autoconf based build system, you need to
            declare PJ_AUTOCONF=1 macro when compiling your applications.
       (Note: the old PJ build system requires declaring the target processor
       with PJ_M_XXX=1 macro, but this has been made obsolete. The target
       processor  will  be  detected  from compiler's predefined macro by
       pjlib/config.h file).
    5. Link with the appropriate PJ libraries. The following libraries will
       need to be included in your library link specifications:

        pjlib
                Base library used by all libraries.

        pjlib-util
                Auxiliary library containing scanner, XML, STUN, MD5, getopt,
                etc, used by the SIP and media stack.

        pjsip
                SIP core stack library.

        pjsip-ua
                SIP user agent library containing INVITE session, call
                transfer, client registration, etc.

        pjsip-simple
                SIP SIMPLE library for base event framework, presence, instant
                messaging, etc.

        pjsua
                High level SIP UA library, combining SIP and media stack into
                high-level easy to use API.

        pjmedia
                The media framework.

        pjmedia-codec
                Container library for various codecs such as GSM, Speex, and
                iLBC.


   Note: the actual library names will be appended with the target name and the
   build configuration. For example:

        For Visual Studio builds
                The actual library names will look like
                pjlib-i386-win32-vc6-debug.lib,
                pjlib-i386-win32-vc6-release.lib, etc., depending on whether
                you build the Debug or Release version of the library.

        For GNU builds
                You can get the library suffix by including PJ's build.mak file
                from the root PJ directory (the suffix is contained in
                TARGET_NAME variable). For example, to link with PJLIB and
                PJMEDIA, you can use this in syntax your LDFLAGS:
                "-lpj-$(TARGET_NAME) -lpjmedia-$(TARGET_NAME)"

   Should you encounter any difficulties with using PJ libraries, you can
   consult the mailing list for some help.


Appendix I: Common Problems/Frequently Asked Question (FAQ)
     _________________________________________________________________

  I.1 fatal error C1083: Cannot open include file: 'pj/config_site.h': No such
  file or directory

   If  you  encounter  this  error, then probably you haven't created the
   config_site.h file. Please follow the Build Preparation instructions
   above to create this file.








     _________________________________________________________________

   Feedback:
          Thanks for downloading PJ libraries and for reading this document. If
          you'd like to comment on anything, send your email to me and I would
          be delighted to hear them. -benny <bennylp at pjsip dot org>

