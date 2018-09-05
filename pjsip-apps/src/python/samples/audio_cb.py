# $Id: audio_cb.py 2171 2008-07-24 09:01:33Z bennylp $
#
# SIP account and registration sample. In this sample, the program
# will block to wait until registration is complete
#
# Copyright (C) 2003-2008 Benny Prijono <benny at prijono.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
#
import sys
import pjsua as pj
import threading
from collections import deque


def log_cb(level, str, len):
    print str,


class AudioCB:
    frames = deque()

    def cb_put_frame(self, frame):
        # An audio frame arrived, it is a string (i.e. ByteArray)
        self.frames.append(frame)
        # Return an integer; 0 means success, but this does not matter now
        return 0

    def cb_get_frame(self, size):
        # Audio frame wanted
        if len(self.frames):
            frame = self.frames.popleft()
            # Send the frame out
            return frame
        else:
            # Do not emit an audio frame
            return None


class MyCallCallback(pj.CallCallback):

    def __init__(self, call=None):
        pj.CallCallback.__init__(self, call)

    def on_state(self):
        if self.call.info().state == pj.CallState.DISCONNECTED:
            global g_current_call
            g_current_call = None
            print "Call hung up"

    def on_media_state(self):
        info = self.call.info()
        call_slot = info.conf_slot
        if (info.media_state == pj.MediaState.ACTIVE) and (call_slot >= 0):
            print "Call slot:", call_slot
            global g_acb_id
            acb_slot = lib.audio_cb_get_slot(g_acb_id)
            print "Audio callback ", g_acb_id, "slot:", acb_slot
            print "Starting loopback via python audio callback"
            lib.conf_connect(call_slot, acb_slot)
            lib.conf_connect(acb_slot, call_slot)


class MyAccountCallback(pj.AccountCallback):

    def __init__(self, account=None):
        pj.AccountCallback.__init__(self, account)

    # Notification on incoming call
    def on_incoming_call(self, call):
        global g_current_call 
        if g_current_call:
            call.answer(486, "Busy")
            return

        call.set_callback(MyCallCallback(call))
        info = call.info()
        print "Incoming call from", info.remote_uri
        call.answer()
        g_current_call = call


lib = pj.Lib()

try:
    lib.init(log_cfg = pj.LogConfig(level=4, callback=log_cb))

    # This is a MUST if not using a HW sound
    lib.set_null_snd_dev()

    # Create UDP transport which listens to any available port
    transport = lib.create_transport(pj.TransportType.UDP, 
                                     pj.TransportConfig(0))
    print "\nListening on", transport.info().host, 
    print "port", transport.info().port, "\n"

    lib.start(True)

    # Create local account
    acc = lib.create_account_for_transport(transport, cb=MyAccountCallback())

    g_current_call = None
    g_acb_id = lib.create_audio_cb(AudioCB())
    print "Audio callback ID:", g_acb_id

    print "\nWaiting for incoming call"
    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)
    print "My SIP URI is", my_sip_uri
    print "\nPress ENTER to quit"
    sys.stdin.readline()

    # Shutdown the library
    lib.audio_cb_destroy(g_acb_id)
    transport = None
    acc.delete()
    acc = None
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()

