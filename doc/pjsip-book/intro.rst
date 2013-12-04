

Introduction to PJSUA2
*******************************
This documentation is intended for developers looking to develop Session Initiation Protocol (SIP) based client application. Some knowledge on SIP is definitely required, and of course some programming experience. Prior knowledge of PJSUA C API is not needed, although it will probably help.

This PJSUA2 module provides very high level API to do SIP calls, presence, and instant messaging, as well as handling media and NAT traversal. Knowledge of SIP protocol details (such as the grammar, transaction, dialog, and whatnot) are not required, however you should know how SIP works in general and particularly how to configure SIP clients to, e.g. register to a SIP provider, specify SIP URI to make call to, and so on, to use the module.

Getting Started with PJSIP
==============================
To begin using PJSIP
http://trac.pjsip.org/repos/wiki/Getting-Started

PJSIP Info and Documentation
================================
PJSIP General Wiki:
http://trac.pjsip.org/repos/wiki

PJSIP FAQ:
http://trac.pjsip.org/repos/wiki/FAQ

PJSIP Reference Manual:
http://trac.pjsip.org/repos/wiki - Reference Manual

Building PJSUA2
=================
PJSUA2 API declaration can be found in pjproject/pjsip/include/pjsua2 while the source codes are located in pjproject/pjsip/src/pjsua2. It will be automatically built when you compile PJSIP.

PJSUA2 Language Binding Support
===================================
Optional if you want to:
​SWIG (minimum version 2.0.5)

