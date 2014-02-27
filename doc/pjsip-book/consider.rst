
Development Guidelines and Considerations
*****************************************

Development Guidelines
======================

Preparation
------------
* **Essential:** Familiarise yourself with SIP. You don't need to be an expert, but SIP knowledge is essential. 
* Check out our features in `Datasheet <http://trac.pjsip.org/repos/wiki/PJSIP-Datasheet>`_. Other features may be provided by our `community <http://trac.pjsip.org/repos/wiki/Projects_Using_PJSIP>`_.
* All PJSIP documentation is indexed in our `Trac site <http://trac.pjsip.org/repos>`_.


Development
-------------
* **Essential:** Follow the `Getting Started <http://trac.pjsip.org/repos/wiki/Getting-Started>`_ instructions to build PJSIP for your platform.
* **Essential:** Interactive debugging capability is essential during development
* Start with default settings in `<pj/config_site_sample.h>`. The default settings should be good to get you started. You can always optimize later after things are running okay.

Coding Style
-------------
**Essential:** set your editor to use 8 characters tab size in order to see PJSIP source correctly.

Detailed below is the PJSIP coding style. You don't need to follow it unless you are submitting patches to PJSIP:

* Indentation uses tabs and spaces. Tab size is 8 characters, indentation 4.
* All public API in header file must be documented in Doxygen format.
* Apart from that, we mostly just use `K & R style <http://en.wikipedia.org/wiki/1_true_brace_style#K.26R_style>`_, which is the only correct style anyway.


Deployment
-----------
* **Essential:** Logging is essential when troubleshooting any problems. The application MUST be equipped with logging capability. Enable PJSIP log at level 5.


Platform Consideration
========================
Platform selection is usually driven by business motives. The selection will affect all aspects of development, and here we will cover  considerations for each platforms that we support.

Windows Desktop
---------------
Windows is supported from Windows 2000 up to the recent Windows 8 and beyond. All features are expected to work. 64bit support was added recently. Development is based on Visual Studio. Considerations for this platform include:

#. Because Visual Studio file format keeps changing on every release, we decided to support the lowest denominator, namely Visual Studio 2005. Unfortunately the project upgrade procedure fails on Visual Studio 2010, and we don't have any solution for that. VS 2008 and VS 2012 onwards should work.

MacOS X
-------
All features are expected to work. Considerations include:

#. Development with XCode is currently not supported. This is **not** to say that you cannot use XCode, but PJSIP only provides basic Makefiles and if you want to use XCode you'd need to arrange the project yourself.
#. Mac systems typically provides very good sound device, so we don't expect any problems with audio on Mac. 

Linux Desktop
-------------
All features are expected to work. Linux considerations:

#. Use our native ALSA backend instead of PortAudio because ALSA has less jitter than OSS and our backend is more lightweight than PortAudio


iOS for iPhone, iPad, and iPod Touch
------------------------------------
All features except video are expected to work (video is coming soon!). Considerations for iOS:

#. You need to use TCP transport for SIP for the background feature to work
#. IP change (for example when user is changing access point) is a feature frequently asked by developers and you can find the documentation here: http://trac.pjsip.org/repos/wiki/IPAddressChange
#. There are some specific issues for iOS 7 and beyond, please see http://trac.pjsip.org/repos/ticket/1697
#. If SSL is needed, you need to compile OpenSSL for iOS


Android
-------
All features except video are expected to work (video is coming soon!). Considerations for Android:

#. You can only use PJSUA2 Java binding for this target.
#. It has been reported that Android audio device is not so good in general, so some audio tuning may be needed. Echo cancellation also needs to be checked.
#. This is also a new platform for us. 


Symbian
-------
Symbian has been supported for a long time. In general all features (excluding video) are expected to work, but we're not going to do Symbian specific development anymore. Other considerations for Symbian:

#. The MDA audio is not very good (it has high latency), so normally you'd want to use Audio Proxy Server (APS) or VoIP Audio Service (VAS) for the audio device, which we support. Using these audio backends will also provide us with high quality echo cancellation as well as low bitrate codecs such as AMR-NB, G.729, and iLBC. But VAS and APS requires purchase of Nokia development certificate to sign the app, and also since APS and VAS only run on specific device type, you need to package the app carefully and manage the deployment to cover various device types.


BlackBerry 10
-------------
BlackBerry 10 (BB10) is supported since PJSIP version 2.2. As this is a relatively new platform for us, we are currently listening to developer's feedback regarding the port. But so far it seems to be working well. Some considerations for BB10 platform include:

#. IP change (for example when user is changing access point) is a feature frequently asked by developers and you can find the documentation here: http://trac.pjsip.org/repos/wiki/IPAddressChange


Windows Mobile
--------------
This is the old Windows Mobile platform that is based on WinCE. This platform has been supported for a long time. We expect all features except video to work, but there may be some errors every now and then because this target is not actively maintained. No new development will be done for this platform.

Other considerations for Windows Mobile platform are:

#. The quality of audio device on WM varies a lot, and this affects audio latency. Audio latency could go as high as hundreds of millisecond on bad hardware.
#. Echo cancellation could be a problem. We can only use basic echo suppressor due to hardware limitation, and combined with bad quality of audio device, it may cause ineffective echo cancellation. This could be mitigated by setting the audio level to low.


Windows Phone 8
---------------
Windows Phone 8 (WP8) support is being added and is still under development on `projects/winphone` branch. Specific considerations for this platform are:

#. WP8 governs specific interaction with WP8 GUI and framework that needs to be followed by application in order to make VoIP call work seamlessly on the device. Some lightweight process will be created by WP8 framework in order for background call to work and PJSIP needs to put its background processing in this process' context. Currently this feature is under development.



Embedded Linux
--------------
In general embedded Linux support is similar to Linux and we find no problems with it. We found some specific considerations for embedded Linux as follows:

#. The performance of the audio device is probably the one with most issues, as some development boards does not have a decent sound device. Typically there is high audio jitter (or burst) and latency. This will affect end to end audio latency and also the performance of the echo canceller. Also we found that ALSA generally works better than OSS, so if you can have ALSA up and running that will be better. Use our native ALSA backend audio device instead of PortAudio since it is simpler and lighter.


QNX or Other Posix Embedded OS
------------------------------
This is not part of our officially supported OS platforms, but users have run PJSIP on QNX and BlackBerry 10 is based on QNX too. Since QNX provides Posix API, and maybe by using the settings found in the configure-bb10 script, PJSIP should be able to run on it, but you need to develop PJMEDIA sound device wrapper for your audio device. Other than this, we don't have enough experience to comment on the platform. 


Other Unix Desktop OSes
-----------------------
Community members, including myself, have occasionally run PJSIP on other Unix OSes such as Solaris, FreeBSD, and OpenBSD. We expect PJSIP to run on these platforms (maybe with a little kick).


Porting to Other Embedded OS
------------------------------
It is possible to port PJSIP to other embedded OS or even directly to device without OS and people have done so. In general, the closer resemblance the new OS to existing supported OS, the easier the porting job will be. The good thing is, PJSIP has been made to be very very portable, and system dependent features are localized in PJLIB and PJMEDIA audio device, so the effort is more quantifiable. Once you are able to successfully run *pjlib-test*, you are more or less there with your porting effort. Other than that, if you really want to port PJSIP to new platform, you probably already know what you're doing. 



Which API to Use
================

PJSIP, PJMEDIA, and PJNATH Level
--------------------------------
At the lowest level we have the individual PJSIP **C** libraries, which consist of PJSIP, PJMEDIA, and PJNATH, with PJLIB-UTIL and PJLIB as support libraries. This level provides the most flexibility, but it's also the hardest to use. The only reason you'd want to use this level is if:

#. You only need the individual library (say, PJNATH)
#. You need to be very very tight in footprint (say when things need to be measured in Kilobytes instead of Megabytes)
#. You are **not** developing a SIP client

Use the corresponding PJSIP, PJMEDIA, PJNATH manuals from http://trac.pjsip.org/repos/ for information on how to use the libraries. If you use PJSIP, the PJSIP Developer's Guide (PDF) from that page provides in-depth information about PJSIP library.  

PJSUA-LIB API
-------------
Next up is PJSUA-LIB API that combines all those libraries into a high level, integrated client user agent library written in C. This is the library that most PJSIP users use, and the highest level abstraction before pjsua2 was created. 

Motivations for using PJSUA-LIB library includes:

#. Developing client application (PJSUA-LIB is optimized for developing client app)
#. Better efficiency than higher level API


PJSUA2 C++ API
--------------
pjsua2 is a new, objected oriented, C++ API created on top of PJSUA-LIB. The API is different than PJSUA-LIB, but it should be even easier to use and it should have better documentation too (such as this book). The pjsua2 API removes most cruxes typically associated with PJSIP, such as the pool and pj_str_t, and add new features such as object persistence so you can save your configs to a file, for example. All data structures are rewritten for more clarity. 

A C++ application can use pjsua2 natively, while at the same time still has access to the lower level objects if it needs to. This means that the C++ application should not lose any information from using the C++ abstraction, compared to if it is using PJSUA-LIB directly. The C++ application also should not lose the ability to extend the library. It would still be able to register a custom PJSIP module, pjmedia_port, pjmedia_transport, and so on.

Benefits of using pjsua2 C++ API include:

#. Cleaner object oriented API
#. Uniform API for higher level language such as Java and Python
#. Persistence API
#. The ability to access PJSUA-LIB and lower level libraries when needed (including the ability to extend the libraries, for example creating custom PJSIP module, pjmedia_port, pjmedia_transport, etc.)


Some considerations on PJSUA2 C++ API are:

#. Instead of returning error, the API uses exception for error reporting
#. It uses standard C++ library (STL)
#. The performance penalty due to the API abstraction should be negligible on typical modern device



PJSUA2 API for Java, Python, and Others
---------------------------------------
The PJSUA2 API is also available for non-native code via SWIG binding. Configurations for Java and Python are provided with the distribution. Thanks to SWIG, other language bindings may be generated relatively easily.
 
The pjsua2 API for non-native code is effectively the same as pjsua2 C++ API. However, unlike C++, you cannot access PJSUA-LIB and the underlying C libraries from the scripting language, hence you are limited to what pjsua2 provides.

You can use this API if native application development is not available in target platform (such as Android), or if you prefer to develop with non-native code instead of C/C++.




Network and Infrastructure Considerations
=========================================

NAT Issues
----------
TBD.

TCP Requirement
---------------
If you support iOS devices in your service, you need to use TCP, because only TCP will work on iOS device when it is in background mode. This means your infrastructure needs to support TCP. 


Sound Device
============

Latency
-------
TBD.

Echo Cancellation
-----------------
TBD.



