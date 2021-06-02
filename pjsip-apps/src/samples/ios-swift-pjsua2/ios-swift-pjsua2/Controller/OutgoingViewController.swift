//
//  OutgoingViewController.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 20.01.2021.
//


import UIKit


class OutgoingViewController: UIViewController {
    
    var outgoingCallId : String = ""
    
    @IBOutlet weak var outgoingCallTitle: UILabel!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Outgoing Call"
        outgoingCallTitle.text = outgoingCallId
        CPPWrapper().outgoingCall(outgoingCallId)
        CPPWrapper().call_listener_wrapper(call_status_listener_swift)
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        CPPWrapper().hangupCall();
    }
    
    @IBAction func hangupClick(_ sender: UIButton) {
        CPPWrapper().hangupCall();
        self.dismiss(animated: true, completion: nil)
    }
}
