import UIKit


var vc_inst: ViewController! = nil

func acc_listener_swift(status: Bool) {
//    DispatchQueue.main.async {
//        vc_inst.updateAccStatus(status: status)
//    }
}

class ViewController: UIViewController {
    
    // Status Label
    var statusLabel: UILabel!
    // Sip settings Text Fields
    var sipIpTField: UITextField!
    var sipPortTField: UITextField!
    var sipUsernameTField: UITextField!
    var sipPasswordTField: UITextField!
    var loginButton: UIButton!
    var logoutButton: UIButton!
    // Destination Uri to Making outgoing call
    var sipDestinationUriTField: UITextField!

    var loaded: Bool = false
    var accStatus: Bool!

    override func viewDidLoad() {
        super.viewDidLoad()
        vc_inst = self
        view.backgroundColor = .white

        setupUI()
        
        sipIpTField.text = "192.168.1.1"
        sipPortTField.text = "5060"
        sipUsernameTField.text = "user"
        sipPasswordTField.text = "password"
        sipDestinationUriTField.text = "sip:user@example.com"
        
    }

    func setupUI() {
        statusLabel = UILabel()
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(statusLabel)

        sipIpTField = createTextField(placeholder: "SIP IP")
        sipPortTField = createTextField(placeholder: "SIP Port")
        sipUsernameTField = createTextField(placeholder: "SIP Username")
        sipPasswordTField = createTextField(placeholder: "SIP Password")
        sipDestinationUriTField = createTextField(placeholder: "Destination URI")

        loginButton = createButton(title: "Login", action: #selector(loginClick(_:)))
        logoutButton = createButton(title: "Logout", action: #selector(logoutClick(_:)))

        let stackView = UIStackView(arrangedSubviews: [
            statusLabel, sipIpTField, sipPortTField, sipUsernameTField, sipPasswordTField, sipDestinationUriTField, loginButton, logoutButton
        ])
        stackView.axis = .vertical
        stackView.spacing = 10
        stackView.translatesAutoresizingMaskIntoConstraints = false

        view.addSubview(stackView)

        NSLayoutConstraint.activate([
            stackView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            stackView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            stackView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20)
        ])
    }

    func createTextField(placeholder: String) -> UITextField {
        let textField = UITextField()
        textField.placeholder = placeholder
        textField.borderStyle = .roundedRect
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.addDoneButtonOnKeyboard()
        return textField
    }

    func createButton(title: String, action: Selector) -> UIButton {
        let button = UIButton(type: .system)
        button.setTitle(title, for: .normal)
        button.addTarget(self, action: action, for: .touchUpInside)
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }

    func updateAccStatus(status: Bool) {
        accStatus = status
        if status {
            statusLabel.text = "Reg Status: REGISTERED"
            loginButton.isEnabled = false
            logoutButton.isEnabled = true
            if !loaded {
                showMainScreen()
                loaded = true
            }
        } else {
            statusLabel.text = "Reg Status: NOT REGISTERED"
            loginButton.isEnabled = true
            logoutButton.isEnabled = false
        }
    }

    @objc func refreshStatus(_ sender: UIButton) {
        // Implement refresh status functionality
    }

    @objc func loginClick(_ sender: UIButton) {
        if !sipUsernameTField.text!.isEmpty &&
           !sipPasswordTField.text!.isEmpty &&
           !sipIpTField.text!.isEmpty &&
           !sipPortTField.text!.isEmpty {
            CPPWrapper().createAccountWrapper(sipUsernameTField.text,
                                              sipPasswordTField.text,
                                              sipIpTField.text,
                                              sipPortTField.text,
                                              "0x24a19b932486ebb74cbaf0c6c4aec4711ab0b43fdbff880d3a0f24a5dc74c7f84c46d53b74a51d7c2d5362762dfbe5a59c3e27f5d7957ac6fa8684a3af80406c1b",
                                              "E22F20CF-6BA3-4464-B5E4-C7EC90B3B64E:1717143850787")
        } else {
            showAlert(title: "SIP SETTINGS ERROR", message: "Please fill the form / Logout")
        }
    }

    @objc func logoutClick(_ sender: UIButton) {
        CPPWrapper().unregisterAccountWrapper()
    }

    @objc func callClick(_ sender: UIButton) {
        if accStatus {
            let id = sipDestinationUriTField.text ?? "<SIP-NUMBER>"
//            CPPWrapper().outgoingCall(id)
            CPPWrapper().call_listener_wrapper(call_status_listener_swift)
        } else {
            showAlert(title: "Outgoing Call Error", message: "Please register to be able to make call")
        }
    }

    func showAlert(title: String, message: String) {
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
}

extension UITextField {
    @IBInspectable var doneAccessory: Bool {
        get { return self.doneAccessory }
        set { if newValue { addDoneButtonOnKeyboard() } }
    }

    func addDoneButtonOnKeyboard() {
        let doneToolbar: UIToolbar = UIToolbar(frame: CGRect(x: 0, y: 0, width: UIScreen.main.bounds.width, height: 50))
        doneToolbar.barStyle = .default

        let flexSpace = UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
        let done: UIBarButtonItem = UIBarButtonItem(title: "Done", style: .done, target: self, action: #selector(doneButtonAction))

        doneToolbar.items = [flexSpace, done]
        doneToolbar.sizeToFit()

        self.inputAccessoryView = doneToolbar
    }

    @objc func doneButtonAction() {
        self.resignFirstResponder()
    }
}
