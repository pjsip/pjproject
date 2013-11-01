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


class Endpoint(pj.Endpoint):
	"""
	This is high level Python object inherited from pj.Endpoint
	"""
	instance = None
	def __init__(self):
		pj.Endpoint.__init__(self)
		Endpoint.instance = self
	
	def startLib(self, ep_cfg):
		# Create lib
		self.libCreate()
		
		# Init lib
		self.libInit(ep_cfg)
	
		# Add transport
		tcfg = pj.TransportConfig()
		tcfg.port = 50060;
		self.transportCreate(pj.PJSIP_TRANSPORT_UDP, tcfg)
		
		# Start!
		self.libStart()
		
	def stopLib(self):
		self.libDestroy()
	
def validateUri(uri):
	return Endpoint.instance.utilVerifyUri(uri) == pj.PJ_SUCCESS

def validateSipUri(uri):
	return Endpoint.instance.utilVerifySipUri(uri) == pj.PJ_SUCCESS


if __name__ == '__main__':
	application.main()
