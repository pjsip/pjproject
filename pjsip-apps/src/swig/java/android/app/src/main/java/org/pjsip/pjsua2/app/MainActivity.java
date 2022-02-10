/* $Id: MainActivity.java 5022 2015-03-25 03:41:21Z nanang $ */
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

import android.content.IntentFilter;
import android.hardware.camera2.CameraManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.SimpleAdapter;
import android.widget.TextView;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.pjsip.PjCameraInfo2;
import org.pjsip.pjsua2.*;

public class MainActivity extends Activity
			  implements Handler.Callback, MyAppObserver
{
    public static MyApp app = null;
    public static MyCall currentCall = null;
    public static MyAccount account = null;
    public static AccountConfig accCfg = null;
    public static MyBroadcastReceiver receiver = null;
    public static IntentFilter intentFilter = null;

    private ListView buddyListView;
    private SimpleAdapter buddyListAdapter;
    private int buddyListSelectedIdx = -1;
    ArrayList<Map<String, String>> buddyList;
    private String lastRegStatus = "";

    private final Handler handler = new Handler(this);
    public class MSG_TYPE
    {
	public final static int INCOMING_CALL = 1;
	public final static int CALL_STATE = 2;
	public final static int REG_STATE = 3;
	public final static int BUDDY_STATE = 4;
	public final static int CALL_MEDIA_STATE = 5;
	public final static int CHANGE_NETWORK = 6;
    }

    private class MyBroadcastReceiver extends BroadcastReceiver {
	private String conn_name = "";

	@Override
	public void onReceive(Context context, Intent intent) {
	    if (isNetworkChange(context))
		notifyChangeNetwork();
	}

	private boolean isNetworkChange(Context context) {
	    boolean network_changed = false;
	    ConnectivityManager connectivity_mgr =
		((ConnectivityManager)context.getSystemService(
		    				 Context.CONNECTIVITY_SERVICE));

	    NetworkInfo net_info = connectivity_mgr.getActiveNetworkInfo();
	    if(net_info != null && net_info.isConnectedOrConnecting() &&
	       !conn_name.equalsIgnoreCase(""))
	    {
		String new_con = net_info.getExtraInfo();
		if (new_con != null && !new_con.equalsIgnoreCase(conn_name))
		    network_changed = true;

		conn_name = (new_con == null)?"":new_con;
	    } else {
		if (conn_name.equalsIgnoreCase(""))
		    conn_name = net_info.getExtraInfo();
	    }
	    return network_changed;
	}
    }

    private HashMap<String, String> putData(String uri, String status)
    {
	HashMap<String, String> item = new HashMap<String, String>();
	item.put("uri", uri);
	item.put("status", status);
	return item;
    }

    private void showCallActivity()
    {
	Intent intent = new Intent(this, CallActivity.class);
	intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
	startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
	super.onCreate(savedInstanceState);
	setContentView(R.layout.activity_main);

	CameraManager cm = (CameraManager)getSystemService(Context.CAMERA_SERVICE);
	PjCameraInfo2.SetCameraManager(cm);

	if (app == null) {
	    app = new MyApp();
	    // Wait for GDB to init, for native debugging only
	    if (false &&
		(getApplicationInfo().flags & 
		ApplicationInfo.FLAG_DEBUGGABLE) != 0)
	    {
		try {
		    Thread.sleep(5000);
		} catch (InterruptedException e) {}
	    }

	    app.init(this, getFilesDir().getAbsolutePath());
	}

	if (app.accList.size() == 0) {
	    accCfg = new AccountConfig();
	    accCfg.setIdUri("sip:localhost");
	    accCfg.getNatConfig().setIceEnabled(true);
	    accCfg.getVideoConfig().setAutoTransmitOutgoing(true);
	    accCfg.getVideoConfig().setAutoShowIncoming(true);
	    account = app.addAcc(accCfg);
	} else {
	    account = app.accList.get(0);
	    accCfg = account.cfg;
	}

	buddyList = new ArrayList<Map<String, String>>();
	for (int i = 0; i < account.buddyList.size(); i++) {
	    buddyList.add(putData(account.buddyList.get(i).cfg.getUri(),
				  account.buddyList.get(i).getStatusText()));
	}

	String[] from = { "uri", "status" };
	int[] to = { android.R.id.text1, android.R.id.text2 };
	buddyListAdapter = new SimpleAdapter(
					this, buddyList,
					android.R.layout.simple_list_item_2,
					from, to);

	buddyListView = (ListView) findViewById(R.id.listViewBuddy);;
	buddyListView.setAdapter(buddyListAdapter);
	buddyListView.setOnItemClickListener(
	    new AdapterView.OnItemClickListener()
	    {
		@Override
		public void onItemClick(AdapterView<?> parent,
					final View view,
					int position, long id) 
		{
		    view.setSelected(true);
		    buddyListSelectedIdx = position;
		}
	    }
	);
	if (receiver == null) {
	    receiver = new MyBroadcastReceiver();
	    intentFilter = new IntentFilter(
			               ConnectivityManager.CONNECTIVITY_ACTION);
	    registerReceiver(receiver, intentFilter);
	}
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
	// Inflate the menu; this adds items to the action bar
	// if it is present.
	getMenuInflater().inflate(R.menu.main, menu);
	return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
	switch (item.getItemId()) {
	    case R.id.action_acc_config:
		dlgAccountSetting();
		break;

	    case R.id.action_quit:
		Message m = Message.obtain(handler, 0);
		m.sendToTarget();
		break;

	    default:
		break;
	}

	return true;
    } 	

    @Override
    public boolean handleMessage(Message m)
    {
	if (m.what == 0) {

	    app.deinit();
	    finish();
	    Runtime.getRuntime().gc();
	    android.os.Process.killProcess(android.os.Process.myPid());

	} else if (m.what == MSG_TYPE.CALL_STATE) {

	    CallInfo ci = (CallInfo) m.obj;

	    if (currentCall == null || ci == null || ci.getId() != currentCall.getId()) {
		System.out.println("Call state event received, but call info is invalid");
		return true;
	    }

	    /* Forward the call info to CallActivity */
	    if (CallActivity.handler_ != null) {
		Message m2 = Message.obtain(CallActivity.handler_, MSG_TYPE.CALL_STATE, ci);
		m2.sendToTarget();
	    }

	    if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
	    {
		currentCall.delete();
		currentCall = null;
	    }

	} else if (m.what == MSG_TYPE.CALL_MEDIA_STATE) {

	    /* Forward the message to CallActivity */
	    if (CallActivity.handler_ != null) {
		Message m2 = Message.obtain(CallActivity.handler_,
		    MSG_TYPE.CALL_MEDIA_STATE,
		    null);
		m2.sendToTarget();
	    }

	} else if (m.what == MSG_TYPE.BUDDY_STATE) {

	    MyBuddy buddy = (MyBuddy) m.obj;
	    int idx = account.buddyList.indexOf(buddy);

	    /* Update buddy status text, if buddy is valid and
	    * the buddy lists in account and UI are sync-ed.
	    */
	    if (idx >= 0 && account.buddyList.size() == buddyList.size())
	    {
		buddyList.get(idx).put("status", buddy.getStatusText());
		buddyListAdapter.notifyDataSetChanged();
		// TODO: selection color/mark is gone after this,
		//       dont know how to return it back.
		//buddyListView.setSelection(buddyListSelectedIdx);
		//buddyListView.performItemClick(buddyListView,
		//				     buddyListSelectedIdx,
		//				     buddyListView.
		//		    getItemIdAtPosition(buddyListSelectedIdx));

		/* Return back Call activity */
		notifyCallState(currentCall);
	    }

	} else if (m.what == MSG_TYPE.REG_STATE) {

	    String msg_str = (String) m.obj;
	    lastRegStatus = msg_str;

	} else if (m.what == MSG_TYPE.INCOMING_CALL) {

	    /* Incoming call */
	    final MyCall call = (MyCall) m.obj;
	    CallOpParam prm = new CallOpParam();

	    /* Only one call at anytime */
	    if (currentCall != null) {
		/*
		prm.setStatusCode(pjsip_status_code.PJSIP_SC_BUSY_HERE);
		try {
		call.hangup(prm);
		} catch (Exception e) {}
		*/
		// TODO: set status code
		call.delete();
		return true;
	    }

	    /* Answer with ringing */
	    prm.setStatusCode(pjsip_status_code.PJSIP_SC_RINGING);
	    try {
		call.answer(prm);
	    } catch (Exception e) {}

	    currentCall = call;
	    showCallActivity();

	} else if (m.what == MSG_TYPE.CHANGE_NETWORK) {
	    app.handleNetworkChange();
	} else {

	    /* Message not handled */
	    return false;

	}

	return true;
    }


    private void dlgAccountSetting()
    {
	LayoutInflater li = LayoutInflater.from(this);
	View view = li.inflate(R.layout.dlg_account_config, null);

	if (lastRegStatus.length()!=0) {
	    TextView tvInfo = (TextView)view.findViewById(R.id.textViewInfo);
	    tvInfo.setText("Last status: " + lastRegStatus);
	}

	AlertDialog.Builder adb = new AlertDialog.Builder(this);
	adb.setView(view);
	adb.setTitle("Account Settings");

	final EditText etId    = (EditText)view.findViewById(R.id.editTextId);
	final EditText etReg   = (EditText)view.findViewById(R.id.editTextRegistrar);
	final EditText etProxy = (EditText)view.findViewById(R.id.editTextProxy);
	final EditText etUser  = (EditText)view.findViewById(R.id.editTextUsername);
	final EditText etPass  = (EditText)view.findViewById(R.id.editTextPassword);

	etId.   setText(accCfg.getIdUri());
	etReg.  setText(accCfg.getRegConfig().getRegistrarUri());
	StringVector proxies = accCfg.getSipConfig().getProxies();
	if (proxies.size() > 0)
	    etProxy.setText(proxies.get(0));
	else
	    etProxy.setText("");
	AuthCredInfoVector creds = accCfg.getSipConfig().getAuthCreds();
	if (creds.size() > 0) {
	    etUser. setText(creds.get(0).getUsername());
	    etPass. setText(creds.get(0).getData());
	} else {
	    etUser. setText("");
	    etPass. setText("");
	}

	adb.setCancelable(false);
	adb.setPositiveButton("OK",
	    new DialogInterface.OnClickListener()
	    {
		public void onClick(DialogInterface dialog,int id)
		{
		    String acc_id 	 = etId.getText().toString();
		    String registrar = etReg.getText().toString();
		    String proxy 	 = etProxy.getText().toString();
		    String username  = etUser.getText().toString();
		    String password  = etPass.getText().toString();

		    accCfg.setIdUri(acc_id);
		    accCfg.getRegConfig().setRegistrarUri(registrar);
		    AuthCredInfoVector creds = accCfg.getSipConfig().
							    getAuthCreds();
		    creds.clear();
		    if (username.length() != 0) {
			creds.add(new AuthCredInfo("Digest", "*", username, 0,
						   password));
		    }
		    StringVector proxies = accCfg.getSipConfig().getProxies();
		    proxies.clear();
		    if (proxy.length() != 0) {
			proxies.add(proxy);
		    }

		    /* Enable ICE */
		    accCfg.getNatConfig().setIceEnabled(true);

		    /* Finally */
		    lastRegStatus = "";
		    try {
			account.modify(accCfg);
		    } catch (Exception e) {}
		}
	    }
	);
	adb.setNegativeButton("Cancel",
	    new DialogInterface.OnClickListener()
	    {
		public void onClick(DialogInterface dialog,int id)
		{
		    dialog.cancel();
		}
	    }
	);

	AlertDialog ad = adb.create();
	ad.show();
    }


    public void makeCall(View view)
    {
	if (buddyListSelectedIdx == -1)
	    return;

	/* Only one call at anytime */
	if (currentCall != null) {
	    return;
	}

	HashMap<String, String> item = (HashMap<String, String>) buddyListView.
				       getItemAtPosition(buddyListSelectedIdx);
	String buddy_uri = item.get("uri");

	MyCall call = new MyCall(account, -1);
	CallOpParam prm = new CallOpParam(true);

	try {
	    call.makeCall(buddy_uri, prm);
	} catch (Exception e) {
	    call.delete();
	    return;
	}

	currentCall = call;
	showCallActivity();
    }

    private void dlgAddEditBuddy(BuddyConfig initial)
    {
	final BuddyConfig cfg = new BuddyConfig();
	final BuddyConfig old_cfg = initial;
	final boolean is_add = initial == null;

	LayoutInflater li = LayoutInflater.from(this);
	View view = li.inflate(R.layout.dlg_add_buddy, null);

	AlertDialog.Builder adb = new AlertDialog.Builder(this);
	adb.setView(view);

	final EditText etUri  = (EditText)view.findViewById(R.id.editTextUri);
	final CheckBox cbSubs = (CheckBox)view.findViewById(R.id.checkBoxSubscribe);

	if (is_add) {
	    adb.setTitle("Add Buddy");
	} else {
	    adb.setTitle("Edit Buddy");
	    etUri. setText(initial.getUri());
	    cbSubs.setChecked(initial.getSubscribe());
	}

	adb.setCancelable(false);
	adb.setPositiveButton("OK",
	    new DialogInterface.OnClickListener()
	    {
		public void onClick(DialogInterface dialog,int id)
		{
		    cfg.setUri(etUri.getText().toString());
		    cfg.setSubscribe(cbSubs.isChecked());

		    if (is_add) {
			account.addBuddy(cfg);
			buddyList.add(putData(cfg.getUri(), ""));
			buddyListAdapter.notifyDataSetChanged();
			buddyListSelectedIdx = -1;
		    } else {
			if (!old_cfg.getUri().equals(cfg.getUri())) {
			    account.delBuddy(buddyListSelectedIdx);
			    account.addBuddy(cfg);
			    buddyList.remove(buddyListSelectedIdx);
			    buddyList.add(putData(cfg.getUri(), ""));
			    buddyListAdapter.notifyDataSetChanged();
			    buddyListSelectedIdx = -1;
			} else if (old_cfg.getSubscribe() != 
				   cfg.getSubscribe())
			{
			    MyBuddy bud = account.buddyList.get(
							buddyListSelectedIdx);
			    try {
				bud.subscribePresence(cfg.getSubscribe());
			    } catch (Exception e) {}
			}
		    }
		}
	    }
	);
	adb.setNegativeButton("Cancel",
	    new DialogInterface.OnClickListener()
	    {
		public void onClick(DialogInterface dialog,int id) {
		    dialog.cancel();
		}
	    }
	);

	AlertDialog ad = adb.create();
	ad.show();
    }

    public void addBuddy(View view)
    {
	dlgAddEditBuddy(null);
    }

    public void editBuddy(View view)
    {
	if (buddyListSelectedIdx == -1)
	    return;

	BuddyConfig old_cfg = account.buddyList.get(buddyListSelectedIdx).cfg;
	dlgAddEditBuddy(old_cfg);
    }

    public void delBuddy(View view) {
	if (buddyListSelectedIdx == -1)
	    return;

	final HashMap<String, String> item = (HashMap<String, String>)
			buddyListView.getItemAtPosition(buddyListSelectedIdx);
	String buddy_uri = item.get("uri");

	DialogInterface.OnClickListener ocl =
					new DialogInterface.OnClickListener()
	{
	    @Override
	    public void onClick(DialogInterface dialog, int which)
	    {
		switch (which) {
		    case DialogInterface.BUTTON_POSITIVE:
			account.delBuddy(buddyListSelectedIdx);
			buddyList.remove(item);
			buddyListAdapter.notifyDataSetChanged();
			buddyListSelectedIdx = -1;
			break;
		    case DialogInterface.BUTTON_NEGATIVE:
			break;
		}
	    }
	};

	AlertDialog.Builder adb = new AlertDialog.Builder(this);
	adb.setTitle(buddy_uri);
	adb.setMessage("\nDelete this buddy?\n");
	adb.setPositiveButton("Yes", ocl);
	adb.setNegativeButton("No", ocl);
	adb.show();
    }


    /*
    * === MyAppObserver ===
    * 
    * As we cannot do UI from worker thread, the callbacks mostly just send
    * a message to UI/main thread.
    */

    public void notifyIncomingCall(MyCall call)
    {
	Message m = Message.obtain(handler, MSG_TYPE.INCOMING_CALL, call);
	m.sendToTarget();
    }

    public void notifyRegState(int code, String reason,
			       long expiration)
    {
	String msg_str = "";
	if (expiration == 0)
	    msg_str += "Unregistration";
	else
	    msg_str += "Registration";

	if (code/100 == 2)
	    msg_str += " successful";
	else
	    msg_str += " failed: " + reason;

	Message m = Message.obtain(handler, MSG_TYPE.REG_STATE, msg_str);
	m.sendToTarget();
    }

    public void notifyCallState(MyCall call)
    {
	if (currentCall == null || call.getId() != currentCall.getId())
	    return;

	CallInfo ci = null;
	try {
	    ci = call.getInfo();
	} catch (Exception e) {}
        
	if (ci == null)
	    return;

	Message m = Message.obtain(handler, MSG_TYPE.CALL_STATE, ci);
	m.sendToTarget();
    }

    public void notifyCallMediaState(MyCall call)
    {
	Message m = Message.obtain(handler, MSG_TYPE.CALL_MEDIA_STATE, null);
	m.sendToTarget();
    }

    public void notifyBuddyState(MyBuddy buddy)
    {
	Message m = Message.obtain(handler, MSG_TYPE.BUDDY_STATE, buddy);
	m.sendToTarget();
    }

    public void notifyChangeNetwork()
    {
	Message m = Message.obtain(handler, MSG_TYPE.CHANGE_NETWORK, null);
	m.sendToTarget();
    }

    /* === end of MyAppObserver ==== */

}
