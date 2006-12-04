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

# lib transport
stunc = py_pjsua.STUN_Config();
py_pjsua.stun_config_default(stunc);

tc = py_pjsua.Transport_Config();
py_pjsua.transport_config_default(tc);

py_pjsua.normalize_stun_config(stunc);

id = py_pjsua.Transport_ID();
status = py_pjsua.transport_create(1, tc, id);
print "py transport create status " + `status`

t_id = id.transport_id;
ti = py_pjsua.Transport_Info();
status = py_pjsua.transport_get_info(t_id,ti)
print "py transport get info status " + `status`

status = py_pjsua.transport_set_enable(t_id,1)
print "py transport set enable status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set enable",status)


status = py_pjsua.transport_close(t_id,1)
print "py transport close status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","close",status)

# end of lib transport

# lib account 

accfg = py_pjsua.Acc_Config()
py_pjsua.acc_config_default(accfg)
accid = py_pjsua.Acc_ID()
status = py_pjsua.acc_add(accfg, 1, accid)
print "py acc add status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","add acc",status)
count = py_pjsua.acc_get_count()
print "acc count " + `count`

accid.acc_id = py_pjsua.acc_get_default()

print "acc id default " + `accid.acc_id`

# end of lib account

py_pjsua.perror("saya","hallo",70006)

status = py_pjsua.destroy()
print "py status " + `status`


