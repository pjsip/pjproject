//
//  ViewController.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 6.01.2021.
//

import UIKit



class ViewController: UIViewController {
    
    // Status Label
    @IBOutlet weak var statusLabel: UILabel!
    
    // Sip settings Text Fields
    @IBOutlet weak var sipIpTField: UITextField!
    @IBOutlet weak var sipPortTField: UITextField!
    @IBOutlet weak var sipUsernameTField: UITextField!
    @IBOutlet weak var sipPasswordTField: UITextField!
    
    //Destination Uri to Making outgoing call
    @IBOutlet weak var sipDestinationUriTField: UITextField!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        
        //Create Lib
        CPPWrapper().createLibWrapper()
        
        //Listen incoming call via function pointer
        CPPWrapper().incoming_call_wrapper(incoming_call_swift)
        
    }
    
    //Refresh Button
    @IBAction func refreshStatus(_ sender: UIButton) {
        if (CPPWrapper().registerStateInfoWrapper()){
            statusLabel.text = "Sip Status: REGISTERED"
        }else {
            statusLabel.text = "Sip Status: NOT REGISTERED"
        }
    }
    
    
    //Login Button
    @IBAction func loginClick(_ sender: UIButton) {
        
        sipUsernameTField.text = "test2"
        sipPasswordTField.text = "test2"
        sipIpTField.text = "10.251.11.134"
        sipPortTField.text = "5060"
        
        //Check user already logged in. && Form is filled
        if (CPPWrapper().registerStateInfoWrapper() == false
                && !sipUsernameTField.text!.isEmpty
                && !sipPasswordTField.text!.isEmpty
                && !sipIpTField.text!.isEmpty
                && !sipPortTField.text!.isEmpty){
            
            //Register to the user
            CPPWrapper().createAccountWrapper(
                sipUsernameTField.text,
                sipPasswordTField.text,
                sipIpTField.text,
                sipPortTField.text)

            
        } else {
            let alert = UIAlertController(title: "SIP SETTINGS ERROR", message: "Please fill the form / Logout", preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "OK", style: .default, handler: { action in
                switch action.style{
                    case .default:
                    print("default")
                    
                    case .cancel:
                    print("cancel")
                    
                    case .destructive:
                    print("destructive")
                    
                @unknown default:
                    fatalError()
                }
            }))
            self.present(alert, animated: true, completion: nil)
        }
        
        //Wait until register/unregister
        sleep(2)
        if (CPPWrapper().registerStateInfoWrapper()){
            statusLabel.text = "Sip Status: REGISTERED"
        } else {
            statusLabel.text = "Sip Status: NOT REGISTERED"
        }

    }
    
    //Logout Button
    @IBAction func logoutClick(_ sender: UIButton) {
        
        /**
        Only unregister from an account.
         */
        //Unregister
        CPPWrapper().unregisterAccountWrapper()
        
        //Wait until register/unregister
        sleep(2)
        if (CPPWrapper().registerStateInfoWrapper()){
            statusLabel.text = "Sip Status: REGISTERED"
        } else {
            statusLabel.text = "Sip Status: NOT REGISTERED"
        }
    }

    //Call Button
    @IBAction func callClick(_ sender: UIButton) {
        
        if(CPPWrapper().registerStateInfoWrapper() != false){
            sipDestinationUriTField.text = "sip:test3@10.251.11.134:5060"
            
            let vcToPresent = self.storyboard!.instantiateViewController(withIdentifier: "outgoingCallVC") as! OutgoingViewController
            vcToPresent.outgoingCallId = sipDestinationUriTField.text ?? "<SIP-NUMBER>"
            self.present(vcToPresent, animated: true, completion: nil)
        }else {
            let alert = UIAlertController(title: "Outgoing Call Error", message: "Please register to be able to make call", preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "OK", style: .default, handler: { action in
                switch action.style{
                    case .default:
                    print("default")
                    
                    case .cancel:
                    print("cancel")
                    
                    case .destructive:
                    print("destructive")
                    
                @unknown default:
                    fatalError()
                }
            }))
            self.present(alert, animated: true, completion: nil)
        }

    }
    
    
}
