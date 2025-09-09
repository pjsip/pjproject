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

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import org.pjsip.pjsua2.*;


class VideoSurfaceHandler implements SurfaceHolder.Callback {
    private SurfaceHolder holder;
    private VideoWindow videoWindow = null;
    private boolean active = false;

    public VideoSurfaceHandler(SurfaceHolder holder_) {
        holder = holder_;
    }

    public void setVideoWindow(VideoWindow vw) {
        videoWindow = vw;
        active = true;
        setSurfaceHolder(holder);
    }

    public void resetVideoWindow() {
        active = false;
        videoWindow = null;
    }

    private void setSurfaceHolder(SurfaceHolder holder) {
        if (!active) return;

        try {
            VideoWindowHandle wh = new VideoWindowHandle();
            wh.getHandle().setWindow(holder != null? holder.getSurface() : null);
            videoWindow.setWindow(wh);
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h)
    {
        setSurfaceHolder(holder);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) { }


    @Override
    public void surfaceDestroyed(SurfaceHolder holder)
    {
        setSurfaceHolder(null);
    }
}


public class CallActivity extends Activity
                          implements Handler.Callback
{

    public static Handler handler_;
    private final Handler handler = new Handler(this);
    private static CallInfo lastCallInfo;

    private VideoSurfaceHandler localVideoHandler;
    private VideoSurfaceHandler remoteVideoHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_call);

        SurfaceView surfaceInVideo = findViewById(R.id.surfaceIncomingVideo);
        SurfaceView surfacePreview = findViewById(R.id.surfacePreviewCapture);

        /* Avoid visible black boxes (blank video views) initially */
        if (MainActivity.currentCall == null ||
            MainActivity.currentCall.vidWin == null)
        {
            surfaceInVideo.setVisibility(View.GONE);
            surfacePreview.setVisibility(View.GONE);
        }

        localVideoHandler = new VideoSurfaceHandler(surfacePreview.getHolder());
        remoteVideoHandler = new VideoSurfaceHandler(surfaceInVideo.getHolder());
        surfaceInVideo.getHolder().addCallback(remoteVideoHandler);
        surfacePreview.getHolder().addCallback(localVideoHandler);

        handler_ = handler;
        if (MainActivity.currentCall != null) {
            try {
                lastCallInfo = MainActivity.currentCall.getInfo();
                updateCallState(lastCallInfo);
            } catch (Exception e) {
                System.out.println(e);
            }
        } else {
            updateCallState(lastCallInfo);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        
        WindowManager wm;
        Display display;
        int rotation;
        int orient;

        wm = (WindowManager)this.getSystemService(Context.WINDOW_SERVICE);
        display = wm.getDefaultDisplay();
        rotation = display.getRotation();
        System.out.println("Device orientation changed: " + rotation);
        
        switch (rotation) {
        case Surface.ROTATION_0:   // Portrait
            orient = pjmedia_orient.PJMEDIA_ORIENT_ROTATE_270DEG;
            break;
        case Surface.ROTATION_90:  // Landscape, home button on the right
            orient = pjmedia_orient.PJMEDIA_ORIENT_NATURAL;
            break;
        case Surface.ROTATION_180:
            orient = pjmedia_orient.PJMEDIA_ORIENT_ROTATE_90DEG;
            break;
        case Surface.ROTATION_270: // Landscape, home button on the left
            orient = pjmedia_orient.PJMEDIA_ORIENT_ROTATE_180DEG;
            break;
        default:
            orient = pjmedia_orient.PJMEDIA_ORIENT_UNKNOWN;
        }

        if (MyApp.ep != null && MainActivity.account != null) {
            try {
                AccountConfig cfg = MainActivity.account.cfg;
                int cap_dev = cfg.getVideoConfig().getDefaultCaptureDevice();
                MyApp.ep.vidDevManager().setCaptureOrient(cap_dev, orient,
                                                          true);
            } catch (Exception e) {
                System.out.println(e);
            }
        }
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();
        handler_ = null;
    }
    
    private void setupIncomingVideoLayout()
    {
        try {
            StreamInfo si = MainActivity.currentCall.getStreamInfo(MainActivity.currentCall.vidGetStreamIdx());
            int w = (int)si.getVidCodecParam().getDecFmt().getWidth();
            int h = (int)si.getVidCodecParam().getDecFmt().getHeight();

            /* Adjust width to match the parent layout */
            RelativeLayout videoLayout = findViewById(R.id.bottom_layout);
            h = (int)((double)videoLayout.getMeasuredWidth() / w * h);
            w = videoLayout.getMeasuredWidth();

            /* Also adjust height to match the parent layout */
            if (h > videoLayout.getMeasuredHeight()) {
                w = (int)((double)videoLayout.getMeasuredHeight() / h * w);
                h = videoLayout.getMeasuredHeight();
            }
            System.out.println("Remote video size=" + w + "x" + h);

            /* Resize the remote video surface */
            SurfaceView svRemoteVideo = findViewById(R.id.surfaceIncomingVideo);
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(w, h);
            params.leftMargin = (w == videoLayout.getMeasuredWidth())? 0 : (videoLayout.getMeasuredWidth()-w)/2;
            params.topMargin = 0;
            svRemoteVideo.setLayoutParams(params);
            svRemoteVideo.setVisibility(View.VISIBLE);

            /* Put local preview always on top */
            if (MainActivity.currentCall.vidPrevStarted) {
                SurfaceView surfacePreview = findViewById(R.id.surfacePreviewCapture);
                surfacePreview.setVisibility(View.VISIBLE);
            }
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    public void setupVideoPreviewLayout()
    {
        /* Resize the preview video surface */
        try {
            int w, h;
            SurfaceView surfacePreview = findViewById(R.id.surfacePreviewCapture);
            VideoWindowInfo vwi = MainActivity.currentCall.vidPrev.getVideoWindow().getInfo();
            w = (int) vwi.getSize().getW();
            h = (int) vwi.getSize().getH();

            /* Adjust width to match the parent layout */
            RelativeLayout videoLayout = findViewById(R.id.bottom_layout);
            h = (int) ((double) videoLayout.getMeasuredWidth() / 2 / w * h);
            w = videoLayout.getMeasuredWidth() / 2;

            /* Also adjust height to match the parent layout */
            if (h > videoLayout.getMeasuredHeight() / 2) {
                w = (int) ((double) videoLayout.getMeasuredHeight() / 2 / h * w);
                h = videoLayout.getMeasuredHeight() / 2;
            }
            System.out.println("Preview video size=" + w + "x" + h);

            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(w, h);
            params.leftMargin = videoLayout.getMeasuredWidth() - w;
            params.topMargin = videoLayout.getMeasuredHeight() - h;
            surfacePreview.setLayoutParams(params);
            surfacePreview.setVisibility(View.VISIBLE);
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    public void acceptCall(View view)
    {
        CallOpParam prm = new CallOpParam();
        prm.setStatusCode(pjsip_status_code.PJSIP_SC_OK);
        try {
            MainActivity.currentCall.answer(prm);
        } catch (Exception e) {
            System.out.println(e);
        }

        view.setVisibility(View.GONE);
    }

    public void hangupCall(View view)
    {
        localVideoHandler.resetVideoWindow();
        remoteVideoHandler.resetVideoWindow();

        handler_ = null;
        finish();

        if (MainActivity.currentCall != null) {
            CallOpParam prm = new CallOpParam();
            prm.setStatusCode(pjsip_status_code.PJSIP_SC_DECLINE);
            try {
                MainActivity.currentCall.hangup(prm);
            } catch (Exception e) {
                System.out.println(e);
            }
        }
    }
    

    @Override
    public boolean handleMessage(Message m)
    {
        if (m.what == MainActivity.MSG_TYPE.CALL_STATE) {

            lastCallInfo = (CallInfo) m.obj;
            if (lastCallInfo.getState()==pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
                localVideoHandler.resetVideoWindow();
                remoteVideoHandler.resetVideoWindow();
            }

            updateCallState(lastCallInfo);

        } else if (m.what == MainActivity.MSG_TYPE.CALL_MEDIA_STATE) {

            if (MainActivity.currentCall.vidWin != null) {
                /* Set capture orientation according to current
                 * device orientation.
                 */
                onConfigurationChanged(getResources().getConfiguration());

            }

            if (MainActivity.currentCall.vidPrev != null) {
                localVideoHandler.setVideoWindow(MainActivity.currentCall.vidPrev.getVideoWindow());
                setupVideoPreviewLayout();
            }

        } else if (m.what == MainActivity.MSG_TYPE.CALL_MEDIA_EVENT) {
            OnCallMediaEventParam prm = (OnCallMediaEventParam)m.obj;

            if (prm.getEv().getType() == pjmedia_event_type.PJMEDIA_EVENT_FMT_CHANGED &&
                prm.getMedIdx() == MainActivity.currentCall.vidGetStreamIdx() &&
                MainActivity.currentCall.vidWin != null)
            {
                CallMediaInfo cmi;
                try {
                    CallInfo ci = MainActivity.currentCall.getInfo();
                    cmi = ci.getMedia().get((int)prm.getMedIdx());
                } catch (Exception e) {
                    System.out.println(e);
                    return false;
                }

                remoteVideoHandler.setVideoWindow(cmi.getVideoWindow());
                setupIncomingVideoLayout();
            }

        } else {

            /* Message not handled */
            return false;

        }

        return true;
    }

    private void updateCallState(CallInfo ci) {
        TextView tvPeer  = (TextView) findViewById(R.id.textViewPeer);
        TextView tvState = (TextView) findViewById(R.id.textViewCallState);
        Button buttonHangup = (Button) findViewById(R.id.buttonHangup);
        Button buttonAccept = (Button) findViewById(R.id.buttonAccept);
        String call_state = "";

        if (ci == null) {
            buttonAccept.setVisibility(View.GONE);
            buttonHangup.setText("OK");
            tvState.setText("Call disconnected");
            return;
        }

        if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAC) {
            buttonAccept.setVisibility(View.GONE);
        }

        if (ci.getState() <
            pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
        {
            if (ci.getRole() == pjsip_role_e.PJSIP_ROLE_UAS) {
                call_state = "Incoming call..";
                /* Default button texts are already 'Accept' & 'Reject' */
            } else {
                buttonHangup.setText("Cancel");
                call_state = ci.getStateText();
            }
        }
        else if (ci.getState() >=
                 pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED)
        {
            buttonAccept.setVisibility(View.GONE);
            call_state = ci.getStateText();
            if (ci.getState() == pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED) {
                buttonHangup.setText("Hangup");
            } else if (ci.getState() ==
                       pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
            {
                buttonHangup.setText("OK");
                call_state = "Call disconnected: " + ci.getLastReason();
            }
        }

        tvPeer.setText(ci.getRemoteUri());
        tvState.setText(call_state);
    }
}
