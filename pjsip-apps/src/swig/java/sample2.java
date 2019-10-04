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

import java.io.IOException;
import org.pjsip.pjsua2.*;
import org.pjsip.pjsua2.app.*;
import javafx.application.Application;
import javafx.event.ActionEvent;
import javafx.event.EventHandler;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.stage.Stage;
import com.sun.javafx.tk.TKStage;
import java.lang.reflect.Method;

class MyObserver implements MyAppObserver {
	private static MyCall currentCall = null;
	private static boolean del_call_scheduled = false;
	
	public void check_call_deletion()
	{
		if (del_call_scheduled && currentCall != null) {
			currentCall.delete();
			currentCall = null;
			del_call_scheduled = false;
		}
	}
	
	@Override
	public void notifyRegState(int code, String reason, long expiration) {}
	
	@Override
	public void notifyIncomingCall(MyCall call) {
		/* Auto answer. */
		CallOpParam call_param = new CallOpParam();
		call_param.setStatusCode(pjsip_status_code.PJSIP_SC_OK);
		try {
			currentCall = call;
			currentCall.answer(call_param);
		} catch (Exception e) {
			System.out.println(e);
			return;
		}	
	}
	
	@Override
	public void notifyCallMediaState(MyCall call) {
	}

	public void notifyCallState(MyCall call) {
		if (currentCall == null || call.getId() != currentCall.getId())
			return;

		CallInfo ci;
		try {
			ci = call.getInfo();
		} catch (Exception e) {
			ci = null;
		}
		if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
			// Should not delete call instance here, so let's schedule it.
			// The call will be deleted by our main worker thread.
			del_call_scheduled = true;                 
		} else if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
			if (ci.getSetting().getVideoCount() != 0) {
				System.out.println("Changing video window using " + sample2.hwnd);
				// Change window
				VideoWindowHandle vidWH = new VideoWindowHandle();	
				vidWH.getHandle().setWindow(sample2.hwnd);	    	    
				try {
					currentCall.vidWin.setWindow(vidWH);
				} catch (Exception e) {
					System.out.println(e);
				}
			}
		}
	}

	@Override
	public void notifyBuddyState(MyBuddy buddy) {}	

	@Override
	public void notifyChangeNetwork() {}
}

class MyThread extends Thread {
	private static MyApp app = new MyApp();
	private static MyObserver observer = new MyObserver();
	private static MyAccount account = null;
	private static AccountConfig accCfg = null;		     
	
	public void run() {
		try {					
			app.init(observer, ".", true);
		} catch (Exception e) {
			System.out.println(e);
			app.deinit();
			System.exit(-1);
		}                 

		if (app.accList.size() == 0) {
			accCfg = new AccountConfig();
			accCfg.setIdUri("sip:localhost");
			account = app.addAcc(accCfg);

			accCfg.setIdUri("sip:test1@pjsip.org");
			AccountSipConfig sipCfg = accCfg.getSipConfig();		
			AuthCredInfoVector ciVec = sipCfg.getAuthCreds();
			ciVec.add(new AuthCredInfo("Digest", 
					"*",
					"test1",
					0,
					"test1"));

			StringVector proxy = sipCfg.getProxies();
			proxy.add("sip:sip.pjsip.org;transport=tcp");							

			AccountRegConfig regCfg = accCfg.getRegConfig();
			regCfg.setRegistrarUri("sip:pjsip.org");

			accCfg.getVideoConfig().setAutoTransmitOutgoing(true);
			accCfg.getVideoConfig().setAutoShowIncoming(true);                        
			account = app.addAcc(accCfg);                        
		} else {
			account = app.accList.get(0);
			accCfg = account.cfg;
		}

		try {
			account.modify(accCfg);
		} catch (Exception e) {}

		while (!Thread.currentThread().isInterrupted()) {
			// Handle events
			MyApp.ep.libHandleEvents(10);

			// Check if any call instance need to be deleted
			observer.check_call_deletion();
			try {
				Thread.sleep(50);
			} catch (InterruptedException ie) {
				break;
			}
		}    
                app.deinit();
	}		
}

public class sample2 extends Application {
	public static long hwnd;
	private static Thread myThread = new MyThread();

	private static long getWindowPointer(Stage stage) {
		try {
			TKStage tkStage = stage.impl_getPeer();
			Method getPlatformWindow = tkStage.getClass().getDeclaredMethod("getPlatformWindow" );
			getPlatformWindow.setAccessible(true);
			Object platformWindow = getPlatformWindow.invoke(tkStage);
			Method getNativeHandle = platformWindow.getClass().getMethod( "getNativeHandle" );
			getNativeHandle.setAccessible(true);
			return (long)getNativeHandle.invoke(platformWindow);
                } catch (Throwable e) {
			System.err.println("Error getting Window Pointer");
			return 0;
                }
	}
        
	@Override
	public void start(Stage primaryStage) {
		primaryStage.setTitle("Pjsua2 javafx sample");
		StackPane root = new StackPane();                
		primaryStage.setScene(new Scene(root, 300, 250));
		primaryStage.show();
		hwnd = getWindowPointer(primaryStage);
		myThread.start();
	}          
	@Override
	public void stop() throws Exception {
		myThread.interrupt();
		myThread.join();
	}                
	public static void main(String argv[]) {
		launch(argv);
	}        
}