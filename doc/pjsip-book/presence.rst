

Buddy (Presence)
================
This class represents a remote buddy (a person, or a SIP endpoint).
To use the Buddy class, application DOES NOT need to subclass it unless application wants to get the notifications on buddy state change.

Subscribe to Buddy's Presence Status
---------------------------------------------------------
To subscribe to buddy's presence status, you need to add a buddy object, install callback to handle buddy's event, and start subscribing to buddy's presence status. The snippet below shows a sample code to achieve these::

  class MyBuddyCallback(pjsua.BuddyCallback):
    def __init__(self, buddy=None):
        pjsua.BuddyCallback.__init__(self, buddy)

    def on_state(self):
        print "Buddy", self.buddy.info().uri, "is",
        print self.buddy.info().online_text

  try:
    uri = '"Alice" <sip:alice@example.com>'
    buddy = acc.add_buddy(uri, cb=MyBuddyCallback())
    buddy.subscribe()

  except pjsua.Error, err:
    print 'Error adding buddy:', err

For more information please see ​Buddy class and ​BuddyCallback class reference documentation.

Responding to Presence Subscription Request

By default, incoming presence subscription to an account will be accepted automatically. You will probably want to change this behavior, for example only to automatically accept subscription if it comes from one of the buddy in the buddy list, and for anything else prompt the user if he/she wants to accept the request.

This can be done by implementing the ​on_incoming_subscribe() method of the ​AccountCallback class.

Changing Account's Presence Status

The ​Account class provides two methods to change account's presence status:

​set_basic_status() can be used to set basic account's presence status (i.e. available or not available).
​set_presence_status() can be used to set both the basic presence status and some extended information (e.g. busy, away, on the phone, etc.).
When the presence status is changed, the account will publish the new status to all of its presence subscriber, either with PUBLISH request or SUBSCRIBE request, or both, depending on account configuration.

