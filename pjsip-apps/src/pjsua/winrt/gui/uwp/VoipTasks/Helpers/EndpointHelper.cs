/* $Id*/
/*
* Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

using System;
using System.Diagnostics;
using System.Threading.Tasks;
using VoipTasks.BackgroundOperations;
using Windows.ApplicationModel.Calls;
using Windows.Foundation.Collections;
using VoipBackEnd;

namespace VoipTasks.Helpers
{
    class EndpointHelper : IntAccount, IntCall
    {        
        private async void sendCallState(String state)
        {            
            ValueSet message = new ValueSet();

            message[UpdateCallStateArguments.CallState.ToString()] = state;
            message[ForegroundOperation.NewForegroundRequest] = (int)ForegroundReguest.UpdateCallState;

            ValueSet response = await Current.SendMessageAsync(message);
        }

        private async void sendRegState(String state)
        {
            ValueSet message = new ValueSet();

            message[UpdateRegStateArguments.RegState.ToString()] = state;
            message[ForegroundOperation.NewForegroundRequest] = (int)ForegroundReguest.UpdateRegState;

            ValueSet response = await Current.SendMessageAsync(message);
        }

        /* Implement IntAccount. */
        public void onIncomingCall(CallInfoRT info)
        {
            if (Current.VoipCall != null)
            {
                /* Only one active call */
                return;
            }

            Current.RequestNewIncomingCall(info.remoteContact,
                                           info.remoteUri,
                                           "Pjsua");
        }

        public void onRegState(OnRegStateParamRT prm)
        {
            sendRegState(prm.reason);
        }

        /* Implement IntCall. */
        public void onCallState(CallInfoRT info)
        {
            switch (info.state) {
                case INV_STATE.PJSIP_INV_STATE_CALLING:
                    sendCallState("Calling");
                    break;
                case INV_STATE.PJSIP_INV_STATE_INCOMING:
                    sendCallState("Incoming");
                    break;
                case INV_STATE.PJSIP_INV_STATE_CONNECTING:
                    sendCallState("Connecting");
                    break;
                case INV_STATE.PJSIP_INV_STATE_CONFIRMED:
                    sendCallState("Connected");
                    break;
                case INV_STATE.PJSIP_INV_STATE_DISCONNECTED:                    
                    sendCallState("Disconnected");
                    Current.EndCall();
                    break;
            }
        }
    }
}
