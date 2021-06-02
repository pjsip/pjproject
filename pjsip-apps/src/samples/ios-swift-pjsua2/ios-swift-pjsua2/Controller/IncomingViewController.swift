//
//  IncomingViewController.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 13.01.2021.
//

import UIKit



class IncomingViewController: UIViewController {

    var incomingCallId : String = ""
    @IBOutlet weak var callTitle: UILabel!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Gelen Arama"
        callTitle.text = incomingCallId
        
        CPPWrapper().call_listener_wrapper(call_status_listener_swift)
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        CPPWrapper().hangupCall();
    }
    
    
    @IBAction func hangupClick(_ sender: UIButton) {
        CPPWrapper().hangupCall();
        self.dismiss(animated: true, completion: nil)
    }
    
    
    @IBAction func answerClick(_ sender: UIButton) {
        CPPWrapper().answerCall();
    }

}
