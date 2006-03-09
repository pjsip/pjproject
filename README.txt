
See INSTALL.txt for compiling.


TOP LEVEL DIRECTORIES
======================
Below is the descriptions of the top-level directories:

-root
 -build..................... Makefiles includes, nothing interesting to see except
                             when porting to new platforms.
 -pjlib..................... Base library used by all other libraries.  It contains 
                             platform abstraction, data structures, etc. 
 -pjlib-util................ Utilities, such as text scanner, XML parser, etc.
 -pjmedia................... Media framework, contains:
                              - pjmedia.......... the core media framework, which 
                                                  contains codec framework, streams,
                                                  stream ports, conference bridge,
                                                  RTP/RTCP, SDP, SDP negotiator, etc.
                              - pjmedia-codec.... the static library container for 
                                                  all codecs. For the moment, it
                                                  contains GSM and SPEEX codec.
 -pjsip..................... SIP stack, contains:
                              - pjsip............ The core SIP stack, which contains
                                                  endpoint, transport layer, message and
                                                  URI structures, transaction layer, 
                                                  UA layer and dialog, utilities, etc.
                              - pjsip-simple..... SIMPLE (+presence, IM), contains
                                                  basic event framework, presence, and
                                                  instant messaging.
                              - pjsip-ua......... SIP "call" abstraction, which blends
                                                  INVITE session and SDP negotiation.
                                                  Also contains call features such as
                                                  call transfer, and client side SIP
                                                  registration.
                              - pjsua-lib........ Very high level UA app. library,
                                                  which blends all functionalities
                                                  together in very easy to use API.
                                                  Good to build a powerfull softphone
                                                  very quickly.
 -pjsip-apps................ Contains some sample applications:
                              - pjsua............ A powerful, console based SIP
                                                  UA, based on pjsua-lib.
                              - pjsip-perf....... SIP performance tester or call 
                                                  generator.


SUB-DIRECTORY LAYOUT
======================
Each subdirectories normally would have this layout:

 -bin...................... The binaries resulted from the build process will
                            go here.
 -build.................... Makefile and project files.
 -docs..................... Documentation specific to the project and doxygen config file
                            to generate documentation from the source code.
 -include.................. Header files.
 -lib...................... The static libraries resulted from the build process
                            will go here.
 -src...................... Source files.


YOUR EDITOR SETTINGS ARE IMPORTANT!
====================================
You need to set your editor settings to tab=8 and indent=4. For example,
with vim, you can do this with:
 :se ts=8
 :se sts=4


