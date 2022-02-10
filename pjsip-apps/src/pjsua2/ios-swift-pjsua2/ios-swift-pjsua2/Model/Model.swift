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

func topMostController() -> UIViewController {
    var topController: UIViewController = (UIApplication.shared.windows.first { $0.isKeyWindow }?.rootViewController!)!
        while (topController.presentedViewController != nil) {
            topController = topController.presentedViewController!
        }
        return topController
    }


    func incoming_call_swift() {
        DispatchQueue.main.async () {
            let storyboard = UIStoryboard(name: "Main", bundle: nil)
            let vc = storyboard.instantiateViewController(withIdentifier: "viewController")
            let topVC = topMostController()
            let vcToPresent = vc.storyboard!.instantiateViewController(withIdentifier: "incomingCallVC") as! IncomingViewController
            vcToPresent.incomingCallId = CPPWrapper().incomingCallInfoWrapper()
            topVC.present(vcToPresent, animated: true, completion: nil)
        }
    }


    func call_status_listener_swift ( call_answer_code: Int32) {
        if (call_answer_code == 0){
                    DispatchQueue.main.async () {
                        UIApplication.shared.windows.first { $0.isKeyWindow}?.rootViewController?.dismiss(animated: true, completion: nil)
                    }
        }
        else if (call_answer_code == 1) {
                    DispatchQueue.main.async () {
                        let storyboard = UIStoryboard(name: "Main", bundle: nil)
                        let vc = storyboard.instantiateViewController(withIdentifier: "viewController")
                        let topVC = topMostController()
                        let vcToPresent = vc.storyboard!.instantiateViewController(withIdentifier: "activeCallVC") as! ActiveViewController
                        vcToPresent.activeCallId = CPPWrapper().incomingCallInfoWrapper()
                        topVC.present(vcToPresent, animated: true, completion: nil)
                    }
                }
        else {
                print("ERROR CODE:")
            }
        }
