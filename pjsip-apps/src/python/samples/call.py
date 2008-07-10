# $Id:$
#
# SIP call sample.
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
#
import sys
import pjsua as pj

LOG_LEVEL=3
current_call = None

# Logging callback
def log_cb(level, str, len):
    print str,


# Callback to receive events from account
class MyAccountCallback(pj.AccountCallback):

    def __init__(self, account):
        pj.AccountCallback.__init__(self, account)

    # Notification on incoming call
    def on_incoming_call(self, call):
        global current_call 

        if current_call:
            call.answer(486, "Busy")
            return
            
        print "Incoming call from ", call.info().remote_uri
        print "Press 'a' to answer"

        current_call = call

        call_cb = MyCallCallback(current_call)
        current_call.set_callback(call_cb)

        current_call.answer(180)

        
# Callback to receive events from Call
class MyCallCallback(pj.CallCallback):

    def __init__(self, call):
        pj.CallCallback.__init__(self, call)

    # Notification when call state has changed
    def on_state(self):
        global current_call

        print "Call with", self.call.info().remote_uri,
        print "is", self.call.info().state_text,
        print "last code =", self.call.info().last_code, 
        print "(" + self.call.info().last_reason + ")"
        
        if self.call.info().state == pj.CallState.DISCONNECTED:
            current_call = None

    # Notification when call's media state has changed.
    def on_media_state(self):
        if self.call.info().media_state == pj.MediaState.ACTIVE:
            # Connect the call to sound device
            call_slot = self.call.info().conf_slot
            pj.Lib.instance().conf_connect(call_slot, 0)
            pj.Lib.instance().conf_connect(0, call_slot)
            print "Media is now active"
        else:
            print "Media is inactive"

# Function to make call
def make_call(uri):
    try:
        print "Making call to", uri
        call = acc.make_call(uri)
        call_cb = MyCallCallback(call)
        call.set_callback(call_cb)
        return call
    except pj.Error, e:
        print "Error: " + str(e)
        return None
        

# Create library instance
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
    acc_cb = MyAccountCallback(acc)
    acc.set_callback(acc_cb)

    # If argument is specified then make call to the URI
    if len(sys.argv) > 1:
        current_call = make_call(sys.argv[1])

    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)

    # Menu loop
    while True:
        print "My SIP URI is", my_sip_uri
        print "Menu:  m=make call, h=hangup call, a=answer call, q=quit"

        input = sys.stdin.readline().rstrip("\r\n")
        if input == "m":
            if current_call:
                print "Already have another call"
                continue
            print "Enter destination URI to call: ", 
            input = sys.stdin.readline().rstrip("\r\n")
            if input == "":
                continue
            current_call = make_call(input)

        elif input == "h":
            if not current_call:
                print "There is no call"
                continue
            current_call.hangup()

        elif input == "a":
            if not current_call:
                print "There is no call"
                continue
            current_call.answer(200)

        elif input == "q":
            break

    # Shutdown the library
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()
    lib = None

