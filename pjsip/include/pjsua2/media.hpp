/* $Id$ */
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

#ifndef __PJSUA2_MEDIA_HPP__
#define __PJSUA2_MEDIA_HPP__

/**
 * @file pjsua2/media.hpp
 * @brief PJSUA2 media operations
 */
#include <pjsua-lib/pjsua.h>
#include <pjsua2/types.hpp>

/**
 * @defgroup PJSUA2_MED Media
 * @ingroup PJSUA2_Ref
 * @{
 */

/** PJSUA2 API is inside pj namespace */
namespace pj
{
using std::string;
using std::vector;

/**
 * This structure contains all the information needed to completely describe
 * a media.
 */
struct MediaFormat
{
    /**
     * The format id that specifies the audio sample or video pixel format.
     * Some well known formats ids are declared in pjmedia_format_id
     * enumeration.
     *
     * @see pjmedia_format_id
     */
    pj_uint32_t		id;

    /**
     * The top-most type of the media, as an information.
     */
    pjmedia_type	type;
};

/**
 * This structure describe detail information about an audio media.
 */
struct MediaFormatAudio : public MediaFormat
{
    unsigned	clockRate;	/**< Audio clock rate in samples or Hz. */
    unsigned	channelCount;	/**< Number of channels.		*/
    unsigned	frameTimeUsec;  /**< Frame interval, in microseconds.	*/
    unsigned	bitsPerSample;	/**< Number of bits per sample.		*/
    pj_uint32_t	avgBps;		/**< Average bitrate			*/
    pj_uint32_t	maxBps;		/**< Maximum bitrate			*/

    /**
     * Construct from pjmedia_format.
     */
    void fromPj(const pjmedia_format &format);

    /**
     * Export to pjmedia_format.
     */
    pjmedia_format toPj() const;
};

/**
 * This structure describe detail information about an video media.
 */
struct MediaFormatVideo : public MediaFormat
{
    unsigned		width;	    /**< Video width. 	*/
    unsigned		height;	    /**< Video height. 	*/
    int			fpsNum;	    /**< Frames per second numerator.	*/
    int			fpsDenum;   /**< Frames per second denumerator.	*/
    pj_uint32_t		avgBps;	    /**< Average bitrate.		*/
    pj_uint32_t		maxBps;	    /**< Maximum bitrate.		*/
};

/** Array of MediaFormat */
typedef std::vector<MediaFormat*> MediaFormatVector;

/**
 * This structure descibes information about a particular media port that
 * has been registered into the conference bridge. 
 */
struct ConfPortInfo
{
    /**
     * Conference port number.
     */
    int			portId;

    /**
     * Port name.
     */
    string		name;

    /**
     * Media audio format information
     */
    MediaFormatAudio	format;

    /**
     * Array of listeners (in other words, ports where this port is
     * transmitting to.
     */
    IntVector		listeners;

public:
    /**
     * Construct from pjsua_conf_port_info.
     */
    void fromPj(const pjsua_conf_port_info &port_info);

};

/**
 * Media port, corresponds to pjmedia_port
 */
typedef void *MediaPort;

/**
 * Media.
 */
class Media
{
public:
    /**
     * Virtual destructor.
     */
    virtual ~Media();

    /**
     * Get type of the media.
     *
     * @return          The media type.
     */
    pjmedia_type getType() const;

protected:
    /**
     * Constructor.
     */
    Media(pjmedia_type med_type);

private:
    /**
     * Media type.
     */
    pjmedia_type        type;
};

/**
 * Audio Media.
 */
class AudioMedia : public Media
{
public:
    /**
    * Get information about the specified conference port.
    */
    ConfPortInfo getPortInfo() const throw(Error);

    /**
     * Get port Id.
     */
    int getPortId() const;

    /**
     * Get information from specific port id.
     */
    static ConfPortInfo getPortInfoFromId(int port_id) throw(Error);

    /**
     * Establish unidirectional media flow to sink. This media port
     * will act as a source, and it may transmit to multiple destinations/sink.
     * And if multiple sources are transmitting to the same sink, the media
     * will be mixed together. Source and sink may refer to the same Media,
     * effectively looping the media.
     *
     * If bidirectional media flow is desired, application needs to call
     * this method twice, with the second one called from the opposite source
     * media.
     *
     * @param sink		The destination Media.
     */
    void startTransmit(const AudioMedia &sink) const throw(Error);

    /**
     *  Stop media flow to destination/sink port.
     *
     * @param sink		The destination media.
     *
     */
    void stopTransmit(const AudioMedia &sink) const throw(Error);

    /**
     * Adjust the signal level to be transmitted from the bridge to this
     * media port by making it louder or quieter.
     *
     * @param level		Signal level adjustment. Value 1.0 means no
     *				level adjustment, while value 0 means to mute
     *				the port.
     */
    void adjustRxLevel(float level) throw(Error);

    /**
     * Adjust the signal level to be received from this media port (to
     * the bridge) by making it louder or quieter.
     *
     * @param level		Signal level adjustment. Value 1.0 means no
     *				level adjustment, while value 0 means to mute
     *				the port.
     */
    void adjustTxLevel(float level) throw(Error);

    /**
     * Get the last received signal level.
     */
    unsigned getRxLevel() const throw(Error);

    /**
     * Get the last transmitted signal level.
     */
    unsigned getTxLevel() const throw(Error);

    /**
     * Virtual Destructor
     */
    virtual ~AudioMedia();

protected:
    /**
     * Conference port Id.
     */
    int			 id;

protected:
    /**
     * Default Constructor.
     */
    AudioMedia();

    /**
     * This method needs to be called by descendants of this class to register
     * the media port created to the conference bridge and Endpoint's
     * media list.
     *
     * param port  the media port to be registered to the conference bridge.
     *
     */
    void registerMediaPort(MediaPort port) throw(Error);

    /**
     * This method needs to be called by descendants of this class to remove
     * the media port from the conference bridge and Endpoint's media list.
     * Descendant should only call this method if it has registered the media
     * with the previous call to registerMediaPort().
     */
    void unregisterMediaPort();

private:
    pj_caching_pool 	 mediaCachingPool;
    pj_pool_t 		*mediaPool;

private:
    unsigned getSignalLevel(bool is_rx = true) const throw(Error);
};

/** Array of Audio Media */
typedef std::vector<AudioMedia*> AudioMediaVector;

/**
 * Audio Media Player.
 */
class AudioMediaPlayer : public AudioMedia
{
public:
    /** 
     * Constructor.
     */
    AudioMediaPlayer();

    /**
     * Create a file player,  and automatically add this 
     * player to the conference bridge.
     *
     * @param file_name	 The filename to be played. Currently only
     *			 WAV files are supported, and the WAV file MUST be
     *			 formatted as 16bit PCM mono/single channel (any
     *			 clock rate is supported).
     * @param options	 Optional option flag. Application may specify
     *			 PJMEDIA_FILE_NO_LOOP to prevent playback loop.
     */
    void createPlayer(const string &file_name,
		      unsigned options=PJMEDIA_FILE_NO_LOOP) throw(Error);

    /**
     * Create a file playlist media port, and automatically add the port
     * to the conference bridge.
     *
     * @param file_names  Array of file names to be added to the play list.
     *			  Note that the files must have the same clock rate,
     *			  number of channels, and number of bits per sample.
     * @param label	  Optional label to be set for the media port.
     * @param options	  Optional option flag. Application may specify
     *			  PJMEDIA_FILE_NO_LOOP to prevent looping.
     */
    void createPlaylist(const StringVector &file_names,
			const string &label="",
			unsigned options=PJMEDIA_FILE_NO_LOOP) throw(Error);

    /**
     * Set playback position. This operation is not valid for playlist.
     */
    void setPos(pj_uint32_t samples) throw(Error);

    /**
     * Virtual destructor.
     */
    virtual ~AudioMediaPlayer();

private:
    /**
     * Player Id.
     */
    int	playerId;

};

/**
 * Audio Media Recorder.
 */
class AudioMediaRecorder : public AudioMedia
{
public:
    /**
     * Constructor.
     */
    AudioMediaRecorder();

    /**
     * Create a file recorder, and automatically connect this recorder to
     * the conference bridge. The recorder currently supports recording WAV
     * file. The type of the recorder to use is determined by the extension of
     * the file (e.g. ".wav").
     *
     * @param file_name	 Output file name. The function will determine the
     *			 default format to be used based on the file extension.
     *			 Currently ".wav" is supported on all platforms.
     * @param enc_type	 Optionally specify the type of encoder to be used to
     *			 compress the media, if the file can support different
     *			 encodings. This value must be zero for now.
     * @param max_size	 Maximum file size. Specify zero or -1 to remove size
     *			 limitation. This value must be zero or -1 for now.
     * @param options	 Optional options.
     */
    void createRecorder(const string &file_name,
			unsigned enc_type=0,
			pj_ssize_t max_size=0,
			unsigned options=PJMEDIA_FILE_WRITE_PCM) throw(Error);

    /**
     * Virtual destructor.
     */
    virtual ~AudioMediaRecorder();

private:
    /**
     * Recorder Id.
     */
    int	recorderId;
};

/*************************************************************************
* Sound device management
*/

/**
 * Audio device information structure.
 */
struct AudioDevInfo
{
    /**
     * The device name
     */
    string name;

    /**
     * Maximum number of input channels supported by this device. If the
     * value is zero, the device does not support input operation (i.e.
     * it is a playback only device).
     */
    unsigned inputCount;

    /**
     * Maximum number of output channels supported by this device. If the
     * value is zero, the device does not support output operation (i.e.
     * it is an input only device).
     */
    unsigned outputCount;

    /**
     * Default sampling rate.
     */
    unsigned defaultSamplesPerSec;

    /**
     * The underlying driver name
     */
    string driver;

    /**
     * Device capabilities, as bitmask combination of pjmedia_aud_dev_cap.
     */
    unsigned caps;

    /**
     * Supported audio device routes, as bitmask combination of
     * pjmedia_aud_dev_route. The value may be zero if the device
     * does not support audio routing.
     */
    unsigned routes;

    /**
     * Array of supported extended audio formats
     */
    MediaFormatVector extFmt;

    /**
     * Construct from pjmedia_aud_dev_info.
     */
    void fromPj(const pjmedia_aud_dev_info &dev_info);

    /**
     * Destructor.
     */
    ~AudioDevInfo();
};

/** Array of audio device info */
typedef std::vector<AudioDevInfo*> AudioDevInfoVector;

/**
 * Audio device manager.
 */
class AudDevManager
{
public:
    /**
     * Get currently active capture sound devices. If sound devices has not been
     * created, it is possible that the function returns -1 as device IDs.
     *
     * @return 	Device ID of the capture device.
     */
    int getCaptureDev() const throw(Error);

    /**
     * Get currently active playback sound devices. If sound devices has not
     * been created, it is possible that the function returns -1 as device IDs.
     *
     * @return 	Device ID of the playback device.
     */
    int getPlaybackDev() const throw(Error);

    /**
     * Select or change capture sound device. Application may call this
     * function at any time to replace current sound device.
     *
     * @param capture dev   	Device ID of the capture device.
     */
    void setCaptureDev(int capture_dev) const throw(Error);

    /**
     * Select or change playback sound device. Application may call this
     * function at any time to replace current sound device.
     *
     * @param playback_dev   	Device ID of the playback device.
     */
    void setPlaybackDev(int playback_dev) const throw(Error);

    /**
     * Enum all audio devices installed in the system.
     *
     * @return		The list of audio device info.
     */
    const AudioDevInfoVector &enumDev() throw(Error);

    /**
     * Set pjsua to use null sound device. The null sound device only provides
     * the timing needed by the conference bridge, and will not interract with
     * any hardware.
     *
     */
    void setNullDev() throw(Error);

    /**
     * Disconnect the main conference bridge from any sound devices, and let
     * application connect the bridge to it's own sound device/master port.
     *
     * @return		The port interface of the conference bridge,
     *			so that application can connect this to it's own
     *			sound device or master port.
     */
    MediaPort *setNoDev();

    /**
     * Change the echo cancellation settings.
     *
     * The behavior of this function depends on whether the sound device is
     * currently active, and if it is, whether device or software AEC is
     * being used.
     *
     * If the sound device is currently active, and if the device supports AEC,
     * this function will forward the change request to the device and it will
     * be up to the device on whether support the request. If software AEC is
     * being used (the software EC will be used if the device does not support
     * AEC), this function will change the software EC settings. In all cases,
     * the setting will be saved for future opening of the sound device.
     *
     * If the sound device is not currently active, this will only change the
     * default AEC settings and the setting will be applied next time the
     * sound device is opened.
     *
     * @param tail_msec		The tail length, in miliseconds. Set to zero to
     *				disable AEC.
     * @param options		Options to be passed to pjmedia_echo_create().
     *				Normally the value should be zero.
     *
     */
    void setEcOptions(unsigned tail_msec, unsigned options) throw(Error);

    /**
     * Get current echo canceller tail length.
     *
     * @return		The EC tail length in milliseconds,
     *			If AEC is disabled, the value will be zero.
     */
    unsigned getEcTail() const throw(Error);

    /**
     * Check whether the sound device is currently active. The sound device
     * may be inactive if the application has set the auto close feature to
     * non-zero (the sndAutoCloseTime setting in MediaConfig), or
     * if null sound device or no sound device has been configured via the
     * setNoDev() function.
     */
    bool sndIsActive() const;

    /**
     * Refresh the list of sound devices installed in the system. This method
     * will only refresh the list of audio device so all active audio streams
     * will be unaffected. After refreshing the device list, application MUST
     * make sure to update all index references to audio devices before calling
     * any method that accepts audio device index as its parameter.
     *
     */
    void refreshDevs() throw(Error);

    /**
     * Get the number of sound devices installed in the system.
     *
     * @return 	The number of sound devices installed in the system.
     *
     */
    unsigned getDevCount() const;

    /**
     * Get device information.
     *
     * @param id		The audio device ID.
     *
     * @return			The device information which will be filled in
     * 				by this method once it returns successfully.
     */
    AudioDevInfo getDevInfo(int id) const throw(Error);

    /**
     * Lookup device index based on the driver and device name.
     *
     * @param drv_name	The driver name.
     * @param dev_name	The device name.
     *
     * @return		The device ID. If the device is not found, Error will be
     * 			thrown.
     *
     */
    int lookupDev(const string &drv_name,
		  const string &dev_name) const throw(Error);

    /**
     * Get string info for the specified capability.
     *
     * @param cap		The capability ID.
     *
     * @return			Capability name.
     */
    string capName(pjmedia_aud_dev_cap cap) const;

    /**
     * This will configure audio format capability (other than PCM) to the
     * sound device being used. If sound device is currently active, the method
     * will forward the setting to the sound device instance to be applied
     * immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_EXT_FORMAT capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param format	The audio format.
     * @param keep	Specify whether the setting is to be kept for
     * 			future use.
     *
     */
    void
    setExtFormat(const MediaFormatAudio &format, bool keep=true) throw(Error);

    /**
     * Get the audio format capability (other than PCM) of the sound device
     * being used. If sound device is currently active, the method will forward
     * the request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_EXT_FORMAT capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * @return	    The audio format.
     *
     */
    MediaFormatAudio getExtFormat() const throw(Error);

    /**
     * This will configure audio input latency control or query capability to
     * the sound device being used. If sound device is currently active,
     * the method will forward the setting to the sound device instance to be
     * applied immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param latency_msec	    The input latency.
     * @param keep		    Specify whether the setting is to be kept
     *				    for future use.
     *
     */
    void
    setInputLatency(unsigned latency_msec, bool keep=true) throw(Error);

    /**
     * Get the audio input latency control or query capability of the sound
     * device being used. If sound device is currently active, the method will
     * forward the request to the sound device. If sound device is currently
     * inactive, and if application had previously set the setting and mark the
     * setting as kept, then that setting will be returned. Otherwise, this
     * method will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * @return	    The audio input latency.
     *
     */
    unsigned getInputLatency() const throw(Error);

    /**
     * This will configure audio output latency control or query capability to
     * the sound device being used. If sound device is currently active,
     * the method will forward the setting to the sound device instance to be
     * applied immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param latency_msec    	The output latency.
     * @param keep		Specify whether the setting is to be kept
     * 				for future use.
     *
     */
    void
    setOutputLatency(unsigned latency_msec, bool keep=true) throw(Error);

    /**
     * Get the audio output latency control or query capability of the sound
     * device being used. If sound device is currently active, the method will
     * forward the request to the sound device. If sound device is currently
     * inactive, and if application had previously set the setting and mark the
     * setting as kept, then that setting will be returned. Otherwise, this
     * method will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY capability in AudioDevInfo.caps flags,
     * otherwise Error will be thrown.
     *
     * @return	    The audio output latency.
     *
     */
    unsigned getOutputLatency() const throw(Error);

    /**
     * This will configure audio input volume level capability to the
     * sound device being used.
     * If sound device is currently active, the method will forward the
     * setting to the sound device instance to be applied immediately,
     * if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param volume	The input volume level, in percent.
     * @param keep	Specify whether the setting is to be kept for future
     * 			use.
     *
     */
    void setInputVolume(unsigned volume, bool keep=true) throw(Error);

    /**
     * Get the audio input volume level capability of the sound device being
     * used. If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.     *

     * @return	    The audio input volume level, in percent.
     *
     */
    unsigned getInputVolume() const throw(Error);

    /**
     * This will configure audio output volume level capability to the sound
     * device being used. If sound device is currently active, the method will
     * forward the setting to the sound device instance to be applied
     * immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param volume	The output volume level, in percent.
     * @param keep	Specify whether the setting is to be kept
     * 			for future use.
     *
     */
    void setOutputVolume(unsigned volume, bool keep=true) throw(Error);

    /**
     * Get the audio output volume level capability of the sound device being
     * used. If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * @return	    The audio output volume level, in percent.
     *
     */
    unsigned getOutputVolume() const throw(Error);

    /**
     * Get the audio input signal level capability of the sound device being
     * used. If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * @return	    The audio input signal level, in percent.
     *
     */
    unsigned getInputSignal() const throw(Error);

    /**
     * Get the audio output signal level capability of the sound device being
     * used. If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * @return	    The audio output signal level, in percent.
     *
     */
    unsigned getOutputSignal() const throw(Error);

    /**
     * This will configure audio input route capability to the sound device
     * being used. If sound device is currently active, the method will
     * forward the setting to the sound device instance to be applied
     * immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param route	The audio input route.
     * @param keep	Specify whether the setting is to be kept
     * 			for future use.
     *
     */
    void
    setInputRoute(pjmedia_aud_dev_route route, bool keep=true) throw(Error);

    /**
     * Get the audio input route capability of the sound device being used.
     * If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * @return	    The audio input route.
     *
     */
    pjmedia_aud_dev_route getInputRoute() const throw(Error);

    /**
     * This will configure audio output route capability to the sound device
     * being used. If sound device is currently active, the method will
     * forward the setting to the sound device instance to be applied
     * immediately, if it supports it.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param route	The audio output route.
     * @param keep	Specify whether the setting is to be kept
     * 			for future use.
     *
     */
    void
    setOutputRoute(pjmedia_aud_dev_route route, bool keep=true) throw(Error);

    /**
     * Get the audio output route capability of the sound device being used.
     * If sound device is currently active, the method will forward the
     * request to the sound device. If sound device is currently inactive,
     * and if application had previously set the setting and mark the setting
     * as kept, then that setting will be returned. Otherwise, this method
     * will raise error.
     *
     * This method is only valid if the device has
     * PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE capability in AudioDevInfo.caps
     * flags, otherwise Error will be thrown.
     *
     * @return	    The audio output route.
     *
     */
    pjmedia_aud_dev_route getOutputRoute() const throw(Error);

    /**
     * This will configure audio voice activity detection capability to
     * the sound device being used. If sound device is currently active,
     * the method will forward the setting to the sound device instance
     * to be applied immediately, if it supports it.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_VAD
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param enable		Enable/disable voice activity detection
     *				feature. Set true to enable.
     * @param keep		Specify whether the setting is to be kept for
     *				future use.
     *
     */
    void setVad(bool enable, bool keep=true) throw(Error);

    /**
     * Get the audio voice activity detection capability of the sound device
     * being used. If sound device is currently active, the method will
     * forward the request to the sound device. If sound device is currently
     * inactive, and if application had previously set the setting and mark
     * the setting as kept, then that setting will be returned. Otherwise,
     * this method will raise error.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_VAD
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * @return	    The audio voice activity detection feature.
     *
     */
    bool getVad() const throw(Error);

    /**
     * This will configure audio comfort noise generation capability to
     * the sound device being used. If sound device is currently active,
     * the method will forward the setting to the sound device instance
     * to be applied immediately, if it supports it.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_CNG
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param enable		Enable/disable comfort noise generation
     *				feature. Set true to enable.
     * @param keep		Specify whether the setting is to be kept for
     *				future use.
     *
     */
    void setCng(bool enable, bool keep=true) throw(Error);

    /**
     * Get the audio comfort noise generation capability of the sound device
     * being used. If sound device is currently active, the method will
     * forward the request to the sound device. If sound device is currently
     * inactive, and if application had previously set the setting and mark
     * the setting as kept, then that setting will be returned. Otherwise,
     * this method will raise error.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_CNG
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * @return	    The audio comfort noise generation feature.
     *
     */
    bool getCng() const throw(Error);

    /**
     * This will configure audio packet loss concealment capability to
     * the sound device being used. If sound device is currently active,
     * the method will forward the setting to the sound device instance
     * to be applied immediately, if it supports it.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_PLC
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * Note that in case the setting is kept for future use, it will be applied
     * to any devices, even when application has changed the sound device to be
     * used.
     *
     * @param enable		Enable/disable packet loss concealment
     *				feature. Set true to enable.
     * @param keep		Specify whether the setting is to be kept for
     *				future use.
     *
     */
    void setPlc(bool enable, bool keep=true) throw(Error);

    /**
     * Get the audio packet loss concealment capability of the sound device
     * being used. If sound device is currently active, the method will
     * forward the request to the sound device. If sound device is currently
     * inactive, and if application had previously set the setting and mark
     * the setting as kept, then that setting will be returned. Otherwise,
     * this method will raise error.
     *
     * This method is only valid if the device has PJMEDIA_AUD_DEV_CAP_PLC
     * capability in AudioDevInfo.caps flags, otherwise Error will be thrown.
     *
     * @return	    The audio packet loss concealment feature.
     *
     */
    bool getPlc() const throw(Error);

private:
    AudioDevInfoVector		 audioDevList;

    /**
     * Constructor.
     */
    AudDevManager();

    /**
     * Destructor.
     */
    ~AudDevManager();

    void clearAudioDevList();
    int getActiveDev(bool is_capture) const throw(Error);

    friend class Endpoint;
};

/*************************************************************************
* Codec management
*/

/**
 * This structure describes codec information.
 */
struct CodecInfo
{
    /**
     * Codec unique identification.
     */
    string		codecId;

    /**
     * Codec priority (integer 0-255).
     */
    pj_uint8_t		priority;

    /**
     * Codec description.
     */
    string		desc;

    /**
     * Construct from pjsua_codec_info.
     */
    void fromPj(const pjsua_codec_info &codec_info);
};

/** Array of codec info */
typedef std::vector<CodecInfo*> CodecInfoVector;

/**
 * Codec parameters, corresponds to pjmedia_codec_param or
 * pjmedia_vid_codec_param.
 */
typedef void *CodecParam;

} // namespace pj

/**
 * @}  // PJSUA2_MED
 */

#endif	/* __PJSUA2_MEDIA_HPP__ */
