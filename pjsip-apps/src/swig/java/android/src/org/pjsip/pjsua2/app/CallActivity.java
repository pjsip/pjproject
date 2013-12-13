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
import android.widget.EditText;
import android.widget.TextView;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;

import org.pjsip.pjsua2.*;

public class CallActivity extends Activity implements Handler.Callback {
	
	public final Handler handler = new Handler(this);
	public static Handler handler_;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_call);
		
		handler_ = handler;
		MyCall call = MainActivity.currentCall;
		
		final TextView tvPeer  = (TextView) findViewById(R.id.textViewPeer);
		final TextView tvState = (TextView) findViewById(R.id.textViewCallState);
		final Button buttonAccept = (Button) findViewById(R.id.buttonAccept);
		final Button buttonHangup = (Button) findViewById(R.id.buttonHangup);

		String remote_uri = "Somebody";
		String call_state = "";
		try {
			CallInfo ci = call.getInfo();
			remote_uri = ci.getRemoteUri();
			if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAS) {
				call_state = "Incoming call..";
			} else {
				buttonAccept.setVisibility(View.GONE);
				buttonHangup.setText("Cancel");
				call_state = ci.getStateText();
			}
		} catch (Exception e) {
			System.out.println(e);
		}
		
		tvPeer.setText(remote_uri);
		tvState.setText(call_state);		
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
			
			TextView tvState = (TextView) findViewById(R.id.textViewCallState);
			Button buttonHangup = (Button) findViewById(R.id.buttonHangup);
			Button buttonAccept = (Button) findViewById(R.id.buttonAccept);
			
			CallInfo ci = (CallInfo) m.obj;
			String call_state = "";
			if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAC ||
				ci.getState().swigValue() >= pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED.swigValue())
			{
				call_state = ci.getStateText();
				tvState.setText(call_state);
			}
			
			if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
				buttonHangup.setText("Hangup");
			}
			
			if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
				buttonHangup.setText("OK");
				buttonAccept.setVisibility(View.GONE);
				tvState.setText("Call disconnected: " + ci.getLastReason());
				MainActivity.currentCall = null;
			}
			
		} else {
			
			/* Message not handled */
			return false;
			
		}
			
		return true;
	}
	
	
}
