/* $Id: MainActivity.java $ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

package org.pjsip.pjsua;

import java.lang.ref.WeakReference;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.os.Handler;
import android.os.Message;
import android.widget.TextView;

class CONST {
    public static final String LIB_FILENAME = "pjsua";
    public static final String TAG = "pjsua";
    public static final Boolean AUTOKILL_ON_FINISH = true;
    public enum MSG_TYPE {
	STR_DEBUG,
	STR_INFO,
	STR_ERROR,
	CLI_STOP,
	CLI_RESTART,
	QUIT
    };
}

class LOG {
    public static void DEBUG(Handler h, String str) {
	Message msg = Message.obtain(h, CONST.MSG_TYPE.STR_DEBUG.ordinal(), 
				     str);
	msg.sendToTarget();
    }	
    public static void INFO(Handler h, String str) {
	Message msg = Message.obtain(h, CONST.MSG_TYPE.STR_INFO.ordinal(), 
				     str);
	msg.sendToTarget();
    }
    public static void ERROR(Handler h, String str) {
	Message msg = Message.obtain(h, CONST.MSG_TYPE.STR_ERROR.ordinal(), 
				     str);
	msg.sendToTarget();
    }
}

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    private MyHandler ui_handler = new MyHandler(this);
    private static MyCallback callback;

    private static class MyHandler extends Handler {
	private final WeakReference<MainActivity> mTarget;

	public MyHandler(MainActivity target) {
	    mTarget = new WeakReference<MainActivity>(target); 
	}

	@Override
	public void handleMessage(Message m) {
	    MainActivity target = mTarget.get();
	    if (target == null)
		return;

	    if (m.what == CONST.MSG_TYPE.STR_DEBUG.ordinal()) {
		Log.d(CONST.TAG, (String)m.obj);	
	    } else if (m.what == CONST.MSG_TYPE.STR_INFO.ordinal()) {
		target.updateStatus((String)m.obj);
		Log.i(CONST.TAG, (String)m.obj);				
	    } else if (m.what == CONST.MSG_TYPE.STR_ERROR.ordinal()) {
		target.updateStatus((String)m.obj);
		Log.e(CONST.TAG, (String)m.obj);
	    } else if (m.what == CONST.MSG_TYPE.CLI_STOP.ordinal()) {
		pjsua.pjsuaDestroy();
		LOG.INFO(this, "Telnet Unavailable");
	    } else if (m.what == CONST.MSG_TYPE.CLI_RESTART.ordinal()) {
		int status = pjsua.pjsuaRestart();
		if (status != 0) {
		    LOG.INFO(this, "Failed restarting telnet");
		}
	    } else if (m.what == CONST.MSG_TYPE.QUIT.ordinal()) {
		target.finish();
		System.gc();
		android.os.Process.killProcess(android.os.Process.myPid());
	    }
	}
    }

    /** Callback object **/
    private static class MyCallback extends PjsuaAppCallback {
	private WeakReference<Handler> ui_handler;

	public MyCallback(Handler in_ui_handler) {
	    set_ui_handler(in_ui_handler);			
	}

	public void set_ui_handler(Handler in_ui_handler) {
	    ui_handler = new WeakReference<Handler>(in_ui_handler);
	}		

	@Override
	public void onStarted(String msg) {
	    Handler ui = ui_handler.get();
	    LOG.INFO(ui, msg);			
	}

	@Override
	public void onStopped(int restart) {
	    Handler ui = ui_handler.get();
	    /** Use timer to stopped/restart **/
	    if (restart != 0) {
		LOG.INFO(ui, "Telnet Restarting");
		Message msg = Message.obtain(ui, 
			CONST.MSG_TYPE.CLI_RESTART.ordinal());
		ui.sendMessageDelayed(msg, 100);
	    } else {
		LOG.INFO(ui, "Telnet Stopping");
		Message msg = Message.obtain(ui, 
			CONST.MSG_TYPE.CLI_STOP.ordinal());
		ui.sendMessageDelayed(msg, 100);
	    }
	}

        @Override
        public void onCallVideoStart() {
            MainActivity ma = ((MyHandler)ui_handler.get()).mTarget.get();
            SurfaceView surfaceView = (SurfaceView)
                ma.findViewById(R.id.surfaceViewIncomingCall);

            WindowHandle wh = new WindowHandle();
            wh.setWindow(surfaceView.getHolder().getSurface());
            pjsua.setVideoWindow(wh);
        }
    }

    private void updateStatus(String output) {
	TextView tStatus = (TextView) findViewById(R.id.textStatus);
	tStatus.setText(output);        
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
	LOG.DEBUG(ui_handler, "=== Activity::onCreate() ===");
	super.onCreate(savedInstanceState);

	init_view();

	init_lib();
    }

    @Override
    protected void onStart() {
	LOG.DEBUG(ui_handler, "=== Activity::onStart() ===");
	super.onStart();
    }

    @Override
    protected void onRestart() {
	LOG.DEBUG(ui_handler, "=== Activity::onRestart() ===");
	super.onRestart();
    }

    @Override
    protected void onResume() {
	LOG.DEBUG(ui_handler, "=== Activity::onResume() ===");
	super.onResume();
    }

    @Override
    protected void onPause() {
	LOG.DEBUG(ui_handler, "=== Activity::onPause() ===");
	super.onPause();
    }

    @Override
    protected void onStop() {
	LOG.DEBUG(ui_handler, "=== Activity::onStop() ===");
	super.onStop();
    }

    @Override
    protected void onDestroy() {
	LOG.DEBUG(ui_handler, "=== Activity::onDestroy() ===");
	super.onDestroy();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
	super.onSaveInstanceState(outState);
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
	super.onRestoreInstanceState(savedInstanceState);
    }

    private void init_view() {
	setContentView(R.layout.activity_main);
    }

    private int init_lib() {        
	LOG.INFO(ui_handler, "Loading module...");

	// Try loading video dependency libs
	try {
	    System.loadLibrary("openh264");
	    System.loadLibrary("yuv");
	} catch (UnsatisfiedLinkError e) {
	    LOG.ERROR(ui_handler, "UnsatisfiedLinkError: " + e.getMessage());
	    LOG.ERROR(ui_handler, "This could be safely ignored if you "+
		    		  "don't need video.");
	}

	// Load pjsua
	try {
	    System.loadLibrary(CONST.LIB_FILENAME);
	} catch (UnsatisfiedLinkError e) {
	    LOG.ERROR(ui_handler, "UnsatisfiedLinkError: " + e.getMessage());
	    return -1;
	}

	// Wait for GDB to init, for native debugging only
	if (false && (getApplicationInfo().flags & 
	     	      ApplicationInfo.FLAG_DEBUGGABLE) != 0)
	{
	    try {
		Thread.sleep(5000);
	    } catch (InterruptedException e) {
		LOG.ERROR(ui_handler, "InterruptedException: " + 
			e.getMessage());
	    }
	}

	// Set callback object
	if (callback == null)
	    callback = new MyCallback(ui_handler);

	pjsua.setCallbackObject(callback);

	SurfaceView surfaceView = (SurfaceView)
				  findViewById(R.id.surfaceViewIncomingCall);
	surfaceView.getHolder().addCallback(this);

	LOG.INFO(ui_handler, "Starting module..");

	int rc = pjsua.pjsuaStart();

	if (rc != 0) {
	    LOG.INFO(ui_handler, "Failed starting telnet");
	}

	return 0;
    }
    
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h)
    {
        WindowHandle wh = new WindowHandle();
        wh.setWindow(holder.getSurface());
        pjsua.setVideoWindow(wh);
    }

    public void surfaceCreated(SurfaceHolder holder)
    {

    }

    public void surfaceDestroyed(SurfaceHolder holder)
    {
        WindowHandle wh = new WindowHandle();
        wh.setWindow(null);
        pjsua.setVideoWindow(wh);
    }
    
}
