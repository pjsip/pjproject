//
//  CallViewController.swift
//  ios-swift-pjsua2
//
//  Created by Emre on 3.08.2021.
//

import UIKit

class CallViewController: UIViewController {

    //Destination Uri to Making outgoing call
    @IBOutlet weak var sipDestinationUriTField: UITextField!
    
    @IBOutlet weak var ipPortLabel: UILabel!
    
    var sipIp: String = ""
    var sipPort: String = ""
    var sipUsername: String = ""
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
        sipDestinationUriTField.addDoneButtonOnKeyboard()

        
        //TODO::FIX IT TERRIBLE USAGE.
         sipIp = UserDefaults.standard.string(forKey: "sipIP") ?? "sipIP"
         sipPort = UserDefaults.standard.string(forKey: "sipPort") ?? "sipPort"
         sipUsername = UserDefaults.standard.string(forKey: "sipUsername") ?? "sipUsername"
        
    }
    
    override func viewDidAppear(_ animated: Bool) {
        //TODO::FIX IT TERRIBLE USAGE.
         sipIp = UserDefaults.standard.string(forKey: "sipIP") ?? "sipIP"
         sipPort = UserDefaults.standard.string(forKey: "sipPort") ?? "sipPort"
         sipUsername = UserDefaults.standard.string(forKey: "sipUsername") ?? "sipUsername"
    
        ipPortLabel.text = "@"+sipIp+":"+sipPort
    }
    

    //Call Button
    @IBAction func callClick(_ sender: UIButton) {
        
        if(CPPWrapper().registerStateInfoWrapper() != false){
            let vcToPresent = self.storyboard!.instantiateViewController(withIdentifier: "outgoingCallVC") as! OutgoingViewController
            if(sipDestinationUriTField.text != nil){
                vcToPresent.outgoingCallId = "sip:" + sipDestinationUriTField.text! + "@" + sipIp + ":" + sipPort
                self.present(vcToPresent, animated: true, completion: nil)
            }
            
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


    @IBAction func call9900(_ sender: UIButton) {
        if(CPPWrapper().registerStateInfoWrapper() != false){
            let vcToPresent = self.storyboard!.instantiateViewController(withIdentifier: "outgoingCallVC") as! OutgoingViewController
            vcToPresent.outgoingCallId = "sip:" + "9900" + "@" + sipIp + ":" + sipPort
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
    
    @IBAction func call9901(_ sender: UIButton) {
        if(CPPWrapper().registerStateInfoWrapper() != false){
            let vcToPresent = self.storyboard!.instantiateViewController(withIdentifier: "outgoingCallVC") as! OutgoingViewController
            vcToPresent.outgoingCallId = "sip:" + "9901" + "@" + sipIp + ":" + sipPort
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
