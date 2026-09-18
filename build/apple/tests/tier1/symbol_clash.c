/*
 * Tier 1: an application that links this framework alongside a WebRTC based
 * SDK brings its own copies of these. They must be local inside the archive,
 * or the consumer's link fails on duplicate symbols.
 */
int srtp_cipher_encrypt(void);
int ARGBDetect(void);
int opus_encoder_create(void);
int WebRtcAec_CreateAec(void);

int srtp_cipher_encrypt(void) { return 0; }
int ARGBDetect(void) { return 0; }
int opus_encoder_create(void) { return 0; }
int WebRtcAec_CreateAec(void) { return 0; }
