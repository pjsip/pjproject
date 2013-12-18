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

import java.io.File;
import java.util.ArrayList;
import org.pjsip.pjsua2.*;


/* Interface to separate UI & engine a bit better */
interface MyAppObserver {
	abstract void notifyRegState(pjsip_status_code code, String reason, int expiration);
	abstract void notifyIncomingCall(MyCall call);
	abstract void notifyCallState(MyCall call);
	abstract void notifyBuddyState(MyBuddy buddy);
}


class MyLogWriter extends LogWriter {
	@Override
	public void write(LogEntry entry) {
		System.out.println(entry.getMsg());
	}
}


class MyCall extends Call {
	MyCall(MyAccount acc, int call_id) {
		super(acc, call_id);
	}

	@Override
	public void onCallState(OnCallStateParam prm) {
		MyApp.observer.notifyCallState(this);
	}
	
	@Override
	public void onCallMediaState(OnCallMediaStateParam prm) {
		CallInfo ci;
		try {
			ci = getInfo();
		} catch (Exception e) {
			return;
		}
		
		CallMediaInfoVector cmiv = ci.getMedia();
		
		for (int i = 0; i < cmiv.size(); i++) {
			CallMediaInfo cmi = cmiv.get(i);
			if (cmi.getType() == pjmedia_type.PJMEDIA_TYPE_AUDIO &&
			    (cmi.getStatus() == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE ||
			     cmi.getStatus() == pjsua_call_media_status.PJSUA_CALL_MEDIA_REMOTE_HOLD))
			{
				// unfortunately, on Java too, the returned Media cannot be downcasted to AudioMedia 
				Media m = getMedia(i);
				AudioMedia am = AudioMedia.typecastFromMedia(m);
				
				// connect ports
				try {
					MyApp.ep.audDevManager().getCaptureDevMedia().startTransmit(am);
					am.startTransmit(MyApp.ep.audDevManager().getPlaybackDevMedia());
				} catch (Exception e) {
					continue;
				}
			}
		}
	}
}


class MyAccount extends Account {
	public ArrayList<MyBuddy> buddyList = new ArrayList<MyBuddy>();
	public AccountConfig cfg;
	
	MyAccount(AccountConfig config) {
		super();
		cfg = config;
	}
	
	public MyBuddy addBuddy(BuddyConfig bud_cfg)
	{
		/* Create Buddy */
		MyBuddy bud = new MyBuddy(bud_cfg);
		try {
			bud.create(this, bud_cfg);
		} catch (Exception e) {
			bud = null;
		}
		
		if (bud != null) {
			buddyList.add(bud);
			if (bud_cfg.getSubscribe())
				try {
					bud.subscribePresence(true);
				} catch (Exception e) {}
		}
		
		return bud;
	}
	
	public void delBuddy(MyBuddy buddy) {
		buddyList.remove(buddy);
	}
	
	public void delBuddy(int index) {
		buddyList.remove(index);
	}
	
	@Override
	public void onRegState(OnRegStateParam prm) {
		MyApp.observer.notifyRegState(prm.getCode(), prm.getReason(), prm.getExpiration());
	}

	@Override
	public void onIncomingCall(OnIncomingCallParam prm) {
		System.out.println("======== Incoming call ======== ");
		MyCall call = new MyCall(this, prm.getCallId());
		MyApp.observer.notifyIncomingCall(call);
	}
	
	@Override
	public void onInstantMessage(OnInstantMessageParam prm) {
		System.out.println("======== Incoming pager ======== ");
		System.out.println("From 		: " + prm.getFromUri());
		System.out.println("To			: " + prm.getToUri());
		System.out.println("Contact		: " + prm.getContactUri());
		System.out.println("Mimetype	: " + prm.getContentType());
		System.out.println("Body		: " + prm.getMsgBody());
	}
}


class MyBuddy extends Buddy {
	public BuddyConfig cfg;
	
	MyBuddy(BuddyConfig config) {
		super();
		cfg = config;
	}
	
	String getStatusText() {
		BuddyInfo bi;
		
		try {
			bi = getInfo();
		} catch (Exception e) {
			return "?";
		}
		
		String status = "";
		if (bi.getSubState() == pjsip_evsub_state.PJSIP_EVSUB_STATE_ACTIVE) {
			if (bi.getPresStatus().getStatus() == pjsua_buddy_status.PJSUA_BUDDY_STATUS_ONLINE) {
				status = bi.getPresStatus().getStatusText();
				if (status == null || status.isEmpty()) {
					status = "Online";
				}
			} else if (bi.getPresStatus().getStatus() == pjsua_buddy_status.PJSUA_BUDDY_STATUS_OFFLINE) {
				status = "Offline";
			} else {
				status = "Unknown";
			}
		}
		return status;
	}

	@Override
	public void onBuddyState() {
		MyApp.observer.notifyBuddyState(this);
	}
	
}


class MyAccountConfig {
	public AccountConfig accCfg = new AccountConfig();
	public ArrayList<BuddyConfig> buddyCfgs = new ArrayList<BuddyConfig>();
	
	public void readObject(ContainerNode node) {
		try {
			ContainerNode acc_node = node.readContainer("Account");
			accCfg.readObject(acc_node);
			ContainerNode buddies_node = acc_node.readArray("buddies");
			buddyCfgs.clear();
			while (buddies_node.hasUnread()) {
				BuddyConfig bud_cfg = new BuddyConfig(); 
				bud_cfg.readObject(buddies_node);
				buddyCfgs.add(bud_cfg);
			}
		} catch (Exception e) {}
	}
	
	public void writeObject(ContainerNode node) {
		try {
			ContainerNode acc_node = node.writeNewContainer("Account");
			accCfg.writeObject(acc_node);
			ContainerNode buddies_node = acc_node.writeNewArray("buddies");
			for (int j = 0; j < buddyCfgs.size(); j++) {
				buddyCfgs.get(j).writeObject(buddies_node);
			}
		} catch (Exception e) {}
	}
}


class MyApp {
	static {
		System.loadLibrary("pjsua2");
		System.out.println("Library loaded");
	}
	
	public static Endpoint ep = new Endpoint();
	public static MyAppObserver observer;
	public ArrayList<MyAccount> accList = new ArrayList<MyAccount>();

	private ArrayList<MyAccountConfig> accCfgs = new ArrayList<MyAccountConfig>();
	private EpConfig epConfig = new EpConfig();
	private TransportConfig sipTpConfig = new TransportConfig();
	private String appDir;

	private final String configName = "pjsua2.json";
	private final int SIP_PORT  = 6000;
	private final int LOG_LEVEL = 4;
	
	public void init(MyAppObserver obs, String app_dir) {
		init(obs, app_dir, false);
	}
	
	public void init(MyAppObserver obs, String app_dir, boolean own_worker_thread) {
		observer = obs;
		appDir = app_dir;
		
		/* Create endpoint */
		try {
			ep.libCreate();
		} catch (Exception e) {
			return;
		}
			
		
		/* Load config */
		String configPath = appDir + "/" + configName;
		File f = new File(configPath);
		if (f.exists()) {
			loadConfig(configPath);
		} else {
			/* Set 'default' values */
			sipTpConfig.setPort(SIP_PORT);
		}
		
		/* Override log level setting */
		epConfig.getLogConfig().setLevel(LOG_LEVEL);
		epConfig.getLogConfig().setConsoleLevel(LOG_LEVEL);
		
		/* Set log config. */
		LogConfig log_cfg = epConfig.getLogConfig();
		log_cfg.setWriter(new MyLogWriter());
		log_cfg.setDecor(log_cfg.getDecor() & 
						 ~(pj_log_decoration.PJ_LOG_HAS_CR.swigValue() | 
						   pj_log_decoration.PJ_LOG_HAS_NEWLINE.swigValue()));
		
		/* Set ua config. */
		UaConfig ua_cfg = epConfig.getUaConfig();
		ua_cfg.setUserAgent("Pjsua2And" + ep.libVersion().getFull());
		if (own_worker_thread) {
			ua_cfg.setThreadCnt(0);
			ua_cfg.setMainThreadOnly(true);
		}
		
		/* Init endpoint */
		try {
			ep.libInit(epConfig);
		} catch (Exception e) {
			return;
		}
		
		/* Create transports. */
		try {
			ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP, sipTpConfig);
		} catch (Exception e) {
			System.out.println(e);
		}

		try {
			ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TCP, sipTpConfig);
		} catch (Exception e) {
			System.out.println(e);
		}
		
		/* Create accounts. */
		for (int i = 0; i < accCfgs.size(); i++) {
			MyAccountConfig my_cfg = accCfgs.get(i);
			MyAccount acc = addAcc(my_cfg.accCfg);
			if (acc == null)
				continue;
			
			/* Add Buddies */
			for (int j = 0; j < my_cfg.buddyCfgs.size(); j++) {
				BuddyConfig bud_cfg = my_cfg.buddyCfgs.get(j);
				acc.addBuddy(bud_cfg);
			}
		}

		/* Start. */
		try {
			ep.libStart();
		} catch (Exception e) {
			return;
		}
	}
	
	public MyAccount addAcc(AccountConfig cfg) {
		MyAccount acc = new MyAccount(cfg);
		try {
			acc.create(cfg);
		} catch (Exception e) {
			acc = null;
			return null;
		}
		
		accList.add(acc);
		return acc;
	}
	
	public void delAcc(MyAccount acc) {
		accList.remove(acc);
	}
	
	private void loadConfig(String filename) {
		JsonDocument json = new JsonDocument();
		
		try {
			/* Load file */
			json.loadFile(filename);
			ContainerNode root = json.getRootContainer();
			
			/* Read endpoint config */
			epConfig.readObject(root);
			
			/* Read transport config */
			ContainerNode tp_node = root.readContainer("SipTransport");
			sipTpConfig.readObject(tp_node);
			
			/* Read account configs */
			accCfgs.clear();
			ContainerNode accs_node = root.readArray("accounts");
			while (accs_node.hasUnread()) {
				MyAccountConfig acc_cfg = new MyAccountConfig();
				acc_cfg.readObject(accs_node);
				accCfgs.add(acc_cfg);
			}
		} catch (Exception e) {
			System.out.println(e);
		}
		
		/* Suggest to delete, as we found this causes crash when the Java
		 * deletes it later after lib has been destroyed.
		 */
		json.delete();
	}

	private void buildAccConfigs() {
		/* Sync accCfgs from accList */
		accCfgs.clear();
		for (int i = 0; i < accList.size(); i++) {
			MyAccount acc = accList.get(i);
			MyAccountConfig my_acc_cfg = new MyAccountConfig();
			my_acc_cfg.accCfg = acc.cfg;
			
			my_acc_cfg.buddyCfgs.clear();
			for (int j = 0; j < acc.buddyList.size(); j++) {
				MyBuddy bud = acc.buddyList.get(j);
				my_acc_cfg.buddyCfgs.add(bud.cfg);
			}
			
			accCfgs.add(my_acc_cfg);
		}
	}
	
	private void saveConfig(String filename) {
		JsonDocument json = new JsonDocument();
		
		try {
			/* Write endpoint config */
			json.writeObject(epConfig);
			
			/* Write transport config */
			ContainerNode tp_node = json.writeNewContainer("SipTransport");
			sipTpConfig.writeObject(tp_node);
			
			/* Write account configs */
			buildAccConfigs();
			ContainerNode accs_node = json.writeNewArray("accounts");
			for (int i = 0; i < accCfgs.size(); i++) {
				accCfgs.get(i).writeObject(accs_node);
			}
			
			/* Save file */
			json.saveFile(filename);
		} catch (Exception e) {}

		/* Suggest to delete, as we found this causes crash when the Java
		 * deletes it later after lib has been destroyed.
		 */
		json.delete();
	}
	
	public void deinit() {
		String configPath = appDir + "/" + configName;
		saveConfig(configPath);
		
		/* Try force GC to avoid late destroy of PJ objects as they should be
		 * deleted before lib is destroyed.
		 */
		System.gc();
		
		try {
			ep.libDestroy();
		} catch (Exception e) {}
		ep = null;
	} 
}
