/*
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Contributed by Emre Tufekci (github.com/emretufekci)
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

import UIKit
import SwiftUI
import WebKit
import PortSIPVoIPSDK

func topMostController() -> Void {
//    print(UIApplication.shared.windows);
//    var topController: UIViewController = (UIApplication.shared.windows.first { $0.isKeyWindow }?.rootViewController!)!
//    while (topController.presentedViewController != nil) {
//        topController = topController.presentedViewController!
//    }
//    return topController
}

func showMainScreen() {
//    DispatchQueue.main.async () {
//        let topVC = topMostController()
//        let vcToPresent = MainController.shared
//        topVC.present(vcToPresent, animated: true, completion: nil)
//    }
}

func incoming_call_swift() {
    print("incoming call")
//    let vcToPresent = MainController.shared
//    vcToPresent.callData.inCallWith = CPPWrapper().incomingCallInfoWrapper()
}

func call_status_listener_swift ( call_answer_code: Int32) {
    if (call_answer_code == 0) {
        print("declined")
        
//        let vcToPresent = MainController.shared
//        let caller = vcToPresent.callData.inCallWith
//        vcToPresent.hostingController!.rootView.sendDataToHTML("{\"action\": \"call-ended\", \"body\": {\"from\": \"\(caller)\"}}")
//        print("caller:", caller)
    }
    else if (call_answer_code == 1) {
        print("answered")
        var handle = CallController.shared.currentCaller
        CallController.shared.startCall(name: handle)
        let jsonString = """
        {
            "action": "call-started",
            "body": {
                "to": \(handle)
            }
        }
        """
        HomeView.shared.webView?.sendDataToHTML(jsonString)
    }
}

func update_video_swift(window: UnsafeMutableRawPointer?) {
//    DispatchQueue.main.async () {
//        let storyboard = UIStoryboard(name: "Main", bundle: nil)
//        let vc = storyboard.instantiateViewController(withIdentifier: "viewController")
//        let activeVc = vc.storyboard!.instantiateViewController(withIdentifier: "activeCallVC") as! ActiveViewController
//        let topVC = topMostController()
//        activeVc.activeCallId = CPPWrapper().incomingCallInfoWrapper()
//        topVC.present(activeVc, animated: true, completion: nil)
//        let vid_view:UIView =
//            Unmanaged<UIView>.fromOpaque(window!).takeUnretainedValue();
//        activeVc.loadViewIfNeeded()
//        activeVc.updateVideo(vid_win: vid_view);
//    }
}
