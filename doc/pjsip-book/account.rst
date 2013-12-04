

Accounts
=========
​Accounts provide identity (or identities) of the user who is currently using the application. An account has one SIP Uniform Resource Identifier (URI) associated with it. In SIP terms, the identity is used as the From header in outgoing requests.

Account may or may not have client registration associated with it. An account is also associated with route set and some authentication credentials, which are used when sending SIP request messages using the account. An account also has presence online status, which will be reported to remote peer when they subscribe to the account's presence, or which is published to a presence server if presence publication is enabled for the account.

At least one account MUST be created in the application, since any outgoing requests require an account context. If no user association is required, application can create a userless account by calling Account.create(). A userless account identifies local endpoint instead of a particular user, and it corresponds to a particular transport ID.

Also one account must be set as the default account, which will be used as the account identity when pjsua fails to match the request with any accounts using the stricter matching rules.

Subclassing the Account class
---------------------------------
To use the Account class, normally application SHOULD create its own subclass, such as::

    class MyAccount : public Account
    {
    public:
        MyAccount() {}
        ~MyAccount() {}

        virtual void onRegState(OnRegStateParam &prm)
        {
            AccountInfo ai = getInfo();
            cout << (ai.regIsActive? "*** Register: code=" : "*** Unregister: code=")
                 << prm.code << endl;

        }
    
        virtual void onIncomingCall(OnIncomingCallParam &iprm)
        {
            Call *call = new MyCall(*this, iprm.callId);
            // Delete the call, which will also hangup the call
            delete call;
        }
    };

In its subclass, application can implement the account callbacks, which is basically used to process events related to the account, such as:

- the status of SIP registration
- incoming calls
- incoming presence subscription requests
- incoming instant message not from buddy

Application needs to override the relevant callback methods in the derived class to handle these particular events.

If the events are not handled, default actions will be invoked:

- incoming calls will not be handled
- incoming presence subscription requests will be accepted
- incoming instant messages from non-buddy will be ignored

Creating Userless Accounts
--------------------------
A userless account identifies a particular SIP endpoint rather than a particular user. Some other SIP softphones may call this peer-to-peer mode, which means that we are calling another computer via its address rather than calling a particular user ID.

So for example, we might identify ourselves as "sip:192.168.0.15" (a userless account) rather than, say, "sip:bennylp@pjsip.org".

In pjsua, a userless account corresponds to a particular transport. Creating userless account is very simple, all we need is the transport ID which is returned by ​Endpoint.transportCreate() method as explained in previous chapter.

Here's a snippet::

    AccountConfig acc_cfg;
    acc_cfg.sipConfig.transportId = tid;
    MyAccount *acc = new MyAccount;
    try {
        acc->create(acc_cfg);
    } catch(Error& err) {
        cout << "Account creation error: " << err.reason << endl;
    }

Once the account is created, you can use the instance as a normal account. More will be explained later.

Accounts created this way will have its URI derived from the transport address. For example, if the transport address is "192.168.0.15:5080", then the account's URI for UDP transport will be "sip:192.168.0.15:5080", or "sip:192.168.0.15:5080;transport=tcp" for TCP transport.

Creating Account
----------------
For the "normal" account, we need to configure ​AccountConfig and call ​Account.create() to create the account.

At the very minimum, pjsua only requires the account's ID, which is an URI to identify the account (or in SIP terms, it's called Address of Record/AOR). Here's a snippet::

    AccountConfig acc_cfg;
    acc_cfg.idUri = "sip:test1@pjsip.org";
    MyAccount *acc = new MyAccount;
    try {
        acc->create(acc_cfg);
    } catch(Error& err) {
        cout << "Account creation error: " << err.reason << endl;
    }

The account created above doesn't do anything except to provide identity in the "From:" header for outgoing requests. The account will not register to SIP server or anything.

Typically you will want the account to authenticate and register to your SIP server so that you can receive incoming calls. To do that you will need to configure some more settings in your ​AccountConfig, something like this::

    AccountConfig acc_cfg;
    acc_cfg.idUri = "sip:test1@pjsip.org";
    acc_cfg.regConfig.registrarUri = "sip:pjsip.org";
    acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", "*", "test1", 0, "test1") );
    MyAccount *acc = new MyAccount;
    try {
        acc->create(acc_cfg);
    } catch(Error& err) {
        cout << "Account creation error: " << err.reason << endl;
    }

Account Configurations
-----------------------
There are many more settings that can be specified in ​AccountConfig, like:

- AccountRegConfig, to specify registration settings, such as registrar server and retry interval.
- AccountSipConfig, to specify SIP settings, such as credential information and proxy server.
- AccountCallConfig, to specify call settings, such as whether reliable provisional response (SIP 100rel) is required.
- AccountPresConfig, to specify presence settings, such as whether presence publication (PUBLISH) is enabled.
- AccountMwiConfig, to specify MWI (Message Waiting Indication) settings.
- AccountNatConfig, to specify NAT settings, such as whether STUN or ICE is used.
- AccountMediaConfig, to specify media settings, such as Secure RTP (SRTP) related settings.
- AccountVideoConfig, to specify video settings, such as default capture and render device.

Please see ​AccountConfig reference documentation for more info.

Account Operations
--------------------------------------
Some of the operations to the ​Account object:

- add buddy objects
- set account's presence online status
- stop/start SIP registration

Please see the reference documentation for Account for more info. Calls, presence, and buddy list will be explained in later sections.


