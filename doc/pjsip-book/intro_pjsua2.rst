PJSUA2-High Level API
******************************
PJSUA2 is an object-oriented abstraction above PJSUA API. It provides high level API for constructing Session Initiation Protocol (SIP) multimedia user agent applications (a.k.a Voice over IP/VoIP softphones). It wraps together the signaling, media, and NAT traversal functionality into easy to use call control API, account management, buddy list management, presence, and instant messaging, along with multimedia features such as local conferencing, file streaming, local playback, and voice recording, and powerful NAT traversal techniques utilizing STUN, TURN, and ICE.

PJSUA2 is implemented on top of PJSUA-LIB API. The SIP and media features and object modelling follows what PJSUA-LIB provides (for example, we still have accounts, call, buddy, and so on), but the API to access them is different. These features will be described later in this chapter. PJSUA2 is a C++ library, which you can find under ``pjsip`` directory in the PJSIP distribution. The C++ library can be used by native C++ applications directly. But PJSUA2 is not just a C++ library. From the beginning, it has been designed to be accessible from high level non-native languages such as Java and Python. This is achieved by SWIG binding. And thanks to SWIG, binding to other languages can be added relatively easily in the future.


Building PJSUA2
======================
The PJSUA2 C++ library will be built by default by PJSIP build system.

The SWIG modules for Python and Java are built by invoking ``make`` manually from ``pjsip-apps/src/swig`` directory.


PJSUA2 Main Classes
======================
Here are the main classes of the PJSUA2:

Endpoint
--------------
This is the main class of PJSUA2. You need to instantiate one and exactly one of this class, and from the instance you can then initialize and start the library.

Account
-------------
An account specifies the identity of the person (or endpoint) on one side of SIP conversation. At least one account instance needs to be created before anything else, and from the account instance you can start making/receiving calls as well as adding buddies.

Media
----------
This is an abstract base class that represents a media element which is capable to either produce media or takes media. It is then subclassed into ``AudioMedia``, which is then subclassed into concrete classes such as ``AudioMediaPlayer`` and ``AudioMediaRecorder``.

Call
------
This class represents an ongoing call (or speaking technically, an INVITE session) and can be used to manipulate it, such as to answer the call, hangup the call, put the call on hold, transfer the call, etc.

Buddy
---------
This class represents a remote buddy (a person, or a SIP endpoint). You can subscribe to presence status of a buddy to know whether the buddy is online/offline/etc., and you can send and receive instant messages to/from the buddy.

General Concepts
==================
Class Usage Patterns
---------------------
With the methods of the main classes above, you will be able to invoke various operations to the object quite easily. But how can we get events/notifications from these classes? Each of the main classes above (except Media) will get their events in the callback methods. So to handle these events, just derive a class from the corresponding class (Endpoint, Call, Account, or Buddy) and implement/override the relevant method (depending on which event you want to handle). More will be explained in later sections.

Error Handling
---------------
We use exceptions as means to report error, as this would make the program flows more naturally. Operations which yield error will raise Error exception. If you prefer to display the error in more structured manner, the Error class has several members to explain the error, such as the operation name that raised the error, the error code, and the error message itself.

Asynchronous Operations
-------------------------
If you have developed applications with PJSIP, you'll know about this already. In PJSIP, all operations that involve sending and receiving SIP messages are asynchronous, meaning that the function that invokes the operation will complete immediately, and you will be given the completion status as callbacks.

Take a look for example the makeCall() method of the Call class. This function is used to initiate outgoing call to a destination. When this function returns successfully, it does not mean that the call has been established, but rather it means that the call has been initiated successfully. You will be given the report of the call progress and/or completion in the onCallState() callback method of Call class.

Threading
----------
For platforms that require polling, the PJSUA2 module provides its own worker thread to poll PJSIP, so it is not necessary to instantiate own your polling thread. Having said that the application should be prepared to have the callbacks called by different thread than the main thread. The PJSUA2 module itself is thread safe.

Problems with Garbage Collection
--------------------------------
Garbage collection (GC) exists in Java and Python (and other languages, but we don't support those for now), and there are some problems with it when it comes to PJSUA2 usage:

- it delays the destruction of objects (including PJSUA2 objects), causing the code in object's destructor to be executed out of order
- the GC operation may run on different thread not previously registered to PJLIB, causing assertion

Due to problems above, application '''MUST immediately destroy PJSUA2 objects using object's delete() method (in Java)''', instead of relying on the GC to clean up the object.

For example, to delete an Account, it's **NOT** enough to just let it go out of scope. Application MUST delete it manually like this (in Java):

.. code-block:: c++

    acc.delete();




Objects Persistence
---------------------
PJSUA2 includes PersistentObject class to provide functionality to read/write data from/to a document (string or file). The data can be simple data types such as boolean, number, string, and string arrays, or a user defined object. Currently the implementation supports reading and writing from/to JSON document ([http://tools.ietf.org/html/rfc4627 RFC 4627]), but the framework allows application to extend the API to support other document formats.

As such, classes which inherit from PersistentObject, such as EpConfig (endpoint configuration), AccountConfig (account configuration), and BuddyConfig (buddy configuration) can be loaded/saved from/to a file. Heres an example to save a config to a file:

.. code-block:: c++

    EpConfig epCfg;
    JsonDocument jDoc;
    epCfg.uaConfig.maxCalls = 61;
    epCfg.uaConfig.userAgent = "Just JSON Test";
    jDoc.writeObject(epCfg);
    jDoc.saveFile("jsontest.js");

To load from the file:

.. code-block:: c++

    EpConfig epCfg;
    JsonDocument jDoc;
    jDoc.loadFile("jsontest.js");
    jDoc.readObject(epCfg);


