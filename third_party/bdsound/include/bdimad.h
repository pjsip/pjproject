 /**
 * @file bdIMADpj.h
 * @brief bdSound IMproved Audio Device for PJSIP.
 */
 
/**
 * @defgroup bd_IMAD bdIMADpj bdSound IMproved Audio Device for PJSIP.
 * @ingroup audio_device_api
 * 
 * <b>bdSound IMproved Audio Device</b> is a multi-platform audio interface
 * created to integrate in <b>PJSIP</b> library with no effort.
 * \n Porting <b>bdIMADpj</b> across the main operating systems is
 * straightforward, without the need of change a single line of code.
 *
 *    - <b>Features</b>
 *       - Echo cancellation (Full Duplex)
 *       - Noise reduction
 *       - Automatic Gain Control
 *       - Audio Enhancement
 *
 *    - <b>Supported operating systems</b>
 *       - Windows
 *       - Android
 *       - MacOS X
 *       - iOS
 *       - Linux / Alsa
 *
 *    - <b>Supported platforms</b>
 *       - x86
 *       - x64
 *       - ARM Cortex-A8/A9/A15 with NEON
 * 
 * Visit <a href="http:/www.bdsound.com" target="new">bdSound</a> for updated
 * features, supported operating systems and platforms.
 * 
 * <b>Using PJSIP with bdIMAD audio device</b>
 * 
 *    - <b>Integration</b>
 *    \n Using <b>bdIMAD</b> within <b>PJSIP</b> is simple:
 *       -# Download trial bdIMADpj library from
 *       <a href="http:/www.bdsound.com" target="new">bdSound</a>;
 *       -# Follow the integration instructions at 
 *		 <a href="http:/www.bdsound.com/support" target="new">bdSound</a>:
 *
 *    - <b>Usage</b>
 *    \n There are only a couple of things the customer have to pay attention on
 *    when using bdIMAD library.
 *
 *       - <b>Initialization</b>
 *       \n Since the bdIMAD library provides itself the echo cancellation
 *       and the latency management, is necessary to disable these features
 *       in the PJSIP library applications.
 *       \n For example in PJSUA sample application there is the need
 *       to provide the following commands:
 *       <pre>
 *       --ec-tail=0
 *       --no-vad
 *       --capture-lat=0
 *       --playback-lat=0
 *       </pre>
 * 
 *       - <b>Supported set capability</b>
 *          - <code>PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING</code>
 *          \n Setting speaker volume.
 *          - <code>PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING</code>
 *          \n Setting microphone volume.
 *          - <code>PJMEDIA_AUD_DEV_CAP_EC</code>
 *          \n Enable/disable echo cancellation.
 *          - <code>PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE</code>
 *          \n Support for audio output routing in mobile application(e.g. loudspeaker vs earpiece).
 *
 * For additional information visit
 * <a href="http:/www.bdsound.com" target="new">www.bdsound.com</a>
 * or write to info@bdsound.com
 *
 * @author bdSound
 * @version   2.0.0 rev.1618
 * @copyright 2015 bdSound srl. All rights reserved.
 *
 */

/**        
 * @defgroup groupFunction Functions      
 * @ingroup bd_IMAD  
 * 
 * Functions defined in bdIMAD.
 */

/**        
 * @defgroup groupCallback Callbacks      
 * @ingroup bd_IMAD    
 * 
 * Callbacks defined in bdIMAD.
 */
 
/**        
 * @defgroup groupStructEnum Structs and Enums      
 * @ingroup bd_IMAD   
 * 
 * Struct and Enum defined in bdIMAD.
 */

#ifndef BD_IMAD_PJ_H__
#define BD_IMAD_PJ_H__
  
/**
 * @brief Macro for Windows DLL Support.
 */
 
#ifdef _BDIMADPJ_EXPORTDLL
	#ifdef WIN32
		#define BDIMADPJ_API __declspec(dllexport)
	#else
		#define BDIMADPJ_API __attribute__((visibility("default")))	
	#endif
#else
	#define BDIMADPJ_API
#endif

#define BD_IMAD_CAPTURE_DEVICES      1
#define BD_IMAD_PLAYBACK_DEVICES     0
#define BD_IMAD_DIAGNOSTIC_ENABLE    1
#define BD_IMAD_DIAGNOSTIC_DISABLE   0

#define BD_IMAD_BITS_X_SAMPLE	     16                  /**< Bits per sample */

typedef void* bdIMADpj;

/**
 * @addtogroup groupCallback
 * @{
 */

/**
 * @brief Callback used to fill the playback buffer of bdIMAD.
 * The function is called by bdIMAD each time are required sample to be played.
 *
 * @param[in] *buffer				pointer to the buffer with the audio
 * 				      				samples to be played (short type).
 *
 * @param[in] nSamples 				number of samples required.
 *
 * @param[in] user_data				pointer to the user data structure
 * 				       				defined in the bdIMADpj_Setting_t structure.
 *
 * @return none.
 */

typedef int (* cb_fillPlayBackB_t) (void *buffer, int nSamples,
				    void *user_data);

/**
 * @brief Callback used to retrieve the capture buffer of bdIMAD. The function
 * is called by bdIMAD each time processed microphone samples are available.
 *
 * @param[out] *buffer              pointer to the buffer with the audio
 * 				       				samples to be downloaded (short type).
 *
 * @param[in] nSamples              number of samples processed to be downloaded.
 *
 * @param[in] user_data				pointer to the user data structure
 *                          		defined in the bdIMADpj_Setting_t structure.
 *
 * @return none.
 */

typedef void (* cb_emptyCaptureB_t) (void *buffer, int nSamples,
				     void *user_data);
/**
 * @}
 */

/**
 * @addtogroup groupStructEnum
 * @{
 */
 
/**
 * @brief Error status returned by some functions in the library.
 */

typedef enum bdIMADpj_Status {
    /** No error. */
    BD_PJ_OK                                     = 0,
    /** The warnings can be find in the bdIMADpj_Warnings_t structure . */
    BD_PJ_WARN_BDIMAD_WARNING_ASSERTED           = 1,
    /** Error not identified. */
    BD_PJ_ERROR_GENERIC                          = 2,
    /** The pointer passed is NULL. */
    BD_PJ_ERROR_NULL_POINTER                     = 3,
    /** Allocation procedure failed. */
    BD_PJ_ERROR_ALLOCATION                       = 4,
    /** The parameter is not existent or the set/get function is not active. */
    BD_PJ_ERROR_PARAMETER_NOT_FOUND              = 5,
    /** No capture device found. */
    BD_PJ_ERROR_IMAD_NONE_CAPTURE_DEV            = 10,
    /** No play device found. */
    BD_PJ_ERROR_IMAD_NONE_PLAY_DEV               = 11,
    /** Frame size not allowed. */
    BD_PJ_ERROR_IMAD_FRAME_SIZE                  = 12,
    /** Sample frequency not allowed. */
    BD_PJ_ERROR_IMAD_SAMPLE_FREQ                 = 13,
    /** Samples missing. */
    BD_PJ_ERROR_IMAD_MISSING_SAMPLES             = 14,
    /** Device list is empty. */
    BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY           = 15,
    /** Library not authorized, entering demo mode. */
    BD_PJ_ERROR_IMAD_LIB_NOT_AUTHORIZED          = 16,
    /** The input channel memory has not been allocated. */
    BD_PJ_ERROR_IMAD_INPUT_CH_NOT_ALLOCATED      = 17,
    /** The library has expired, entering demo mode. */
    BD_PJ_ERROR_IMAD_LICENSE_EXPIRED             = 18,
    /** Open of capture device failed. */
    BD_PJ_ERROR_IMAD_OPEN_CAPTURE_DEV_FAILED     = 19,
    /** Open of play device failed.  */
    BD_PJ_ERROR_IMAD_OPEN_PLAY_DEV_FAILED        = 20,
    /** Start of play device failed. */
    BD_PJ_ERROR_IMAD_START_PLAY_DEV_FAILED       = 21,
    /** Start of capture device failed. */
    BD_PJ_ERROR_IMAD_START_CAPTURE_DEV_FAILED    = 22,
    /** Start of time process failed. */
    BD_PJ_ERROR_IMAD_START_TIME_PROCESS_FAILED   = 23,
    /** Start of thread process failed. */
    BD_PJ_ERROR_IMAD_THREAD_PROCESS_FAILED       = 24,
    /** No volume control available. */
    BD_PJ_ERROR_IMAD_NO_VOL_CONTROL_AVAILABLE    = 25,
} bdIMADpj_Status;

/**
 * @brief Parameter to pass to set and get parameter functions. 
 *
 * For each enumeration are defined the data type and the supported operations
 * on that parameter (set and get).
 */
   
typedef enum bdIMADpj_Parameter {
    /** int*   \n set/get \n 1 enable / 0 disable echo cancellation. */
    BD_PARAM_IMAD_PJ_AEC_ENABLE									= 0,
	/** int*   \n set/get \n 0 -> 256 ms of echo tail. */
    BD_PARAM_IMAD_PJ_AEC_ECHO_TAIL_MS							= 1,
	/** int*   \n set/get \n 0 -> 400 ms of delay offset. */
    BD_PARAM_IMAD_PJ_AEC_DELAY_OFFSET_MS						= 2,
	/** int*   \n set/get \n 1 enable / 0 disable automatic delay estimation. */
    BD_PARAM_IMAD_PJ_AEC_AUTO_DELAY_ESTIMATION_ENABLE			= 3,
	/** int*   \n get \n estimated delay in ms. */
    BD_PARAM_IMAD_PJ_AEC_AUTO_DELAY_ESTIMATION_VALUE			= 4,
	/** int*   \n get \n 1 is stable / 0 not yet stable estimated delay. */
    BD_PARAM_IMAD_PJ_AEC_AUTO_DELAY_ESTIMATION_IS_STABLE		= 5,
    /** int*   \n set/get \n 1 enable / 0 disable Noise Reduction. */
    BD_PARAM_IMAD_PJ_NR_ENABLE									= 6,	
	/** int*   \n set/get \n 0 low / 1 medium / 2 high / 3 very high / 4 adaptive level of Noise Reduction. */
    BD_PARAM_IMAD_PJ_NR_LEVEL									= 7,
	/** int*   \n set/get \n 1 enable / 0 disable Comfort Noise Generator. */
    BD_PARAM_IMAD_PJ_CNG_ENABLE									= 8,
	/** int*   \n set/get \n 1 adaptive / 0 not adaptive mode of Comfort Noise Generator. */
    BD_PARAM_IMAD_PJ_CNG_SET_ADAPTIVE							= 9,
	/** int*   \n set/get \n -40 -> -100 dBFS fixed power level of Comfort Noise Generator, when adaptive mode is disabled. */
    BD_PARAM_IMAD_PJ_CNG_FIXED_LEVEL_DB							= 10,
	/** int*   \n set/get \n 0 minimal / 1 low / 2 intermediate / 3 high / 4 aggressive effort level of Residual Echo Canceller. */
    BD_PARAM_IMAD_PJ_REC_EFFORT_LEVEL							= 11,
	/** int*   \n set/get \n 1 enable / 0 disable Non Linear Processor. */
    BD_PARAM_IMAD_PJ_NLP_ENABLE									= 12,
	/** float*   \n set/get \n 6.0f -> 24.0f dB Double Talk Detector sensitivity. */
    BD_PARAM_IMAD_PJ_NLP_DTD_SENSITIVITY						= 13,
	/** float*   \n set/get \n -50.0f -> 0.0f dB maximum applicable gain by the Non Linear Processor. */
    BD_PARAM_IMAD_PJ_NLP_GAIN									= 14,
    /** int*   \n set/get \n 1 enable / 0 disable microphone control
     * (when possible). */
    BD_PARAM_IMAD_PJ_MIC_CONTROL_ENABLE							= 15,
    /** float* \n set/get \n 0.0f -> 1.0f volume of
     * the microphone(when possible). */
    BD_PARAM_IMAD_PJ_MIC_VOLUME									= 16,
    /** int*   \n set/get \n 0 mute / 1 not mute on microphone
     * (when possible). */
    BD_PARAM_IMAD_PJ_MIC_MUTE									= 17,
    /** float* \n set/get \n 0.0f -> 1.0f volume of the speaker. */
    BD_PARAM_IMAD_PJ_SPK_VOLUME									= 18,
    /** int*   \n set/get \n 0 mute / 1 not mute on speaker. */
    BD_PARAM_IMAD_PJ_SPK_MUTE									= 19,
} bdIMADpj_Parameter;

/**
 * @brief Direction path of the parameter to be used in set and get audio process parameter functions. 
 *
 */
typedef enum bdIMADpj_DirPath {
	/** Indicates the send direction path (microphone). */
	BD_IMAD_PJ_DIR_PATH_SEND	= 0,
	/** Indicates the receive direction path (speaker). */
	BD_IMAD_PJ_DIR_PATH_RECV	= 1,
} bdIMADpj_DirPath;

/**
 * @brief Audio processing parameter to pass to set and get audio processing parameter functions. 
 *
 * For each enumeration are defined the data type and the supported operations
 * on that parameter (set and get).
 */
   
typedef enum bdIMADpj_AudioProcessParameter {
    /** int*   \n set/get \n 1 enable / 0 disable whole audio processing chain. */
    BD_AP_PARAM_IMAD_PJ_AUDIO_PROC_ENABLE				= 200,
	/** int*   \n set/get \n 1 enable / 0 disable digital gain. */
    BD_AP_PARAM_IMAD_PJ_GAIN_ENABLE						= 201,
	/** float*   \n set/get \n 0.0f -> 20.0f dB digital gain value. */
    BD_AP_PARAM_IMAD_PJ_GAIN_VALUE_DB					= 202,
	/** int*   \n set/get \n 1 enable / 0 disable Automatic Gain Control. */
    BD_AP_PARAM_IMAD_PJ_AGC_ENABLE						= 203,
	/** float*   \n set/get \n 0.0f -> -30.0f dBFS target RMS power of the Automatic Gain Control. */
    BD_AP_PARAM_IMAD_PJ_AGC_TARGET_RMS_DB				= 204,
	/** float*   \n set/get \n 0.0f -> 15.0f dB maximum applicable gain by the Automatic Gain Control. */
    BD_AP_PARAM_IMAD_PJ_AGC_MAX_GAIN_DB					= 205,
	/** int*   \n set/get \n 1 enable / 0 disable Graphic Equalizer (10 bands). */
    BD_AP_PARAM_IMAD_PJ_GEQ_ENABLE						= 206,
	/** float*   \n set/get \n Graphic Equalizer (10 bands) frequencies and gains.  \n
	Float array of 30 elements, composed by 10 triplets (enable, frequency, gain_dB). \n		
	First field, enable: 1.0f enable / 0.0f disable the frequency filter. \n 
	Second field, frequency: 0.0f -> Fs/2 Hz centre frequency of the filter. \n
	Third field, gain_dB: -18.0f -> 18.0f dB gain of the filter. 
	Frequency values in ascending order.  \n */	
    BD_AP_PARAM_IMAD_PJ_GEQ_FREQ_GAIN					= 207,
	/** int*   \n set/get \n 1 enable / 0 disable Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_ENABLE						= 208,
	/** float*   \n set/get \n 0.0f -> 5000.0f ms attack time of gain processor of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_ATTACK_TIME_GAIN_MS			= 209,
	/** float*   \n set/get \n 0.0f -> 5000.0f ms release time of gain processor of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_RELEASE_TIME_GAIN_MS		= 210,
	/** float*   \n set/get \n 0.0f -> 5000.0f ms attack time of level measurement of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_ATTACK_TIME_LEVEL_MS		= 211,
	/** float*   \n set/get \n 0.0f -> 5000.0f ms release time of level measurement of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_RELEASE_TIME_LEVEL_MS		= 212,
	/** int*   \n set/get \n 0 -> 10 ms lookahead time of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_LOOK_AHEAD_MS				= 213,
	/** int*   \n set/get \n 1 RMS / 0 peak detector of the Compander selected. */
    BD_AP_PARAM_IMAD_PJ_CMP_RMS_DETECTOR				= 214,
	/** float*   \n set/get \n 0.0f -> 12.0f dB compensation gain of the Compander. */
    BD_AP_PARAM_IMAD_PJ_CMP_COMPENSATION_GAIN_DB		= 215,
	/** float*   \n set/get \n Float sorted array of static gain curve points in dB - up to 4 points: input0, output0, … , input3, output3. 
	Gain values: 0.0f -> -100.0f dB. */
    BD_AP_PARAM_IMAD_PJ_CMP_TABLE						= 216,
	/** int*   \n set/get \n 1 enable / 0 disable Limiter. */
    BD_AP_PARAM_IMAD_PJ_LIM_ENABLE						= 217,
	/** float*   \n set/get \n 0.0f -> -40.0f dBFS threshold value of the Limiter. */
    BD_AP_PARAM_IMAD_PJ_LIM_THRESHOLD					= 218,
} bdIMADpj_AudioProcessParameter;


/**
 * @brief Test signal type to be used in set and get test parameter functions. 
 *
 */
typedef enum bdIMADpj_TestSignalType {
	/** Indicates a sine wave. */
	BD_IMAD_PJ_TS_SINE		= 0,
	/** Indicates a White Gaussian Noise. */
	BD_IMAD_PJ_TS_WGN		= 1,
} bdIMADpj_TestSignalType;


/**
 * @brief Test parameter to pass to set and get test parameter functions. 
 *
 * For each enumeration are defined the data type and the supported operations
 * on that parameter (set and get).
 */
typedef enum bdIMADpj_TestParameter {
	/** int*   \n set/get \n 1 enable / 0 disable Test Signal. */
	BD_TEST_PARAM_IMAD_PJ_TEST_SIGNAL_ENABLE					= 400,
	/** bdIMADpj_TestSignalType*   \n set/get \n Test Signal type: sine or Wgn. */
	BD_TEST_PARAM_IMAD_PJ_TEST_SIGNAL_TYPE						= 401,
	/** int*   \n set/get \n 0 -> -100 dBFS amplitude of Test Signal. */
	BD_TEST_PARAM_IMAD_PJ_TEST_SIGNAL_AMPLITUDE					= 402,
	/** float*   \n set/get \n 0 -> Fs/2 Hz frequency of sine Test Signal . */
	BD_TEST_PARAM_IMAD_PJ_TEST_SIGNAL_FREQUENCY					= 403,
} bdIMADpj_TestParameter;

/**
 * @brief Side to be used in get VU meter level value function. 
 *
 */
typedef enum bdIMADpj_Side {
	/** Indicates the input side of a direction path. */
	BD_IMAD_PJ_SIDE_INPUT		= 0,
	/** Indicates the output side of a direction path. */
	BD_IMAD_PJ_SIDE_OUTPUT		= 1,
} bdIMADpj_Side;


/**
 * @brief Instance structure for the information regarding the AEC engine.
 */

typedef struct bdIMADpj_Setting_t {
    /** Sample frequency (8kHz - 16kHz - 32kHz - 44.1kHz - 48kHz). */
    int                 SamplingFrequency;
    /** Audio buffer managed by the AEC bdIMAD functions.
     * (from 16ms to 80ms, 16ms recommended). */
    int                 FrameSize_ms;
    /** Pointer to the validation functions in the validation library. */
    void                *validate;
    /** Pointer to the the callback function used for filling
     * the playback buffer of bdIMAD. */
    cb_fillPlayBackB_t  cb_fillPlayBackBuffer;
    /** Pointer to user data to pass to the callback (playback). */
    void                *cb_fillPlayBackBuffer_user_data;
    /** Pointer to the callback function used for retrieve the processed
     * audio present in the capture buffer of bdIMAD. */
    cb_emptyCaptureB_t  cb_emptyCaptureBuffer;
    /** Pointer to user data to pass to the callback (capture). */
    void                *cb_emptyCaptureBuffer_user_data;
    /** Is a wide char pointer to the capture device name. */
    wchar_t             *CaptureDevice;
    /** Is a wide char pointer to the play device name. */
    wchar_t             *PlayDevice;
    /** True to enable diagnostic, false to disable. */
    int 	            DiagnosticEnable;
    /** Directory which will contains the files generated for diagnostic. */
    wchar_t             *DiagnosticFolderPath;
    /** Is an auxiliary settings pointer used internally by bdIMAD. */
    void                *bdIMADwr_SettingsData;
} bdIMADpj_Setting_t;

/**
 * @brief Instance structure for the warnings generated by the initialization
 * functions.
 */

typedef struct bdIMADpj_Warnings_t {
    /** The capture device indicated can't be opened, then the default capture device
     *  has been selected. */
    int DefaultCaptureDeviceAutomaticallySelected;
    /** The capture device opened has not volume control. */
    int CaptureDeviceWithoutVolumeControl;
    /** The play device indicated can't be opened, then the default play device
     *  has been selected. */
    int DefaultPlayDeviceAutomaticallySelected;
    /** The number of channels requested is out of range. The number of
     *  channels opened is equal to the maximum. */
    int NumberOfChannelsOutOfRange;
    /** The diagnostic files could not be saved. */
    int DiagnosticSaveNotAllowed;
    /** The nlp level requested is not allowed, it has been automatically
     *  changed to the default value. */
    int nlpLevelChangeSettting;
    /** No capture device is present. Anyway the bdSES has been instantiated
     *  only for playback. */
    int NoCaptureDevicePresent;
    /** The CPU is not adapt to run the AEC engine, the AEC has been disabled.
     *  This happens for very old CPU like pentium III. */
    int oldCPUdetected_AECdisable;
    /** Windows Direct Sound error. */
    long directSoundError;
    /** Windows Direct Sound volume error. */
    long directSoundLevel;
    /** No play device is present. Anyway the bdSES has been instantiated
     *  only for capture. */
    int NoPlayDevicePresent;
} bdIMADpj_Warnings_t;

/**
 * @brief Instance structure for the library version
 */

typedef struct bdIMADpj_libVersion_t {
    int     major;                                /**< major version. */
    int	    minor;                                /**< minor version. */
    int	    build;                                /**< build number. */
    char    *name;                                /**< name "bdIMADpj ver.X". */
    char    *version;                             /**< beta, RC, release. */
    char    *buildDate;                           /**< build date. */
} bdIMADpj_libVersion_t;


/**
 * @brief Audio output routing setting to pass to set and get route output device functions.
 */
typedef enum bdIMADpj_out_dev_route {
    /** Default route */
    BD_AUD_DEV_ROUTE_DEFAULT		= 0,

    /** Route to loudspeaker */
    BD_AUD_DEV_ROUTE_LOUDSPEAKER	= 1,

    /** Route to earpiece */
    BD_AUD_DEV_ROUTE_EARPIECE		= 2
}bdIMADpj_out_dev_route;


/**
 * @}
 */



/**
 * @addtogroup groupFunction
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Must be used to allocate and set to default parameter the memory
 * for the bdIMAD.
 *
 * The function generates a structure bdIMADpj_Setting_t filled with the
 * default settings.
 * \n The user can change this settings according to the need and then
 * launch the ::bdIMADpj_InitAEC.
 * \n The function generates also a warning structure (::bdIMADpj_Warnings_t)
 * that could be used in ::bdIMADpj_InitAEC to handle eventual warnings.
 *
 * @param[out] **ppSettings           Pointer to the pointer of the
 *                                    allocated ::bdIMADpj_Setting_t.
 *
 * @param[out] **ppWarningMessages    Pointer to the pointer of the
 *                                    allocated ::bdIMADpj_Warnings_t.
 *
 * @return                            ::BD_PJ_OK if the function has been
 *                                    performed successfully, otherwise return
 *                                    an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_CreateStructures(
				    bdIMADpj_Setting_t **ppSettings,
				    bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to free the memory for the ::bdIMADpj_Setting_t structure and
 * ::bdIMADpj_Warnings_t structure allocated with
 * the ::bdIMADpj_CreateStructures.
 * @param[in] **ppSettings			Pointer to a memory location filled
 * 				       				with the address of the
 * 				       				::bdIMADpj_Setting_t structure to free.
 * 									This address will be set to NULL.
 *
 * @param[in] **ppWarningMessages   Pointer to a memory location filled
 * 				       				with the address of the allocated
 * 				       				::bdIMADpj_Warnings_t structure to free.
 * 				      				This address will be set to NULL.
 *
 * @return                          ::BD_PJ_OK if the function has been
 * 				       				performed successfully, otherwise return
 * 				      				an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_FreeStructures(
				    bdIMADpj_Setting_t **ppSettings,
				    bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to initialize the memory for bdIMAD with the settings
 * contained in the <code>ppSettings</code>.
 *
 * @param[out] *pBdIMADInstance     Is the pointer to the bdIMAD object.
 *
 * @param[in] **ppSettings          Pointer to pointer to a
 * 				       				::bdIMADpj_Setting_t structure, filled
 * 				       				with initialization settings to be
 * 				       				applied to the bdIMAD.
 *                               	\n Note, the <code>pBdIMADInstance</code>
 *                                  is modified with the applied settings.
 *
 * @param[out] **ppWarningMessages  Pointer to pointer to a
 * 				       				::bdIMADpj_Warnings_t structure,
 * 				       				which reports the warnings after the
 * 				     			 	initialization.
 *
 * @return                         ::BD_PJ_OK if the function has been
 *			 				       performed successfully, otherwise return
 * 							       an error (refer to ::bdIMADpj_Status).
 *                                 \n If the error is
 *                                 ::BD_PJ_WARN_BDIMAD_WARNING_ASSERTED
 *                                 the init function has been performed with success,
 *                                 but with a different settings
 *                                 respect to the ones required.
 *                                 This mainly happens if the audio
 *                                 device opened is different to the
 *                                 one requested.
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_InitAEC(bdIMADpj *pBdIMADInstance,
				      bdIMADpj_Setting_t **ppSettings,
				      bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to free the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] *pBdIMADInstance		Pointer to the bdIMAD object to free.
 *
 * @return                          ::BD_PJ_OK if the function has been
 * 				       				performed successfully, otherwise return
 * 				       				an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_FreeAEC(bdIMADpj *pBdIMADInstance);

/**
 * @brief Is used to make a list of capture and play devices available
 * on the system.
 *
 * @param[in] captureDevice			Set to 1 to get the list of capture
 * 				       				devices. Set to 0 to get the list of
 * 				       				play devices.
 *
 * @param[in] **deviceName			Pointer to pointer to a wide char
 * 				      				containing the names of capture/play
 * 				       				devices.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getDeviceName(int captureDevice,
						    wchar_t **deviceName);

/**
 * @brief Is used to freeze the bdIMAD, stopping the audio playback
 * and recording.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_stop(bdIMADpj bdIMADInstance);

/**
 * @brief Is used to put back in play the audio after it has been stopped by the
 * ::bdIMADpj_stop functions.
 * @param[in] bdIMADInstance           bdIMAD object.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise
 * 				       return an error (refer to
 * 				       ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_run(bdIMADpj bdIMADInstance);

/**
 * @brief Print on a standard output the warning messages.
 *
 * @param[in] *pWarningMessages		Pointer to the warning structure
 *									to be printed.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_DisplayWarnings(
					bdIMADpj_Warnings_t *pWarningMessages);

/**
 * @brief Clear the warning structure after being read.
 *
 * @param[out] **ppWarningMessages	Pointer to pointer to the warning
 *									structure to be cleared.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_ClearAllWarnings(
				       bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to set a parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName 		Indicates the parameter to be set.
 *
 * @param[in] *pValue				Is a pointer to the value to be set.
 *									\n In the ::bdIMADpj_Parameter
 *									declaration is indicated the type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_setParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_Parameter parameterName, void *pValue);

/**
 * @brief Is used to get a parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName			Indicates the parameter to be get.
 *
 * @param[out] *pValue				Is a pointer to the value to be get.
 *									\n In the ::bdIMADpj_Parameter
 *									declaration is indicated the type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_Parameter parameterName, void *pValue);

 
 /**
 * @brief Is used to set an audio processing parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName			Indicates the parameter to be set.
 *
 * @param[in] directionPath			Indicates the send/receive direction path.
 *									The parameter will be set to the audio process block 
 *									located in the selected path.
 *
 * @param[in] *pValue				Is a pointer to the value to be set.
 *									\n In the ::bdIMADpj_AudioProcessParameter
 *									declaration is indicated the real type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_setAudioProcessParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_AudioProcessParameter parameterName, bdIMADpj_DirPath directionPath, void *pValue);

/**
 * @brief Is used to get an audio processing parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName			Indicates the parameter to be get.
 *
 * @param[in] directionPath			Indicates the send/receive direction path.
 *									The parameter will be get from the audio process block 
 *									located in the selected path.
 *
 * @param[out] *pValue				Is a pointer to the value to be get.
 *									\n In the ::bdIMADpj_AudioProcessParameter
 *									declaration is indicated the real type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getAudioProcessParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_AudioProcessParameter parameterName, bdIMADpj_DirPath directionPath, void *pValue);

/**
 * @brief Is used to set a test parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName			Indicates the parameter to be set.
 *
 * @param[in] directionPath			Indicates the send/receive direction path.
 *									The parameter will be set to the test generator block
 *									located in the selected path.
 *
 * @param[in] *pValue				Is a pointer to the value to be set.
 *									\n In the ::bdIMADpj_TestParameter
 *									declaration is indicated the real type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_setTestParameter(bdIMADpj bdIMADInstance, bdIMADpj_TestParameter parameterName, bdIMADpj_DirPath directionPath, void* pValue);

/**
 * @brief Is used to get a test parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] parameterName			Indicates the parameter to be get.
 *
 * @param[in] directionPath			Indicates the send/receive direction path.
 *									The parameter will be get from the test generator block 
 *									located in the selected path.
 *
 * @param[out] *pValue				Is a pointer to the value to be get.
 *									\n In the ::bdIMADpj_TestParameter
 *									declaration is indicated the real type of
 *									the value, depending on the
 *									<code>parameterName</code>.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getTestParameter(bdIMADpj bdIMADInstance, bdIMADpj_TestParameter parameterName, bdIMADpj_DirPath directionPath, void* pValue);

/**
 * @brief Is used to get the VU meter level value located at the input/output 
 * of the send/receive path of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] directionPath			Indicates the send/receive direction path.
 *									The level value will be get from
 *									the VU meter located in the selected path.
 *
 * @param[in] side					Indicates the input/output side.
 *									The level value will be get from
 *									the VU meter located at the selected 
 *									side of the selected path.
 *
 * @param[out] *pValue				Is a pointer to the float level value to be get.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getVuMeterLevelValue(bdIMADpj bdIMADInstance, bdIMADpj_DirPath directionPath, bdIMADpj_Side side, float* pValue);

/**
 * @brief Is used to enable/disable socket communication with GUI of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] port					Receive port number to be used for socket communication (bdController).
 *
 * @param[in] enable				Set 1 to enable, 0 to disable the socket communication [default=disabled]. 
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_enableGuiSocketCommunication(bdIMADpj bdIMADInstance,  int port, int enable);

/**
 * @brief Is used to set the route of the output device of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[in] outputRoute			Indicates the route of the output device to be set.
 *
 * @param[out] **ppWarningMessages	Pointer to pointer to a
 * 									::bdIMADpj_Warnings_t structure,
 *									which reports the warnings after the
 *									set function.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_setRouteOutputDevice(bdIMADpj bdIMADInstance, bdIMADpj_out_dev_route outputRoute, bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to get the route of the output device of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] bdIMADInstance		bdIMAD object.
 *
 * @param[out] *outputRoute			Pointer to the route of the output device currently set.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
 
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getRouteOutputDevice(bdIMADpj bdIMADInstance, bdIMADpj_out_dev_route *outputRoute);

/**
 * @brief Is used to get the device capabilities of capture/playback device of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 *
 * @param[in] captureDevice			Set to 1 to get the capabilities of capture
 *									devices. Set to 0 to get the capabilities of
 *									play devices.
 *
 * @caps[out] *caps					Is a pointer to the device capabilities, 
 *									as bitmask combination of #pjmedia_aud_dev_cap.
 *
 * @return							::BD_PJ_OK if the function has been
 *									performed successfully, otherwise return
 *									an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getDeviceCapabilities(int captureDevice, unsigned *caps);

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif //BD_IMAD_PJ_H__
