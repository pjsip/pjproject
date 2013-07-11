/* $Id$ */

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

import org.pjsip.pjsua.pj_pool_t;
import org.pjsip.pjsua.pjsip_rx_data;
import org.pjsip.pjsua.pjsip_transport_type_e;

import org.pjsip.pjsua.pjsua;
import org.pjsip.pjsua.pjsua_acc_config;
import org.pjsip.pjsua.pjsua_call_info;
import org.pjsip.pjsua.pjsua_call_media_status;
import org.pjsip.pjsua.pjsua_config;
import org.pjsip.pjsua.pjsua_logging_config;
import org.pjsip.pjsua.pjsua_transport_config;
import org.pjsip.pjsua.PjsuaCallback;

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
			/* Setup callback */
			cfg.setCb(new MyPjsuaCallback());
			status = pjsua.init(cfg, null, null);
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
			status = pjsua.call_make_call(acc_id[0], "sip:localhost", null, 0, null, call_id);
			if (status != pjsua.PJ_SUCCESS) {
				pj_error_exit("Error making call", status);
			}
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
			}
		}

		/* Finally, destroy pjsua */
		{
			pjsua.destroy();
		}
		
	}
}
