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
     
        //Done button to the keyboard
        sipIpTField.addDoneButtonOnKeyboard()
        sipPortTField.addDoneButtonOnKeyboard()
        sipUsernameTField.addDoneButtonOnKeyboard()
        sipPasswordTField.addDoneButtonOnKeyboard()
        sipDestinationUriTField.addDoneButtonOnKeyboard()
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
extension UITextField{
    @IBInspectable var doneAccessory: Bool{
        get{
            return self.doneAccessory
        }
        set (hasDone) {
            if hasDone{
                addDoneButtonOnKeyboard()
            }
        }
    }

    func addDoneButtonOnKeyboard()
    {
        let doneToolbar: UIToolbar = UIToolbar(frame: CGRect.init(x: 0, y: 0, width: UIScreen.main.bounds.width, height: 50))
        doneToolbar.barStyle = .default

        let flexSpace = UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
        let done: UIBarButtonItem = UIBarButtonItem(title: "Done", style: .done, target: self, action: #selector(self.doneButtonAction))

        let items = [flexSpace, done]
        doneToolbar.items = items
        doneToolbar.sizeToFit()

        self.inputAccessoryView = doneToolbar
    }

    @objc func doneButtonAction()
    {
        self.resignFirstResponder()
    }
}
