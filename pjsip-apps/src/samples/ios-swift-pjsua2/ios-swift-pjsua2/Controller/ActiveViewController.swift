//
//  ActiveViewController.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 21.01.2021.
//

import UIKit


class ActiveViewController: UIViewController {

    var activeCallId : String = ""
    @IBOutlet weak var holdButton: UIButton!
    
    var holdFlag : Bool = false
    @IBOutlet weak var activeCallTitle: UILabel!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        activeCallTitle.text = activeCallId
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        CPPWrapper().hangupCall();
        call_status_listener_swift(call_answer_code: 1);
    }
    
    @IBAction func hangupClick(_ sender: UIButton) {
        CPPWrapper().hangupCall()
        self.view.window!.rootViewController?.dismiss(animated: true, completion: nil)
    }
    
    @IBAction func holdClick(_ sender: Any) {
        
        if(holdFlag == false){
            CPPWrapper().holdCall()
            holdButton.setTitle("Unhold", for: .normal)
        } else if (holdFlag == true) {
            CPPWrapper().unholdCall()
            holdButton.setTitle("Hold", for: .normal)
        }
        
        //switch hold flag
        holdFlag = !holdFlag
    
    }
    
 
}
