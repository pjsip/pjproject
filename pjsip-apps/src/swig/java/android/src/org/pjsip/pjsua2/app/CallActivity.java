/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
package org.pjsip.pjsua2.app;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.app.Activity;

import org.pjsip.pjsua2.*;

public class CallActivity extends Activity implements Handler.Callback {
	
	public static Handler handler_;

	private final Handler handler = new Handler(this);
	private static CallInfo lastCallInfo;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_call);
		
		handler_ = handler;
		if (MainActivity.currentCall != null) {
			try {
				lastCallInfo = MainActivity.currentCall.getInfo();
				updateCallState(lastCallInfo);
			} catch (Exception e) {
				System.out.println(e);
			}
		} else {
			updateCallState(lastCallInfo);
		}
	}

    @Override
    protected void onDestroy() {
    	super.onDestroy();
    	handler_ = null;
    }
	
	public void acceptCall(View view) {
		CallOpParam prm = new CallOpParam();
		prm.setStatusCode(pjsip_status_code.PJSIP_SC_OK);
		try {
			MainActivity.currentCall.answer(prm);
		} catch (Exception e) {
			System.out.println(e);
		}
		
		view.setVisibility(View.GONE);
	}

	public void hangupCall(View view) {
		handler_ = null;
		finish();
		
		if (MainActivity.currentCall != null) {
			CallOpParam prm = new CallOpParam();
			prm.setStatusCode(pjsip_status_code.PJSIP_SC_DECLINE);
			try {
				MainActivity.currentCall.hangup(prm);
			} catch (Exception e) {
				System.out.println(e);
			}

			MainActivity.currentCall = null;
		}
	}
	
	@Override
	public boolean handleMessage(Message m) {
		
		if (m.what == MainActivity.MSG_TYPE.CALL_STATE) {
			
			lastCallInfo = (CallInfo) m.obj;
			updateCallState(lastCallInfo);
			
		} else {
			
			/* Message not handled */
			return false;
			
		}
			
		return true;
	}
	
	private void updateCallState(CallInfo ci) {
		TextView tvPeer  = (TextView) findViewById(R.id.textViewPeer);
		TextView tvState = (TextView) findViewById(R.id.textViewCallState);
		Button buttonHangup = (Button) findViewById(R.id.buttonHangup);
		Button buttonAccept = (Button) findViewById(R.id.buttonAccept);
		String call_state = "";
		
		if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAC) {
			buttonAccept.setVisibility(View.GONE);
		}
				
		if (ci.getState().swigValue() < pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED.swigValue())
		{
			if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAS) {
				call_state = "Incoming call..";
				/* Default button texts are already 'Accept' & 'Reject' */
			} else {
				buttonHangup.setText("Cancel");
				call_state = ci.getStateText();
			}
		}
		else if (ci.getState().swigValue() >= pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED.swigValue())
		{
			buttonAccept.setVisibility(View.GONE);
			call_state = ci.getStateText();
			if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
				buttonHangup.setText("Hangup");
			} else if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
				buttonHangup.setText("OK");
				call_state = "Call disconnected: " + ci.getLastReason();
				MainActivity.currentCall = null;
			}
		}
		
		tvPeer.setText(ci.getRemoteUri());
		tvState.setText(call_state);
	}
}
