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

import call
import chatgui as gui
import pjsua2 as pj
	
class Chat(gui.ChatObserver):
	def __init__(self, acc, bud, call_inst=None):
		self._acc = acc
		self._participantList = []
		self._callList = []
		self._gui = gui.ChatFrame(self)
		self.addParticipant(bud, call_inst)
	
	def _updateGui(self):
		if self.isPrivate():
			bud = self._participantList[0]
			self._gui.title(bud.cfg.uri)
		else:
			self._gui.title('Conference (%d participants)' % (len(self._participantList)))

	def _getCallFromBuddy(self, bud):
		try:
			idx = self._participantList.index(bud)
			the_call = self._callList[idx]
		except:
			return None
		return the_call
		
	def _sendTypingIndication(self, is_typing):
		type_ind_param = pj.SendTypingIndicationParam()
		type_ind_param.isTyping = is_typing
		for bud in self._participantList:
			c = self._getCallFromBuddy(bud)
			try:
				if c:
					c.sendTypingIndication(type_ind_param)
				else:
					bud.sendTypingIndication(type_ind_param)
			except:
				pass

	def isPrivate(self):
		return len(self._participantList) <= 1
		
	def isBuddyParticipant(self, bud, call_inst):
		return bud in self._participantList
		
	def isCallRegistered(self, call_inst):
		return call_inst in self._callList
		
	def registerCall(self, bud, call_inst):
		try:
			idx = self._participantList.index(bud)
			print "registering call...", idx, self._participantList
			if len(self._callList) < idx+1:
				self._callList.append(call_inst)
			else:
				self._callList[idx] = call_inst
		except:
			return None
		
	def showWindow(self):
		self._gui.bringToFront()

	def addParticipant(self, bud, call_inst=None):
		self._participantList.append(bud)
		if call_inst:
			self._callList.append(call_inst)
		self._gui.addParticipant(bud.cfg.uri)

		self._updateGui()
		if not self.isPrivate():
			self.startCall()
			self._gui.enableAudio()
	
	def kickParticipant(bud):
		if bud in self._participantList:
			idx = self._participantList.index(bud)
			self._participantList.remove(bud)
			self._gui.delParticipant(bud.cfg.uri)
			
			# also clear call, if any
			if self._callList: del self._callList[idx]
			
		if self._participantList:
			self._updateGui()
		else:
			# will this eventually destroy itself?
			self._acc.chatList.remove(self)
			
	def addMessage(self, from_uri, msg):
		if from_uri:
			msg = from_uri + ': ' + msg
			self._gui.textAddMessage(msg)
		else:
			self._gui.textAddMessage(msg, False)
			
	def setTypingIndication(self, from_uri, is_typing):
		self._gui.textSetTypingIndication(from_uri, is_typing)
		
	def startCall(self):
		call_param = pj.CallOpParam()
		call_param.opt.audioCount = 1
		call_param.opt.videoCount = 0
		for idx, bud in enumerate(self._participantList):
			# just skip if call is instantiated
			if len(self._callList)>=idx+1 and self._callList[idx]:
				continue
				
			c = call.Call(self._acc, bud.cfg.uri, self)
			if len(self._callList) < idx+1:
				self._callList.append(c)
			else:
				self._callList[idx] = c

			self._gui.audioUpdateState(bud.cfg.uri, gui.AudioState.INITIALIZING)
			
			try:
				c.makeCall(bud.cfg.uri, call_param)
			except:
				self._gui.audioUpdateState(bud.cfg.uri, gui.AudioState.FAILED)
			
	def stopCall(self):
		for bud in self._participantList:
			self._gui.audioUpdateState(bud.cfg.uri, gui.AudioState.DISCONNECTED)
		
		# clear call list, calls should be auto-destroyed by GC (and hungup by destructor)
		del self._callList[:]
		
	def updateCallState(self, thecall, info):
		if info.state == pj.PJSIP_INV_STATE_CONFIRMED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.CONNECTED)
		elif info.state == pj.PJSIP_INV_STATE_DISCONNECTED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.DISCONNECTED)
			# reset entry in the callList
			try:
				idx = self._callList.index(thecall)
				if idx >= 0: self._callList[idx] = None
			except:
				pass

			
	# ** callbacks from GUI (ChatObserver implementation) **
	
	# Text
	def onSendMessage(self, msg):
		send_im_param = pj.SendInstantMessageParam()
		send_im_param.content = str(msg)
		for bud in self._participantList:
			# send via call, if any, or buddy
			c = self._getCallFromBuddy(bud)
			try:
				if c:
					c.sendInstantMessage(send_im_param)
				else:
					bud.sendInstantMessage(send_im_param)
			except:
				# error will be handled via Account::onInstantMessageStatus()
				pass

	def onStartTyping(self):
		self._sendTypingIndication(True)
		
	def onStopTyping(self):
		self._sendTypingIndication(False)
		
	# Audio
	def onRetry(self, peer_uri):
		pass
	def onKick(self, peer_uri):
		pass
	def onHold(self, peer_uri):
		pass
	def onRxMute(self, peer_uri, is_muted):
		pass
	def onRxVol(self, peer_uri, vol_pct):
		pass
	def onTxMute(self, peer_uri, is_muted):
		pass

	# Chat room
	def onAddParticipant(self):
		#self.addParticipant()
		pass
	def onStartAudio(self):
		self.startCall()

	def onStopAudio(self):
		self.stopCall()
