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

import SwiftUI

struct VidView: UIViewRepresentable {
    @Binding var vidWin: UIView?

    func makeUIView(context: Context) -> UIView {
        let view = UIView();
        view.backgroundColor = UIColor.black;
        return view;
    }

    func updateUIView(_ uiView: UIView, context: Context) {
        if let vid_win = vidWin {
            /* Add the video window as subview */
            if (!vid_win.isDescendant(of: uiView)) {
                uiView.addSubview(vid_win)
                /* Resize it to fit width */
                vid_win.bounds = CGRect(x:0, y:0, width:uiView.bounds.size.width,
                                        height:(uiView.bounds.size.height *
                                                1.0 * uiView.bounds.size.width /
                                                vid_win.bounds.size.width));
                /* Center it horizontally */
                vid_win.center = CGPoint(x:uiView.bounds.size.width / 2.0,
                                         y:vid_win.bounds.size.height / 2.0);
            }
        }
    }
}

class PjsipVars: ObservableObject {
    @Published var calling = false
    var dest: String = "sip:test@pjsip.org"
    var call_id: pjsua_call_id = PJSUA_INVALID_ID.rawValue
}

private func call_func(user_data: UnsafeMutableRawPointer?) {
    let pjsip_vars = Unmanaged<PjsipVars>.fromOpaque(user_data!).takeUnretainedValue()
    if (!pjsip_vars.calling) {
        var status: pj_status_t;
        var opt = pjsua_call_setting();

        pjsua_call_setting_default(&opt);
        opt.aud_cnt = 1;
        opt.vid_cnt = 1;

        let dest_str = strdup(pjsip_vars.dest);
        var dest:pj_str_t = pj_str(dest_str);
        status = pjsua_call_make_call(0, &dest, &opt, nil, nil, &pjsip_vars.call_id);
        DispatchQueue.main.sync {
            pjsip_vars.calling = (status == PJ_SUCCESS.rawValue);
        }
        free(dest_str);
    } else {
        if (pjsip_vars.call_id != PJSUA_INVALID_ID.rawValue) {
            DispatchQueue.main.sync {
                pjsip_vars.calling = false;
            }
            pjsua_call_hangup(pjsip_vars.call_id, 200, nil, nil);
            pjsip_vars.call_id = PJSUA_INVALID_ID.rawValue;
        }
    }

}

struct ContentView: View {
    @StateObject var pjsip_vars = PjsipVars()
    @EnvironmentObject var vinfo: VidInfo;

    var body: some View {
        VStack(alignment: .center) {
            HStack(alignment: .center) {
                if (!pjsip_vars.calling) {
                    Text("Destination:")
                    TextField(pjsip_vars.dest, text: $pjsip_vars.dest)
                        .frame(minWidth:0, maxWidth:200)
                }
            }

            Button(action: {
                let user_data =
                    UnsafeMutableRawPointer(Unmanaged.passUnretained(pjsip_vars).toOpaque())

                /* IMPORTANT:
                 * We need to call PJSIP API from a separate thread since
                 * PJSIP API can potentially block the main/GUI thread.
                 * And make sure we don't use Apple's Dispatch / gcd since
                 * it's incompatible with POSIX threads.
                 * In this example, we take advantage of PJSUA's timer thread
                 * to make and hangup call. For a more complex application,
                 * it is recommended to create your own separate thread
                 * instead for this purpose.
                 */
                pjsua_schedule_timer2_dbg(call_func, user_data, 0, "swift", 0);
            }) {
                if (!pjsip_vars.calling) {
                    Text("Make call")
                        .foregroundColor(Color.black)
                } else {
                    Text("Hangup call")
                        .foregroundColor(Color.black)
                }
            }
                .padding(.all, 8.0)
                .background(Color.green);

            VidView(vidWin: $vinfo.vid_win)
        }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
