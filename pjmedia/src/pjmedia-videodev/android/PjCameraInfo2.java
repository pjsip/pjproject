/*
 * Copyright (C) 2021 Teluu Inc. (http://www.teluu.com)
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

import java.util.Arrays;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.util.Log;
import android.util.Size;
import android.graphics.ImageFormat;

public class PjCameraInfo2 {
    public String id;
    public int facing;			// 0: back, 1: front, 2: external
    public int orient;
    public int[] supportedSize;		// [w1, h1, w2, h2, ...]
    public int[] supportedFps1000;	// [min1, max1, min2, max2, ...]
    public int[] supportedFormat;	// [fmt1, fmt2, ...]

    private static final String TAG = "PjCameraInfo2";

    /* The camera2 is a bit tricky with format, for example it reports
     * for I420 support (and no NV21 support), however the incoming frame
     * buffers are actually in NV21 format (e.g: pixel stride is 2).
     *
     * For now, we only support I420.
     */
    private static final int[] PJ_SUPPORTED_FORMAT = {
    	ImageFormat.YUV_420_888,	// PJMEDIA_FORMAT_I420
    };

    private static boolean is_inited = false;
    private static PjCameraInfo2[] camInfo = null;
    private static int camInfoCnt = 0;
    private static CameraManager cameraManager = null;

    public static void SetCameraManager(CameraManager cm) {
        cameraManager = cm;
    }

    public static CameraManager GetCameraManager() {
	return cameraManager;
    }

    private static int Refresh() {
	CameraManager cm = GetCameraManager();
	if (cm==null) {
	    Log.e(TAG, "Need camera manager instance for enumerating camera");
	    return -1;
	}

	is_inited = false;

	String[] camIds;
	try {
	    camIds = cm.getCameraIdList();
	} catch (CameraAccessException e) {
	    Log.d(TAG, e.getMessage());
	    e.printStackTrace();
	    return -1;
	}

	camInfo = new PjCameraInfo2[camIds.length];
	camInfoCnt = 0;

	Log.i(TAG, "Found " + camIds.length + " cameras:");
	for (int i = 0; i < camIds.length; ++i) {
	    Integer facing;
	    CameraCharacteristics cc;
	    StreamConfigurationMap scm;
	    int[] outFmts;

	    /* Query basic info */
	    try {
		cc = cm.getCameraCharacteristics(camIds[i]);
		facing = cc.get(CameraCharacteristics.LENS_FACING);
		scm = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
		outFmts = scm.getOutputFormats();
	    } catch (Exception e) {
		Log.d(TAG, e.getMessage());
		Log.w(TAG, String.format("%3d: id=%s skipped due to failure in querying info",
			i, camIds[i]));
		continue;
	    }

	    PjCameraInfo2 pjci = new PjCameraInfo2();
	    pjci.id = camIds[i];
	    switch(facing) {
		case CameraCharacteristics.LENS_FACING_BACK:
		    pjci.facing = 0;
		    break;
		case CameraCharacteristics.LENS_FACING_FRONT:
		    pjci.facing = 1;
		    break;
		default:
		    pjci.facing = 2;
		    break;
	    }

	    int[] fmts = new int[outFmts.length];
	    int fmtCnt = 0;
	    for (int pjFmt : PJ_SUPPORTED_FORMAT) {
		for (int outFmt : outFmts) {
		    if (outFmt == pjFmt) {
			fmts[fmtCnt++] = pjFmt;
			break;
		    }
		}
	    }
	    Log.i(TAG, String.format("%3d: id=%s formats=%s",
		    i, pjci.id, Arrays.toString(outFmts)));
	    if (fmtCnt == 0) {
		Log.w(TAG, String.format("%3d: id=%s skipped due to no PJSIP compatible format",
			i, pjci.id));
		continue;
	    }

	    pjci.supportedFormat = new int[fmtCnt];
	    System.arraycopy(fmts, 0, pjci.supportedFormat, 0, fmtCnt);

	    /* Query additional info */
	    try {
		pjci.orient = cc.get(CameraCharacteristics.SENSOR_ORIENTATION);
	    } catch (Exception e) {
		Log.d(TAG, e.getMessage());
		Log.w(TAG, String.format("%3d: id=%s failed in getting orient",
			i, camIds[i]));
	    }

	    try {
		Size[] suppSize = scm.getOutputSizes(pjci.supportedFormat[0]);
		pjci.supportedSize = new int[suppSize.length * 2];
		for (int j = 0; j < suppSize.length; ++j) {
		    pjci.supportedSize[j*2] 	= suppSize[j].getWidth();
		    pjci.supportedSize[j*2 + 1] = suppSize[j].getHeight();
		}
	    } catch (Exception e) {
		Log.d(TAG, e.getMessage());
		Log.w(TAG, String.format("%3d: id=%s failed in getting sizes",
			i, camIds[i]));

		/* Query failed, let's just hardcode the supported sizes */
		pjci.supportedSize = new int[] {176, 144, 320, 240, 352, 288, 640, 480, 1280, 720, 1920, 1080};
	    }

	    /* FPS info is not really used for now, let's just hardcode it */
	    pjci.supportedFps1000 = new int[] { 1000, 30000 };

	    camInfo[camInfoCnt++] = pjci;

	    Log.i(TAG, String.format("%3d: id=%s facing=%d orient=%d formats=%s sizes=%s",
		    		     i, pjci.id, pjci.facing, pjci.orient,
		    		     Arrays.toString(pjci.supportedFormat),
		    		     Arrays.toString(pjci.supportedSize)));
	}

	is_inited = true;
	return 0;
    }

    public static int GetCameraCount()
    {
        /* Always refresh */
	if (Refresh() != 0) {
	    return -1;
	}
	return camInfoCnt;
    }

    /* Get camera info: facing, orientation, supported size/fps/format. */
    public static PjCameraInfo2 GetCameraInfo(int idx)
    {
	if (!is_inited) {
	    Log.e(TAG, "Not initalized");
	    return null;
	}

	if (idx < 0 || idx >= camInfoCnt) {
	    Log.e(TAG, "Invalid camera ID");
	    return null;
	}

	return camInfo[idx];
    }
}
