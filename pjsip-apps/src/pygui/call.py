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
import application

# Call class
class Call(pj.Call):
	"""
	High level Python Call object, derived from pjsua2's Call object.
	"""
	def __init__(self, app, acc, callId):
		pj.Call.__init__(self, acc, callId)
		self.app = app
                self.acc = acc
                self.uri = ''
                self.randId = random.randint(1, 9999)

	def statusText(self):
                uri = ''
		status = '?'
		if self.isActive():
			ci = self.getInfo()
                        status = ci.stateText
                        uri = ci.remoteURI
		else:
			status = '- not established -'
		return uri, status
	
	def onCallState(self, prm):
                ci = self.getInfo()
                if ci.state == pj.PJSIP_INV_STATE_DISCONNECTED:
                        iid = str(self.randId)
			self.acc.callList.remove(self)
			self.app.tv.delete( (iid,) )
                        del self
                else:
                        self.app.updateCall(self.acc)

if __name__ == '__main__':
	application.main()
