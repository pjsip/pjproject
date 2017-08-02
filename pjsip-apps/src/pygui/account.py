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
import _pjsua2
import accountsetting
import application
import call
import chat as ch

write=sys.stdout.write

# Account class
class Account(pj.Account):
    """
    High level Python Account object, derived from pjsua2's Account object.
    """
    def __init__(self, app):
        pj.Account.__init__(self)
        self.app = app
        self.randId = random.randint(1, 9999)
        self.cfg =  pj.AccountConfig()
        self.cfgChanged = False
        self.buddyList = []
        self.chatList = []
        self.deleting = False

    def findChat(self, uri_str):
        uri = ch.ParseSipUri(uri_str)
        if not uri: return None

        for chat in self.chatList:
            if chat.isUriParticipant(uri) and chat.isPrivate():
                return chat
        return None

    def newChat(self, uri_str):
        uri = ch.ParseSipUri(uri_str)
        if not uri: return None

        chat = ch.Chat(self.app, self, uri)
        self.chatList.append(chat)
        self.app.updateWindowMenu()
        return chat

    def statusText(self):
        status = '?'
        if self.isValid():
            ai = self.getInfo()
            if ai.regLastErr:
                status = self.app.ep.utilStrError(ai.regLastErr)
            elif ai.regIsActive:
                if ai.onlineStatus:
                    if len(ai.onlineStatusText):
                        status = ai.onlineStatusText
                    else:
                        status = "Online"
                else:
                    status = "Registered"
            else:
                if ai.regIsConfigured:
                    if ai.regStatus/100 == 2:
                        status = "Unregistered"
                    else:
                        status = ai.regStatusText
                else:
                    status = "Doesn't register"
        else:
            status = '- not created -'
        return status

    def onRegState(self, prm):
        self.app.updateAccount(self)

    def onIncomingCall(self, prm):
        c = call.Call(self, call_id=prm.callId)
        call_prm = pj.CallOpParam()
        call_prm.statusCode = 180
        c.answer(call_prm)
        ci = c.getInfo()
        msg = "Incoming call for account '%s'" % self.cfg.idUri
        if msgbox.askquestion(msg, "Accept call from '%s'?" % (ci.remoteUri), default=msgbox.YES) == u'yes':
            call_prm.statusCode = 200
            c.answer(call_prm)

            # find/create chat instance
            chat = self.findChat(ci.remoteUri)
            if not chat: chat = self.newChat(ci.remoteUri)

            chat.showWindow()
            chat.registerCall(ci.remoteUri, c)
            chat.updateCallState(c, ci)
        else:
            c.hangup(call_prm)

    def onInstantMessage(self, prm):
        chat = self.findChat(prm.fromUri)
        if not chat: chat = self.newChat(prm.fromUri)

        chat.showWindow()
        chat.addMessage(prm.fromUri, prm.msgBody)

    def onInstantMessageStatus(self, prm):
        if prm.code/100 == 2: return

        chat = self.findChat(prm.toUri)
        if not chat:
            write("=== IM status to " + prm.toUri + "cannot find chat\r\n")
            return

        chat.addMessage(None, "Failed sending message to '%s': %s" % (prm.toUri, prm.reason))

    def onTypingIndication(self, prm):
        chat = self.findChat(prm.fromUri)
        if not chat:
            write("=== Incoming typing indication from " + prm.fromUri + "cannot find chat\r\n")
            return

        chat.setTypingIndication(prm.fromUri, prm.isTyping)


# Account frame, to list accounts
class AccountListFrame(ttk.Frame):
    """
    This implements a Frame which contains account list and buttons to operate
    on them (Add, Modify, Delete, etc.).
    """
    def __init__(self, parent, app, acc_list = []):
        ttk.Frame.__init__(self, parent, name='acclist')
        self.app = app
        self.accList = acc_list
        self.accDeletedList = []
        self.pack(expand='yes', fill='both')
        self._createWidgets()
        for acc in self.accList:
            self._showAcc(acc)

    def _createWidgets(self):
        self.tv = ttk.Treeview(self, columns=('ID', 'Registrar', 'Default'), selectmode='browse')
        self.tv.heading('#0', text='Priority')
        self.tv.heading(0, text='ID')
        self.tv.heading(1, text='Registrar')
        self.tv.heading(2, text='Default?')
        self.tv.column('#0', width=60)
        self.tv.column(0, width=300)
        self.tv.column(1, width=200)
        self.tv.column(2, width=60)
        self.tv.grid(column=0, row=0, rowspan=4, padx=5, pady=5)

        ttk.Button(self, text='Add..', command=self._onBtnAdd).grid(column=1, row=0, padx=5)
        ttk.Button(self, text='Settings..', command=self._onBtnSettings).grid(column=1, row=1)
        ttk.Button(self, text='Set Default', command=self._onBtnSetDefault).grid(column=1, row=2)
        ttk.Button(self, text='Delete..', command=self._onBtnDelete).grid(column=1, row=3)

    def _showAcc(self, acc):
        is_default = 'Yes' if acc.isValid() and acc.isDefault() else ''
        values = (acc.cfg.idUri, acc.cfg.regConfig.registrarUri, is_default)
        self.tv.insert('', 0, str(acc.randId), open=True, text=str(acc.cfg.priority), values=values)

    def updateAccount(self, acc):
        is_default = 'Yes' if acc.isValid() and acc.isDefault() else ''
        values = (acc.cfg.idUri, acc.cfg.regConfig.registrarUri, is_default)
        self.tv.item(str(acc.randId), text=str(acc.cfg.priority), values=values)

    def _getSelectedAcc(self):
        items = self.tv.selection()
        if not items:
            return None
        iid = int(items[0])
        return [acc for acc in self.accList if acc.randId==iid][0]

    def _onBtnAdd(self):
        cfg = pj.AccountConfig()
        dlg = accountsetting.Dialog(self.master, cfg)
        if dlg.doModal():
            acc = Account(self.app)
            acc.cfg = cfg
            self._showAcc(acc)
            self.accList.append(acc)
            self.cfgChanged = True

    def _onBtnSettings(self):
        acc = self._getSelectedAcc()
        if not acc:
            return
        dlg = accountsetting.Dialog(self.master, acc.cfg)
        if dlg.doModal():
            self.updateAccount(acc)
            self.cfgChanged = True

    def _onBtnDelete(self):
        acc = self._getSelectedAcc()
        if not acc:
            return
        msg = "Do you really want to delete account '%s'" % acc.cfg.idUri
        if msgbox.askquestion('Delete account?', msg, default=msgbox.NO) != u'yes':
            return
        self.accList.remove(acc)
        self.accDeletedList.append(acc)
        self.tv.delete( (str(acc.randId),) )

    def _onBtnSetDefault(self):
        acc = self._getSelectedAcc()
        if not acc:
            return
        if acc.isValid():
            acc.setDefault()
        for acc in self.accList:
            self.updateAccount(acc)


if __name__ == '__main__':
    application.main()
