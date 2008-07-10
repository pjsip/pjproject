# $Id:$
#
# Presence and instant messaging
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
#
import sys
import pjsua as pj
import threading

LOG_LEVEL = 3

def log_cb(level, str, len):
    print str,

class MyBuddyCallback(pj.BuddyCallback):
    def __init__(self, buddy):
        pj.BuddyCallback.__init__(self, buddy)

    def on_state(self):
        print "Buddy", self.buddy.info().uri, "is",
        print self.buddy.info().online_text

    def on_pager(self, mime_type, body):
        print "Instant message from", self.buddy.info().uri, 
        print "(", mime_type, "):"
        print body

    def on_pager_status(self, body, im_id, code, reason):
        if code >= 300:
            print "Message delivery failed for message",
            print body, "to", self.buddy.info().uri, ":", reason

    def on_typing(self, is_typing):
        if is_typing:
            print self.buddy.info().uri, "is typing"
        else:
            print self.buddy.info().uri, "stops typing"


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

    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)

    buddy = None

    # Menu loop
    while True:
        print "My SIP URI is", my_sip_uri
        print "Menu:  a=add buddy, t=toggle online status, i=send IM, q=quit"

        input = sys.stdin.readline().rstrip("\r\n")
        if input == "a":
            # Add buddy
            print "Enter buddy URI: ", 
            input = sys.stdin.readline().rstrip("\r\n")
            if input == "":
                continue

            buddy = acc.add_buddy(input)
            cb = MyBuddyCallback(buddy)
            buddy.set_callback(cb)

            buddy.subscribe()

        elif input == "t":
            acc.set_basic_status(not acc.info().online_status)

        elif input == "i":
            if not buddy:
                print "Add buddy first"
                continue

            buddy.send_typing_ind(True)

            print "Type the message: ", 
            input = sys.stdin.readline().rstrip("\r\n")
            if input == "":
                buddy.send_typing_ind(False)
                continue
            
            buddy.send_pager(input)
            
        elif input == "q":
            break

    # Shutdown the library
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()
    lib = None

