/* $Id$ */

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

import org.pjsip.pjsua.*;

class app_config {
	public static int cur_call_id = -1;
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
		System.out.print("LOG: " + data);
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

	public static void main(String[] args) {
		int[] tp_id = new int[1];
		int[] acc_id = new int[1];
		int[] call_id = new int[1];
		int status;

		/* Say hello first */
		System.out.println("Hello, World");

		/* Create pjsua */
		{
			status = pjsua.create();
			if (status != pjsua.PJ_SUCCESS) {
				System.out.println("Error creating pjsua: " + status);
				System.exit(status);
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
				pj_error_exit("Error inintializing pjsua", status);
			}
		}
		
		/* Add SIP UDP transport */
		{
			pjsua_transport_config cfg = new pjsua_transport_config();
			pjsua.transport_config_default(cfg);
			cfg.setPort(6000);
			status = pjsua.transport_create(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP, cfg, tp_id);
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error creating transport", status);
			}
		}
		
		/* Add local account */
		{
			status = pjsua.acc_add_local(tp_id[0], true, acc_id);
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error creating local UDP account", status);
			}
		}
		
		/* Start pjsua */
		{
			status = pjsua.start();
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error starting pjsua", status);
			}
		}
		
		/* Make call to the URL. */
		if (false) {
			status = pjsua.call_make_call(acc_id[0], "sip:localhost:6000", null, 0, null, call_id);
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error making call", status);
			}
			app_config.cur_call_id = call_id[0];
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
				if (app_config.cur_call_id == -1) {
					System.out.println("No active call");
					continue;
				}
				
				byte[] buf = new byte[1024*2];
				pjsua.call_dump(app_config.cur_call_id, true, buf, "");

				/* Build string from byte array, find null terminator first */
				int len = 0;
				while (len < buf.length && buf[len] != 0) ++len;
				String call_dump = new String(buf, 0, len);

				System.out.println("Statistics of call " + app_config.cur_call_id);
				System.out.println(call_dump);
			}
		}

		/* Finally, destroy pjsua */
		{
			pjsua.destroy();
		}
		
	}
}
