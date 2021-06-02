//
//  Model.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 22.01.2021.
//

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
