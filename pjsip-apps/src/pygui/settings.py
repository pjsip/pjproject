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

import pjsua2 as pj
#import application

# Transport setting
class SipTransportConfig:
	def __init__(self, type, enabled):
		#pj.PersistentObject.__init__(self)
		self.type = type
		self.enabled = enabled
		self.config = pj.TransportConfig()
	def readObject(self, node):
		child_node = node.readContainer("SipTransport")
		self.type = child_node.readInt("type")
		self.enabled = child_node.readBool("enabled")
		self.config.readObject(child_node)
	def writeObject(self, node):
		child_node = node.writeNewContainer("SipTransport")
		child_node.writeInt("type", self.type)
		child_node.writeBool("enabled", self.enabled)
		self.config.writeObject(child_node)

# Account setting with buddy list
class AccConfig:
	def __init__(self):
		self.enabled = True
		self.config = pj.AccountConfig()
		self.buddyConfigs = []
	def readObject(self, node):
		acc_node = node.readContainer("Account")
		self.enabled = acc_node.readBool("enabled")
		self.config.readObject(acc_node)
		buddy_node = acc_node.readArray("buddies")
		while buddy_node.hasUnread():
			buddy_cfg = pj.BuddyConfig()
			buddy_cfg.readObject(buddy_node)
			self.buddyConfigs.append(buddy_cfg)
	def writeObject(self, node):
		acc_node = node.writeNewContainer("Account")
		acc_node.writeBool("enabled", self.enabled)
		self.config.writeObject(acc_node)
		buddy_node = acc_node.writeNewArray("buddies")
		for buddy in self.buddyConfigs:
			buddy_node.writeObject(buddy)

	
# Master settings
class AppConfig:
	def __init__(self):
		self.epConfig = pj.EpConfig()	# pj.EpConfig()
		self.udp = SipTransportConfig(pj.PJSIP_TRANSPORT_UDP, True)
		self.tcp = SipTransportConfig(pj.PJSIP_TRANSPORT_TCP, True)
		self.tls = SipTransportConfig(pj.PJSIP_TRANSPORT_TLS, False)
		self.accounts = []		# Array of AccConfig
		
	def loadFile(self, file):
		json = pj.JsonDocument()
		json.loadFile(file)
		root = json.getRootContainer()
		self.epConfig = pj.EpConfig()
		self.epConfig.readObject(root)
		
		tp_node = root.readArray("transports")
		self.udp.readObject(tp_node)
		self.tcp.readObject(tp_node)
		if tp_node.hasUnread():
			self.tls.readObject(tp_node)
			
		acc_node = root.readArray("accounts")
		while acc_node.hasUnread():
			acfg = AccConfig()
			acfg.readObject(acc_node)
			self.accounts.append(acfg)
			
	def saveFile(self,file):
		json = pj.JsonDocument()
		
		# Write endpoint config
		json.writeObject(self.epConfig)
		
		# Write transport config
		tp_node = json.writeNewArray("transports")
		self.udp.writeObject(tp_node)
		self.tcp.writeObject(tp_node)
		self.tls.writeObject(tp_node)
		
		# Write account configs
		node = json.writeNewArray("accounts")
		for acc in self.accounts:
			acc.writeObject(node)
				
		json.saveFile(file)
		

# Settings dialog
class Dialog(tk.Toplevel):
	"""
	This implements account settings dialog to manipulate account settings.
	"""
	def __init__(self, parent, cfg):
		tk.Toplevel.__init__(self, parent)
		self.transient(parent)
		self.parent = parent
		self.title('Settings')
		
		self.frm = ttk.Frame(self)
		self.frm.pack(expand='yes', fill='both')
		
		self.isOk = False
		self.cfg = cfg
		
		self.createWidgets()
	
	def doModal(self):
		if self.parent:
			self.parent.wait_window(self)
		else:
			self.wait_window(self)
		return self.isOk
		
	def createWidgets(self):
		# The notebook
		self.frm.rowconfigure(0, weight=1)
		self.frm.rowconfigure(1, weight=0)
		self.frm.columnconfigure(0, weight=1)
		self.frm.columnconfigure(1, weight=1)
		self.wTab = ttk.Notebook(self.frm)
		self.wTab.grid(column=0, row=0, columnspan=2, padx=10, pady=10, ipadx=20, ipady=20, sticky=tk.N+tk.S+tk.W+tk.E)
		
		# Main buttons
		btnOk = ttk.Button(self.frm, text='Ok', command=self.onOk)
		btnOk.grid(column=0, row=1, sticky=tk.E, padx=20, pady=10)
		btnCancel = ttk.Button(self.frm, text='Cancel', command=self.onCancel)
		btnCancel.grid(column=1, row=1, sticky=tk.W, padx=20, pady=10)
		
		# Tabs
		self.createBasicTab()
		self.createNetworkTab()
		self.createMediaTab()

	def createBasicTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgLogFile = tk.StringVar(value=self.cfg.epConfig.logConfig.filename)
		self.cfgLogAppend = tk.BooleanVar(value=True if (self.cfg.epConfig.logConfig.fileFlags & pj.PJ_O_APPEND) else False)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		ttk.Label(frm, text='User Agent:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Label(frm, text=self.cfg.epConfig.uaConfig.userAgent).grid(row=row, column=1, sticky=tk.W, pady=2, padx=6)
		row += 1
		ttk.Label(frm, text='Max calls:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Label(frm, text=str(self.cfg.epConfig.uaConfig.maxCalls)).grid(row=row, column=1, sticky=tk.W, pady=2, padx=6)
		row += 1
		ttk.Label(frm, text='Log file:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Entry(frm, textvariable=self.cfgLogFile, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Checkbutton(frm, text='Append log file', variable=self.cfgLogAppend).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)

		self.wTab.add(frm, text='Basic')
		
	def createNetworkTab(self):
		self.cfgNameserver = tk.StringVar()
		if len(self.cfg.epConfig.uaConfig.nameserver):
			self.cfgNameserver.set(self.cfg.epConfig.uaConfig.nameserver[0])
		self.cfgStunServer = tk.StringVar()
		if len(self.cfg.epConfig.uaConfig.stunServer):
			self.cfgStunServer.set(self.cfg.epConfig.uaConfig.stunServer[0])
		self.cfgStunIgnoreError = tk.BooleanVar(value=self.cfg.epConfig.uaConfig.stunIgnoreFailure)
		
		self.cfgUdpEnabled = tk.BooleanVar(value=self.cfg.udp.enabled)
		self.cfgUdpPort = tk.IntVar(value=self.cfg.udp.config.port)
		self.cfgTcpEnabled = tk.BooleanVar(value=self.cfg.tcp.enabled)
		self.cfgTcpPort = tk.IntVar(value=self.cfg.tcp.config.port)
		self.cfgTlsEnabled = tk.BooleanVar(value=self.cfg.tls.enabled)
		self.cfgTlsPort = tk.IntVar(value=self.cfg.tls.config.port)
		
		self.cfgTlsCaFile = tk.StringVar(value=self.cfg.tls.config.tlsConfig.CaListFile)
		self.cfgTlsCertFile = tk.StringVar(value=self.cfg.tls.config.tlsConfig.certFile)
		self.cfgTlsVerifyClient = tk.BooleanVar(value=self.cfg.tls.config.tlsConfig.verifyClient)
		self.cfgTlsVerifyServer = tk.BooleanVar(value=self.cfg.tls.config.tlsConfig.verifyServer)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		#ttk.Label(frm, text='UDP transport:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Checkbutton(frm, text='Enable UDP transport', variable=self.cfgUdpEnabled).grid(row=row, column=0, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='UDP port:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=0, to=65535, textvariable=self.cfgUdpPort, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0 for any)').grid(row=row, column=1, sticky=tk.E, pady=6, padx=6)
		row += 1
		#ttk.Label(frm, text='TCP transport:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Checkbutton(frm, text='Enable TCP transport', variable=self.cfgTcpEnabled).grid(row=row, column=0, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='TCP port:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=0, to=65535, textvariable=self.cfgTcpPort, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0 for any)').grid(row=row, column=1, sticky=tk.E, pady=6, padx=6)
		row += 1
		#ttk.Label(frm, text='TLS transport:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Checkbutton(frm, text='Enable TLS transport', variable=self.cfgTlsEnabled).grid(row=row, column=0, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='TLS port:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=0, to=65535, textvariable=self.cfgTlsPort, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0 for any)').grid(row=row, column=1, sticky=tk.E, pady=6, padx=6)
		row += 1
		ttk.Label(frm, text='TLS CA file:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Entry(frm, textvariable=self.cfgTlsCaFile, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='TLS cert file:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Entry(frm, textvariable=self.cfgTlsCertFile, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Checkbutton(frm, text='TLS verify server', variable=self.cfgTlsVerifyServer).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='TLS verify client', variable=self.cfgTlsVerifyClient).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='DNS and STUN:').grid(row=row, column=0, sticky=tk.W, pady=2, padx=8)
		row += 1
		ttk.Label(frm, text='Nameserver:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Entry(frm, textvariable=self.cfgNameserver, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='STUN Server:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Entry(frm, textvariable=self.cfgStunServer, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Checkbutton(frm, text='Ignore STUN failure at startup', variable=self.cfgStunIgnoreError).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		
		self.wTab.add(frm, text='Network')

	def createMediaTab(self):
		self.cfgClockrate = tk.IntVar(value=self.cfg.epConfig.medConfig.clockRate)
		self.cfgSndClockrate = tk.IntVar(value=self.cfg.epConfig.medConfig.sndClockRate)
		self.cfgAudioPtime = tk.IntVar(value=self.cfg.epConfig.medConfig.audioFramePtime)
		self.cfgMediaQuality = tk.IntVar(value=self.cfg.epConfig.medConfig.quality)
		self.cfgCodecPtime = tk.IntVar(value=self.cfg.epConfig.medConfig.ptime)
		self.cfgVad = tk.BooleanVar(value=not self.cfg.epConfig.medConfig.noVad)
		self.cfgEcTailLen = tk.IntVar(value=self.cfg.epConfig.medConfig.ecTailLen)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		ttk.Label(frm, text='Max media ports:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Label(frm, text=str(self.cfg.epConfig.medConfig.maxMediaPorts)).grid(row=row, column=1, sticky=tk.W, pady=2, padx=6)
		row += 1
		ttk.Label(frm, text='Core clock rate:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=8000, to=48000, increment=8000, textvariable=self.cfgClockrate, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Snd device clock rate:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=0, to=48000, increment=8000, textvariable=self.cfgSndClockrate, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0: follow core)').grid(row=row, column=1, sticky=tk.E, pady=6, padx=6)
		row += 1
		ttk.Label(frm, text='Core ptime:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=10, to=400, increment=10, textvariable=self.cfgAudioPtime, width=3).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='RTP ptime:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=20, to=400, increment=10, textvariable=self.cfgCodecPtime, width=3).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Media quality (1-10):').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=1, to=10, textvariable=self.cfgMediaQuality, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='VAD:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		ttk.Checkbutton(frm, text='Enable', variable=self.cfgVad).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='Echo canceller tail length:').grid(row=row, column=0, sticky=tk.E, pady=2, padx=8)
		tk.Spinbox(frm, from_=0, to=400, increment=10, textvariable=self.cfgEcTailLen, width=3).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(ms, 0 to disable)').grid(row=row, column=1, sticky=tk.E, pady=6, padx=6)
		
		self.wTab.add(frm, text='Media')

	def onOk(self):
		# Check basic settings
		errors = "";
		if errors:
			msgbox.showerror("Error detected:", errors)
			return
		
		# Basic settings
		self.cfg.epConfig.logConfig.filename = self.cfgLogFile.get()
		flags = pj.PJ_O_APPEND if self.cfgLogAppend.get() else 0
		self.cfg.epConfig.logConfig.fileFlags = self.cfg.epConfig.logConfig.fileFlags | flags 
		
		# Network settings
		self.cfg.epConfig.uaConfig.nameserver.clear()
		if len(self.cfgNameserver.get()): 
			self.cfg.epConfig.uaConfig.nameserver.append(self.cfgNameserver.get())
		self.cfg.epConfig.uaConfig.stunServer.clear()
		if len(self.cfgStunServer.get()):
			self.cfg.epConfig.uaConfig.stunServer.append(self.cfgStunServer.get())
			
		self.cfg.epConfig.uaConfig.stunIgnoreFailure = self.cfgStunIgnoreError.get()
		
		self.cfg.udp.enabled 	= self.cfgUdpEnabled.get()
		self.cfg.udp.config.port = self.cfgUdpPort.get()
		self.cfg.tcp.enabled 	= self.cfgTcpEnabled.get()
		self.cfg.tcp.config.port = self.cfgTcpPort.get()
		self.cfg.tls.enabled 	= self.cfgTlsEnabled.get()
		self.cfg.tls.config.port = self.cfgTlsPort.get()
		
		self.cfg.tls.config.tlsConfig.CaListFile = self.cfgTlsCaFile.get()
		self.cfg.tls.config.tlsConfig.certFile   = self.cfgTlsCertFile.get()
		self.cfg.tls.config.tlsConfig.verifyClient = self.cfgTlsVerifyClient.get()
		self.cfg.tls.config.tlsConfig.verifyServer = self.cfgTlsVerifyServer.get()

		# Media
		self.cfg.epConfig.medConfig.clockRate	= self.cfgClockrate.get()
		self.cfg.epConfig.medConfig.sndClockRate = self.cfgSndClockrate.get()
		self.cfg.epConfig.medConfig.audioFramePtime = self.cfgAudioPtime.get()
		self.cfg.epConfig.medConfig.quality	= self.cfgMediaQuality.get()
		self.cfg.epConfig.medConfig.ptime	= self.cfgCodecPtime.get()
		self.cfg.epConfig.medConfig.noVad	= not self.cfgVad.get()
		self.cfg.epConfig.medConfig.ecTailLen	= self.cfgEcTailLen.get()
		
		self.isOk = True
		self.destroy()
		
	def onCancel(self):
		self.destroy()


if __name__ == '__main__':
	#application.main()
	acfg = AppConfig()
	acfg.loadFile('pygui.js')

	dlg = Dialog(None, acfg)
	if dlg.doModal():
		acfg.saveFile('pygui.js')
		