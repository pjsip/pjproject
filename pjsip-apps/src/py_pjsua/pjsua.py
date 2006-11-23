import py_pjsua

status = py_pjsua.create()
print "py status " + `status`


#
# Create configuration objects
#
ua_cfg = py_pjsua.Config()
log_cfg = py_pjsua.Logging_Config()
media_cfg = py_pjsua.Media_Config()

#
# Logging callback.
#
def logging_cb1(level, str, len):
    print str,


#
# Initialize configs with default values.
#
py_pjsua.config_default(ua_cfg)
py_pjsua.logging_config_default(log_cfg)
py_pjsua.media_config_default(media_cfg)

#
# Configure logging
#
log_cfg.cb = logging_cb1
log_cfg.console_level = 4

#
# Initialize pjsua!
#
status = py_pjsua.init(ua_cfg, log_cfg, media_cfg);
print "py status after initialization :" + `status`


#
# Start pjsua!
#
status = py_pjsua.start()
if status != 0:
    exit(1)


message = py_pjsua.Msg_Data()
py_pjsua.msg_data_init(message)
print "identitas object message data :" + `message`

sipaddr = 'sip:167.205.34.99'
print "checking sip address [%s] : %d" % (sipaddr, py_pjsua.verify_sip_url(sipaddr))

sipaddr = '167.205.34.99'
print "checking invalid sip address [%s] : %d" % (sipaddr, py_pjsua.verify_sip_url(sipaddr))

object = py_pjsua.get_pjsip_endpt()
print "identitas Endpoint :" + `object` + ""

mediaend = py_pjsua.get_pjmedia_endpt()
print "identitas Media Endpoint :" + `mediaend` + ""

pool = py_pjsua.get_pool_factory()
print "identitas pool factory :" + `pool` + ""

status = py_pjsua.handle_events(3000)
print "py status after 3 second of blocking wait :" + `status`



# end of new testrun

#

py_pjsua.perror("saya","hallo",70006)

status = py_pjsua.destroy()
print "py status " + `status`


