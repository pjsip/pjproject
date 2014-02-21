# $Id$
#
# pjsua Python GUI Demo
#
# Copyright (C)2013 Teluu Inc. (http://www.teluu.com)
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
if sys.version_info[0] >= 3: # Python 3
	import tkinter as tk
	from tkinter import ttk
	from tkinter import messagebox as msgbox
else:
	import Tkinter as tk
	import tkMessageBox as msgbox
	import ttk

import random
import pjsua2 as pj
import application
import endpoint as ep

# Call class
class Call(pj.Call):
	"""
	High level Python Call object, derived from pjsua2's Call object.
	"""
	def __init__(self, acc, peer_uri='', chat=None, call_id = pj.PJSUA_INVALID_ID):
		pj.Call.__init__(self, acc, call_id)
                self.acc = acc
		self.peerUri = peer_uri
		self.chat = chat
		self.connected = False
		self.onhold = False

	def onCallState(self, prm):
		ci = self.getInfo()
		self.connected = ci.state == pj.PJSIP_INV_STATE_CONFIRMED			
		if self.chat:
			self.chat.updateCallState(self, ci)
			
	def onCallMediaState(self, prm):
		ci = self.getInfo()
		for mi in ci.media:
			if mi.type == pj.PJMEDIA_TYPE_AUDIO and \
			  (mi.status == pj.PJSUA_CALL_MEDIA_ACTIVE or \
			   mi.status == pj.PJSUA_CALL_MEDIA_REMOTE_HOLD):
				m = self.getMedia(mi.index)
				am = pj.AudioMedia.typecastFromMedia(m)
				# connect ports
				ep.Endpoint.instance.audDevManager().getCaptureDevMedia().startTransmit(am)
				am.startTransmit(ep.Endpoint.instance.audDevManager().getPlaybackDevMedia())

				if mi.status == pj.PJSUA_CALL_MEDIA_REMOTE_HOLD and not self.onhold:
					self.chat.addMessage(None, "'%s' sets call onhold" % (self.peerUri))
					self.onhold = True
				elif mi.status == pj.PJSUA_CALL_MEDIA_ACTIVE and self.onhold:
					self.chat.addMessage(None, "'%s' sets call active" % (self.peerUri))
					self.onhold = False
		if self.chat:
			self.chat.updateCallMediaState(self, ci)
			
	def onInstantMessage(self, prm):
		# chat instance should have been initalized
		if not self.chat: return
			
		self.chat.addMessage(self.peerUri, prm.msgBody)
		self.chat.showWindow()
			
	def onInstantMessageStatus(self, prm):
		if prm.code/100 == 2: return
		# chat instance should have been initalized
		if not self.chat: return
		
		self.chat.addMessage(None, "Failed sending message to '%s' (%d): %s" % (self.peerUri, prm.code, prm.reason))
		
	def onTypingIndication(self, prm):
		# chat instance should have been initalized
		if not self.chat: return
		
		self.chat.setTypingIndication(self.peerUri, prm.isTyping)
	
	def onDtmfDigit(self, prm):
		#msgbox.showinfo("pygui", 'Got DTMF:' + prm.digit)
		pass
		
	def onCallMediaTransportState(self, prm):
		#msgbox.showinfo("pygui", "Media transport state")
		pass
		
		
if __name__ == '__main__':
	application.main()
