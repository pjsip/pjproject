set(PJ_DEPENDENCY_PREFIX PJ_DEPENDENCY)
set(PJ_DEPENDENCY_SUFFIX PROVIDER)
set(PJ_DEPENDENCY_NAMESPACE Pj::Dep)

include(Pj/Dependency)

pj_dependency(G7221
  DESCRIPTION "G7221 codec implementation provider"
  SUBDIR "g7221" g7221
  PREFER "vendored"
)

pj_dependency(GSM
  DESCRIPTION "GSM codec implementation provider"
  SUBDIR "gsm" gsm
  PACKAGE "GSM" GSM::GSM
  PREFER "vendored"
)

pj_dependency(iLBC
  DESCRIPTION "iLBC codec implementation provider"
  SUBDIR "ilbc" ilbc
  PREFER "vendored"
)

pj_dependency(Resample
  DESCRIPTION "libresample implementation provider"
  SUBDIR "resample" resample
  PACKAGE "Resample" Resample::Resample
  PREFER "vendored"
)

pj_dependency(Speex
  DESCRIPTION "Speex codec & resampling implementation provider"
  SUBDIR "speex" speex
  PACKAGE "Speex" Speex::Speex
  PREFER "vendored"
)

pj_dependency(SRTP
  DESCRIPTION "Secure RTP (sRTP) implementation provider"
  SUBDIR "srtp" srtp
  PACKAGE "SRTP" SRTP::SRTP
  PREFER "vendored"
)

pj_dependency(WebRTC
  DESCRIPTION "WebRTC implementation provider"
  SUBDIR "webrtc" webrtc
  PREFER "vendored"
)

pj_dependency(WebRTC::AEC3
  DESCRIPTION "WebRTC AEC3 implementation provider"
  SUBDIR "webrtc_aec3" webrtc_aec3
  PREFER "vendored"
)

pj_dependency(YUV
  DESCRIPTION "libyuv codec implementation provider"
  SUBDIR "yuv" yuv
  PACKAGE "YUV" YUV::YUV
  PREFER "vendored"
)
