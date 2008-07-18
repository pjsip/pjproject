# $Id$
#
# Authorization of incoming subscribe request
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
#
import sys
import pjsua as pj

LOG_LEVEL = 3

pending_pres = None

def log_cb(level, str, len):
    print str,

class MyAccountCallback(pj.AccountCallback):
    def __init__(self, account):
        pj.AccountCallback.__init__(self, account)

    def on_incoming_subscribe(self, buddy, from_uri, pres):
        # Allow buddy to subscribe to our presence
        global pending_pres

        if buddy:
            return (200, None)
        print 'Incoming SUBSCRIBE request from', from_uri
        print 'Press "A" to accept and add, "R" to reject the request'
        pending_pres = pres
        return (202, None)


lib = pj.Lib()

try:
    # Init library with default config and some customized
    # logging config.
    lib.init(log_cfg = pj.LogConfig(level=LOG_LEVEL, callback=log_cb))

    # Create UDP transport which listens to any available port
    transport = lib.create_transport(pj.TransportType.UDP, 
                                     pj.TransportConfig(0))
    print "\nListening on", transport.info().host, 
    print "port", transport.info().port, "\n"
    
    # Start the library
    lib.start()

    # Create local account
    acc = lib.create_account_for_transport(transport)
    acc.set_callback(MyAccountCallback(acc))

    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)

    buddy = None

    # Menu loop
    while True:
        print "My SIP URI is", my_sip_uri
        print "Menu:  t=toggle online status, q=quit"

        input = sys.stdin.readline().rstrip("\r\n")

        if input == "t":
            acc.set_basic_status(not acc.info().online_status)

        elif input == "A":
            if pending_pres:
                acc.pres_notify(pending_pres, pj.SubscriptionState.ACTIVE)
                pending_pres = None
            else:
                print "No pending request"

        elif input == "R":
            if pending_pres:
                acc.pres_notify(pending_pres, pj.SubscriptionState.TERMINATED,
                                "rejected")
                pending_pres = None
            else:
                print "No pending request"
        
        elif input == "q":
            break

    # Shutdown the library
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()
    lib = None

