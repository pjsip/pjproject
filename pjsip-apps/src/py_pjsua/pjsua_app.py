import py_pjsua
import sys
import thread

#
# Configurations
#
APP = "pjsua_app.py"
C_QUIT = 0
C_LOG_LEVEL = 3

C_SIP_PORT = 5060
C_STUN_SRV = ""
C_STUN_PORT = 3478

C_ACC_REGISTRAR = ""
#C_ACC_REGISTRAR = "sip:iptel.org"
C_ACC_ID = "sip:bulukucing1@iptel.org"
C_ACC_REALM = "iptel.org"
C_ACC_USERNAME = "bulukucing1"
C_ACC_PASSWORD = "netura"

# Display PJ error and exit
def err_exit(title, rc):
    py_pjsua.perror(APP, title, rc)
    exit(1)

# Logging callback
def log_cb(level, str, len):
    if level >= C_LOG_LEVEL:
        print str,

# Initialize pjsua
def app_init():
    # Create pjsua before anything else
    status = py_pjsua.create()
    if status != 0:
        err_exit("pjsua create() error", status)

    # Create and initialize logging config
    log_cfg = py_pjsua.logging_config_default()
    log_cfg.level = C_LOG_LEVEL
    log_cfg.cb = log_cb

    # Create and initialize pjsua config
    ua_cfg = py_pjsua.config_default()
    ua_cfg.thread_cnt = 0
    ua_cfg.user_agent = "PJSUA/Python 0.1"

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
    # Note: transport_id is supposed to be integer
    status, transport_id = py_pjsua.transport_create(1, transport_cfg)
    if status != 0:
        py_pjsua.destroy()
        err_exit("Error creating UDP transport", status)


    # Configure account configuration
    acc_cfg = py_pjsua.acc_config_default()
    acc_cfg.id = C_ACC_ID
    acc_cfg.reg_uri = C_ACC_REGISTRAR
    acc_cfg.cred_count = 1
    acc_cfg.cred_info[0].realm = C_ACC_REALM
    acc_cfg.cred_info[0].scheme = "digest"
    acc_cfg.cred_info[0].username = C_ACC_USERNAME
    acc_cfg.cred_info[0].data_type = 0
    acc_cfg.cred_info[0].data = C_ACC_PASSWORD

    # Add new SIP account
    # Note: acc_id is supposed to be integer
    status, acc_id = py_pjsua.acc_add(acc_cfg, 1)
    if status != 0:
        py_pjsua.destroy()
        err_exit("Error adding SIP account", status)


# Worker thread function
def worker_thread_main(arg):
    thread_desc = 0;
    status = py_pjsua.thread_register("worker thread", thread_desc)
    if status != 0:
        py_pjsua.perror(APP, "Error registering thread", status)
    else:
        while C_QUIT == 0:
            py_pjsua.handle_events(50)

# Start pjsua
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
def print_menu():
    print "Menu:"
    print "  q   Quit application"
    print "  s   Add buddy"
    print "Choice: ",

    
# Menu
def app_menu():
    quit = 0
    while quit == 0:
        print_menu()
        choice = sys.stdin.readline()
        if choice[0] == "q":
            quit = 1
        elif choice[0] == "s":
            bc = py_pjsua.Buddy_Config()
            print "Buddy URI: ",
            bc.uri = sys.stdin.readline()
            if bc.uri == "":
                continue
            
            bc.subscribe = 1
            status = py_pjsua.buddy_add(bc)
            if status != 0:
                py_pjsua.perror(APP, "Error adding buddy", status)


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
py_pjsua.destroy()

