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
import log
import accountsetting
import account
import endpoint

import os
import pickle
import traceback

#----------------------------------------------------------
# Unfortunately SWIG classes cannot be serialized directly
# hence need to write these config classes
#
class AccountConfig:
	"""
	"Proxy" for the pj.AccountConfig class, serializable
	"""
	def __init__(self, acc_cfg = None):
		if not acc_cfg:
			acc_cfg = pj.AccountConfig()
		self.priority 	= acc_cfg.priority
		self.idUri 	= acc_cfg.idUri
		self.regUri 	= acc_cfg.regConfig.registrarUri
		self.registerOnAdd = acc_cfg.regConfig.registerOnAdd
		self.proxies 	= [proxy for proxy in acc_cfg.sipConfig.proxies]
		self.userName 	= ""
		self.password 	= ""
		if len(acc_cfg.sipConfig.authCreds):
			self.userName = acc_cfg.sipConfig.authCreds[0].username
			self.password = acc_cfg.sipConfig.authCreds[0].data

	def getAccConfig(self, acc_cfg = None):
		"""
		Convert this class to pj.AccountConfig class
		"""
		if not acc_cfg:
			acc_cfg  = pj.AccountConfig()
		acc_cfg.priority = self.priority
		acc_cfg.idUri	 = self.idUri
		acc_cfg.regConfig.registrarUri = self.regUri
		acc_cfg.regConfig.registerOnAdd = self.registerOnAdd
		for proxy in self.proxies:
			acc_cfg.sipConfig.proxies.append(proxy)
		if self.userName:
			cred = pj.AuthCredInfo()
			cred.scheme	= "digest"
			cred.realm	= "*"
			cred.username	= self.userName
			cred.data	= self.password
			acc_cfg.sipConfig.authCreds.append(cred)
		return acc_cfg
		
class ApplicationConfig:
	"""
	Application config is serializable and contains all settings that application need.
	"""
	def __init__(self, ep_cfg = None, acc_cfgs = []):
		if not ep_cfg:
			ep_cfg = pj.EpConfig()
		
		self.logFile	= ep_cfg.logConfig.filename
		self.logFlags	= ep_cfg.logConfig.fileFlags
		self.logLevel	= ep_cfg.logConfig.consoleLevel
		
		self.accCfgs = []
		for acc_cfg in acc_cfgs:
			self.accCfgs.append( AccountConfig(acc_cfg) )
		
	def getEpConfig(self, ep_cfg = None):
		if not ep_cfg:
			ep_cfg = pj.EpConfig()
		ep_cfg.logConfig.filename = self.logFile 
		ep_cfg.logConfig.fileFlags = self.logFlags 
		ep_cfg.logConfig.consoleLevel = self.logLevel
		return ep_cfg

		
#----------------------------------------------------------

		
class Application(ttk.Frame):
	"""
	The Application main frame.
	"""
	def __init__(self):
		ttk.Frame.__init__(self, name='application', width=300, height=500)
		self.pack(expand='yes', fill='both')
		self.master.title('pjsua2 Demo')
		
		# Logger
		self.logger = log.Logger()
		
		# Global config, fill with default
		self.epCfg = pj.EpConfig()
		self.epCfg.uaConfig.threadCnt = 0;
		self.epCfg.logConfig.writer = self.logger
		self.epCfg.logConfig.filename = "pygui.log"
		self.epCfg.logConfig.fileFlags = pj.PJ_O_APPEND
		self.epCfg.logConfig.level = 5
		self.epCfg.logConfig.consoleLevel = 5
		
		# Accounts
		self.accList = []
		
		# GUI variables
		self.showLogWindow = tk.IntVar()
		self.showLogWindow.set(1)
		self.quitting = False 
		
		# Construct GUI
		self._createWidgets()
		
		# Log window
		self.logWindow = log.LogWindow(self)
		self._onMenuShowHideLogWindow()
		
	def saveConfig(self, filename='pygui.dat'):
		acc_cfgs = [acc.cfg for acc in self.accList]
		app_cfg = ApplicationConfig(self.epCfg, acc_cfgs)
		f = open(filename, 'wb')
		pickle.dump(app_cfg, f, 0)
		f.close()
	
	def start(self, cfg_file='pygui.dat'):
		# Load config
		acc_cfgs = []
		if cfg_file and os.path.exists(cfg_file):
			f = open(cfg_file, 'rb')
			app_cfg = pickle.load(f)
			app_cfg.getEpConfig(self.epCfg)
			for c in app_cfg.accCfgs:
				cfg = c.getAccConfig()
				acc_cfgs.append(cfg)
			f.close()
		
		# Instantiate endpoint
		self.ep = endpoint.Endpoint()
		self.epCfg.uaConfig.userAgent = "pygui-" + self.ep.libVersion().full;
		self.ep.startLib(self.epCfg)
		self.master.title('pjsua2 Demo version ' + self.ep.libVersion().full)
		
		# Add accounts
		for cfg in acc_cfgs:
			self._createAcc(cfg)
		
		# Start polling
		self._onTimer()

	def updateAccount(self, acc):
		iid = str(acc.randId)
		text = acc.cfg.idUri
		status = acc.statusText()
		
		values = (status,)
		if self.tv.exists(iid):
			self.tv.item(iid, text=text, values=values)
		else:
			self.tv.insert('', 0,  iid, open=True, text=text, values=values)
			self.tv.insert(iid, 0, '', open=True, text='Buddy 1', values=('Online',))
			self.tv.insert(iid, 1, '', open=True, text='Buddy 2', values=('Online',))
		
	def _createAcc(self, acc_cfg):
		acc = account.Account(self)
		acc.cfg = acc_cfg
		self.accList.append(acc)
		self.updateAccount(acc)
		acc.create(acc.cfg)
		acc.cfgChanged = False
		self.updateAccount(acc)
				
	def _createWidgets(self):
		self._createAppMenu()
		
		# Main pane, a Treeview
		self.tv = ttk.Treeview(self, columns=('Status'), show='tree')
		self.tv.pack(side='top', fill='both', expand='yes', padx=5, pady=5)

		self._createContextMenu()
		
		# Handle close event
		self.master.protocol("WM_DELETE_WINDOW", self._onClose)
	
	def _createAppMenu(self):
		# Main menu bar
		top = self.winfo_toplevel()
		self.menubar = tk.Menu()
		top.configure(menu=self.menubar)
		
		# File menu
		file_menu = tk.Menu(self.menubar, tearoff=False)
		self.menubar.add_cascade(label="File", menu=file_menu)
		file_menu.add_command(label="Add account..", command=self._onMenuAddAccount)
		file_menu.add_checkbutton(label="Show/hide log window", command=self._onMenuShowHideLogWindow, variable=self.showLogWindow)
		file_menu.add_separator()
		file_menu.add_command(label="Settings...", command=self._onMenuSettings)
		file_menu.add_command(label="Save Settings", command=self._onMenuSaveSettings)
		file_menu.add_separator()
		file_menu.add_command(label="Quit", command=self._onMenuQuit)

		# Help menu
		help_menu = tk.Menu(self.menubar, tearoff=False)
		self.menubar.add_cascade(label="Help", menu=help_menu)
		help_menu.add_command(label="About", underline=2, command=self._onMenuAbout)
		
	def _createContextMenu(self):
		top = self.winfo_toplevel()
		self.accMenu = tk.Menu(top, tearoff=False)
		# Labels, must match with _onAccContextMenu()
		labels = ['Unregister', 'Reregister', '-', 'Online', 'Invisible', 'Away', 'Busy', '-', 'Settings...', '-', 'Delete...']
		for label in labels:
			if label=='-':
				self.accMenu.add_separator()
			else:
				cmd = lambda arg=label: self._onAccContextMenu(arg)
				self.accMenu.add_command(label=label, command=cmd)
		
		if (top.tk.call('tk', 'windowingsystem')=='aqua'):
			self.tv.bind('<2>', self._onTvRightClick)
			self.tv.bind('<Control-1>', self._onTvRightClick)
		else:
			self.tv.bind('<3>', self._onTvRightClick)

	def _getSelectedAccount(self):
		items = self.tv.selection()
		if not items:
			return None
		try:
			iid = int(items[0])
		except:
			return None
		accs = [acc for acc in self.accList if acc.randId==iid]
		if not accs:
			return None
		return accs[0]
	
	def _onTvRightClick(self, event):
		iid = self.tv.identify('item', event.x, event.y)
		if iid:
			self.tv.selection_add( (iid,) )
			acc = self._getSelectedAccount()
			if acc:
				self.accMenu.post(event.x_root, event.y_root)
			else:
				# A buddy is selected
				pass
	
	def _onAccContextMenu(self, label):
		acc = self._getSelectedAccount()
		if not acc:
			return
		
		if label=='Unregister':
			acc.setRegistration(False)
		elif label=='Reregister':
			acc.setRegistration(True)
		elif label=='Online':
			ps = pj.AccountPresenceStatus()
			ps.isOnline = True
			acc.setOnlineStatus(ps)
		elif label=='Invisible':
			ps = pj.AccountPresenceStatus()
			ps.isOnline = False
			acc.setOnlineStatus(ps)
		elif label=='Away':
			ps = pj.AccountPresenceStatus()
			ps.isOnline = True
			ps.activity = pj.PJRPID_ACTIVITY_AWAY
			ps.note = "Away"
			acc.setOnlineStatus(ps)
		elif label=='Busy':
			ps = pj.AccountPresenceStatus()
			ps.isOnline = True
			ps.activity = pj.PJRPID_ACTIVITY_BUSY
			ps.note = "Busy"
			acc.setOnlineStatus(ps)
		elif label=='Settings...':
			self.cfgChanged = False
			dlg = accountsetting.Dialog(self.master, acc.cfg)
			if dlg.doModal():
				self.updateAccount(acc)
				acc.modify(acc.cfg)
		elif label=='Delete...':
			msg = "Do you really want to delete account '%s'?" % acc.cfg.idUri
			if msgbox.askquestion('Delete account?', msg, default=msgbox.NO) != u'yes':
				return
			iid = str(acc.randId)
			self.accList.remove(acc)
			del acc
			self.tv.delete( (iid,) )
		else:
			assert not ("Unknown menu " + label)
	
	def _onTimer(self):
		if not self.quitting:
			self.ep.libHandleEvents(10)
			if not self.quitting:
				self.master.after(50, self._onTimer)
			
	def _onClose(self):
		self.quitting = True
		self.ep.stopLib()
		self.ep = None
		self.update()
		self.quit()
		
	def _onMenuAddAccount(self):
		cfg = pj.AccountConfig()
		dlg = accountsetting.Dialog(self.master, cfg)
		if dlg.doModal():
			self._createAcc(cfg)
			
	def _onMenuShowHideLogWindow(self):
		if self.showLogWindow.get():
			self.logWindow.deiconify()
		else:
			self.logWindow.withdraw()
	
	def _onMenuSettings(self):
		msgbox.showinfo(self.master.title(), 'Settings')
	
	def _onMenuSaveSettings(self):
		self.saveConfig()
		
	def _onMenuQuit(self):
		self._onClose()

	def _onMenuAbout(self):
		msgbox.showinfo(self.master.title(), 'About')
		

class ExceptionCatcher:
	"""Custom Tk exception catcher, mainly to display more information 
	   from pj.Error exception
	""" 
	def __init__(self, func, subst, widget):
		self.func = func 
		self.subst = subst
		self.widget = widget
	def __call__(self, *args):
		try:
			if self.subst:
				args = apply(self.subst, args)
			return apply(self.func, args)
		except pj.Error, error:
			print 'Exception:'
			print '  ', error.info()
			print 'Traceback:'
			print traceback.print_stack()
			log.writeLog2(1, 'Exception: ' + error.info() + '\n')
		except Exception, error:
			print 'Exception:'
			print '  ', str(error)
			print 'Traceback:'
			print traceback.print_stack()
			log.writeLog2(1, 'Exception: ' + str(error) + '\n')

def main():
	#tk.CallWrapper = ExceptionCatcher
	app = Application()
	app.start()
	app.mainloop()
	app.saveConfig()
		
if __name__ == '__main__':
	main()
