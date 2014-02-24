
Buddy (Presence)
================
Presence feature in PJSUA2 centers around Buddy class. This class represents a remote buddy (a person, or a SIP endpoint).

Subclassing the Buddy class
----------------------------
To use the Buddy class, normally application SHOULD create its own subclass, such as:

.. code-block:: c++

    class MyBuddy : public Buddy
    {
    public:
        MyBuddy() {}
        ~MyBuddy() {}

        virtual void onBuddyState();
    };

In its subclass, application can implement the buddy callback to get the notifications on buddy state change.

Subscribing to Buddy's Presence Status
---------------------------------------
To subscribe to buddy's presence status, you need to add a buddy object and subscribe to buddy's presence status. The snippet below shows a sample code to achieve these:

.. code-block:: c++

    BuddyConfig cfg;
    cfg.uri = "sip:alice@example.com";
    MyBuddy buddy;
    try {
        buddy.create(*acc, cfg);
        buddy.subscribePresence(true);
    } catch(Error& err) {
    }

Then you can get the buddy's presence state change inside the onBuddyState() callback:

.. code-block:: c++

    void MyBuddy::onBuddyState()
    {
        BuddyInfo bi = getInfo();
        cout << "Buddy " << bi.uri << " is " << bi.presStatus.statusText << endl;
    }

For more information, please see Buddy class reference documentation.

Responding to Presence Subscription Request
-------------------------------------------
By default, incoming presence subscription to an account will be accepted automatically. You will probably want to change this behavior, for example only to automatically accept subscription if it comes from one of the buddy in the buddy list, and for anything else prompt the user if he/she wants to accept the request.

This can be done by overriding the onIncomingSubscribe() method of the Account class. Please see the documentation of this method for more info.

Changing Account's Presence Status
----------------------------------
To change account's presence status, you can use the function Account.setOnlineStatus() to set basic account's presence status (i.e. available or not available) and optionally, some extended information (e.g. busy, away, on the phone, etc), such as:

.. code-block:: c++

    try {
        PresenceStatus ps;
        ps.status = PJSUA_BUDDY_STATUS_ONLINE;
        // Optional, set the activity and some note
        ps.activity = PJRPID_ACTIVITY_BUSY;
        ps.note = "On the phone";
        acc->setOnlineStatus(ps);
    } catch(Error& err) {
    }

When the presence status is changed, the account will publish the new status to all of its presence subscriber, either with PUBLISH request or NOTIFY request, or both, depending on account configuration.

Instant Messaging(IM)
---------------------
You can send IM using Buddy.sendInstantMessage(). The transmission status of outgoing instant messages is reported in Account.onInstantMessageStatus() callback method of Account class.

In addition to sending instant messages, you can also send typing indication to remote buddy using Buddy.sendTypingIndication().

Incoming IM and typing indication received not within the scope of a call will be reported in the callback functions Account.onInstantMessage() and Account.onTypingIndication().

Alternatively, you can send IM and typing indication within a call by using Call.sendInstantMessage() and Call.sendTypingIndication(). For more information, please see Call documentation.


Class Reference
---------------
Buddy
+++++
.. doxygenclass:: pj::Buddy
        :path: xml
        :members:

Status
++++++
.. doxygenstruct:: pj::PresenceStatus
        :path: xml
        
Info
++++
.. doxygenstruct:: pj::BuddyInfo
        :path: xml

Config
++++++
.. doxygenstruct:: pj::BuddyConfig
        :path: xml


