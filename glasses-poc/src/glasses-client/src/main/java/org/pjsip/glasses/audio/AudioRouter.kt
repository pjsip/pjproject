package org.pjsip.glasses.audio

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build

/**
 * Routes call audio to the glasses. On Horizon OS (VR Glasses, on-device
 * PJSIP) the built-in mic/speaker are the default route. On tethered AI
 * Glasses the phone runs PJSIP and audio goes over Bluetooth SCO/LE Audio;
 * PJSUA2's Android audio backend inherits whatever route AudioManager has
 * when the sound device opens, so set it *before* the first call.
 */
class AudioRouter(ctx: Context) {
    private val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var previousMode = AudioManager.MODE_NORMAL

    fun enterCallMode() {
        previousMode = am.mode
        am.mode = AudioManager.MODE_IN_COMMUNICATION
        val glasses = findGlassesDevice() ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            am.setCommunicationDevice(glasses)
        } else {
            @Suppress("DEPRECATION")
            am.startBluetoothSco()
        }
    }

    fun leaveCallMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) am.clearCommunicationDevice()
        else @Suppress("DEPRECATION") am.stopBluetoothSco()
        am.mode = previousMode
    }

    private fun findGlassesDevice(): AudioDeviceInfo? {
        val candidates = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            am.availableCommunicationDevices else emptyList()
        return candidates.firstOrNull { it.type == AudioDeviceInfo.TYPE_BLE_HEADSET }
            ?: candidates.firstOrNull { it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO }
            ?: candidates.firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE }
    }
}
