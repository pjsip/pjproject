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

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;
import android.util.SparseArray;
import java.util.Arrays;

public class PjAudioDevInfo {

    /* API */

    public int id;
    public String name;
    public int direction;
    public int[] supportedClockRates;
    public int[] supportedChannelCounts;

    public static int GetCount()
    {
        return devices.size();
    }

    public static PjAudioDevInfo GetInfo(int idx)
    {
        return devices.valueAt(idx);
    }

    public static void RefreshDevices(Context context)
    {
        devices = new SparseArray<>();

        /* Default device */
        PjAudioDevInfo pj_adi = new PjAudioDevInfo();
        pj_adi.id = 0;
        pj_adi.name = "Default";
        pj_adi.direction = 3;
        devices.put(0, pj_adi);

        /* Enumerate devices (for API level 23 or later) */
        if (android.os.Build.VERSION.SDK_INT < 23)
            return;

        AudioManager am = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
        AudioDeviceInfo[] devs = am.getDevices(AudioManager.GET_DEVICES_ALL);
        Log.i("Oboe", "Enumerating AudioManager devices..");
        for (AudioDeviceInfo adi: devs) {
            LogDevInfo(adi);

            pj_adi = new PjAudioDevInfo();
            pj_adi.id = adi.getId();
            pj_adi.name = DevTypeStr(adi.getType()) + " - " + adi.getProductName().toString();
            pj_adi.direction = 0;
            if (adi.isSource()) pj_adi.direction |= 1;
            if (adi.isSink())   pj_adi.direction |= 2;
            pj_adi.supportedChannelCounts = adi.getChannelCounts();
            pj_adi.supportedClockRates = adi.getSampleRates();
            devices.put(adi.getId(), pj_adi);
        }
    }

    /* Private members */
    
    private static SparseArray<PjAudioDevInfo> devices;

    private static String DevTypeStr(int type) {
        if (android.os.Build.VERSION.SDK_INT < 23)
            return "Unknown";

        /*
        if (android.os.Build.VERSION.SDK_INT > 30) {
            if (type == AudioDeviceInfo.TYPE_BLE_HEADSET)
                return "BLE Headset";
            if (type == AudioDeviceInfo.TYPE_BLE_SPEAKER)
                return "BLE Speaker";
        }
        */

        switch (type) {
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                return "Builtin Earpiece";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                return "Builtin Speaker";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                return "Wired Headset";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                return "Wired Headphones";
            case AudioDeviceInfo.TYPE_LINE_ANALOG:
                return "Line Analog";
            case AudioDeviceInfo.TYPE_LINE_DIGITAL:
                return "Line Digital";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                return "Bluetooth SCO";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                return "Bluetooth A2DP";
            case AudioDeviceInfo.TYPE_HDMI:
                return "HDMI";
            case AudioDeviceInfo.TYPE_HDMI_ARC:
                return "HDMI ARC";
            case AudioDeviceInfo.TYPE_USB_DEVICE:
                return "USB Device";
            case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                return "USB Accessory";
            case AudioDeviceInfo.TYPE_DOCK:
                return "Dock";
            case AudioDeviceInfo.TYPE_FM:
                return "FM";
            case AudioDeviceInfo.TYPE_BUILTIN_MIC:
                return "Builtin Mic";
            case AudioDeviceInfo.TYPE_FM_TUNER:
                return "FM Tuner";
            case AudioDeviceInfo.TYPE_TV_TUNER:
                return "TV Tuner";
            case AudioDeviceInfo.TYPE_TELEPHONY:
                return "Telephony";
            case AudioDeviceInfo.TYPE_AUX_LINE:
                return "AUX Line";
            case AudioDeviceInfo.TYPE_IP:
                return "IP";
            case AudioDeviceInfo.TYPE_BUS:
                return "Bus";
            case AudioDeviceInfo.TYPE_USB_HEADSET:
                return "USB Headset";
            default:
                return "Unknown ("+ type +")";
        }
    }

    private static void LogDevInfo(AudioDeviceInfo adi) {
        if (Build.VERSION.SDK_INT < 23)
            return;

        StringBuilder info = new StringBuilder();
        info.append("id=").append(adi.getId());
        info.append(",");
        if (adi.isSource()) info.append(" in");
        if (adi.isSink()) info.append(" out");
        info.append(", ").append(DevTypeStr(adi.getType()));
        if (adi.getChannelCounts().length > 0) {
            info.append(", channels=").append(Arrays.toString(adi.getChannelCounts()));
        }
        if (adi.getSampleRates().length > 0) {
            info.append(", clock rates=").append(Arrays.toString(adi.getSampleRates()));
        }
        Log.i("Oboe", info.toString());
    }
}

