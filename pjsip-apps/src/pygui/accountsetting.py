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
import endpoint
import application

class Dialog(tk.Toplevel):
	"""
	This implements account settings dialog to manipulate account settings.
	"""
	def __init__(self, parent, cfg):
		tk.Toplevel.__init__(self, parent)
		self.transient(parent)
		self.parent = parent
		self.geometry("+100+100")
		self.title('Account settings')
		
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
		self.createSipTab()
		self.createMediaTab()
		self.createMediaNatTab()
		
	def createBasicTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgPriority = tk.IntVar(value=self.cfg.priority)
		self.cfgAccId = tk.StringVar(value=self.cfg.idUri)
		self.cfgRegistrar = tk.StringVar(value=self.cfg.regConfig.registrarUri)
		self.cfgRegisterOnAdd = tk.IntVar(value=self.cfg.regConfig.registerOnAdd)
		self.cfgUsername = tk.StringVar()
		self.cfgPassword = tk.StringVar()
		if len(self.cfg.sipConfig.authCreds):
			self.cfgUsername.set( self.cfg.sipConfig.authCreds[0].username )
			self.cfgPassword.set( self.cfg.sipConfig.authCreds[0].data )
		self.cfgProxy = tk.StringVar()
		if len(self.cfg.sipConfig.proxies):
			self.cfgProxy.set( self.cfg.sipConfig.proxies[0] )
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		ttk.Label(frm, text='Priority:').grid(row=row, column=0, sticky=tk.E, pady=2)
		tk.Spinbox(frm, from_=0, to=9, textvariable=self.cfgPriority, width=2).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='ID (URI):').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgAccId, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Registrar URI:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgRegistrar, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Checkbutton(frm, text='Register on add', variable=self.cfgRegisterOnAdd).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='Optional proxy URI:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgProxy, width=32).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Auth username:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgUsername, width=16).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Password:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgPassword, show='*', width=16).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)

		self.wTab.add(frm, text='Basic Settings')
		

	def createSipTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgPrackUse 	= tk.IntVar(value=self.cfg.callConfig.prackUse)
		self.cfgTimerUse 	= tk.IntVar(value=self.cfg.callConfig.timerUse)
		self.cfgTimerExpires 	= tk.IntVar(value=self.cfg.callConfig.timerSessExpiresSec)
		self.cfgPublish 	= tk.BooleanVar(value=self.cfg.presConfig.publishEnabled)
		self.cfgMwiEnabled 	= tk.BooleanVar(value=self.cfg.mwiConfig.enabled)
		self.cfgEnableContactRewrite = tk.BooleanVar(value=self.cfg.natConfig.contactRewriteUse != 0) 
		self.cfgEnableViaRewrite = tk.BooleanVar(value=self.cfg.natConfig.viaRewriteUse != 0) 
		self.cfgEnableSdpRewrite = tk.BooleanVar(value=self.cfg.natConfig.sdpNatRewriteUse != 0)
		self.cfgEnableSipOutbound = tk.BooleanVar(value=self.cfg.natConfig.sipOutboundUse != 0)
		self.cfgKaInterval 	= tk.IntVar(value=self.cfg.natConfig.udpKaIntervalSec)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		ttk.Label(frm, text='100rel/PRACK:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Only offer PRACK', value=pj.PJSUA_100REL_NOT_USED, variable=self.cfgPrackUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Offer and use if remote supports', value=pj.PJSUA_100REL_OPTIONAL, variable=self.cfgPrackUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Required', value=pj.PJSUA_100REL_MANDATORY, variable=self.cfgPrackUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Session Timer:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Not offered', value=pj.PJSUA_SIP_TIMER_INACTIVE, variable=self.cfgTimerUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Optional', value=pj.PJSUA_SIP_TIMER_OPTIONAL, variable=self.cfgTimerUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Required', value=pj.PJSUA_SIP_TIMER_REQUIRED, variable=self.cfgTimerUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text="Always use", value=pj.PJSUA_SIP_TIMER_ALWAYS, variable=self.cfgTimerUse).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Session Timer Expiration:').grid(row=row, column=0, sticky=tk.E, pady=2)
		tk.Spinbox(frm, from_=90, to=7200, textvariable=self.cfgTimerExpires, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(seconds)').grid(row=row, column=1, sticky=tk.E)
		row += 1
		ttk.Label(frm, text='Presence:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable PUBLISH', variable=self.cfgPublish).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='Message Waiting Indication:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable MWI', variable=self.cfgMwiEnabled).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='NAT Traversal:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable Contact Rewrite', variable=self.cfgEnableContactRewrite).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Enable Via Rewrite', variable=self.cfgEnableViaRewrite).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Enable SDP IP Address Rewrite', variable=self.cfgEnableSdpRewrite).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Enable SIP Outbound Extension', variable=self.cfgEnableSipOutbound).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='UDP Keep-Alive Interval:').grid(row=row, column=0, sticky=tk.E, pady=2)
		tk.Spinbox(frm, from_=0, to=3600, textvariable=self.cfgKaInterval, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(seconds) Zero to disable.').grid(row=row, column=1, sticky=tk.E)


		self.wTab.add(frm, text='SIP Features')

	def createMediaTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgMedPort = tk.IntVar(value=self.cfg.mediaConfig.transportConfig.port)
		self.cfgMedPortRange = tk.IntVar(value=self.cfg.mediaConfig.transportConfig.portRange)
		self.cfgMedLockCodec = tk.BooleanVar(value=self.cfg.mediaConfig.lockCodecEnabled)
		self.cfgMedSrtp = tk.IntVar(value=self.cfg.mediaConfig.srtpUse)
		self.cfgMedSrtpSecure = tk.IntVar(value=self.cfg.mediaConfig.srtpSecureSignaling)
		self.cfgMedIpv6 = tk.BooleanVar(value=self.cfg.mediaConfig.ipv6Use==pj.PJSUA_IPV6_ENABLED)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=21)
		row = 0
		ttk.Label(frm, text='Secure RTP (SRTP):').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Disable', value=pj.PJMEDIA_SRTP_DISABLED, variable=self.cfgMedSrtp).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Mandatory', value=pj.PJMEDIA_SRTP_MANDATORY, variable=self.cfgMedSrtp).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Optional (non-standard)', value=pj.PJMEDIA_SRTP_OPTIONAL, variable=self.cfgMedSrtp).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='SRTP signaling:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Does not require secure signaling', value=0, variable=self.cfgMedSrtpSecure).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Require secure next hop (TLS)', value=1, variable=self.cfgMedSrtpSecure).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Require secure end-to-end (SIPS)', value=2, variable=self.cfgMedSrtpSecure).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='RTP transport start port:').grid(row=row, column=0, sticky=tk.E, pady=2)
		tk.Spinbox(frm, from_=0, to=65535, textvariable=self.cfgMedPort, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0: any)').grid(row=row, column=1, sticky=tk.E, pady=2)
		row += 1
		ttk.Label(frm, text='Port range:').grid(row=row, column=0, sticky=tk.E, pady=2)
		tk.Spinbox(frm, from_=0, to=65535, textvariable=self.cfgMedPortRange, width=5).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='(0: not limited)').grid(row=row, column=1, sticky=tk.E, pady=2)
		row += 1
		ttk.Label(frm, text='Lock codec:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable', variable=self.cfgMedLockCodec).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='Use IPv6:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Yes', variable=self.cfgMedIpv6).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)

		self.wTab.add(frm, text='Media settings')

	def createMediaNatTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgSipUseStun = tk.IntVar(value = self.cfg.natConfig.sipStunUse)
		self.cfgMediaUseStun = tk.IntVar(value = self.cfg.natConfig.mediaStunUse)
		self.cfgIceEnabled = tk.BooleanVar(value = self.cfg.natConfig.iceEnabled)
		self.cfgIceAggressive = tk.BooleanVar(value = self.cfg.natConfig.iceAggressiveNomination)
		self.cfgAlwaysUpdate = tk.BooleanVar(value = True if self.cfg.natConfig.iceAlwaysUpdate else False)
		self.cfgIceNoHostCands = tk.BooleanVar(value = True if self.cfg.natConfig.iceMaxHostCands == 0 else False)
		self.cfgTurnEnabled = tk.BooleanVar(value = self.cfg.natConfig.turnEnabled)
		self.cfgTurnServer = tk.StringVar(value = self.cfg.natConfig.turnServer)
		self.cfgTurnConnType = tk.IntVar(value = self.cfg.natConfig.turnConnType)
		self.cfgTurnUser = tk.StringVar(value = self.cfg.natConfig.turnUserName)
		self.cfgTurnPasswd = tk.StringVar(value = self.cfg.natConfig.turnPassword)
		
		# Build the tab page
		frm = ttk.Frame(self.frm)
		frm.columnconfigure(0, weight=1)
		frm.columnconfigure(1, weight=2)
		row = 0
		ttk.Label(frm, text='SIP STUN Usage:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Default', value=pj.PJSUA_STUN_USE_DEFAULT, variable=self.cfgSipUseStun).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Disable', value=pj.PJSUA_STUN_USE_DISABLED, variable=self.cfgSipUseStun).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Media STUN Usage:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='Default', value=pj.PJSUA_STUN_USE_DEFAULT, variable=self.cfgMediaUseStun).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='Disable', value=pj.PJSUA_STUN_USE_DISABLED, variable=self.cfgMediaUseStun).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='ICE:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable', variable=self.cfgIceEnabled).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Use aggresive nomination', variable=self.cfgIceAggressive).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Always re-INVITE after negotiation', variable=self.cfgAlwaysUpdate).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Checkbutton(frm, text='Disable host candidates', variable=self.cfgIceNoHostCands).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='TURN:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Checkbutton(frm, text='Enable', variable=self.cfgTurnEnabled).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='TURN server:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgTurnServer, width=20).grid(row=row, column=1, sticky=tk.W, padx=6)
		ttk.Label(frm, text='host[:port]').grid(row=row, column=1, sticky=tk.E, pady=6)
		row += 1
		ttk.Label(frm, text='TURN connection:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Radiobutton(frm, text='UDP', value=pj.PJ_TURN_TP_UDP, variable=self.cfgTurnConnType).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Radiobutton(frm, text='TCP', value=pj.PJ_TURN_TP_TCP, variable=self.cfgTurnConnType).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='TURN username:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgTurnUser, width=16).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='TURN password:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgTurnPasswd, show='*', width=16).grid(row=row, column=1, sticky=tk.W, padx=6)

		self.wTab.add(frm, text='NAT settings')
		
	def onOk(self):
		# Check basic settings
		errors = "";
		if not self.cfgAccId.get():
			errors += "Account ID is required\n"
		if self.cfgAccId.get():
			if not endpoint.validateSipUri(self.cfgAccId.get()):
				errors += "Invalid SIP ID URI: '%s'\n" % (self.cfgAccId.get())
		if self.cfgRegistrar.get():
			if not endpoint.validateSipUri(self.cfgRegistrar.get()):
				errors += "Invalid SIP registrar URI: '%s'\n" % (self.cfgRegistrar.get())
		if self.cfgProxy.get():
			if not endpoint.validateSipUri(self.cfgProxy.get()):
				errors += "Invalid SIP proxy URI: '%s'\n" % (self.cfgProxy.get())
		if self.cfgTurnEnabled.get():
			if not self.cfgTurnServer.get():
				errors += "TURN server is required\n"
		if errors:
			msgbox.showerror("Error detected:", errors)
			return
		
		# Basic settings
		self.cfg.priority = self.cfgPriority.get()
		self.cfg.idUri = self.cfgAccId.get()
		self.cfg.regConfig.registrarUri = self.cfgRegistrar.get()
		self.cfg.regConfig.registerOnAdd = self.cfgRegisterOnAdd.get()
		while len(self.cfg.sipConfig.authCreds):
			self.cfg.sipConfig.authCreds.pop()
		if self.cfgUsername.get():
			cred = pj.AuthCredInfo()
			cred.scheme = "digest"
			cred.realm = "*"
			cred.username = self.cfgUsername.get()
			cred.data = self.cfgPassword.get()
			self.cfg.sipConfig.authCreds.append(cred)
		while len(self.cfg.sipConfig.proxies):
			self.cfg.sipConfig.proxies.pop()
		if self.cfgProxy.get():
			self.cfg.sipConfig.proxies.append(self.cfgProxy.get())

		# SIP features
		self.cfg.callConfig.prackUse		= self.cfgPrackUse.get() 
		self.cfg.callConfig.timerUse		= self.cfgTimerUse.get()
		self.cfg.callConfig.timerSessExpiresSec	= self.cfgTimerExpires.get() 
		self.cfg.presConfig.publishEnabled	= self.cfgPublish.get() 
		self.cfg.mwiConfig.enabled		= self.cfgMwiEnabled.get() 
		self.cfg.natConfig.contactRewriteUse	= 1 if self.cfgEnableContactRewrite.get() else 0
		self.cfg.natConfig.viaRewriteUse 	= 1 if self.cfgEnableViaRewrite.get() else 0
		self.cfg.natConfig.sdpNatRewriteUse	= 1 if self.cfgEnableSdpRewrite.get() else 0
		self.cfg.natConfig.sipOutboundUse	= 1 if self.cfgEnableSipOutbound.get() else 0
		self.cfg.natConfig.udpKaIntervalSec	= self.cfgKaInterval.get()

		# Media
		self.cfg.mediaConfig.transportConfig.port	= self.cfgMedPort.get()
		self.cfg.mediaConfig.transportConfig.portRange	= self.cfgMedPortRange.get()
		self.cfg.mediaConfig.lockCodecEnabled		= self.cfgMedLockCodec.get()
		self.cfg.mediaConfig.srtpUse			= self.cfgMedSrtp.get()
		self.cfg.mediaConfig.srtpSecureSignaling	= self.cfgMedSrtpSecure.get()
		self.cfg.mediaConfig.ipv6Use			= pj.PJSUA_IPV6_ENABLED if self.cfgMedIpv6.get() else pj.PJSUA_IPV6_DISABLED
		
		# NAT
		self.cfg.natConfig.sipStunUse		= self.cfgSipUseStun.get()
		self.cfg.natConfig.mediaStunUse		= self.cfgMediaUseStun.get()
		self.cfg.natConfig.iceEnabled		= self.cfgIceEnabled.get()
		self.cfg.natConfig.iceAggressiveNomination = self.cfgIceAggressive .get()
		self.cfg.natConfig.iceAlwaysUpdate	= self.cfgAlwaysUpdate.get()
		self.cfg.natConfig.iceMaxHostCands	= 0 if self.cfgIceNoHostCands.get() else -1 
		self.cfg.natConfig.turnEnabled		= self.cfgTurnEnabled.get()
		self.cfg.natConfig.turnServer		= self.cfgTurnServer.get()
		self.cfg.natConfig.turnConnType		= self.cfgTurnConnType.get()
		self.cfg.natConfig.turnUserName		= self.cfgTurnUser.get()
		self.cfg.natConfig.turnPasswordType	= 0
		self.cfg.natConfig.turnPassword		= self.cfgTurnPasswd.get()
		
		self.isOk = True
		self.destroy()
		
	def onCancel(self):
		self.destroy()


if __name__ == '__main__':
	application.main()
