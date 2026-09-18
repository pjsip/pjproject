// Tier 2: a real SwiftPM consumer. Proves the module map, the binary target
// and the linker settings in Package.swift work together, and that the
// framework runs rather than merely links.
import PJSIP

func require(_ ok: Bool, _ what: String) {
    if !ok {
        print("VERIFY-FAIL \(what)")
        exit(1)
    }
}

require(pj_init() == PJ_SUCCESS.rawValue, "pj_init")

let version = String(cString: pj_get_version())
require(!version.isEmpty, "pj_get_version")

// Referencing these forces the linker to resolve the video subsystem and the
// VideoToolbox codec out of the framework.
let videoFormatMgr = pjmedia_video_format_mgr_instance
let vidDevInit = pjmedia_vid_dev_subsys_init
let vidToolboxInit = pjmedia_codec_vid_toolbox_init
require(videoFormatMgr is Any && vidDevInit is Any && vidToolboxInit is Any,
        "video entry points")

require(pjsua_create() == PJ_SUCCESS.rawValue, "pjsua_create")
pjsua_destroy()

print("VERIFY-OK swiftpm pjsip=\(version)")
