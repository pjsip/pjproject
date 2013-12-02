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
     * with the previous call to #registerMediaPort().
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

} // namespace pj

/**
 * @}  // PJSUA2_MED
 */

#endif	/* __PJSUA2_MEDIA_HPP__ */
