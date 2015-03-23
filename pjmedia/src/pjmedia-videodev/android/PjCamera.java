/* $Id$ */
/*
 * Copyright (C) 2015 Teluu Inc. (http://www.teluu.com)
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
package org.pjsip;

import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.util.Log;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

import java.io.IOException;

public class PjCamera implements Camera.PreviewCallback, SurfaceHolder.Callback
{
    private final String TAG = "PjCamera";

    public class Param {
	public int width;
	public int height;
	public int format;
	public int fps1000;
    }

    private Camera camera = null;
    private boolean isRunning = false;
    private int camIdx;
    private long userData;

    private Param param = null;

    private SurfaceView surfaceView = null;
    private SurfaceHolder surfaceHolder = null;
    private SurfaceTexture surfaceTexture = null;

    public PjCamera(int idx, int w, int h, int fmt, int fps,
	    	    long userData_, SurfaceView surface)
    {
	camIdx = idx;
	userData = userData_;

	param = new Param();
	param.width = w;
	param.height = h;
	param.format = fmt;
	param.fps1000 = fps;

	SetSurfaceView(surface);
    }

    public void SetSurfaceView(SurfaceView surface)
    {
	boolean isCaptureRunning = isRunning;

	if (isCaptureRunning)
	    Stop();

	if (surface != null) {
	    surfaceView = surface;
	    surfaceHolder = surfaceView.getHolder();
	} else {
	    // Create dummy texture
	    surfaceHolder = null;
	    surfaceView = null;
	    if (surfaceTexture == null) {
		surfaceTexture = new SurfaceTexture(10);
	    }
	}

	if (isCaptureRunning)
	    Start();
    }

    public int SwitchDevice(int idx)
    {
	boolean isCaptureRunning = isRunning;
	int oldIdx = camIdx;

	if (isCaptureRunning)
	    Stop();

	camIdx = idx;

	if (isCaptureRunning) {
	    int ret = Start();
	    if (ret != 0) {
		/* Try to revert back */
		camIdx = oldIdx;
		Start();
		return ret; 
	    }
	}

	return 0;
    }

    public int Start()
    {
	try {
	    camera = Camera.open(camIdx);
	} catch (Exception e) {
	    Log.d("IOException", e.getMessage());
	    return -10;
	}

	try {
	    if (surfaceHolder != null) {
		camera.setPreviewDisplay(surfaceHolder);
		surfaceHolder.addCallback(this);
	    } else {
		camera.setPreviewTexture(surfaceTexture);
	    }
	} catch (IOException e) {
	    Log.d("IOException", e.getMessage());
	    return -20;
	}

	Camera.Parameters cp = camera.getParameters();
	cp.setPreviewSize(param.width, param.height);
	cp.setPreviewFormat(param.format);
	// Some devices such as Nexus require an exact FPS range from the
	// supported FPS ranges, specifying a subset range will raise
	// exception.
	//cp.setPreviewFpsRange(param.fps1000, param.fps1000);
	try {
	    camera.setParameters(cp);
	} catch (RuntimeException e) {
	    Log.d("RuntimeException", e.getMessage());
	    return -30;
	}

	camera.setPreviewCallback(this);
	camera.startPreview();
	isRunning = true;

	return 0;
    }

    public void Stop()
    {
	isRunning = false;
	if (camera == null)
	    return;

	if (surfaceHolder != null)
	    surfaceHolder.removeCallback(this);

	camera.setPreviewCallback(null);
	camera.stopPreview();
	camera.release();
	camera = null;
    }

    native void PushFrame(byte[] data, int length, long userData_);

    public void onPreviewFrame(byte[] data, Camera camera)
    {
	if (isRunning) {
	    PushFrame(data, data.length, userData);
	}
    }

    public void surfaceChanged(SurfaceHolder holder,
	int format, int width, int height)
    {
	Log.d(TAG, "VideoCaptureAndroid::surfaceChanged");
    }

    public void surfaceCreated(SurfaceHolder holder)
    {
	Log.d(TAG, "VideoCaptureAndroid::surfaceCreated");
	try {
	    if(camera != null) {
		camera.setPreviewDisplay(holder);
	    }
	} catch (IOException e) {
	    Log.e(TAG, "Failed to set preview surface!", e);
	}
    }

    public void surfaceDestroyed(SurfaceHolder holder)
    {
	Log.d(TAG, "VideoCaptureAndroid::surfaceDestroyed");
	try {
	    if(camera != null) {
		camera.setPreviewDisplay(null);
	    }
	} catch (IOException e) {
	    Log.e(TAG, "Failed to clear preview surface!", e);
	} catch (RuntimeException e) {
	    Log.w(TAG, "Clear preview surface useless", e);
	}
    }

}
