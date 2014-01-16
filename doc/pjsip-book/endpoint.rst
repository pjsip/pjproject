

Endpoint
************
The ​Endpoint class is a singleton class, and application MUST create one and at most one of this class instance before it can do anything else. This class is the core class of PJSUA2, and it provides the following functions:

- Starting up and shutting down
- Customization of configurations, such as core UA (User Agent) SIP configuration, media configuration, and logging configuration

This section will describe the functions above.

To use the Endpoint class, normally application does not need to subclass it unless:

- application wants to implement/override Endpoint’s callback methods to get the events such as transport state change or NAT detection completion, or
- application schedules a timer using Endpoint.utilTimerSchedule() API. In this case, application needs to implement the onTimer() callback to get the notification when the timer expires.

Instantiating the Endpoint
--------------------------
Before anything else, you must instantiate the Endpoint class::

    Endpoint *ep = new Endpoint;

Once the endpoint is instantiated, you can retrieve the Endpoint instance using Endpoint.instance() static method.

Creating the Library
----------------------
Create the library by calling its libCreate() method::

    try {
        ep->libCreate();
    } catch(Error& err) {
        cout << "Startup error: " << err.reason << endl;
    }

The libCreate() method will raise exception if error occurs, so we need to trap the exception using try/catch clause as above.

Initializing the Library and Configuring the Settings
----------------------------------------------------------------------------

The EpConfig class provides endpoint configuration which allows the customization of the following settings:

- UAConfig, to specify core SIP user agent settings.
- MediaConfig, to specify various media settings, including ICE and TURN.
- LogConfig, to customize logging settings.

To customize the settings, create instance of EpConfig class and specify them during the endpoint initialization (will be explained more later), for example::

    EpConfig ep_cfg;
    ep_cfg.logConfig.level = 5;
    ep_cfg.uaConfig.maxCalls = 4;
    ep_cfg.mediaConfig.sndClockRate = 16000;

Next, you can initialize the library by calling libInit()::

    try {
        EpConfig ep_cfg;
        // Specify customization of settings in ep_cfg
        ep->libInit(ep_cfg);
    } catch(Error& err) {
        cout << "Initialization error: " << err.reason << endl;
    }

The snippet above initializes the library with the default settings.

Creating One or More Transports
--------------------------------------------------
Application needs to create one or more ​transports before it can send or receive SIP messages::

    try {
        TransportConfig tcfg;
        tcfg.port = 5060;
        TransportId tid = ep->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    } catch(Error& err) {
        cout << "Transport creation error: " << err.reason << endl;
    }

The transportCreate() method returns the newly created ​Transport ID and it takes the transport type and ​TransportConfig object to customize the transport settings like bound address and listening port number. Without this, by default the transport will be bound to INADDR_ANY and any available port.

There is no real use of the ​Transport ID, except to create userless account (with ​Account.create(), as will be explained later), and perhaps to display the list of transports to user if the application wants it.

Starting the Library
--------------------
Now we're ready to start the library. We need to start the library to finalize the initialization phase, e.g. to complete the initial STUN address resolution, initialize/start the sound device, etc. To start the library, call ​libStart() method::

    try {
        ep->libStart();
    } catch(Error& err) {
        cout << "Startup error: " << err.reason << endl;
    }

Shutting Down the Library
--------------------------------------
Once the application exits, the library needs to be shutdown so that resources can be released back to the operating system. This is done by deleting the Endpoint instance, which will internally call ​libDestroy()::

    delete ep;

