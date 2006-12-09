import py_pjsua

status = py_pjsua.create()
print "py status " + `status`


#
# Create configuration objects
#
ua_cfg = py_pjsua.config_default()
log_cfg = py_pjsua.logging_config_default()
media_cfg = py_pjsua.media_config_default()

#
# Logging callback.
#
def logging_cb1(level, str, len):
    print str,


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


message = py_pjsua.msg_data_init()

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
stunc = py_pjsua.stun_config_default();


tc = py_pjsua.transport_config_default();


py_pjsua.normalize_stun_config(stunc);


status, id = py_pjsua.transport_create(1, tc);
print "py transport create status " + `status`

ti = py_pjsua.Transport_Info();
ti = py_pjsua.transport_get_info(id)
print "py transport get info status " + `status`

status = py_pjsua.transport_set_enable(id,1)
print "py transport set enable status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set enable",status)


status = py_pjsua.transport_close(id,1)
print "py transport close status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","close",status)

# end of lib transport

# lib account 

accfg = py_pjsua.acc_config_default()
status, accid = py_pjsua.acc_add(accfg, 1)
print "py acc add status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","add acc",status)
count = py_pjsua.acc_get_count()
print "acc count " + `count`

accid = py_pjsua.acc_get_default()

print "acc id default " + `accid`

# end of lib account

#lib buddy

bcfg = py_pjsua.Buddy_Config()
status, id = py_pjsua.buddy_add(bcfg)
print "py buddy add status " + `status` + " id " + `id`
bool = py_pjsua.buddy_is_valid(id)
print "py buddy is valid " + `bool`
count = py_pjsua.get_buddy_count()
print "buddy count " + `count`
binfo = py_pjsua.buddy_get_info(id)
ids = py_pjsua.enum_buddies(3)
status = py_pjsua.buddy_del(id)
print "py buddy del status " + `status`
status = py_pjsua.buddy_subscribe_pres(id, 1)
print "py buddy subscribe pres status " + `status`
py_pjsua.pres_dump(1)
status = py_pjsua.im_send(accid, "fahris@divusi.com", "", "hallo", message, 0)
print "py im send status " + `status`
status = py_pjsua.im_typing(accid, "fahris@divusi.com", 1, message)
print "py im typing status " + `status`
#print "binfo " + `binfo`

#end of lib buddy

py_pjsua.perror("saya","hallo",70006)

status = py_pjsua.destroy()
print "py status " + `status`


