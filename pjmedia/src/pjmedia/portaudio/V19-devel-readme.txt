STATUS:

MME, DirectSound and ASIO versions are more-or-less working. See FIXMEs @todos
and the proposals matrix at portaudio.com for further status.

The pa_tests directory contains tests. pa_tests/README.txt notes which tests
currently build.  

The PaUtil support code is finished enough for other implementations to be
ported. No changes are expected to be made to the definition of the PaUtil
functions.

Note that it's not yet 100% clear how the current support functions
will interact with blocking read/write streams.

BUILD INSTRUCTIONS

to build tests/patest_sine.c you will need to compile and link the following
files (MME)
pa_common\pa_process.c
pa_common\pa_skeleton.c
pa_common\pa_stream.c
pa_common\pa_trace.c
pa_common\pa_converters.c
pa_common\pa_cpuload.c
pa_common\pa_dither.c
pa_common\pa_front.c
pa_common\pa_allocation.h
pa_win\pa_win_util.c
pa_win\pa_win_hostapis.c
pa_win_wmme\pa_win_wmme.c

see below for a description of these files.
               

FILES:

portaudio.h
    public api header file

pa_front.c
    implements the interface defined in portaudio.h. manages multiple host apis.
    validates function parameters before calling through to host apis. tracks
    open streams and closes them at Pa_Terminate().

pa_util.h 
    declares utility functions for use my implementations. including utility
    functions which must be implemented separately for each platform.

pa_hostapi.h
    hostapi representation structure used to interface between pa_front.c
    and implementations
    
pa_stream.c/h
    stream interface and representation structures and helper functions
    used to interface between pa_front.c and implementations

pa_cpuload.c/h
    source and header for cpu load calculation facility

pa_trace.c/h
    source and header for debug trace log facility

pa_converters.c/h
    sample buffer conversion facility

pa_dither.c/h
    dither noise generator

pa_process.c/h
    callback buffer processing facility including interleave and block adaption

pa_allocation.c/h
    allocation context for tracking groups of allocations

pa_skeleton.c
    an skeleton implementation showing how the common code can be used.

pa_win_util.c
    Win32 implementation of platform specific PaUtil functions (memory allocation,
    usec clock, Pa_Sleep().)  The file will be used with all Win32 host APIs.
    
pa_win_hostapis.c
    contains the paHostApiInitializers array and an implementation of
    Pa_GetDefaultHostApi() for win32 builds.

pa_win_wmme.c
    Win32 host api implementation for the windows multimedia extensions audio API.

pa_win_wmme.h
    public header file containing interfaces to mme-specific functions and the
    deviceInfo data structure.


CODING GUIDELINES:

naming conventions:
    #defines begin with PA_
    #defines local to a file end with _
    global utility variables begin with paUtil
    global utility types begin with PaUtil  (including function types)
    global utility functions begin with PaUtil_
    static variables end with _
    static constants begin with const and end with _
    static funtions have no special prefix/suffix

In general, implementations should declare all of their members static,
except for their initializer which should be exported. All exported names
should be preceeded by Pa<MN>_ where MN is the module name, for example
the windows mme initializer should be named PaWinWmme_Initialize().

Every host api should define an initializer which returns an error code
and a PaHostApiInterface*. The initializer should only return an error other
than paNoError if it encounters an unexpected and fatal error (memory allocation
error for example). In general, there may be conditions under which it returns
a NULL interface pointer and also returns paNoError. For example, if the ASIO
implementation detects that ASIO is not installed, it should return a
NULL interface, and paNoError.

Platform-specific shared functions should begin with Pa<PN>_ where PN is the
platform name. eg. PaWin_ for windows, PaUnix_ for unix.

The above two conventions should also be followed whenever it is necessary to
share functions accross multiple source files.

Two utilities for debug messages are provided. The PA_DEBUG macro defined in
pa_implementation.h provides a simple way to print debug messages to stderr.
Due to real-time performance issues, PA_DEBUG may not be suitable for use
within the portaudio processing callback, or in other threads. In such cases
the event tracing facility provided in pa_trace.h may be more appropriate.

If PA_LOG_API_CALLS is defined, all calls to the public PortAudio API
will be logged to stderr along with parameter and return values.


TODO:
    (this list is totally out of date)
    
    finish coding converter functions in pa_converters.c (anyone?)

    implement block adaption in pa_process.c (phil?)

    fix all current tests to work with new code. this should mostly involve
    changing PortAudioStream to PaStream, and GetDefaultDeviceID to GetDefaultDevice etc.

    write some new tests to exercise the multi-api functions

    write (doxygen) documentation for pa_trace (phil?)

    remove unused typeids from PaHostAPITypeID

    create a global configuration file which documents which PA_ defines can be
    used for configuration

    need a coding standard for comment formatting

    migrate directx (phil)

    migrate asio (ross?, stephane?)

    see top of pa_win_wmme.c for MME todo items (ross)

    write style guide document (ross)
    
    
DESIGN ISSUES:
    (this list is totally out of date)
    
    consider removing Pa_ConvertHostApiDeviceIndexToGlobalDeviceIndex() from the API

    switch to new latency parameter mechanism now (?)
    
    question: if input or outputDriverInfo structures are passed for a different
    hostApi from the one being called, do we return an error or just ignore
    them? (i think return error)

    consider renaming PortAudioCallback to PaStreamCallback

    consider renaming PaError, PaResult
    

ASSORTED DISORGANISED NOTES:

    NOTE:
        pa_lib.c performs the following validations for Pa_OpenStream() which we do not currently do:
        - checks the device info to make sure that the device supports the requested sample rate,
            it may also change the sample rate to the "closest available" sample rate if it
            is within a particular error margin

    rationale for breaking up internalPortAudioStream:
        each implementation has its own requirements and behavior, and should be
        able to choose the best way to operate without being limited by the
        constraints imposed by a common infrastructure. in other words the
        implementations should be able to pick and choose services from the
        common infrastructure. currently identified services include:

        - cpu load tracking
        - buffering and conversion service (same code works for input and output)
            - should support buffer multiplexing (non-integer length input and output buffers)
            - in-place conversion where possible (only for callback, read/write always copies)
            - should manage allocation of temporary buffers if necessary
        - instrumentation (should be able to be disabled): callback count, framesProcessed
        - common data: magic, streamInterface, callback, userdata


- conversion functions: 
	- should handle temp buffer allocation
	- dithering (random number state per-stream)
	- buffer size mismatches
	- with new buffer slip rules, temp buffers may always be needed
	- we should aim for in-place conversion wherever possible
	- does phil's code support in-place conversion?  (yes)              

- dicuss relationship between user and host buffer sizes
	- completely independent.. individual implementations may constrain
    host buffer sizes if necessary


- discuss device capabilities:
	- i'd like to be able to request certain information:
	- channel count for example

