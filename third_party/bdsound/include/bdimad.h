 /**
 * @file bdIMADpj.h
 * @brief bdSound IMproved Audio Device for PJSIP.
 */
 
/**
 * @defgroup bd_IMAD bdIMADpj bdSound IMproved Audio Device for PJSIP.
 * @ingroup audio_device_api
 * 
 * <b>bdSound IMproved Audio Device</b> is a multiplatform audio interface
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
 *       -# Request for bdIMADpj library to
 *       <a href="http:/www.bdsound.com" target="new">bdSound</a>:
 *        bdSound will provide instruction to integrate the library depending on
 *        the platform / O.S. / toolchain;
 *       -# Add the <code>bdimad_dev.c</code> file to
 *       <code>pjmedia/src/pjmedia-audiodev</code> folder;
 *       -# Enable the bdIMAD audio device defining the periferal in the
 *       <code>pj/config_site.h</code> and disabling all other devices:
 *       <pre>
 *       #define PJMEDIA_AUDIO_DEV_HAS_BDIMAD 1
 *       </pre>
 *
 *    - <b>Usage</b>
 *    \n There are only a couple of things the customer have to pay attention on
 *    §when using bdIMAD library.
 *
 *       - <b>Initialization</b>
 *       \n Since the bdIMAD library provide itself the echo cancellation
 *       and the latency management, is necessary to disable these features
 *       in the PJSIP librariy applications.
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
 *          \n Setting michrophone volume.
 *          - <code>PJMEDIA_AUD_DEV_CAP_EC</code>
 *          \n Enable/disable echo cancellation.
 *
 * For additional information visit
 * <a href="http:/www.bdsound.com" target="new">www.bdsound.com</a>
 * or write to info@bdsound.com
 *
 * @author bdSound
 * @version   1.0.1
 * @copyright 2012 bdSound srl. All rights reserved.
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
 * @param[in] *buffer                  pointer to the buffer with the audio
 * 				       samples to be played(short type).
 * @param[in] nSamples                 number of samples required.
 * @param[in] user_data                pointer to the user data structure
 * 				       defined in the bdIMADpj_Setting_t
 * 				       structure.
 * @return none.
 */

typedef int (* cb_fillPlayBackB_t) (void *buffer, int nSamples,
				    void *user_data);

/**
 * @brief Callback used to retrive the caputre buffer of bdIMAD. The function
 * is called by bdIMAD each time processed mic samples are available.
 * @param[out] *buffer                 pointer to the buffer with the audio
 * 				       samples to download(short type).
 * @param[in] nSamples                 number of samples processed to download.
 * @param[in] user_data                pointer to the user data structure
 *                                     defined in the MainSet structure.
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

typedef enum bdIMADpj_Status{
    /**< No error. */
    BD_PJ_OK                                     = 0,
    /**< Watch bdIMADpj_Warnings_t structure to find the warnings. */
    BD_PJ_WARN_BDIMAD_WARNING_ASSERTED           = 1,
    /**< Error not identified. */
    BD_PJ_ERROR_GENERIC                          = 2,
    /**< The pointer passed is NULL. */
    BD_PJ_ERROR_NULL_POINTER                     = 3,
    /**< Allocation procedure failed. */
    BD_PJ_ERROR_ALLOCATION                       = 4,
    /**< The parameter is not existent or the set/get function is not active. */
    BD_PJ_ERROR_PARAMETER_NOT_FOUND              = 5,
    /**< No capture device found. */
    BD_PJ_ERROR_IMAD_NONE_CAPTURE_DEV            = 10,
    /**< No play device found. */
    BD_PJ_ERROR_IMAD_NONE_PLAY_DEV               = 11,
    /**< Frame size not allowed. */
    BD_PJ_ERROR_IMAD_FRAME_SIZE                  = 12,
    /**< Sample frequency not allowed. */
    BD_PJ_ERROR_IMAD_SAMPLE_FREQ                 = 13,
    /**< Samples missing. */
    BD_PJ_ERROR_IMAD_MISSING_SAMPLES             = 14,
    /**< Device list is empty. */
    BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY           = 15,
    /**< Library not authorized, entering demo mode. */
    BD_PJ_ERROR_IMAD_LIB_NOT_AUTHORIZED          = 16,
    /**< The input channel memory has not been allocated. */
    BD_PJ_ERROR_IMAD_INPUT_CH_NOT_ALLOCATED      = 17,
    /**< The library has expired, entering demo mode. */
    BD_PJ_ERROR_IMAD_LICENSE_EXPIRED             = 18,
    /**< Open of capture device failed. */
    BD_PJ_ERROR_IMAD_OPEN_CAPTURE_DEV_FAILED     = 19,
    /**< Open of play device failed.  */
    BD_PJ_ERROR_IMAD_OPEN_PLAY_DEV_FAILED        = 20,
    /**< Start of play device failed. */
    BD_PJ_ERROR_IMAD_START_PLAY_DEV_FAILED       = 21,
    /**< Start of capture device failed. */
    BD_PJ_ERROR_IMAD_START_CAPTURE_DEV_FAILED    = 22,
    /**< Start of time process failed. */
    BD_PJ_ERROR_IMAD_START_TIME_PROCESS_FAILED   = 23,
    /**< Start of thread process failed. */
    BD_PJ_ERROR_IMAD_THREAD_PROCESS_FAILED       = 24,
    /**< No volume control available. */
    BD_PJ_ERROR_IMAD_NO_VOL_CONTROL_AVAILABLE    = 25,
} bdIMADpj_Status;

/**
 * @brief Parameter to pass to set and get parameter functions. 
 *
 * For each enumeration are defined the data type and the supported operations
 * on that parameter (set and get).
 */
   
typedef enum bdIMADpj_Parameter{
    /**< int*   \n set/get \n 1 enable / 0 disable echo cancellation. */
    BD_PARAM_IMAD_PJ_AEC_ENABLE                 = 1,
    /**< int*   \n set/get \n 1 enable / 0 disable microphone control
     * (when possible). */
    BD_PARAM_IMAD_PJ_MIC_CONTROL_ENABLE         = 2,
    /**< int*   \n set/get \n 1 ebable / 0 disable noise reduction. */
    BD_PARAM_IMAD_PJ_NOISE_REDUCTION_ENABLE     = 3,
    /**< int*   \n set     \n number of channel to reset. Used to reset
     * the input channel statistics. To be used when the same channel
     * is assigned to another partecipant. */
    BD_PARAM_IMAD_PJ_RESET_STATISTIC_IN_CH      = 4,
    /**< float* \n set/get \n 0.0f -> 1.0f volume of
     * the microphone(when possible). */
    BD_PARAM_IMAD_PJ_MIC_VOLUME                 = 5,
    /**< int*   \n set/get \n 0 mute / 1 not mute on microphone
     * (when possible). */
    BD_PARAM_IMAD_PJ_MIC_MUTE                   = 6,
    /**< float* \n set/get \n 0.0f -> 1.0f volume of the speaker. */
    BD_PARAM_IMAD_PJ_SPK_VOLUME                 = 7,
    /**< int*   \n set/get \n 0 mute / 1 not mute on speaker. */
    BD_PARAM_IMAD_PJ_SPK_MUTE                   = 8,
} bdIMADpj_Parameter;


/**
 * @brief Instance structure for the information regarding the aec engine.
 */

typedef struct bdIMADpj_Setting_t{
    /**< Sample frequency (8kHz or 16kHz). */
    int                 SamplingFrequency;
    /**< Audio buffer managed by the aec bdIMAD functions.
     * (from 16ms to 80ms, 16ms recommended). */
    int                 FrameSize_ms;
    /**< Points to the validation functions in the validation library. */
    void                *validate;
    /**< Points to the the callback function used for filling
     * the playback buffer of bdIMAD. */
    cb_fillPlayBackB_t  cb_fillPlayBackBuffer;
    /**< Points to user data to pass to the callback. */
    void                *cb_fillPlayBackBuffer_user_data;
    /**< Points to the callback function used for retreive the processed
     * audio present in the capture buffer of bdIMAD. */
    cb_emptyCaptureB_t  cb_emptyCaptureBuffer;
    /**< Points to user data to pass to the callback. */
    void                *cb_emptyCaptureBuffer_user_data;
    /**< Is a wide char pointer to the capture device name. */
    wchar_t             *CaptureDevice;
    /**< Is a wide char pointer to the play device name. */
    wchar_t             *PlayDevice;
    /**< True to enable diagnostic, false to disable. */
    int 	            DiagnosticEnable;
    /**< Directory which will contains the files generated for diagnostic. */
    wchar_t             *DiagnosticFolderPath;
    /**< Is an auxiliary settings pointer used internally by bdIMAD. */
    void                *bdIMADwr_SettingsData;
} bdIMADpj_Setting_t;

/**
 * @brief Instance structure for the warnings generated by the initialization
 * functions.
 */

typedef struct bdIMADpj_Warnings_t{
    /**< The capture device indicated can't be opened, has been selected
     * the default capture device. */
    int DefaultCaptureDeviceAutomaticallySelected;
    /**< The capture device opened has not volume control. */
    int CaptureDeviceWithoutVolumeControl;
    /**< The play device indicated can't be opened, has been selected
     * the default play device. */
    int DefaultPlayDeviceAutomaticallySelected;
    /**< The number of channel requested is out of range. The number of
     * channel opened is equal to the maximum. */
    int NumberOfChannelsOutOfRange;
    /**< The diagnostic files could not be saved. */
    int DiagnosticSaveNotAllowed;
    /**< The nlp level requested is not allowed, it has been automatically
     * changed to the default value. */
    int nlpLevelChangeSettting;
    /**< No capture device is present. Anyway the bdSES has been
     * istantiated only for playback. */
    int NoCaptureDevicePresent;
    /**< The cpu is not adapt to run the aec engine, the aec has been disabled.
     * This appens for very old cpu like pentium III. */
    int oldCPUdetected_AECdisable;
    /**< Windows Direct Sound error. */
    long directSoundError;
    /**< Windows Direct Sound volume error. */
    long directSoundLevel;
    /**< No play device is present. Anyway the bdSES has been istantiated
     * only for capture. */
    int NoPlayDevicePresent;
} bdIMADpj_Warnings_t;

/**
 * @brief Instance structure for the library version
 */

typedef struct bdIMADpj_libVersion_t{
    int     major;                                /**< major version. */
    int	    minor;                                /**< minor version. */
    int	    build;                                /**< build number. */
    char    *name;                                /**< name "bdIMADpj ver.X". */
    char    *version;                             /**< beta, RC, release. */
    char    *buildDate;                           /**< build date. */
} bdIMADpj_libVersion_t;

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
 * The function generate a structure bdIMADpj_Setting_t filled with the
 * default settings.
 * \n The user can change this settings according to the need and then
 * launch the ::bdIMADpj_InitAEC.
 * \n The function generate also a warning structure (::bdIMADpj_Warnings_t)
 * that could be used in ::bdIMADpj_InitAEC to handle eventual warnings.
 * @param[out] **ppSettings            Points to the pointer of the
 *                                     allocated ::bdIMADpj_Setting_t.
 * @param[out] **ppWarningMessages     Points to the pointer of the
 *                                     allocated ::bdIMADpj_Warnings_t.
 * @return                             ::BD_PJ_OK if the function has been
 *                                     performed successfully, otherwise return
 *                                     an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_CreateStructures(
				    bdIMADpj_Setting_t **ppSettings,
				    bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to free the memory for the ::bdIMADpj_Setting_t structure and
 * ::bdIMADpj_Warnings_t structure allocated with
 * the ::bdIMADpj_CreateStructures.
 * @param[in] **ppSettings             Pointer to a memory location filled
 * 				       with the address of the
 * 				       ::bdIMADpj_Setting_t structure to free.
 * This address will be set to NULL.
 * @param[in] **ppWarningMessages      Pointer to a memory location filled
 * 				       with the address of the allocated
 * 				       ::bdIMADpj_Warnings_t structure to free.
 * 				       This address will be set to NULL.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise return
 * 				       an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_FreeStructures(
				    bdIMADpj_Setting_t **ppSettings,
				    bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to initialize the memory for bdIMAD with the settings
 * contained in the <code>ppSettings</code>.
 * @param[out] *pBdIMADInstance        Is the pointer to the bdIMAD object.
 * @param[in] **ppSettings             Pointer to pointer to a
 * 				       ::bdIMADpj_Setting_t structure, filled
 * 				       with initialization settings to be
 * 				       applied to the bdIMAD.
 *                                     \n Note, the <code>pBdIMADInstance</code>
 *                                     is modified with the applied settings.
 * @param[out] **ppWarningMessages     Pointer to pointer to a
 * 				       ::bdIMADpj_Warnings_t sructure,
 * 				       which reports the warnings after the
 * 				       initialization.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise return
 * 				       an error (refer to ::bdIMADpj_Status).
 *                                     \n If the error is
 *                                     ::BD_PJ_WARN_BDIMAD_WARNING_ASSERTED
 *                                     the init has been performed with success,
 *                                     but with a different settings
 *                                     respect to the ones required.
 *                                     This mainly happens if the audio
 *                                     device opened is different to the
 *                                     one requested.
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_InitAEC(bdIMADpj *pBdIMADInstance,
				      bdIMADpj_Setting_t **ppSettings,
				      bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to free the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 * @param[in] *pBdIMADInstance         Pointer to the bdIMAD object to free.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise return
 * 				       an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_FreeAEC(bdIMADpj *pBdIMADInstance);

/**
 * @brief Is used to make a list of capure and play devices available
 * on the system.
 * @param[in] captureDevice            Set to 1 to get the list of capture
 * 				       devices. Set to 0 to get the list of
 * 				       play devices.
 * @param[in] **deviceName             Pointer to pointer to a wide char
 * 				       containing the names of capture/play
 * 				       devices.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise return
 * 				       an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getDeviceName(int captureDevice,
						    wchar_t **deviceName);

/**
 * @brief Is used to freeze the bdIMAD, stopping the audio playback
 * and recording.
 * @param[in] bdIMADInstance           bdIMAD object.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise
 * 				       return an error (refer to
 * 				       ::bdIMADpj_Status).
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
 * @param[in] *pWarningMessages        Pointer to the warning structure
 * 				       to be printed.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise
 * 				       return an error
 * 				       (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_DisplayWarnings(
					bdIMADpj_Warnings_t *pWarningMessages);

/**
 * @brief Clear the warning structure after being read.
 * @param[out] **ppWarningMessages     Pointer to pointer to the warning
 * 				       structure to be cleared.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise
 * 				       return an error (refer to
 * 				       ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_ClearAllWarnings(
				       bdIMADpj_Warnings_t **ppWarningMessages);

/**
 * @brief Is used to set a parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 * @param[in] bdIMADInstance           bdIMAD object.
 * @param[in] parameterName            Indicate the parameter to set.
 * @param[in] *pValue                  Is a pointer to the value to set
 * 				       cast to void.
 * 				       \n In the ::bdIMADpj_Parameter
 * 				       declaration is indicated the real type of
 * 				       the value, depending on the
 * 				       <code>parameterName</code>.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise
 * 				       return an error (refer to
 * 				       §::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_setParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_Parameter parameterName, void *pValue);

/**
 * @brief Is used to get a parameter of the bdIMAD object pointed by the
 * <code>pBdIMADInstance</code>.
 * @param[in] bdIMADInstance           bdIMAD object.
 * @param[in] parameterName            Indicate the parameter to get.
 * @param[out] *pValue                 Is a pointer to the value to get cast
 * 				       to void. \n In the
 * 				       ::bdIMADpj_Parameter declaration is
 * 				       indicated the real type of the value,
 * 				       depending on the
 * 				       <code>parameterName</code>.
 * @return                             ::BD_PJ_OK if the function has been
 * 				       performed successfully, otherwise return
 * 				       an error (refer to ::bdIMADpj_Status).
 */
BDIMADPJ_API bdIMADpj_Status bdIMADpj_getParameter(bdIMADpj bdIMADInstance,
				bdIMADpj_Parameter parameterName, void *pValue);


#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif //BD_IMAD_PJ_H__
