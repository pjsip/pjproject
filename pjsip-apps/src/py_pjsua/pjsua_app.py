import py_pjsua
import sys
import thread

#
# Configurations
#
THIS_FILE = "pjsua_app.py"
C_QUIT = 0
C_LOG_LEVEL = 4

# STUN config.
# Set C_STUN_SRV to the address of the STUN server to enable STUN
#
C_STUN_SRV = ""
C_SIP_PORT = 5060
C_STUN_PORT = 3478


# Globals
#
g_acc_id = py_pjsua.PJSUA_INVALID_ID
g_current_call = py_pjsua.PJSUA_INVALID_ID


# Utility to get call info
#
def call_name(call_id):
	ci = py_pjsua.call_get_info(call_id)
	return "[Call " + `call_id` + " " + ci.remote_info + "]"

# Handler when invite state has changed.
#
def on_call_state(call_id, e):	
	global g_current_call
	ci = py_pjsua.call_get_info(call_id)
	write_log(3, call_name(call_id) + " state = " + `ci.state_text`)
	if ci.state == 6:
		g_current_call = py_pjsua.PJSUA_INVALID_ID

# Handler for incoming call
#
def on_incoming_call(acc_id, call_id, rdata):
	global g_current_call
	if g_current_call != py_pjsua.PJSUA_INVALID_ID:
		py_pjsua.call_answer(call_id, 486, "", None)
		return
	g_current_call = call_id
	ci = py_pjsua.call_get_info(call_id)
	write_log(3, "Incoming call: " + call_name(call_id))
	py_pjsua.call_answer(call_id, 200, "", None)

	
# Handler when media state has changed (e.g. established or terminated)
#
def on_call_media_state(call_id):
	ci = py_pjsua.call_get_info(call_id)
	if ci.media_status == 1:
		py_pjsua.conf_connect(ci.conf_slot, 0)
		py_pjsua.conf_connect(0, ci.conf_slot)
		write_log(3, call_name(call_id) + ": media is active")
	else:
		write_log(3, call_name(call_id) + ": media is inactive")


# Handler when account registration state has changed
#
def on_reg_state(acc_id):
	acc_info = py_pjsua.acc_get_info(acc_id)
	if acc_info.status != 0 and acc_info.status != 200:
		write_log(3, "Account (un)registration failed: rc=" + `acc_info.status` + " " + acc_info.status_text)
	else:
		write_log(3, "Account successfully (un)registered")


def on_buddy_state(buddy_id):
	write_log(3, "On Buddy state called")
	buddy_info = py_pjsua.buddy_get_info(buddy_id)
	if buddy_info.status != 0 and buddy_info.status != 200:
		write_log(3, "Status of " + `buddy_info.uri` + " is " + `buddy_info.status_text`)
	else:
		write_log(3, "Status : " + `buddy_info.status`)
		
def on_pager(call_id, strfrom, strto, contact, mime_type, text):
	write_log(3, "MESSAGE from " + `strfrom` + " : " + `text`)
	
def on_pager_status(call_id, strto, body, user_data, status, reason):
	write_log(3, "MESSAGE to " + `strto` + " status " + `status` + " reason " + `reason`)

# Utility: display PJ error and exit
#
def err_exit(title, rc):
    py_pjsua.perror(THIS_FILE, title, rc)
    exit(1)


# Logging function (also callback, called by pjsua-lib)
#
def log_cb(level, str, len):
    if level <= C_LOG_LEVEL:
        print str,

def write_log(level, str):
    log_cb(level, str + "\n", 0)


#
# Initialize pjsua.
#
def app_init():
	global g_acc_id

	# Create pjsua before anything else
	status = py_pjsua.create()
	if status != 0:
		err_exit("pjsua create() error", status)

	# Create and initialize logging config
	log_cfg = py_pjsua.logging_config_default()
	log_cfg.level = C_LOG_LEVEL
	log_cfg.cb = log_cb

	# Create and initialize pjsua config
	# Note: for this Python module, thread_cnt must be 0 since Python
	#       doesn't like to be called from alien thread (pjsua's thread
	#       in this case)	    
	ua_cfg = py_pjsua.config_default()
	ua_cfg.thread_cnt = 0
	ua_cfg.user_agent = "PJSUA/Python 0.1"
	ua_cfg.cb.on_incoming_call = on_incoming_call
	ua_cfg.cb.on_call_media_state = on_call_media_state
	ua_cfg.cb.on_reg_state = on_reg_state
	ua_cfg.cb.on_call_state = on_call_state
	ua_cfg.cb.on_buddy_state = on_buddy_state
	ua_cfg.cb.on_pager = on_pager
	ua_cfg.cb.on_pager_status = on_pager_status
	

	# Create and initialize media config
	med_cfg = py_pjsua.media_config_default()
	med_cfg.ec_tail_len = 0

	#
	# Initialize pjsua!!
	#
	status = py_pjsua.init(ua_cfg, log_cfg, med_cfg)
	if status != 0:
		err_exit("pjsua init() error", status)

	# Configure STUN config
	stun_cfg = py_pjsua.stun_config_default()
	stun_cfg.stun_srv1 = C_STUN_SRV
	stun_cfg.stun_srv2 = C_STUN_SRV
	stun_cfg.stun_port1 = C_STUN_PORT
	stun_cfg.stun_port2 = C_STUN_PORT

	# Configure UDP transport config
	transport_cfg = py_pjsua.transport_config_default()
	transport_cfg.port = C_SIP_PORT
	transport_cfg.stun_config = stun_cfg
	if C_STUN_SRV != "":
		transport_cfg.use_stun = 1

	# Create UDP transport
	status, transport_id = py_pjsua.transport_create(1, transport_cfg)
	if status != 0:
		py_pjsua.destroy()
		err_exit("Error creating UDP transport", status)

	# Create initial default account
	status, acc_id = py_pjsua.acc_add_local(transport_id, 1)
	if status != 0:
		py_pjsua.destroy()
		err_exit("Error creating account", status)

	g_acc_id = acc_id

# Add SIP account interractively
#
def add_account():
	global g_acc_id

	acc_domain = ""
	acc_username = ""
	acc_passwd =""
	confirm = ""
	
	# Input account configs
	print "Your SIP domain (e.g. myprovider.com): ",
	acc_domain = sys.stdin.readline()
	if acc_domain == "\n": 
		return
	acc_domain = acc_domain.replace("\n", "")

	print "Your username (e.g. alice): ",
	acc_username = sys.stdin.readline()
	if acc_username == "\n":
		return
	acc_username = acc_username.replace("\n", "")

	print "Your password (e.g. secret): ",
	acc_passwd = sys.stdin.readline()
	if acc_passwd == "\n":
		return
	acc_passwd = acc_passwd.replace("\n", "")

	# Configure account configuration
	acc_cfg = py_pjsua.acc_config_default()
	acc_cfg.id = "sip:" + acc_username + "@" + acc_domain
	acc_cfg.reg_uri = "sip:" + acc_domain
	acc_cfg.cred_count = 1
	acc_cfg.cred_info[0].realm = acc_domain
	acc_cfg.cred_info[0].scheme = "digest"
	acc_cfg.cred_info[0].username = acc_username
	acc_cfg.cred_info[0].data_type = 0
	acc_cfg.cred_info[0].data = acc_passwd

	# Add new SIP account
	status, acc_id = py_pjsua.acc_add(acc_cfg, 1)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error adding SIP account", status)
	else:
		g_acc_id = acc_id
		write_log(3, "Account " + acc_cfg.id + " added")


#
# Worker thread function.
# Python doesn't like it when it's called from an alien thread
# (pjsua's worker thread, in this case), so for Python we must
# disable worker thread in pjsua and poll pjsua from Python instead.
#
def worker_thread_main(arg):
	global C_QUIT
	thread_desc = 0;
	status = py_pjsua.thread_register("python worker", thread_desc)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error registering thread", status)
	else:
		while C_QUIT == 0:
			py_pjsua.handle_events(50)
		print "Worker thread quitting.."
		C_QUIT = 2


# Start pjsua
#
def app_start():
	# Done with initialization, start pjsua!!
	#
	status = py_pjsua.start()
	if status != 0:
		py_pjsua.destroy()
		err_exit("Error starting pjsua!", status)

	# Start worker thread
	thr = thread.start_new(worker_thread_main, (0,))
    
	print "PJSUA Started!!"


# Print application menu
#
def print_menu():
	print """
Menu:
  q   Quit application
 +a   Add account
 +b   Add buddy
  m   Make call
  h   Hangup current call (if any)
  i   Send instant message
	"""
	print "Choice: ", 

# Menu
#
def app_menu():
	global g_acc_id
	global g_current_call

	quit = 0
	while quit == 0:
		print_menu()
		choice = sys.stdin.readline()

		if choice[0] == "q":
			quit = 1

		elif choice[0] == "i":
			# Sending IM	
			print "Send IM to SIP URL: ",
			url = sys.stdin.readline()
			if url == "\n":
				continue

			# Send typing indication
			py_pjsua.im_typing(g_acc_id, url, 1, None) 

			print "The content: ",
			message = sys.stdin.readline()
			if message == "\n":
				py_pjsua.im_typing(g_acc_id, url, 0, None) 		
				continue

			# Send the IM!
			py_pjsua.im_send(g_acc_id, url, "", message, None, 0)

		elif choice[0] == "m":
			# Make call 
			print "Using account ", g_acc_id
			print "Make call to SIP URL: ",
			url = sys.stdin.readline()
			url = url.replace("\n", "")
			if url == "":
				continue

			# Initiate the call!
			status, call_id = py_pjsua.call_make_call(g_acc_id, url, 0, 0, None)
            
			if status != 0:
				py_pjsua.perror(THIS_FILE, "Error making call", status)
			else:
				g_current_call = call_id

		elif choice[0] == "+" and choice[1] == "b":
			# Add new buddy
			bc = py_pjsua.Buddy_Config()
			print "Buddy URL: ",
			bc.uri = sys.stdin.readline()
			if bc.uri == "\n":
				continue
            
			bc.subscribe = 1
			status, buddy_id = py_pjsua.buddy_add(bc)
			if status != 0:
				py_pjsua.perror(THIS_FILE, "Error adding buddy", status)

		elif choice[0] == "+" and choice[1] == "a":
			# Add account
			add_account()

		elif choice[0] == "h":
			if g_current_call != py_pjsua.PJSUA_INVALID_ID:
				py_pjsua.call_hangup(g_current_call, 603, "", None)
			else:
				print "No current call"


#
# main
#
app_init()
app_start()
app_menu()

#
# Done, quitting..
#
print "PJSUA shutting down.."
C_QUIT = 1
# Give the worker thread chance to quit itself
while C_QUIT != 2:
    py_pjsua.handle_events(50)

print "PJSUA destroying.."
py_pjsua.destroy()

