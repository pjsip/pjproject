//
//  ContentView.swift
//  ipjsua-swift
//
//  Created by Ming on 4/2/21.
//

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

struct ContentView: View {
    @State private var calling = false
    @State private var dest: String = "sip:402@pjsip.org"
    @State private var call_id: pjsua_call_id = PJSUA_INVALID_ID.rawValue
    @EnvironmentObject var vinfo: VidInfo;

    var body: some View {
        VStack(alignment: .center) {
            HStack(alignment: .center) {
                if (!calling) {
                    Text("Destination:")
                    TextField(dest, text: $dest)
                        .frame(minWidth:0, maxWidth:200)
                }
            }

            Button(action: {
                if (!calling) {
                    calling = true;
                    var status: pj_status_t;
                    var opt = pjsua_call_setting();

                    pjsua_call_setting_default(&opt);
                    opt.aud_cnt = 1;
                    opt.vid_cnt = 1;

                    var dest_str:pj_str_t = pj_str(strdup(dest));
                    status = pjsua_call_make_call(0, &dest_str, &opt, nil, nil, &call_id);
                    if (status != PJ_SUCCESS.rawValue) {
                        calling = false;
                    }
                } else {
                    if (call_id != PJSUA_INVALID_ID.rawValue) {
                        calling = false;
                        pjsua_call_hangup(call_id, 200, nil, nil);
                        call_id = PJSUA_INVALID_ID.rawValue;
                    }
                }
            }) {
                if (!calling) {
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
