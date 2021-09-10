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
