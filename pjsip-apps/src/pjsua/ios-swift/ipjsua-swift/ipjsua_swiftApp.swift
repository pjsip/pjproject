//
//  ipjsua_swiftApp.swift
//  ipjsua-swift
//
//  Created by Ming on 4/2/21.
//

import SwiftUI

class VidInfo: ObservableObject {
    /* Video window */
    @Published var vid_win:UIView? = nil
}

class AppDelegate: NSObject, UIApplicationDelegate {
    static let Shared = AppDelegate()
    var vinfo = VidInfo()
}

@main
struct ipjsua_swiftApp: App {
    init() {
        /* Create pjsua */
        var status: pj_status_t;
        status = pjsua_create();
        if (status != PJ_SUCCESS.rawValue) {
            NSLog("Failed creating pjsua");
        }

        /* Init configs */
        var cfg = pjsua_config();
        var log_cfg = pjsua_logging_config();
        var media_cfg = pjsua_media_config();
        pjsua_config_default(&cfg);
        pjsua_logging_config_default(&log_cfg);
        pjsua_media_config_default(&media_cfg);

        /* Initialize application callbacks */
        let on_call_media_state_ptr: @convention(c) (pjsua_call_id) -> () = on_call_media_state
        cfg.cb.on_call_media_state = on_call_media_state_ptr;

        /* Init pjsua */
        status = pjsua_init(&cfg, &log_cfg, &media_cfg);
        
        /* Create transport */
        var transport_id = pjsua_transport_id();
        var tcp_cfg = pjsua_transport_config();
        pjsua_transport_config_default(&tcp_cfg);
        tcp_cfg.port = 5060;
        status = pjsua_transport_create(PJSIP_TRANSPORT_TCP,
                                        &tcp_cfg, &transport_id);

        /* Init account config */
        let id = strdup("Test<sip:401@pjsip.org>");
        let username = strdup("401");
        let passwd = strdup("pw401");
        let realm = strdup("*");
        let registrar = strdup("sip:pjsip.org");
        let proxy = strdup("sip:sip.pjsip.org;transport=tcp");

        var acc_cfg = pjsua_acc_config();
        pjsua_acc_config_default(&acc_cfg);
        acc_cfg.id = pj_str(id);
        acc_cfg.cred_count = 1;
        acc_cfg.cred_info.0.username = pj_str(username);
        acc_cfg.cred_info.0.realm = pj_str(realm);
        acc_cfg.cred_info.0.data = pj_str(passwd);
        acc_cfg.reg_uri = pj_str(registrar);
        acc_cfg.proxy_cnt = 1;
        acc_cfg.proxy.0 = pj_str(proxy);
        acc_cfg.vid_out_auto_transmit = pj_bool_t(PJ_TRUE.rawValue);
        acc_cfg.vid_in_auto_show = pj_bool_t(PJ_TRUE.rawValue);

        /* Add account */
        pjsua_acc_add(&acc_cfg, pj_bool_t(PJ_TRUE.rawValue), nil);

        /* Free strings */
        free(id); free(username); free(passwd); free(realm);
        free(registrar); free(proxy);
        
        /* Start pjsua */
        status = pjsua_start();
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(AppDelegate.Shared.vinfo)
                .preferredColorScheme(.light)
        }
    }
}

private func tupleToArray<Tuple, Value>(tuple: Tuple) -> [Value] {
    let tupleMirror = Mirror(reflecting: tuple)
    return tupleMirror.children.compactMap { (child: Mirror.Child) -> Value? in
        return child.value as? Value
    }
}

private func on_call_media_state(call_id: pjsua_call_id) {
    var ci = pjsua_call_info();
    pjsua_call_get_info(call_id, &ci);

    for mi in 0...ci.media_cnt {
        let media: [pjsua_call_media_info] = tupleToArray(tuple: ci.media);

        if (media[Int(mi)].status == PJSUA_CALL_MEDIA_ACTIVE ||
            media[Int(mi)].status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
        {
            switch (media[Int(mi)].type) {
            case PJMEDIA_TYPE_AUDIO:
                var call_conf_slot: pjsua_conf_port_id;

                call_conf_slot = media[Int(mi)].stream.aud.conf_slot;
                pjsua_conf_connect(call_conf_slot, 0);
                pjsua_conf_connect(0, call_conf_slot);
                break;
        
            case PJMEDIA_TYPE_VIDEO:
                let wid = media[Int(mi)].stream.vid.win_in;
                var wi = pjsua_vid_win_info();
                    
                if (pjsua_vid_win_get_info(wid, &wi) == PJ_SUCCESS.rawValue) {
                    let vid_win:UIView = Unmanaged<UIView>.fromOpaque(wi.hwnd.info.ios.window).takeUnretainedValue();

                    /* UIView update must be done in the main thread */
                    DispatchQueue.main.sync {
                        AppDelegate.Shared.vinfo.vid_win = vid_win;
                    }
                }
                break;
            
            default:
                break;
            }
        }
    }
}
