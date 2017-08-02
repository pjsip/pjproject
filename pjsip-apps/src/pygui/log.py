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
import application

write=sys.stdout.write

class LogWindow(tk.Toplevel):
    """
    Log window
    """
    instance = None
    def __init__(self, app):
        tk.Toplevel.__init__(self, name='logwnd', width=640, height=480)
        LogWindow.instance = self
        self.app = app
        self.state('withdrawn')
        self.title('Log')
        self._createWidgets()
        self.protocol("WM_DELETE_WINDOW", self._onHide)

    def addLog(self, entry):
        """entry fields:
            int		level;
            string	msg;
            long	threadId;
            string	threadName;
        """
        self.addLog2(entry.level, entry.msg)

    def addLog2(self, level, msg):
        if level==5:
            tags = ('trace',)
        elif level==3:
            tags = ('info',)
        elif level==2:
            tags = ('warning',)
        elif level<=1:
            tags = ('error',)
        else:
            tags = None
        self.text.insert(tk.END, msg, tags)
        self.text.see(tk.END)

    def _createWidgets(self):
        self.rowconfigure(0, weight=1)
        self.rowconfigure(1, weight=0)
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=0)

        self.text = tk.Text(self, font=('Courier New', '8'), wrap=tk.NONE, undo=False, padx=4, pady=5)
        self.text.grid(row=0, column=0, sticky='nswe', padx=5, pady=5)

        scrl = ttk.Scrollbar(self, orient=tk.VERTICAL, command=self.text.yview)
        self.text.config(yscrollcommand=scrl.set)
        scrl.grid(row=0, column=1, sticky='nsw', padx=5, pady=5)

        scrl = ttk.Scrollbar(self, orient=tk.HORIZONTAL, command=self.text.xview)
        self.text.config(xscrollcommand=scrl.set)
        scrl.grid(row=1, column=0, sticky='we', padx=5, pady=5)

        self.text.bind("<Key>", self._onKey)

        self.text.tag_configure('normal', font=('Courier New', '8'), foreground='black')
        self.text.tag_configure('trace', font=('Courier New', '8'), foreground='#777777')
        self.text.tag_configure('info', font=('Courier New', '8', 'bold'), foreground='black')
        self.text.tag_configure('warning', font=('Courier New', '8', 'bold'), foreground='cyan')
        self.text.tag_configure('error', font=('Courier New', '8', 'bold'), foreground='red')

    def _onKey(self, event):
        # Ignore key event to make text widget read-only
        return "break"

    def _onHide(self):
        # Hide when close ('x') button is clicked
        self.withdraw()
        self.app.showLogWindow.set(0)


def writeLog2(level, msg):
    if LogWindow.instance:
        LogWindow.instance.addLog2(level, msg)

def writeLog(entry):
    if LogWindow.instance:
        LogWindow.instance.addLog(entry)

class Logger(pj.LogWriter):
    """
    Logger to receive log messages from pjsua2
    """
    def __init__(self):
        pj.LogWriter.__init__(self)

    def write(self, entry):
        write(entry.msg + "\r\n")
        writeLog(entry)

if __name__ == '__main__':
    application.main()
