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
else:
	import Tkinter as tk
	import ttk

import buddy
import call
import chatgui as gui
import endpoint as ep
import pjsua2 as pj
import re

SipUriRegex = re.compile('(sip|sips):([^:;>\@]*)@?([^:;>]*):?([^:;>]*)')
ConfIdx = 1

# Simple SIP uri parser, input URI must have been validated
def ParseSipUri(sip_uri_str):
	m = SipUriRegex.search(sip_uri_str)
	if not m:
		assert(0)
		return None
	
	scheme = m.group(1)
	user = m.group(2)
	host = m.group(3)
	port = m.group(4)
	if host == '':
		host = user
		user = ''
		
	return SipUri(scheme.lower(), user, host.lower(), port)
	
class SipUri:
	def __init__(self, scheme, user, host, port):
		self.scheme = scheme
		self.user = user
		self.host = host
		self.port = port
		
	def __cmp__(self, sip_uri):
		if self.scheme == sip_uri.scheme and self.user == sip_uri.user and self.host == sip_uri.host:
			# don't check port, at least for now
			return 0
		return -1
	
	def __str__(self):
		s = self.scheme + ':'
		if self.user: s += self.user + '@'
		s += self.host
		if self.port: s+= ':' + self.port
		return s
	
class Chat(gui.ChatObserver):
	def __init__(self, app, acc, uri, call_inst=None):
		self._app = app
		self._acc = acc
		self.title = ''
		
		global ConfIdx
		self.confIdx = ConfIdx
		ConfIdx += 1
		
		# each participant call/buddy instances are stored in call list
		# and buddy list with same index as in particpant list
		self._participantList = []	# list of SipUri
		self._callList = []		# list of Call
		self._buddyList = []		# list of Buddy
		
		self._gui = gui.ChatFrame(self)
		self.addParticipant(uri, call_inst)
	
	def _updateGui(self):
		if self.isPrivate():
			self.title = str(self._participantList[0])
		else:
			self.title = 'Conference #%d (%d participants)' % (self.confIdx, len(self._participantList))
		self._gui.title(self.title)
		self._app.updateWindowMenu()
		
	def _getCallFromUriStr(self, uri_str, op = ''):
		uri = ParseSipUri(uri_str)
		if uri not in self._participantList:
			print "=== %s cannot find participant with URI '%s'" % (op, uri_str)
			return None
		idx = self._participantList.index(uri)
		if idx < len(self._callList):
			return self._callList[idx]
		return None
	
	def _getActiveMediaIdx(self, thecall):
		ci = thecall.getInfo()
		for mi in ci.media:
			if mi.type == pj.PJMEDIA_TYPE_AUDIO and \
			  (mi.status != pj.PJSUA_CALL_MEDIA_NONE and \
			   mi.status != pj.PJSUA_CALL_MEDIA_ERROR):
				return mi.index
		return -1
		
	def _getAudioMediaFromUriStr(self, uri_str):
		c = self._getCallFromUriStr(uri_str)
		if not c: return None

		idx = self._getActiveMediaIdx(c)
		if idx < 0: return None

		m = c.getMedia(idx)
		am = pj.AudioMedia.typecastFromMedia(m)
		return am
		
	def _sendTypingIndication(self, is_typing, sender_uri_str=''):
		sender_uri = ParseSipUri(sender_uri_str) if sender_uri_str else None
		type_ind_param = pj.SendTypingIndicationParam()
		type_ind_param.isTyping = is_typing
		for idx, p in enumerate(self._participantList):
			# don't echo back to the original sender
			if sender_uri and p == sender_uri:
				continue
				
			# send via call, if any, or buddy
			sender = None
			if self._callList[idx] and self._callList[idx].connected:
				sender = self._callList[idx]
			else:
				sender = self._buddyList[idx]
			assert(sender)
				
			try:
				sender.sendTypingIndication(type_ind_param)
			except:
				pass

	def _sendInstantMessage(self, msg, sender_uri_str=''):
		sender_uri = ParseSipUri(sender_uri_str) if sender_uri_str else None
		send_im_param = pj.SendInstantMessageParam()
		send_im_param.content = str(msg)
		for idx, p in enumerate(self._participantList):
			# don't echo back to the original sender
			if sender_uri and p == sender_uri:
				continue
				
			# send via call, if any, or buddy
			sender = None
			if self._callList[idx] and self._callList[idx].connected:
				sender = self._callList[idx]
			else:
				sender = self._buddyList[idx]
			assert(sender)
			
			try:
				sender.sendInstantMessage(send_im_param)
			except:
				# error will be handled via Account::onInstantMessageStatus()
				pass

	def isPrivate(self):
		return len(self._participantList) <= 1
		
	def isUriParticipant(self, uri):
		return uri in self._participantList
		
	def registerCall(self, uri_str, call_inst):
		uri = ParseSipUri(uri_str)
		try:
			idx = self._participantList.index(uri)
			bud = self._buddyList[idx]
			self._callList[idx] = call_inst
			call_inst.chat = self
			call_inst.peerUri = bud.cfg.uri
		except:
			assert(0) # idx must be found!
		
	def showWindow(self, show_text_chat = False):
		self._gui.bringToFront()
		if show_text_chat:
			self._gui.textShowHide(True)
		
	def addParticipant(self, uri, call_inst=None):
		# avoid duplication
		if self.isUriParticipant(uri): return
		
		uri_str = str(uri)
		
		# find buddy, create one if not found (e.g: for IM/typing ind),
		# it is a temporary one and not really registered to acc
		bud = None
		try:
			bud = self._acc.findBuddy(uri_str)
		except:
			bud = buddy.Buddy(None)
			bud_cfg = pj.BuddyConfig()
			bud_cfg.uri = uri_str
			bud_cfg.subscribe = False
			bud.create(self._acc, bud_cfg)
			bud.cfg = bud_cfg
			bud.account = self._acc
			
		# update URI from buddy URI
		uri = ParseSipUri(bud.cfg.uri)
		
		# add it
		self._participantList.append(uri)
		self._callList.append(call_inst)
		self._buddyList.append(bud)
		self._gui.addParticipant(str(uri))
		self._updateGui()
	
	def kickParticipant(self, uri):
		if (not uri) or (uri not in self._participantList):
			assert(0)
			return
		
		idx = self._participantList.index(uri)
		del self._participantList[idx]
		del self._callList[idx]
		del self._buddyList[idx]
		self._gui.delParticipant(str(uri))
		
		if self._participantList:
			self._updateGui()
		else:
			self.onCloseWindow()
			
	def addMessage(self, from_uri_str, msg):
		if from_uri_str:
			# print message on GUI
			msg = from_uri_str + ': ' + msg
			self._gui.textAddMessage(msg)
			# now relay to all participants
			self._sendInstantMessage(msg, from_uri_str)
		else:
			self._gui.textAddMessage(msg, False)
			
	def setTypingIndication(self, from_uri_str, is_typing):
		# notify GUI
		self._gui.textSetTypingIndication(from_uri_str, is_typing)
		# now relay to all participants
		self._sendTypingIndication(is_typing, from_uri_str)
		
	def startCall(self):
		self._gui.enableAudio()
		call_param = pj.CallOpParam()
		call_param.opt.audioCount = 1
		call_param.opt.videoCount = 0
		fails = []
		for idx, p in enumerate(self._participantList):
			# just skip if call is instantiated
			if self._callList[idx]:
				continue
			
			uri_str = str(p)
			c = call.Call(self._acc, uri_str, self)
			self._callList[idx] = c
			self._gui.audioUpdateState(uri_str, gui.AudioState.INITIALIZING)
			
			try:
				c.makeCall(uri_str, call_param)
			except:
				self._callList[idx] = None
				self._gui.audioUpdateState(uri_str, gui.AudioState.FAILED)
				fails.append(p)
				
		for p in fails:
			# kick participants with call failure, but spare the last (avoid zombie chat)
			if not self.isPrivate():
				self.kickParticipant(p)
			
	def stopCall(self):
		for idx, p in enumerate(self._participantList):
			self._gui.audioUpdateState(str(p), gui.AudioState.DISCONNECTED)
			c = self._callList[idx]
			if c:
				c.hangup(pj.CallOpParam())

	def updateCallState(self, thecall, info = None):
		# info is optional here, just to avoid calling getInfo() twice (in the caller and here)
		if not info: info = thecall.getInfo()
		
		if info.state < pj.PJSIP_INV_STATE_CONFIRMED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.INITIALIZING)
		elif info.state == pj.PJSIP_INV_STATE_CONFIRMED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.CONNECTED)
			med_idx = self._getActiveMediaIdx(thecall)
			si = thecall.getStreamInfo(med_idx)
			stats_str = "Audio codec: %s/%s\n..." % (si.codecName, si.codecClockRate)
			self._gui.audioSetStatsText(thecall.peerUri, stats_str)
		elif info.state == pj.PJSIP_INV_STATE_DISCONNECTED:
			if info.lastStatusCode/100 != 2:
				self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.FAILED)
			else:
				self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.DISCONNECTED)
			
			# reset entry in the callList
			try:
				idx = self._callList.index(thecall)
				if idx >= 0: self._callList[idx] = None
			except:
				pass
			
			self.addMessage(None, "Call to '%s' disconnected: %s" % (thecall.peerUri, info.lastReason))
			
			# kick the disconnected participant, but the last (avoid zombie chat)
			if not self.isPrivate():
				self.kickParticipant(ParseSipUri(thecall.peerUri))

			
	# ** callbacks from GUI (ChatObserver implementation) **
	
	# Text
	def onSendMessage(self, msg):
		self._sendInstantMessage(msg)

	def onStartTyping(self):
		self._sendTypingIndication(True)
		
	def onStopTyping(self):
		self._sendTypingIndication(False)
		
	# Audio
	def onHangup(self, peer_uri_str):
		c = self._getCallFromUriStr(peer_uri_str, "onHangup()")
		if not c: return
		call_param = pj.CallOpParam()
		c.hangup(call_param)

	def onHold(self, peer_uri_str):
		c = self._getCallFromUriStr(peer_uri_str, "onHold()")
		if not c: return
		call_param = pj.CallOpParam()
		c.setHold(call_param)

	def onUnhold(self, peer_uri_str):
		c = self._getCallFromUriStr(peer_uri_str, "onUnhold()")
		if not c: return
		
		call_param = pj.CallOpParam()
		call_param.opt.audioCount = 1
		call_param.opt.videoCount = 0
		call_param.opt.flag |= pj.PJSUA_CALL_UNHOLD
		c.reinvite(call_param)
		
	def onRxMute(self, peer_uri_str, mute):
		am = self._getAudioMediaFromUriStr(peer_uri_str)
		if not am: return
		if mute:
			am.stopTransmit(ep.Endpoint.instance.audDevManager().getPlaybackDevMedia())
			self.addMessage(None, "Muted audio from '%s'" % (peer_uri_str))
		else:
			am.startTransmit(ep.Endpoint.instance.audDevManager().getPlaybackDevMedia())
			self.addMessage(None, "Unmuted audio from '%s'" % (peer_uri_str))
		
	def onRxVol(self, peer_uri_str, vol_pct):
		am = self._getAudioMediaFromUriStr(peer_uri_str)
		if not am: return
		# pjsua volume range = 0:mute, 1:no adjustment, 2:100% louder
		am.adjustRxLevel(vol_pct/50.0)
		self.addMessage(None, "Adjusted volume level audio from '%s'" % (peer_uri_str))
			
	def onTxMute(self, peer_uri_str, mute):
		am = self._getAudioMediaFromUriStr(peer_uri_str)
		if not am: return
		if mute:
			ep.Endpoint.instance.audDevManager().getCaptureDevMedia().stopTransmit(am)
			self.addMessage(None, "Muted audio to '%s'" % (peer_uri_str))
		else:
			ep.Endpoint.instance.audDevManager().getCaptureDevMedia().startTransmit(am)
			self.addMessage(None, "Unmuted audio to '%s'" % (peer_uri_str))

	# Chat room
	def onAddParticipant(self):
		buds = []
		dlg = AddParticipantDlg(None, self._app, buds)
		if dlg.doModal():
			for bud in buds:
				uri = ParseSipUri(bud.cfg.uri)
				self.addParticipant(uri)
			if not self.isPrivate():
				self.startCall()
				
	def onStartAudio(self):
		self.startCall()

	def onStopAudio(self):
		self.stopCall()
		
	def onCloseWindow(self):
		self.stopCall()
		# will remove entry from list eventually destroy this chat?
		if self in self._acc.chatList: self._acc.chatList.remove(self)
		self._app.updateWindowMenu()
		# destroy GUI
		self._gui.destroy()


class AddParticipantDlg(tk.Toplevel):
	"""
	List of buddies
	"""
	def __init__(self, parent, app, bud_list):
		tk.Toplevel.__init__(self, parent)
		self.title('Add participants..')
		self.transient(parent)
		self.parent = parent
		self._app = app
		self.buddyList = bud_list
		
		self.isOk = False
		
		self.createWidgets()
	
	def doModal(self):
		if self.parent:
			self.parent.wait_window(self)
		else:
			self.wait_window(self)
		return self.isOk
		
	def createWidgets(self):
		# buddy list
		list_frame = ttk.Frame(self)
		list_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=1, padx=20, pady=20)
		#scrl = ttk.Scrollbar(self, orient=tk.VERTICAL, command=list_frame.yview)
		#list_frame.config(yscrollcommand=scrl.set)
		#scrl.pack(side=tk.RIGHT, fill=tk.Y)
		
		# draw buddy list
		self.buddies = []
		for acc in self._app.accList:
			self.buddies.append((0, acc.cfg.idUri))
			for bud in acc.buddyList:
				self.buddies.append((1, bud))
		
		self.bud_var = []
		for idx,(flag,bud) in enumerate(self.buddies):
			self.bud_var.append(tk.IntVar())
			if flag==0:
				s = ttk.Separator(list_frame, orient=tk.HORIZONTAL)
				s.pack(fill=tk.X)
				l = tk.Label(list_frame, anchor=tk.W, text="Account '%s':" % (bud))
				l.pack(fill=tk.X)
			else:
				c = tk.Checkbutton(list_frame, anchor=tk.W, text=bud.cfg.uri, variable=self.bud_var[idx])
				c.pack(fill=tk.X)
		s = ttk.Separator(list_frame, orient=tk.HORIZONTAL)
		s.pack(fill=tk.X)

		# Ok/cancel buttons
		tail_frame = ttk.Frame(self)
		tail_frame.pack(side=tk.BOTTOM, fill=tk.BOTH, expand=1)
		
		btnOk = ttk.Button(tail_frame, text='Ok', default=tk.ACTIVE, command=self.onOk)
		btnOk.pack(side=tk.LEFT, padx=20, pady=10)
		btnCancel = ttk.Button(tail_frame, text='Cancel', command=self.onCancel)
		btnCancel.pack(side=tk.RIGHT, padx=20, pady=10)
		
	def onOk(self):
		self.buddyList[:] = []
		for idx,(flag,bud) in enumerate(self.buddies):
			if not flag: continue
			if self.bud_var[idx].get() and not (bud in self.buddyList):
				self.buddyList.append(bud)
			
		self.isOk = True
		self.destroy()
		
	def onCancel(self):
		self.destroy()
