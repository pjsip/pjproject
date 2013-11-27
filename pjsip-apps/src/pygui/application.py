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
import buddy
import endpoint

import os
import traceback

class SipTransportConfig:
	def __init__(self):
		#pj.PersistentObject.__init__(self)
		self.type = pj.PJSIP_TRANSPORT_UNSPECIFIED;
		self.cfg = pj.TransportConfig()
		
	def readObject(self, node):
		child_node = node.readContainer("SipTransportConfig")
		self.type = child_node.readInt("type")
		self.cfg.readObject(child_node)
	
	def writeObject(self, node):
		child_node = node.writeNewContainer("SipTransportConfig")
		child_node.writeInt("type", self.type)
		self.cfg.writeObject(child_node)
	 
	
class Application(ttk.Frame):
	"""
	The Application main frame.
	"""
	def __init__(self):
		ttk.Frame.__init__(self, name='application', width=300, height=500)
		self.pack(expand='yes', fill='both')
		self.master.title('pjsua2 Demo')
		self.master.geometry('500x500+100+100')
		
		# Logger
		self.logger = log.Logger()
		
		# Accounts
		self.accList = []
		
		# GUI variables
		self.showLogWindow = tk.IntVar()
		self.showLogWindow.set(0)
		self.quitting = False 
		
		# Construct GUI
		self._createWidgets()
		
		# Log window
		self.logWindow = log.LogWindow(self)
		self._onMenuShowHideLogWindow()
		
		# Instantiate endpoint
		self.ep = endpoint.Endpoint()
		self.ep.libCreate()
		
		# Default config
		self.epCfg = pj.EpConfig()
		self.epCfg.uaConfig.threadCnt = 0;
		self.epCfg.logConfig.writer = self.logger
		self.epCfg.logConfig.filename = "pygui.log"
		self.epCfg.logConfig.fileFlags = pj.PJ_O_APPEND
		self.epCfg.logConfig.level = 5
		self.epCfg.logConfig.consoleLevel = 5
		
		self.transportCfgs = []
		t = SipTransportConfig()
		t.type = pj.PJSIP_TRANSPORT_UDP
		t.cfg.port = 0
		self.transportCfgs.append(t)
		t = SipTransportConfig()
		t.type = pj.PJSIP_TRANSPORT_TCP
		t.cfg.port = 0
		self.transportCfgs.append(t)
		
	
	def saveConfig(self, filename='pygui.js'):
		json = pj.JsonDocument()
		
		# Write endpoint config
		json.writeObject(self.epCfg)
		
		# Write transport config
		node = json.writeNewArray("transports")
		for t in self.transportCfgs:
			t.writeObject(node);
		
		# Write account configs
		node = json.writeNewArray("accounts")
		for acc in self.accList:
			acc_node = node.writeNewContainer("Account")
			acc_node.writeObject(acc.cfg);

			# Write buddy configs
			buddy_node = acc_node.writeNewArray("buddies")
			for bud in acc.buddyList:
				buddy_node.writeObject(bud.cfg)
				
		json.saveFile(filename)
	
	def start(self, cfg_file='pygui.js'):
		# Load config
		acc_cfgs = []
		if cfg_file and os.path.exists(cfg_file):
			json = pj.JsonDocument()
			json.loadFile(cfg_file)
			
			# Load endpoint config
			json.readObject(self.epCfg)
			
			# Load transport configs
			node = json.readArray("transports")
			if node.hasUnread():
				self.transportCfgs = []
				while node.hasUnread():
					t = SipTransportConfig()
					t.readObject(node)
					self.transportCfgs.append(t)
			
			# Load account configs
			node = json.readArray("accounts")
			while node.hasUnread():
				acc_node = node.readContainer("Account")
				acc_cfg = pj.AccountConfig()
				acc_cfg.readObject(acc_node)
		
				# Load buddy configs
				buddy_cfgs = []
				buddy_node = acc_node.readArray("buddies")
				while buddy_node.hasUnread():
					buddy_cfg = pj.BuddyConfig()
					buddy_cfg.readObject(buddy_node)
					buddy_cfgs.append(buddy_cfg)

				acc_cfgs.append((acc_cfg, buddy_cfgs))
				
				
		# Initialize library
		self.epCfg.uaConfig.userAgent = "pygui-" + self.ep.libVersion().full;
		self.ep.libInit(self.epCfg)
		self.master.title('pjsua2 Demo version ' + self.ep.libVersion().full)
		
		# Create transports
		for t in self.transportCfgs:
			self.ep.transportCreate(t.type, t.cfg)
			
		# Add accounts
		for cfg, buddy_cfgs in acc_cfgs:
			self._createAcc(cfg)
			acc = self.accList[-1]
			for buddy_cfg in buddy_cfgs:
				self._createBuddy(acc, buddy_cfg)
				
		# Start library
		self.ep.libStart()
		
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
			self.tv.insert('', 'end',  iid, open=True, text=text, values=values)
		
	def updateBuddy(self, bud):
		iid = 'buddy' + str(bud.randId)
		text = bud.cfg.uri
		status = bud.statusText()
		
		values = (status,)
		if self.tv.exists(iid):
			self.tv.item(iid, text=text, values=values)
		else:
			self.tv.insert(str(bud.account.randId), 'end',  iid, open=True, text=text, values=values)
		
	def _createAcc(self, acc_cfg):
		acc = account.Account(self)
		acc.cfg = acc_cfg
		self.accList.append(acc)
		self.updateAccount(acc)
		acc.create(acc.cfg)
		acc.cfgChanged = False
		self.updateAccount(acc)
				
	def _createBuddy(self, acc, buddy_cfg):
		bud = buddy.Buddy(self)
		bud.cfg = buddy_cfg
		bud.account = acc
		bud.create(acc, bud.cfg)
		self.updateBuddy(bud)
		acc.buddyList.append(bud)

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

		# Create Account context menu
		self.accMenu = tk.Menu(top, tearoff=False)
		# Labels, must match with _onAccContextMenu()
		labels = ['Unregister', 'Reregister', 'Add buddy...', '-',
			  'Online', 'Invisible', 'Away', 'Busy', '-',
			  'Settings...', '-',
			  'Delete...']
		for label in labels:
			if label=='-':
				self.accMenu.add_separator()
			else:
				cmd = lambda arg=label: self._onAccContextMenu(arg)
				self.accMenu.add_command(label=label, command=cmd)
		
		# Create Buddy context menu
		# Labels, must match with _onAccContextMenu()
		self.buddyMenu = tk.Menu(top, tearoff=False)
		labels = ['Video call', 'Audio call', 'Send instant message', '-',
			  'Settings...', '-',
			  'Delete...']
		
		for label in labels:
			if label=='-':
				self.buddyMenu.add_separator()
			else:
				cmd = lambda arg=label: self._onBuddyContextMenu(arg)
				self.buddyMenu.add_command(label=label, command=cmd)
		
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
	
	def _getSelectedBuddy(self):
		items = self.tv.selection()
		if not items:
			return None
		try:
			iid = int(items[0][5:])
			iid_parent = int(self.tv.parent(items[0]))
		except:
			return None
			
		accs = [acc for acc in self.accList if acc.randId==iid_parent]
		if not accs:
			return None
			
		buds = [b for b in accs[0].buddyList if b.randId==iid]
		if not buds:
			return None
			
		return buds[0]
	
	def _onTvRightClick(self, event):
		iid = self.tv.identify_row(event.y)
		#iid = self.tv.identify('item', event.x, event.y)
		if iid:
			self.tv.selection_set( (iid,) )
			acc = self._getSelectedAccount()
			if acc:
				self.accMenu.post(event.x_root, event.y_root)
			else:
				# A buddy is selected
				self.buddyMenu.post(event.x_root, event.y_root)
	
	def _onAccContextMenu(self, label):
		acc = self._getSelectedAccount()
		if not acc:
			return
		
		if label=='Unregister':
			acc.setRegistration(False)
		elif label=='Reregister':
			acc.setRegistration(True)
		elif label=='Online':
			ps = pj.PresenceStatus()
			ps.isOnline = True
			acc.setOnlineStatus(ps)
		elif label=='Invisible':
			ps = pj.PresenceStatus()
			ps.isOnline = False
			acc.setOnlineStatus(ps)
		elif label=='Away':
			ps = pj.PresenceStatus()
			ps.isOnline = True
			ps.activity = pj.PJRPID_ACTIVITY_AWAY
			ps.note = "Away"
			acc.setOnlineStatus(ps)
		elif label=='Busy':
			ps = pj.PresenceStatus()
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
		elif label=='Add buddy...':
			cfg = pj.BuddyConfig()
			dlg = buddy.SettingDialog(self.master, cfg)
			if dlg.doModal():
				self._createBuddy(acc, cfg)
		else:
			assert not ("Unknown menu " + label)
	
	def _onBuddyContextMenu(self, label):
		bud = self._getSelectedBuddy()
		if not bud:
			return
		acc = bud.account
			
		if label=='Video call':
			pass
		elif label=='Audio call':
			pass
		elif label=='Send instant message':
			pass
		elif label=='Settings...':
			subs = bud.cfg.subscribe
			uri  = bud.cfg.uri
			dlg = buddy.SettingDialog(self.master, bud.cfg)
			if dlg.doModal():
				self.updateBuddy(bud)
				# URI updated?
				if uri != bud.cfg.uri:
					cfg = bud.cfg
					# del old
					iid = 'buddy' + str(bud.randId)
					acc.buddyList.remove(bud)
					del bud
					self.tv.delete( (iid,) )
					# add new
					self._createBuddy(acc, cfg)
				# presence subscribe setting updated
				elif subs != bud.cfg.subscribe:
					bud.subscribePresence(bud.cfg.subscribe)
		elif label=='Delete...':
			msg = "Do you really want to delete buddy '%s'?" % bud.cfg.uri
			if msgbox.askquestion('Delete buddy?', msg, default=msgbox.NO) != u'yes':
				return
			iid = 'buddy' + str(bud.randId)
			acc.buddyList.remove(bud)
			del bud
			self.tv.delete( (iid,) )
		else:
			assert not ("Unknown menu " + label)
			
	def _onTimer(self):
		if not self.quitting:
			self.ep.libHandleEvents(10)
			if not self.quitting:
				self.master.after(50, self._onTimer)
			
	def _onClose(self):
		self.saveConfig()
		self.quitting = True
		self.ep.libDestroy()
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
		
if __name__ == '__main__':
	main()
