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
	def __init__(self, app, acc, bud, call_inst=None):
		self._app = app
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
		
	def _getBuddyFromUri(self, uri):
		for bud in self._participantList:
			if uri == bud.cfg.uri:
				return bud
		return None
		
	def _getCallFromUri(self, uri, op = ''):
		for idx, bud in enumerate(self._participantList):
			if uri == bud.cfg.uri:
				if idx < len(self._callList):
					return self._callList[idx]
				return None
		print "=== %s cannot find buddy URI '%s'" % (op, uri)
		return None
		
	def _sendTypingIndication(self, is_typing):
		type_ind_param = pj.SendTypingIndicationParam()
		type_ind_param.isTyping = is_typing
		for bud in self._participantList:
			c = self._getCallFromBuddy(bud)
			try:
				if c and c.connected:
					c.sendTypingIndication(type_ind_param)
				else:
					bud.sendTypingIndication(type_ind_param)
			except:
				pass

	def _sendInstantMessage(self, msg, sender_uri=''):
		send_im_param = pj.SendInstantMessageParam()
		send_im_param.content = str(msg)
		for bud in self._participantList:
			# don't echo back to the original sender
			if sender_uri and bud.cfg.uri == sender_uri:
				continue
				
			# send via call, if any, or buddy
			c = self._getCallFromBuddy(bud)
			try:
				if c and c.connected:
					c.sendInstantMessage(send_im_param)
				else:
					bud.sendInstantMessage(send_im_param)
			except:
				# error will be handled via Account::onInstantMessageStatus()
				pass

	def isPrivate(self):
		return len(self._participantList) <= 1
		
	def isBuddyParticipant(self, bud):
		return bud in self._participantList
		
	def isCallRegistered(self, call_inst):
		return call_inst in self._callList
		
	def registerCall(self, bud, call_inst):
		try:
			idx = self._participantList.index(bud)
			if len(self._callList) < idx+1:
				self._callList.append(call_inst)
			else:
				self._callList[idx] = call_inst

			call_inst.chat = self
			call_inst.peerUri = bud.cfg.uri
		except:
			pass
		
	def showWindow(self):
		self._gui.bringToFront()

	# helper
	def dumpParticipantList(self):
		print "Number of participants: %d" % (len(self._participantList))
		for b in self._participantList:
			print b.cfg.uri
		
	def addParticipant(self, bud, call_inst=None):
		# avoid duplication
		if self.isBuddyParticipant(bud): return
		for b in self._participantList:
			if bud.cfg.uri == b.cfg.uri: return
			
		# add it
		self._participantList.append(bud)
		if call_inst:
			self._callList.append(call_inst)
		self._gui.addParticipant(bud.cfg.uri)

		self._updateGui()
	
	def kickParticipant(self, bud):
		if bud in self._participantList:
			idx = self._participantList.index(bud)
			self._participantList.remove(bud)
			self._gui.delParticipant(bud.cfg.uri)
			
			# also clear call, if any
			if self._callList: del self._callList[idx]
			
		if self._participantList:
			self._updateGui()
		else:
			# will remove entry from list eventually destroy this chat?
			self._acc.chatList.remove(self)
			
			# let's destroy GUI manually
			self._gui.destroy()
			#self.destroy()
			
	def addMessage(self, from_uri, msg):
		if from_uri:
			msg = from_uri + ': ' + msg
			self._gui.textAddMessage(msg)
			self._sendInstantMessage(msg, from_uri)
		else:
			self._gui.textAddMessage(msg, False)
			
	def setTypingIndication(self, from_uri, is_typing):
		self._gui.textSetTypingIndication(from_uri, is_typing)
		
	def startCall(self):
		self._gui.enableAudio()
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
				#self._callList[idx] = None
				#self._gui.audioUpdateState(bud.cfg.uri, gui.AudioState.FAILED)
				self.kickParticipant(bud)
			
	def stopCall(self):
		for bud in self._participantList:
			self._gui.audioUpdateState(bud.cfg.uri, gui.AudioState.DISCONNECTED)
		
		# clear call list, calls should be auto-destroyed by GC (and hungup by destructor)
		del self._callList[:]
		
	def updateCallState(self, thecall, info = None):
		# info is optional here, just to avoid calling getInfo() twice (in the caller and here)
		if not info: info = thecall.getInfo()
		if info.state < pj.PJSIP_INV_STATE_CONFIRMED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.INITIALIZING)
		elif info.state == pj.PJSIP_INV_STATE_CONFIRMED:
			self._gui.audioUpdateState(thecall.peerUri, gui.AudioState.CONNECTED)
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
				bud = self._getBuddyFromUri(thecall.peerUri)
				if bud: self.kickParticipant(bud)

			
	# ** callbacks from GUI (ChatObserver implementation) **
	
	# Text
	def onSendMessage(self, msg):
		self._sendInstantMessage(msg)

	def onStartTyping(self):
		self._sendTypingIndication(True)
		
	def onStopTyping(self):
		self._sendTypingIndication(False)
		
	# Audio
	def onHangup(self, peer_uri):
		c = self._getCallFromUri(peer_uri, "onHangup()")
		if not c: return
		call_param = pj.CallOpParam()
		c.hangup(call_param)

	def onHold(self, peer_uri):
		c = self._getCallFromUri(peer_uri, "onHold()")
		if not c: return
		call_param = pj.CallOpParam()
		c.setHold(call_param)

	def onUnhold(self, peer_uri):
		c = self._getCallFromUri(peer_uri, "onUnhold()")
		if not c: return
		call_param = pj.CallOpParam()
		c.reinvite(call_param)
		
	def onRxMute(self, peer_uri, is_muted):
		pass
	def onRxVol(self, peer_uri, vol_pct):
		pass
	def onTxMute(self, peer_uri, is_muted):
		pass

	# Chat room
	def onAddParticipant(self):
		buds = []
		dlg = AddParticipantDlg(None, self._app, buds)
		if dlg.doModal():
			for bud in buds:
				self.addParticipant(bud)
			self.startCall()
				
	def onStartAudio(self):
		self.startCall()

	def onStopAudio(self):
		self.stopCall()


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
