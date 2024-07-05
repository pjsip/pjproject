#ifndef PORTSIP_ERRORS_hxx
#define PORTSIP_ERRORS_hxx

#define INVALID_SESSION_ID					-1
#define CONFERENCE_SESSION_ID				0x7FFF //Conference Session Id, only use on enableVideoStreamCallback API
#define LOCALVIDEO_SESSION_ID				0x7FFE //Local Video Session Id, only use on enableVideoStreamCallback API

#define ECoreAlreadyInitialized				-60000
#define ECoreNotInitialized					-60001
#define ECoreSDKObjectNull					-60002
#define ECoreArgumentNull					-60003
#define ECoreInitializeWinsockFailure		-60004
#define ECoreUserNameAuthNameEmpty			-60005
#define ECoreInitializeStackFailure			-60006
#define ECorePortOutOfRange					-60007
#define ECoreAddTcpTransportFailure			-60008
#define ECoreAddTlsTransportFailure			-60009
#define ECoreAddUdpTransportFailure			-60010
#define ECoreMiniAudioPortOutOfRange		-60011
#define ECoreMaxAudioPortOutOfRange			-60012
#define ECoreMiniVideoPortOutOfRange		-60013
#define ECoreMaxVideoPortOutOfRange			-60014
#define ECoreMiniAudioPortNotEvenNumber		-60015
#define	ECoreMaxAudioPortNotEvenNumber		-60016
#define ECoreMiniVideoPortNotEvenNumber		-60017
#define ECoreMaxVideoPortNotEvenNumber		-60018
#define ECoreAudioVideoPortOverlapped		-60019
#define ECoreAudioVideoPortRangeTooSmall	-60020
#define ECoreAlreadyRegistered				-60021
#define ECoreSIPServerEmpty					-60022
#define ECoreExpiresValueTooSmall			-60023
#define ECoreCallIdNotFound					-60024
#define	ECoreNotRegistered					-60025
#define ECoreCalleeEmpty					-60026
#define ECoreInvalidUri						-60027
#define ECoreAudioVideoCodecEmpty			-60028
#define ECoreNoFreeDialogSession			-60029
#define ECoreCreateAudioChannelFailed		-60030
#define ECoreSessionTimerValueTooSmall		-60040
#define ECoreAudioHandleNull				-60041
#define ECoreVideoHandleNull				-60042
#define ECoreCallIsClosed					-60043
#define ECoreCallAlreadyHold				-60044
#define ECoreCallNotEstablished				-60045
#define ECoreCallNotHold					-60050
#define ECoreSipMessaegEmpty				-60051
#define ECoreSipHeaderNotExist				-60052
#define ECoreSipHeaderValueEmpty			-60053
#define ECoreSipHeaderBadFormed				-60054
#define ECoreBufferTooSmall					-60055
#define ECoreSipHeaderValueListEmpty		-60056
#define ECoreSipHeaderParserEmpty			-60057
#define ECoreSipHeaderValueListNull			-60058
#define ECoreSipHeaderNameEmpty				-60059
#define ECoreAudioSampleNotmultiple			-60060	//	The audio sample should be multiple of 10
#define ECoreAudioSampleOutOfRange			-60061	//	The audio sample ranges from 10 to 60
#define ECoreInviteSessionNotFound			-60062
#define ECoreStackException					-60063
#define ECoreMimeTypeUnknown				-60064
#define ECoreDataSizeTooLarge				-60065
#define ECoreSessionNumsOutOfRange			-60066
#define ECoreNotSupportCallbackMode			-60067
#define ECoreNotFoundSubscribeId			-60068
#define ECoreCodecNotSupport				-60069
#define ECoreCodecParameterNotSupport		-60070
#define ECorePayloadOutofRange				-60071	//  Dynamic Payload ranges from 96 to 127
#define ECorePayloadHasExist				-60072	//  Duplicate Payload values are not allowed.
#define ECoreFixPayloadCantChange			-60073
#define ECoreCodecTypeInvalid				-60074
#define ECoreCodecWasExist					-60075
#define ECorePayloadTypeInvalid				-60076
#define ECoreArgumentTooLong				-60077
#define ECoreMiniRtpPortMustIsEvenNum		-60078
#define ECoreCallInHold						-60079
#define ECoreNotIncomingCall				-60080
#define ECoreCreateMediaEngineFailure		-60081
#define ECoreAudioCodecEmptyButAudioEnabled -60082
#define ECoreVideoCodecEmptyButVideoEnabled -60083
#define ECoreNetworkInterfaceUnavailable	-60084
#define ECoreWrongDTMFTone					-60085
#define ECoreWrongLicenseKey				-60086
#define ECoreTrialVersionLicenseKey			-60087
#define ECoreOutgoingAudioMuted				-60088
#define ECoreOutgoingVideoMuted				-60089
#define ECoreFailedCreateSdp				-60090
#define ECoreTrialVersionExpired			-60091
#define ECoreStackFailure					-60092
#define ECoreTransportExists				-60093
#define ECoreUnsupportTransport				-60094
#define ECoreAllowOnlyOneUser				-60095
#define ECoreUserNotFound					-60096
#define ECoreTransportsIncorrect			-60097
#define ECoreCreateTransportFailure			-60098
#define ECoreTransportNotSet				-60099
#define ECoreECreateSignalingFailure		-60100
#define ECoreArgumentIncorrect				-60101
#define ECoreSipMethodNameEmpty				-60102
#define ECoreSipAlreadySubscribed			-60103
#define ECoreStartRecordFailure				-60104
#define ECoreParsedSdpFailure				-60105
#define EOpenPlayFileFailure				-60106
#define EFilePlayerNotExist					-60107

// IVR
#define ECoreIVRObjectNull					-61001
#define ECoreIVRIndexOutOfRange				-61002
#define ECoreIVRReferFailure				-61003
#define ECoreIVRWaitingTimeOut				-61004

// Conference
#define EConferenceAlreadyExists			-62001
#define EConferenceNotExist					-62002
#define EConferenceCreateAudioConfFailure	-62003
#define EConferenceCreateVideoConfFailure	-62004
#define EConferenceUnsupportedLayout        -62005

// Audio

#define EAudioFileNameEmpty					-70000
#define EAudioChannelNotFound				-70001
#define EAudioPlayFileAlreadyEnable			-70006

#define EAudioPlaySteamNotEnabled			-70008
#define EAudioRegisterCallbackFailure		-70009
#define EAudioCreateAudioConferenceFailure	-70010
#define EAudioPlayFileModeNotSupport		-70012
#define EAudioPlayFileFormatNotSupport		-70013
#define EAudioPlaySteamAlreadyEnabled		-70014
#define EAudioCodecNotSupport				-70016
#define EAudioPlayFileNotEnabled			-70017
#define EAudioPlayFileGetPositionFailure	-70018
#define EAudioVolumeOutOfRange              -70020
#define EAudioNotSupportDTMF2833			-70021

// Video
#define EVideoFileNameEmpty					-80000
#define EVideoGetDeviceNameFailure			-80001
#define EVideoGetDeviceIdFailure			-80002
#define EVideoStartCaptureFailure			-80003
#define EVideoChannelNotFound				-80004
#define EVideoStartSendFailure				-80005
#define EVideoGetStatisticsFailure			-80006
#define EVideoStartPlayAviFailure			-80007
#define EVideoSendAviFileFailure			-80008
#define EVideoRecordUnknowCodec				-80009
#define EVideoCantSetDeviceIdDuringCall		-80010
#define EVideoUnsupportCaptureRotate		-80011
#define EVideoUnsupportCaptureResolution	-80012
#define ECameraSwitchTooOften               -80013
#define EMTUOutOfRange                      -80014
#define EVideoCodecNotSupport               -80015
#define EVideoSendStreamAlreadyExists		-80016

// Device
#define EDeviceObjectNull					-90000
#define EDeviceGetDeviceNameFailure			-90001

//Screen
#define EScreenCapturerNotSupported			-90002
#define EScreenSourceIdNotFound				-90003
#define EScreenChannelNotFound				-90004
#define EScreenCapturerNotInitialized		-90005
#define EScreenCapturerRuning				-90006

#endif
