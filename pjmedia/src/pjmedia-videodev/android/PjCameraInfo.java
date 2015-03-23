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

import java.util.List;

import android.hardware.Camera;
import android.util.Log;

public class PjCameraInfo {
    public int facing;
    public int orient;
    public int[] supportedSize;		// [w1, h1, w2, h2, ...]
    public int[] supportedFps1000;	// [min1, max1, min2, max2, ...]
    public int[] supportedFormat;	// [fmt1, fmt2, ...]

    // convert Format list {fmt1, fmt2, ...} to [fmt1, fmt2, ...]
    private static int[] IntegerListToIntArray(List<Integer> list)
    {
	int[] li = new int[list.size()];
	int i = 0;
	for (Integer e : list)  {
	    li[i++] = e.intValue();
	}
	return li;
    }

    // convert Fps list {[min1, max1], [min2, max2], ...} to 
    // [min1, max1, min2, max2, ...]
    private static int[] IntArrayListToIntArray(List<int[]> list)
    {
	int[] li = new int[list.size() * 2];
	int i = 0;
	for (int[] e : list)  {
	    li[i++] = e[0];
	    li[i++] = e[1];
	}
	return li;
    }

    // convert Size list {{w1, h1}, {w2, h2}, ...} to [w1, h1, w2, h2, ...]
    private static int[] CameraSizeListToIntArray(List<Camera.Size> list)
    {
	int[] li = new int[list.size() * 2];
	int i = 0;
	for (Camera.Size e : list)  {
	    li[i++] = e.width;
	    li[i++] = e.height;
	}
	return li;
    }

    public static int GetCameraCount()
    {
	return Camera.getNumberOfCameras();
    }

    // Get camera info: facing, orientation, supported size/fps/format.
    // Camera must not be opened.
    public static PjCameraInfo GetCameraInfo(int idx)
    {
	if (idx < 0 || idx >= GetCameraCount())
	    return null;

	Camera cam;
	try {
	    cam = Camera.open(idx);
	} catch (Exception e) {
	    Log.d("IOException", e.getMessage());
	    return null;
	}

	PjCameraInfo pjci = new PjCameraInfo();

	Camera.CameraInfo ci = new Camera.CameraInfo();
	Camera.getCameraInfo(idx, ci);

	pjci.facing = ci.facing;
	pjci.orient = ci.orientation;

	Camera.Parameters param = cam.getParameters();
	cam.release();
	cam = null;

	pjci.supportedFormat = IntegerListToIntArray(
				    param.getSupportedPreviewFormats());
	pjci.supportedFps1000 = IntArrayListToIntArray(
				    param.getSupportedPreviewFpsRange());
	pjci.supportedSize = CameraSizeListToIntArray(
				    param.getSupportedPreviewSizes());

	return pjci;
    }
}
