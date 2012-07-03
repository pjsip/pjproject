package org.pjsip.pjsua;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.app.Activity;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.Layout;
import android.text.method.ScrollingMovementMethod;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.EditText;
import android.widget.TextView;

/* This class is for running pjsua in a secondary thread. */
class PjsuaThread extends Thread {
	public static Boolean finished;
	
	public void run() {
		if (pjsua_app.initApp() != 0)
        	return;
		
        pjsua_app.startPjsua(ApjsuaActivity.CFG_FNAME);
        finished = true;
        pjsua_app.deinitApp();
	}
}

/**
 * The purpose of this class is to create an async task for fetching
 * output message from pjsua and display it in our GUI TextView.
 */
class TextOutTask extends AsyncTask<Void, String ,Void> {
	public TextView tv;
	
	protected Void doInBackground(Void... args) {
		while (!PjsuaThread.finished) {
			publishProgress(pjsua_app.getMessage());
		}
		return null;
	}
	
	protected void onProgressUpdate(String... progress) {
		tv.append(progress[0]);
		pjsua_app.finishDisplayMsg();

		/* Auto-scroll to bottom */
		final Layout layout = tv.getLayout();
		if (layout != null) {
			int scrollDelta = layout.getLineBottom(tv.getLineCount() - 1) -
							  tv.getScrollY() - tv.getHeight();
			if (scrollDelta > 0)
				tv.scrollBy(0, scrollDelta);
		}
	}
}

public class ApjsuaActivity extends Activity {
	public static String CFG_NAME = "config.txt";
	public static String CFG_FNAME;
	
	public void setupConfig() {
        try {
            String readLine = null;
            String newLine = "\n";
            InputStream is;
            BufferedReader br;
        	FileOutputStream fos;
        	File file = this.getFileStreamPath(CFG_NAME);
    		
    		CFG_FNAME = file.toString();
    		/* Uncomment this when apjsua already has a built-in config editor.
    		if (file.exists())
    			return;
 			 */

            /* Read the config from our raw resource and copy it to
             * the app's private directory.
             */
    		is = this.getResources().openRawResource(R.raw.config);
            br = new BufferedReader(new InputStreamReader(is));
        	fos = this.openFileOutput(CFG_NAME, Context.MODE_PRIVATE);

        	while ((readLine = br.readLine()) != null) {
        		fos.write(readLine.getBytes());
        		fos.write(newLine.getBytes());
        	}
        	br.close();
        	is.close();
        	fos.close();
        } catch (IOException e) {
        }
	}
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        TextView tv = (TextView) findViewById(R.id.textOut);
        tv.setMovementMethod(new ScrollingMovementMethod());
        
        /* Send the input to pjsua_app when user presses Enter. */
        final EditText cmd = (EditText) findViewById(R.id.command);
        cmd.setOnKeyListener(new OnKeyListener() {
        	public boolean onKey(View v, int keyCode, KeyEvent event) {
        		if ((event.getAction() == KeyEvent.ACTION_DOWN) &&
        			(keyCode == KeyEvent.KEYCODE_ENTER))
        		{
        			pjsua_app.setInput(cmd.getText().toString());
        			cmd.setText("");
        			return true;
        		}
        		return false;
        	}
        });

        /* Load pjsua_app */
        try {
        	System.loadLibrary("pjsua_app");
        } catch (UnsatisfiedLinkError e) {
        	tv.append("Failed to load pjsua_app\n");
        	return;
        }

        /* Setup config file */
        setupConfig();
        
        /* Create pjsua and output thread */
        PjsuaThread.finished = false;
        TextOutTask outTask = new TextOutTask();
        outTask.tv = tv;
        outTask.execute();
        PjsuaThread pjsuaThread = new PjsuaThread();
        pjsuaThread.start();
    }
}
