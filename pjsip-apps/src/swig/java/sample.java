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

class MyObserver implements MyAppObserver {
	private static MyCall currentCall = null;
	
	@Override
	public void notifyRegState(pjsip_status_code code, String reason, int expiration) {}
	
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
	public void notifyCallState(MyCall call) {
		if (currentCall == null || call.getId() != currentCall.getId())
			return;
		
		CallInfo ci;
		try {
			ci = call.getInfo();
		} catch (Exception e) {
			ci = null;
		}
		if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
			currentCall = null;		
	}
	
	@Override
	public void notifyBuddyState(MyBuddy buddy) {}	
}

class MyShutdownHook extends Thread {
	Thread thread;
	MyShutdownHook(Thread thr) {
		thread = thr;
	}
	public void run() {
		thread.interrupt();
		try {
			thread.join();
		} catch (Exception e) {
			;
		}
	}
}	

public class sample {
	private static MyApp app = new MyApp();
	private static MyAppObserver observer = new MyObserver();
	private static MyAccount account = null;
	private static AccountConfig accCfg = null;			
	
	private static void runWorker() {
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

			accCfg.setIdUri("sip:301@pjsip.org");
			AccountSipConfig sipCfg = accCfg.getSipConfig();		
			AuthCredInfoVector ciVec = sipCfg.getAuthCreds();
			ciVec.add(new AuthCredInfo("Digest", 
					"*",
					"301",
					0,
					"pw301"));

			StringVector proxy = sipCfg.getProxies();
			proxy.add("sip:pjsip.org;transport=tcp");							

			AccountRegConfig regCfg = accCfg.getRegConfig();
			regCfg.setRegistrarUri("sip:pjsip.org");
			account = app.addAcc(accCfg);
		} else {
			account = app.accList.get(0);
			accCfg = account.cfg;
		}				

		try {
			account.modify(accCfg);
		} catch (Exception e) {}				

		while (!Thread.currentThread().isInterrupted()) {
			MyApp.ep.libHandleEvents(10);
			try {						
				Thread.currentThread().sleep(50);
			} catch (InterruptedException ie) {						
				break;
			}					
		}
		app.deinit();
	}	
		
	public static void main(String argv[]) {
		Runtime.getRuntime().addShutdownHook(new MyShutdownHook(Thread.currentThread()));

		runWorker();
    }
}
