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
		self.wTab.grid(column=0, row=0, columnspan=2, padx=5, pady=5, sticky=tk.N+tk.S+tk.W+tk.E)
		
		# Main buttons
		btnOk = ttk.Button(self.frm, text='Ok', command=self.onOk)
		btnOk.grid(column=0, row=1, sticky=tk.E, padx=20, pady=10)
		btnCancel = ttk.Button(self.frm, text='Cancel', command=self.onCancel)
		btnCancel.grid(column=1, row=1, sticky=tk.W, padx=20, pady=10)
		
		# Tabs
		self.createBasicTab()
		
	def createBasicTab(self):
		# Prepare the variables to set/receive values from GUI
		self.cfgPriority = tk.IntVar()
		self.cfgPriority.set( self.cfg.priority )
		self.cfgAccId = tk.StringVar()
		self.cfgAccId.set( self.cfg.idUri )
		self.cfgRegistrar = tk.StringVar()
		self.cfgRegistrar.set( self.cfg.regConfig.registrarUri )
		self.cfgRegisterOnAdd = tk.IntVar()
		self.cfgRegisterOnAdd.set(self.cfg.regConfig.registerOnAdd)
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
		ttk.Entry(frm, textvariable=self.cfgAccId, width=25).grid(row=row, column=1, sticky=tk.W+tk.E, padx=6)
		row += 1
		ttk.Label(frm, text='Registrar URI:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgRegistrar, width=25).grid(row=row, column=1, sticky=tk.W+tk.E, padx=6)
		row += 1
		ttk.Checkbutton(frm, text='Register on add', variable=self.cfgRegisterOnAdd).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)
		row += 1
		ttk.Label(frm, text='Optional proxy URI:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgProxy, width=25).grid(row=row, column=1, sticky=tk.W+tk.E, padx=6)
		row += 1
		ttk.Label(frm, text='Username:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgUsername, width=12).grid(row=row, column=1, sticky=tk.W, padx=6)
		row += 1
		ttk.Label(frm, text='Password:').grid(row=row, column=0, sticky=tk.E, pady=2)
		ttk.Entry(frm, textvariable=self.cfgPassword, show='*', width=12).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)

		self.wTab.add(frm, text='Basic Settings')
		
	
	def onOk(self):
		# Check basic settings
		errors = "";
		if self.cfgAccId.get():
			if not endpoint.validateSipUri(self.cfgAccId.get()):
				errors += "Invalid SIP ID URI: '%s'\n" % (self.cfgAccId.get())
		if self.cfgRegistrar.get():
			if not endpoint.validateSipUri(self.cfgRegistrar.get()):
				errors += "Invalid SIP registrar URI: '%s'\n" % (self.cfgRegistrar.get())
		if self.cfgProxy.get():
			if not endpoint.validateSipUri(self.cfgProxy.get()):
				errors += "Invalid SIP proxy URI: '%s'\n" % (self.cfgProxy.get())
				
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
		
		self.isOk = True
		self.destroy()
		
	def onCancel(self):
		self.destroy()


if __name__ == '__main__':
	application.main()
