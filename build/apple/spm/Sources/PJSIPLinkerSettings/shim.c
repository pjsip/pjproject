/*
 * This target exists only to carry linkerSettings. SwiftPM does not accept
 * them on a binaryTarget, so the frameworks the XCFramework needs are
 * declared here instead. It contributes no code of its own.
 */
void pjsip_spm_linker_settings_anchor(void);
void pjsip_spm_linker_settings_anchor(void) {}
