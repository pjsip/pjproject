// $id$

package org.pjsip.apjloader;

import java.lang.ref.WeakReference;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

class CONST {
	public static final String LIB_FILENAME = "apjloader";
	public static final String TAG = "apjloader";
	public static final String NEWLINE = "\r\n";
	public static final Boolean AUTOKILL_ON_FINISH = false;
	public static final int MAX_LOG_CHARS = 20000; // set to zero to disable limit
	public enum MSG_TYPE {
		STR_INFO,
		STR_ERROR,
		QUIT
	};
}

class LOG {
	public static void INFO(Handler h, String str) {
		Message msg = Message.obtain(h, CONST.MSG_TYPE.STR_INFO.ordinal(), str);
		msg.sendToTarget();
	}
	public static void ERROR(Handler h, String str) {
		Message msg = Message.obtain(h, CONST.MSG_TYPE.STR_ERROR.ordinal(), str);
		msg.sendToTarget();
	}
}

class LoaderThread extends Thread {
	private WeakReference<Handler> ui_handler;

	public LoaderThread(Handler ui_handler_) {
		set_ui_handler(ui_handler_);
	}
	
	public void set_ui_handler(Handler ui_handler_) {
		ui_handler = new WeakReference<Handler>(ui_handler_);
	}
	
	public void run() {
		// Console application's params. Note that, the first param will be
		// ignored (it is supposed to contain the app path).
		// Here we just put some pjsua app params, this sample params will be
		// ignored by PJSIP test apps such as pjlib-test, pjsystest, etc.
		String argv[] = {"",
						 "--clock-rate=8000",
						 "--auto-answer=200",
						 };
		
		Handler ui = ui_handler.get();
		LOG.INFO(ui, "Starting module.." + CONST.NEWLINE);
		int rc = apjloader.main(argv.length, argv);
		
		ui = ui_handler.get();
		LOG.INFO(ui, "Module finished with return code: " + 
				 Integer.toString(rc) + CONST.NEWLINE);
		
		apjloader.destroy_stdio_pipe();

		if (CONST.AUTOKILL_ON_FINISH) {
			ui = ui_handler.get();
			Message msg = Message.obtain(ui, CONST.MSG_TYPE.QUIT.ordinal());
			ui.sendMessageDelayed(msg, 2000);
		}
	}
}

class OutputThread extends Thread {
	private WeakReference<Handler> ui_handler;
	
	public OutputThread(Handler ui_handler_) {
		set_ui_handler(ui_handler_);
	}
	
	public void set_ui_handler(Handler ui_handler_) {
		ui_handler = new WeakReference<Handler>(ui_handler_);
	}
	
	public void run() {
		final int BUFFERSIZE = 200;
		StringBuilder sb = new StringBuilder(BUFFERSIZE);
		
		while (true) 
		{
			byte ch[] = new byte[1];
			int rc = apjloader.read_from_stdout(ch);
			Handler ui = ui_handler.get();
			
			if (rc == 0) {
				if (ch[0]=='\r' || ch[0]== '\n') {
					if (sb.length() > 0) {
						LOG.INFO(ui, sb.toString()+CONST.NEWLINE);
						sb.delete(0, sb.length());
					}
				} else {
					sb.append((char)ch[0]);
				}					
			} else {
				LOG.INFO(ui, "Stdout pipe stopped, rc="+Integer.toString(rc)+CONST.NEWLINE);
				break;
			}
		}
	}
}


public class MainActivity extends Activity {
	private TextView log_view;
	private ScrollView log_scroll_view;
	private EditText input_cmd;
	private MyHandler ui_handler = new MyHandler(this);
	private static OutputThread ot;
	private static LoaderThread lt;
	
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
			
			if (m.what == CONST.MSG_TYPE.STR_INFO.ordinal() ||
				m.what == CONST.MSG_TYPE.STR_ERROR.ordinal())
			{
				target.print_log((String)m.obj);
			} else if (m.what == CONST.MSG_TYPE.QUIT.ordinal()) {
				target.finish();
				System.gc();
				android.os.Process.killProcess(android.os.Process.myPid());
			}
		}
    }
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
    	Log.d(CONST.TAG, "=== Activity::onCreate() ===");
    	super.onCreate(savedInstanceState);
    	
    	init_view();

    	int rc = init_lib();
        if (rc != 0) {
        	print_log("Failed loading module: " + Integer.toString(rc) + CONST.NEWLINE);
        	return;
        }

        ot.set_ui_handler(ui_handler);
        lt.set_ui_handler(ui_handler);
	}

    @Override
    protected void onStart() {
    	Log.d(CONST.TAG, "=== Activity::onStart() ===");
    	super.onStart();
    }
    
    @Override
    protected void onRestart() {
    	Log.d(CONST.TAG, "=== Activity::onRestart() ===");
    	super.onRestart();
    }

    @Override
    protected void onResume() {
    	Log.d(CONST.TAG, "=== Activity::onResume() ===");
    	super.onResume();
    }

    @Override
    protected void onPause() {
    	Log.d(CONST.TAG, "=== Activity::onPause() ===");
    	super.onPause();
    }

    @Override
    protected void onStop() {
    	Log.d(CONST.TAG, "=== Activity::onStop() ===");
    	super.onStop();
    }

    @Override
    protected void onDestroy() {
    	Log.d(CONST.TAG, "=== Activity::onDestroy() ===");
    	super.onDestroy();
    }
    
    @Override
    protected void onSaveInstanceState(Bundle outState) {
		super.onSaveInstanceState(outState);
		outState.putString("INPUT_CMD", input_cmd.getText().toString());
		outState.putString("LOG_VIEW", log_view.getText().toString());
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
		super.onRestoreInstanceState(savedInstanceState);
		input_cmd.setText(savedInstanceState.getString("INPUT_CMD"));
		log_view.setText(savedInstanceState.getString("LOG_VIEW"));
    	log_view.post(new Runnable() {
            public void run() {
            	log_scroll_view.fullScroll(View.FOCUS_DOWN);
            }
        });   	
    }
    
          
    private View.OnClickListener on_send_cmd_click = new View.OnClickListener() {
    	public void onClick(View v) {
        	String cmd = input_cmd.getText().toString().trim();
        	input_cmd.setText("");
    		send_cmd(cmd);
    	}
    };
    
    private View.OnClickListener on_quit_click = new View.OnClickListener() {
    	public void onClick(View v) {
    		/* Send quit command, only for pjsua-app */
    		send_cmd("q");
    		
    		print_log("Quitting..");
    		Message msg = Message.obtain(ui_handler, CONST.MSG_TYPE.QUIT.ordinal());
    		ui_handler.sendMessageDelayed(msg, 2000);
    	}
    };
    
    private View.OnKeyListener on_input_cmd_key = new View.OnKeyListener() {
		
		public boolean onKey(View v, int keyCode, KeyEvent event) {
			if ((event.getAction() == KeyEvent.ACTION_DOWN) &&
    			(keyCode == KeyEvent.KEYCODE_ENTER))
    		{
	        	String cmd = input_cmd.getText().toString().trim();
	        	input_cmd.setText("");
				send_cmd(cmd);
    			return true;
    		}
    		return false;
		}
	};
	
    private void print_log(String st) {
    	log_view.append(st);
    	Log.d(CONST.TAG, st);
    	
    	// Limit log view text length
    	if (CONST.MAX_LOG_CHARS > 0 && log_view.length() > CONST.MAX_LOG_CHARS) {
    		int to_del = log_view.length() - CONST.MAX_LOG_CHARS + 100;
    		if (to_del > log_view.length())
    			to_del = log_view.length();
        	log_view.getEditableText().delete(0, to_del);
    	}

    	// Scroll to bottom
    	log_view.post(new Runnable() {
            public void run() {
            	log_scroll_view.fullScroll(View.FOCUS_DOWN);
            }
        });   	
    }
    
    private void hide_soft_keyboard() {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.hideSoftInputFromWindow(input_cmd.getWindowToken(), InputMethodManager.HIDE_NOT_ALWAYS);
    }
    
	private void init_view() {
    	setContentView(R.layout.activity_main);

    	Button send_cmd_button = (Button)findViewById(R.id.send_cmd_button);
        send_cmd_button.setOnClickListener(on_send_cmd_click);

        Button quit_button = (Button)findViewById(R.id.quit_button);
        quit_button.setOnClickListener(on_quit_click);

        input_cmd = (EditText)findViewById(R.id.input_cmd);
        input_cmd.setOnKeyListener(on_input_cmd_key);

        log_view = (TextView)findViewById(R.id.output);
        log_view.setMovementMethod(new ScrollingMovementMethod());
        
        log_scroll_view = (ScrollView)findViewById(R.id.output_scroller);
	}
	
	private int init_lib() {
		if (ot != null || lt != null)
			return 0;
		
        print_log("Loading module.." + CONST.NEWLINE);
        
		try {
			System.loadLibrary(CONST.LIB_FILENAME);
		} catch (UnsatisfiedLinkError e) {
			print_log("UnsatisfiedLinkError: " + e.getMessage() + CONST.NEWLINE);
			return -1;
		}
		
		// Wait for GDB to init
		if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
			try {
				Thread.sleep(5000);
	        } catch (InterruptedException e) {
				print_log("InterruptedException: " + e.getMessage() + CONST.NEWLINE);
	        }
		}
		
		int rc = apjloader.init_stdio_pipe();
        print_log("Stdio pipes inited: " + Integer.toString(rc) + CONST.NEWLINE);
        
    	ot = new OutputThread(ui_handler);
		ot.start();
		
        lt = new LoaderThread(ui_handler);
        lt.start();
        
		return 0;
	}
	
    private void send_cmd(String cmd) {
		hide_soft_keyboard();
    	if (cmd == "")
    		return;
    	
		print_log("Sending command: " + cmd + CONST.NEWLINE);
		apjloader.write_to_stdin(cmd);
    }
}
