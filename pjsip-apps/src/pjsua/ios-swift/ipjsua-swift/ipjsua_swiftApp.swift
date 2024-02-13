/*
 * Copyright (C) 2021-2021 Teluu Inc. (http://www.teluu.com)
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

/* This is a simple Swift demo app that shows an example of how to use
 * PJSUA API to make one audio+video call to another user.
 */

import SwiftUI

class PjsipVars: ObservableObject {
    @Published var calling = false
    var dest: String = "sip:test@sip.pjsip.org"
    var call_id: pjsua_call_id = PJSUA_INVALID_ID.rawValue
    /* Video window */
    @Published var vid_win:UIView? = nil
}

class AppDelegate: NSObject, UIApplicationDelegate {
    static let Shared = AppDelegate()
    var pjsip_vars = PjsipVars()
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
        cfg.cb.on_incoming_call = on_incoming_call;
        cfg.cb.on_call_state = on_call_state;
        cfg.cb.on_call_media_state = on_call_media_state;

        /* Init pjsua */
        status = pjsua_init(&cfg, &log_cfg, &media_cfg);
        
        /* Create transport */
        var transport_id = pjsua_transport_id();
        var tcp_cfg = pjsua_transport_config();
        pjsua_transport_config_default(&tcp_cfg);
        tcp_cfg.port = 5080;
        status = pjsua_transport_create(PJSIP_TRANSPORT_TCP,
                                        &tcp_cfg, &transport_id);

        /* Add local account */
        var aid = pjsua_acc_id();
        status = pjsua_acc_add_local(transport_id, pj_bool_t(PJ_TRUE.rawValue), &aid);

        /* Use colorbar for local account and enable incoming video */
        var acc_cfg = pjsua_acc_config();
        var tmp_pool:UnsafeMutablePointer<pj_pool_t>? = nil;
        var info : [pjmedia_vid_dev_info] =
            Array(repeating: pjmedia_vid_dev_info(), count: 16);
        var count:UInt32 = UInt32(info.capacity);

        tmp_pool = pjsua_pool_create("tmp-ipjsua", 1000, 1000);
        pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);
        acc_cfg.vid_in_auto_show = pj_bool_t(PJ_TRUE.rawValue);

        pjsua_vid_enum_devs(&info, &count);
        for i in 0..<count {
            let name: [CChar] = tupleToArray(tuple: info[Int(i)].name);
            if let dev_name = String(validatingUTF8: name) {
                if (dev_name == "Colorbar generator") {
                    acc_cfg.vid_cap_dev = pjmedia_vid_dev_index(i);
                    break;
                }
            }
        }
        pjsua_acc_modify(aid, &acc_cfg);

        /* Init account config */
        let id = strdup("Test<sip:test@sip.pjsip.org>");
        let username = strdup("test");
        let passwd = strdup("pwd");
        let realm = strdup("*");
        let registrar = strdup("sip:sip.pjsip.org");
        let proxy = strdup("sip:sip.pjsip.org;transport=tcp");

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

        pj_pool_release(tmp_pool);

        /* Start pjsua */
        status = pjsua_start();
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(AppDelegate.Shared.pjsip_vars)
                .preferredColorScheme(.light)
        }
    }
}

private func on_incoming_call(acc_id: pjsua_acc_id, call_id: pjsua_call_id,
                              rdata: UnsafeMutablePointer<pjsip_rx_data>?)
{
    var opt = pjsua_call_setting();

    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 1;
    opt.vid_cnt = 1;

    /* Automatically answer call with 200 */
    pjsua_call_answer2(call_id, &opt, 200, nil, nil);
}

private func on_call_state(call_id: pjsua_call_id,
                           e: UnsafeMutablePointer<pjsip_event>?)
{
    var ci = pjsua_call_info();
    pjsua_call_get_info(call_id, &ci);
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        /* UIView update must be done in the main thread */
        DispatchQueue.main.sync {
            AppDelegate.Shared.pjsip_vars.vid_win = nil;
            AppDelegate.Shared.pjsip_vars.calling = false;
        }
    }
}

private func tupleToArray<Tuple, Value>(tuple: Tuple) -> [Value] {
    let tupleMirror = Mirror(reflecting: tuple)
    return tupleMirror.children.compactMap { (child: Mirror.Child) -> Value? in
        return child.value as? Value
    }
}

private func on_call_media_state(call_id: pjsua_call_id)
{
    var ci = pjsua_call_info();
    pjsua_call_get_info(call_id, &ci);
    let media: [pjsua_call_media_info] = tupleToArray(tuple: ci.media);

    for mi in 0...ci.media_cnt {
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
                    let vid_win:UIView =
                        Unmanaged<UIView>.fromOpaque(wi.hwnd.info.ios.window).takeUnretainedValue();

                    /* For local loopback test, one acts as a transmitter,
                       the other as a receiver.
                     */
                    if (AppDelegate.Shared.pjsip_vars.vid_win == nil) {
                        /* UIView update must be done in the main thread */
                        DispatchQueue.main.sync {
                            AppDelegate.Shared.pjsip_vars.vid_win = vid_win;
                        }
                    } else {
                        if (AppDelegate.Shared.pjsip_vars.vid_win != vid_win) {
                            /* Transmitter */
                            var param = pjsua_call_vid_strm_op_param ();

                            pjsua_call_vid_strm_op_param_default(&param);
                            param.med_idx = 1;
                            pjsua_call_set_vid_strm(call_id,
                                                    PJSUA_CALL_VID_STRM_START_TRANSMIT,
                                                    &param);
                        }
                    }
                }
                break;
            
            default:
                break;
            }
        }
    }
}
