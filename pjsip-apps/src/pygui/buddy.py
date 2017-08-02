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
import endpoint
import application

# Buddy class
class Buddy(pj.Buddy):
    """
    High level Python Buddy object, derived from pjsua2's Buddy object.
    """
    def __init__(self, app):
        pj.Buddy.__init__(self)
        self.app = app
        self.randId = random.randint(1, 9999)
        self.cfg = None
        self.account = None

    def statusText(self):
        bi = self.getInfo()
        status = ''
        if bi.subState == pj.PJSIP_EVSUB_STATE_ACTIVE:
            if bi.presStatus.status == pj.PJSUA_BUDDY_STATUS_ONLINE:
                status = bi.presStatus.statusText
                if not status:
                    status = 'Online'
            elif bi.presStatus.status == pj.PJSUA_BUDDY_STATUS_OFFLINE:
                status = 'Offline'
            else:
                status = 'Unknown'
        return status

    def onBuddyState(self):
        self.app.updateBuddy(self)

class SettingDialog(tk.Toplevel):
    """
    This implements buddy settings dialog to manipulate buddy settings.
    """
    def __init__(self, parent, cfg):
        tk.Toplevel.__init__(self, parent)
        self.transient(parent)
        self.parent = parent
        self.geometry("+100+100")
        self.title('Buddy settings')

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
        self.cfgUri = tk.StringVar()
        self.cfgUri.set( self.cfg.uri )
        self.cfgSubscribe = tk.IntVar()
        self.cfgSubscribe.set(self.cfg.subscribe)

        # Build the tab page
        frm = ttk.Frame(self.frm)
        frm.columnconfigure(0, weight=1)
        frm.columnconfigure(1, weight=2)
        row = 0
        ttk.Label(frm, text='URI:').grid(row=row, column=0, sticky=tk.E, pady=2)
        ttk.Entry(frm, textvariable=self.cfgUri, width=40).grid(row=row, column=1, sticky=tk.W+tk.E, padx=6)
        row += 1
        ttk.Checkbutton(frm, text='Subscribe presence', variable=self.cfgSubscribe).grid(row=row, column=1, sticky=tk.W, padx=6, pady=2)

        self.wTab.add(frm, text='Basic Settings')


    def onOk(self):
        # Check basic settings
        errors = "";
        if self.cfgUri.get():
            if not endpoint.validateSipUri(self.cfgUri.get()):
                errors += "Invalid Buddy URI: '%s'\n" % (self.cfgUri.get())

        if errors:
            msgbox.showerror("Error detected:", errors)
            return

        # Basic settings
        self.cfg.uri = self.cfgUri.get()
        self.cfg.subscribe = self.cfgSubscribe.get()

        self.isOk = True
        self.destroy()

    def onCancel(self):
        self.destroy()


if __name__ == '__main__':
    application.main()
