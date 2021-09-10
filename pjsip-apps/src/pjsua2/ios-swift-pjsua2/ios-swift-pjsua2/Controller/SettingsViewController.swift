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
    
    //NAT Settings
    
    //Turn Settings
    @IBOutlet weak var turnIPTField: UITextField!
    @IBOutlet weak var turnPortTField: UITextField!
    @IBOutlet weak var turnUsernameTField: UITextField!
    @IBOutlet weak var turnPasswordTField: UITextField!
    
    //Stun Settings
    @IBOutlet weak var stunIpTField: UITextField!
    @IBOutlet weak var stunPortTField: UITextField!
    
    
    
    //@IBOutlet weak var udpSwitch: UISwitch!
    @IBOutlet weak var stunSwitch: UISwitch!
    @IBOutlet weak var turnSwitch: UISwitch!
    @IBOutlet weak var tlsSwitch: UISwitch!
    @IBOutlet weak var iceSwitch: UISwitch!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        loadUserDefaultsForm()

        
        //Create Lib
        if(stunSwitch.isOn){
            CPPWrapper().createLibWrapper(stunIpTField.text, stunPortTField.text, stunSwitch.isOn, tlsSwitch.isOn)
        }else {
            //First craete lib
            CPPWrapper().createLibWrapper("", "", false, false)
        }
        
        //Listen incoming call via function pointer
        CPPWrapper().incoming_call_wrapper(incoming_call_swift)
     
        //Done button to the keyboard
        sipIpTField.addDoneButtonOnKeyboard()
        sipPortTField.addDoneButtonOnKeyboard()
        sipUsernameTField.addDoneButtonOnKeyboard()
        sipPasswordTField.addDoneButtonOnKeyboard()
        
        turnIPTField.addDoneButtonOnKeyboard()
        turnPortTField.addDoneButtonOnKeyboard()
        turnUsernameTField.addDoneButtonOnKeyboard()
        turnPasswordTField.addDoneButtonOnKeyboard()
        
        stunIpTField.addDoneButtonOnKeyboard()
        stunPortTField.addDoneButtonOnKeyboard()
        
        
    }
    
    
    //Refresh Button
    @IBAction func refreshStatus(_ sender: UIButton) {
        if (CPPWrapper().registerStateInfoWrapper()){
            statusLabel.text = "Sip Status: REGISTERED"
        }else {
            statusLabel.text = "Sip Status: NOT REGISTERED"
        }
        showAlert(alertTitle: "Refreshing", alertMessage: "Sip status refreshing...", duration: 0.5)
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
                    sipPortTField.text,
                    turnUsernameTField.text,
                    turnPasswordTField.text,
                    turnIPTField.text,
                    turnPortTField.text,

                    turnSwitch.isOn,
                    tlsSwitch.isOn,
                    iceSwitch.isOn
                )
            showAlert(alertTitle: "LOGIN/REGISTER", alertMessage: "SIP REGISTER request successfully sent", duration: 1.0)

            
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
        showAlert(alertTitle: "LOGOUT/UNREGISTER", alertMessage: "SIP UNREGISTER request successfully sent", duration: 1.0)

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


    @IBAction func saveBtnClick(_ sender: UIButton) {
        
        let settings = UserDefaults.standard
        
        
        //Sip Settings
        settings.set(sipIpTField.text ?? "", forKey: "sipIP")
        settings.set(sipPortTField.text ?? "", forKey: "sipPort")
        settings.set(sipUsernameTField.text ?? "", forKey: "sipUsername")
        settings.set(sipPasswordTField.text ?? "", forKey: "sipPassword")
        
        //NAT Settings
        
        //Turn Settings
        settings.set(turnIPTField.text ?? "", forKey: "turnIP")
        settings.set(turnPortTField.text ?? "", forKey: "turnPort")
        settings.set(turnUsernameTField.text ?? "", forKey: "turnUsername")
        settings.set(turnPasswordTField.text ?? "", forKey: "turnPassword")
        
        //Stun Settings
        settings.set(stunIpTField.text ?? "", forKey: "stunIP")
        settings.set(stunPortTField.text ?? "", forKey: "stunPort")
        
        //Switch Keys
        //settings.set(udpSwitch.isOn, forKey: "udpSwitch")
        settings.set(stunSwitch.isOn, forKey: "stunSwitch")
        settings.set(turnSwitch.isOn, forKey: "turnSwitch")
        settings.set(tlsSwitch.isOn, forKey: "tlsSwitch")
        settings.set(iceSwitch.isOn, forKey: "iceSwitch")
        
        showAlert(alertTitle: "Successfully Saved", alertMessage: "Settings saved successfully!", duration: 1.5)
    }
    
    func showAlert(alertTitle:String, alertMessage: String, duration:Double) {
        let alert = UIAlertController(title: alertTitle, message: alertMessage, preferredStyle: .alert)
        self.present(alert, animated: true, completion: nil)
        Timer.scheduledTimer(withTimeInterval: duration, repeats: false, block: { _ in alert.dismiss(animated: true, completion: nil)} )
    }
    
    func loadUserDefaultsForm(){
        
        //Load Sip Settings
        sipIpTField.text = UserDefaults.standard.string(forKey: "sipIP")
        sipPortTField.text = UserDefaults.standard.string(forKey: "sipPort")
        sipUsernameTField.text = UserDefaults.standard.string(forKey: "sipUsername")
        sipPasswordTField.text = UserDefaults.standard.string(forKey: "sipPassword")
        
        //Load Nat Settings
        
        //Load Turn Settings
        turnIPTField.text = UserDefaults.standard.string(forKey: "turnIP")
        turnPortTField.text = UserDefaults.standard.string(forKey: "turnPort")
        turnUsernameTField.text = UserDefaults.standard.string(forKey: "turnUsername")
        turnPasswordTField.text = UserDefaults.standard.string(forKey: "turnPassword")
        
        //Load Stun Settings
        stunIpTField.text = UserDefaults.standard.string(forKey: "stunIP")
        stunPortTField.text = UserDefaults.standard.string(forKey: "stunPort")
        
        //Load Switches
        //udpSwitch.isOn = UserDefaults.standard.bool(forKey: "udpSwitch")
        stunSwitch.isOn = UserDefaults.standard.bool(forKey: "stunSwitch")
        turnSwitch.isOn = UserDefaults.standard.bool(forKey: "turnSwitch")
        tlsSwitch.isOn = UserDefaults.standard.bool(forKey: "tlsSwitch")
        iceSwitch.isOn = UserDefaults.standard.bool(forKey: "iceSwitch")
        
        
    }
    
    @IBAction func saveReinitBtnClick(_ sender: UIButton) {

        let settings = UserDefaults.standard
        
        
        //Sip Settings
        settings.set(sipIpTField.text ?? "", forKey: "sipIP")
        settings.set(sipPortTField.text ?? "", forKey: "sipPort")
        settings.set(sipUsernameTField.text ?? "", forKey: "sipUsername")
        settings.set(sipPasswordTField.text ?? "", forKey: "sipPassword")
        
        //NAT Settings
        
        //Turn Settings
        settings.set(turnIPTField.text ?? "", forKey: "turnIP")
        settings.set(turnPortTField.text ?? "", forKey: "turnPort")
        settings.set(turnUsernameTField.text ?? "", forKey: "turnUsername")
        settings.set(turnPasswordTField.text ?? "", forKey: "turnPassword")
        
        //Stun Settings
        settings.set(stunIpTField.text ?? "", forKey: "stunIP")
        settings.set(stunPortTField.text ?? "", forKey: "stunPort")
        
        //Switch Keys
        //settings.set(udpSwitch.isOn, forKey: "udpSwitch")
        settings.set(stunSwitch.isOn, forKey: "stunSwitch")
        settings.set(turnSwitch.isOn, forKey: "turnSwitch")
        settings.set(tlsSwitch.isOn, forKey: "tlsSwitch")
        settings.set(iceSwitch.isOn, forKey: "iceSwitch")
        
        showAlert(alertTitle: "Successfully Saved", alertMessage: "Settings saved successfully. PJSIP Library  reinit process started...", duration: 3.0)
        CPPWrapper().deleteLibWrapper()
        CPPWrapper().createLibWrapper(stunIpTField.text, stunPortTField.text, stunSwitch.isOn, tlsSwitch.isOn)
    }
    
    
    @IBAction func stunInfoClick(_ sender: UIButton) {
        // create the alert
          let alert = UIAlertController(title: "STUN", message: "To be able to enable STUN please save & re-launch the application.", preferredStyle: UIAlertController.Style.alert)

          // add an action (button)
          alert.addAction(UIAlertAction(title: "OK", style: UIAlertAction.Style.default, handler: nil))

          // show the alert
          self.present(alert, animated: true, completion: nil)
    }
    
    
    @IBAction func turnInfoClick(_ sender: UIButton) {
        // create the alert
          let alert = UIAlertController(title: "TURN", message: "To be able to enable turn save>logout>login.", preferredStyle: UIAlertController.Style.alert)

          // add an action (button)
          alert.addAction(UIAlertAction(title: "OK", style: UIAlertAction.Style.default, handler: nil))

          // show the alert
          self.present(alert, animated: true, completion: nil)
    }
    
    
    @IBAction func tlsIfoClick(_ sender: UIButton) {
        // create the alert
          let alert = UIAlertController(title: "TLS 1.3", message: "To be able to enable TLS 1.3 please save & re-launch the application.", preferredStyle: UIAlertController.Style.alert)

          // add an action (button)
          alert.addAction(UIAlertAction(title: "OK", style: UIAlertAction.Style.default, handler: nil))

          // show the alert
          self.present(alert, animated: true, completion: nil)
    }
    
    @IBAction func iceInfoClick(_ sender: UIButton) {
        // create the alert
          let alert = UIAlertController(title: "ICE", message: "To be able to enable ice save>logout>login.", preferredStyle: UIAlertController.Style.alert)

          // add an action (button)
          alert.addAction(UIAlertAction(title: "OK", style: UIAlertAction.Style.default, handler: nil))

          // show the alert
          self.present(alert, animated: true, completion: nil)
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
