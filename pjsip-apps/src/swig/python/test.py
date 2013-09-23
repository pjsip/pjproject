import pjsua2 as pj
import sys

#
# Basic data structure test, to make sure basic struct
# and array operations work
#
def ua_data_test():
	#
	# CredInfo
	#
	print "UA data types test.."
	the_realm = "pjsip.org"
	ci = pj.CredInfo()
	ci.realm = the_realm
	ci.dataType = 20
	
	ci2 = ci
	assert ci.dataType == 20
	assert ci2.realm == the_realm
	
	#
	# UaConfig
	# See here how we manipulate std::vector
	#
	uc = pj.UaConfig()
	uc.maxCalls = 10
	uc.userAgent = "Python"
	uc.nameserver = pj.StringVector(["10.0.0.1", "10.0.0.2"])
	uc.nameserver.append("NS1")
	
	uc2 = uc
	assert uc2.maxCalls == 10
	assert uc2.userAgent == "Python"
	assert len(uc2.nameserver) == 3
	assert uc2.nameserver[0] == "10.0.0.1"
	assert uc2.nameserver[1] == "10.0.0.2"
	assert uc2.nameserver[2] == "NS1"

	print "  Dumping nameservers: ",
	for s in uc2.nameserver:
		print s,
	print ""

#
# Exception test
#
def ua_run_test_exception():
	print "Exception test.."
	ep = pj.Endpoint.instance()
	got_exception = False
	try:
		ep.testException()
	except pj.Error, e:
		got_exception = True
		print "  Got exception: status=%u, reason=%s,\n  title=%s,\n  srcFile=%s, srcLine=%d" % \
			(e.status, e.reason, e.title, e.srcFile, e.srcLine)
		assert e.status == 70013
		assert e.reason == "Invalid operation (PJ_EINVALIDOP)"
		assert e.title == "Endpoint::testException()"
	assert got_exception

#
# Custom log writer
#
class MyLogWriter(pj.LogWriter):
	def write(self, entry):
		print "This is Python:", entry.msg
		
#
# Testing log writer callback
#
def ua_run_log_test():
	print "Logging test.."
	ep_cfg = pj.EpConfig()
	
	lw = MyLogWriter()
	ep_cfg.logConfig.writer = lw
	ep_cfg.logConfig.decor = ep_cfg.logConfig.decor & ~(pj.PJ_LOG_HAS_CR | pj.PJ_LOG_HAS_NEWLINE) 
	
	ep = pj.Endpoint.instance()
	ep.libCreate()
	ep.libInit(ep_cfg)
	ep.libDestroy()
	
#
# Simple create, init, start, and destroy sequence
#
def ua_run_ua_test():
	print "UA test run.."
	ep_cfg = pj.EpConfig()
	
	ep = pj.Endpoint.instance()
	ep.libCreate()
	ep.libInit(ep_cfg)
	ep.libStart()
	
	print "************* Endpoint started ok, now shutting down... *************"
	ep.libDestroy()

#
# main()
#
if __name__ == "__main__":
	ua_data_test()
	ua_run_test_exception()
	ua_run_log_test()
	ua_run_ua_test()
	sys.exit(0)

	