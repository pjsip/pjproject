/* $Id: hello.java 4566 2013-07-17 20:20:50Z nanang $ */

package org.pjsip.hello;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

import org.pjsip.pjsua.*;

class app_config {
	public static int cur_call_id = -1;
	public static int cur_acc_id = -1;
	
	public static int sip_port = 6000;
	
	public static boolean is_reg = true;
	public static String user = "301";
	public static String pwd = "pw301"; 
	public static String domain = "pjsip.org"; 
	public static String proxy[] = { "sip:sip.pjsip.org;transport=tcp" }; 
}

class MyPjsuaCallback extends PjsuaCallback {
	@Override
	public void on_call_media_state(int call_id)
	{
		System.out.println("======== Call media started (call id: " + call_id + ")");
		pjsua_call_info info = new pjsua_call_info();
		pjsua.call_get_info(call_id, info);
		if (info.getMedia_status() == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE) {
			pjsua.conf_connect(info.getConf_slot(), 0);
			pjsua.conf_connect(0, info.getConf_slot());
		}
	}
	
	/* Testing pj_str_t-String map with director */
	@Override
	public void on_pager(int call_id, String from, String to, String contact, String mime_type, String body)
	{
		System.out.println("======== Incoming pager (call id: " + call_id + ")");
		System.out.println("From     : " + from);
		System.out.println("To       : " + to);
		System.out.println("Contact  : " + contact);
		System.out.println("Mimetype : " + mime_type);
		System.out.println("Body     : " + body);
	}
	@Override
	public void on_incoming_call(int acc_id, int call_id, pjsip_rx_data rdata) {
		/* Auto answer */
		pjsua.call_answer(call_id, 200, null, null);
		app_config.cur_call_id = call_id;
	}
}

class MyLogger extends PjsuaLoggingConfigCallback {
	@Override
	public void on_log(int level, String data)
	{
		System.out.print("LOG" + level + ": " + data);
	}
}

class MyTimerCallback extends PjTimerHeapCallback {
	private int _user_data;
	
	/* Use this class itself to store app user data */
	MyTimerCallback(int user_data) { _user_data = user_data; }
	
	@Override
	public void on_timer(pj_timer_heap_t timer_heap, pj_timer_entry entry)
	{
		System.out.println("======== Timer fired (user data = " + _user_data + ")");
	}
}


public class hello {
	static {
		System.loadLibrary("pjsua");
	}

	protected static void pj_error_exit(String message, int status) {
		pjsua.perror("hello", message, status);
		pjsua.destroy();
		System.exit(status);
	}
	
	protected static boolean check_active_call() {
		if (app_config.cur_call_id == -1)
			System.out.println("No active call");
		return (app_config.cur_call_id > -1);
	}
	
	public static int init() {
		int[] tp_id = new int[1];
		int[] acc_id = new int[1];
		int status;

		/* Say hello first */
		System.out.println("Hello, World");

		/* Create pjsua */
		{
			status = pjsua.create();
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating pjsua: " + status);
				return status;
			}
		}
		
		/* Init pjsua */
		{
			pjsua_config cfg = new pjsua_config();
			pjsua.config_default(cfg);
			cfg.setCb(new MyPjsuaCallback());

			pjsua_logging_config log_cfg = new pjsua_logging_config();
			pjsua.logging_config_default(log_cfg);
			log_cfg.setLevel(4);
			log_cfg.setCb(new MyLogger());

			status = pjsua.init(cfg, log_cfg, null);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error initializing pjsua: " + status);
				return status;
			}
		}
		
		/* Add SIP UDP transport */
		{
			pjsua_transport_config cfg = new pjsua_transport_config();
			pjsua.transport_config_default(cfg);
			cfg.setPort(app_config.sip_port);
			status = pjsua.transport_create(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP, cfg, tp_id);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating transport: " + status);
				return status;
			}
		}
		
		/* Add UDP local account */
		{
			status = pjsua.acc_add_local(tp_id[0], true, acc_id);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating local UDP account: " + status);
				return status;
			}
			app_config.cur_acc_id = acc_id[0]; 
		}
		
		/* Add SIP TCP transport */
		{
			pjsua_transport_config cfg = new pjsua_transport_config();
			pjsua.transport_config_default(cfg);
			cfg.setPort(app_config.sip_port);
			status = pjsua.transport_create(pjsip_transport_type_e.PJSIP_TRANSPORT_TCP, cfg, tp_id);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating transport: " + status);
				return status;
			}
		}
		
		/* Add TCP local account */
		{
			status = pjsua.acc_add_local(tp_id[0], true, acc_id);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating local UDP account: " + status);
				return status;
			}
			app_config.cur_acc_id = acc_id[0]; 
		}
		
		/* Add registered account */
		if (app_config.is_reg) {
			pjsip_cred_info [] cred_info = { new pjsip_cred_info() };
			cred_info[0].setUsername(app_config.user);
			cred_info[0].setData_type(0);
			cred_info[0].setData(app_config.pwd);
			cred_info[0].setRealm("*");
			cred_info[0].setScheme("Digest");

			pjsua_acc_config acc_cfg = new pjsua_acc_config();
			pjsua.acc_config_default(acc_cfg);
			acc_cfg.setId("sip:" + app_config.user + "@" + app_config.domain);
			acc_cfg.setCred_info(cred_info);
			acc_cfg.setReg_uri("sip:" + app_config.domain);
			acc_cfg.setProxy(app_config.proxy);
			
			status = pjsua.acc_add(acc_cfg, true, acc_id);
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating account " + acc_cfg.getId() + ": " + status);
				return status;
			}
			app_config.cur_acc_id = acc_id[0];
		}
		
		/* Start pjsua */
		{
			status = pjsua.start();
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error starting pjsua: " + status);
				return status;
			}
		}
		
		/* Test timer */
		{
			pj_timer_entry timer = new pj_timer_entry();
			timer.setCb(new MyTimerCallback(1234567890));

			pj_time_val tv = new pj_time_val();
			tv.setSec(0);
			tv.setMsec(1000);

			pjsua.schedule_timer(timer, tv);
		}
		
		return pjsua.PJ_SUCCESS;
	}


	public static void destroy() {
		pjsua.destroy();
	}
	

	public static void main(String[] args)
	{
		/* Init pjsua */
		int status = init();
		if (status != pjsua.PJ_SUCCESS) {
			pj_error_exit("Failed initializing pjsua", status);
		}
		
		/* Make call to the URL. */
		if (args.length > 1) {
			int[] call_id = new int[1];
			status = pjsua.call_make_call(app_config.cur_acc_id, args[1], null, 0, null, call_id);
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error making call", status);
			}
			app_config.cur_call_id = call_id[0];
		}
		
		/* Wait until user press "q" to quit. */
		for (;;) {
			String userInput;
			BufferedReader inBuffReader = new BufferedReader(new InputStreamReader(System.in));

			System.out.println("Press 'h' to hangup all calls, 'q' to quit");

			try {
				userInput = inBuffReader.readLine();
			} catch (IOException e) {
				System.out.println("Error in readLine(): " + e);
				break;
			}
			
			if (userInput.equals("q")) {
				break;
			} else if (userInput.equals("h")) {
				pjsua.call_hangup_all();
				app_config.cur_call_id = -1;
			} else if (userInput.equals("c")) {
				/* Test string as output param, it is wrapped as string array */
				String[] contact = new String[1];
				pj_pool_t my_pool = pjsua.pool_create("hello", 256, 256);
				
				pjsua.acc_create_uac_contact(my_pool, contact, 0, "sip:localhost");
				System.out.println("Test create contact: " + contact[0]);
				
				pjsua.pj_pool_release(my_pool);
			} else if (userInput.equals("d")) {
				pjsua.dump(false);
			} else if (userInput.equals("dd")) {
				pjsua.dump(true);
			} else if (userInput.equals("dq")) {
				if (!check_active_call()) continue;
				
				byte[] buf = new byte[1024*2];
				pjsua.call_dump(app_config.cur_call_id, true, buf, "");

				/* Build string from byte array, find null terminator first */
				int len = 0;
				while (len < buf.length && buf[len] != 0) ++len;
				String call_dump = new String(buf, 0, len);

				System.out.println("Statistics of call " + app_config.cur_call_id);
				System.out.println(call_dump);
			} else if (userInput.equals("si")) {
				/* Test nested struct in pjsua_stream_info */
				if (!check_active_call()) continue;

				pjsua_stream_info si = new pjsua_stream_info();
				pjsua.call_get_stream_info(app_config.cur_call_id, 0, si);
				System.out.println("Audio codec being used: " + si.getInfo().getAud().getFmt().getEncoding_name());
			}
		}
		
		/* Finally, destroy pjsua */
		destroy();

	}
}
