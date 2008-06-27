# $Id$
#
from socket import *
import re
import random
import time
import sys
import inc_cfg as cfg
from select import *

# SIP request template
req_templ = \
"""$METHOD $TARGET_URI SIP/2.0\r
Via: SIP/2.0/UDP $LOCAL_IP:$LOCAL_PORT;rport;branch=z9hG4bK$BRANCH\r
Max-Forwards: 70\r
From: <sip:caller@pjsip.org>$FROM_TAG\r
To: <$TARGET_URI>$TO_TAG\r
Contact: <sip:$LOCAL_IP:$LOCAL_PORT;transport=udp>\r
Call-ID: $CALL_ID@pjsip.org\r
CSeq: $CSEQ $METHOD\r
Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, REFER\r
Supported: replaces, 100rel, norefersub\r
User-Agent: pjsip.org Python tester\r
Content-Length: $CONTENT_LENGTH\r
$SIP_HEADERS"""


def is_request(msg):
	return msg.split(" ", 1)[0] != "SIP/2.0"
	
def is_response(msg):
	return msg.split(" ", 1)[0] == "SIP/2.0"

def get_code(msg):
	if msg=="":
		return 0
	return int(msg.split(" ", 2)[1])

def get_tag(msg, hdr="To"):
	pat = "^" + hdr + ":.*"
	result = re.search(pat, msg, re.M | re.I)
	if result==None:
		return ""
	line = result.group()
	#print "line=", line
	tags = line.split(";tag=")
	if len(tags)>1:
		return tags[1]
	return ""
	#return re.split("[;& ]", s)


class Dialog:
	sock = None
	dst_addr = ""
	dst_port = 5060
	local_ip = ""
	local_port = 0
	tcp = False
	call_id = str(random.random())
	cseq = 0
	local_tag = ";tag=" + str(random.random())
	rem_tag = ""
	last_resp_code = 0
	inv_branch = ""
	trace_enabled = True
	last_request = ""
	def __init__(self, dst_addr, dst_port=5060, tcp=False, trace=True):
		self.dst_addr = dst_addr
		self.dst_port = dst_port
		self.tcp = tcp
		self.trace_enabled = trace
		if tcp==True:
			self.sock = socket(AF_INET, SOCK_STREAM)
			self.sock.connect(dst_addr, dst_port)
		else:
			self.sock = socket(AF_INET, SOCK_DGRAM)
			self.sock.bind(("127.0.0.1", 0))
		
		self.local_ip, self.local_port = self.sock.getsockname()
		self.trace("Dialog socket bound to " + self.local_ip + ":" + str(self.local_port))

	def trace(self, txt):
		if self.trace_enabled:
			print str(time.strftime("%H:%M:%S ")) + txt

	def create_req(self, method, sdp, branch="", extra_headers=""):
		if branch=="":
			self.cseq = self.cseq + 1
		msg = req_templ
		msg = msg.replace("$METHOD", method)
		if self.tcp:
			transport_param = ";transport=tcp"
		else:
			transport_param = ""
		msg = msg.replace("$TARGET_URI", "sip:"+self.dst_addr+":"+str(self.dst_port) + transport_param)
		msg = msg.replace("$LOCAL_IP", self.local_ip)
		msg = msg.replace("$LOCAL_PORT", str(self.local_port))
		if branch=="":
			branch=str(random.random())
		msg = msg.replace("$BRANCH", branch)
		msg = msg.replace("$FROM_TAG", self.local_tag)
		msg = msg.replace("$TO_TAG", self.rem_tag)
		msg = msg.replace("$CALL_ID", self.call_id)
		msg = msg.replace("$CSEQ", str(self.cseq))
		msg = msg.replace("$SIP_HEADERS", extra_headers)
		if sdp!="":
			msg = msg.replace("$CONTENT_LENGTH", str(len(sdp)))
			msg = msg + "Content-Type: application/sdp\r\n"
		else:
			msg = msg.replace("$CONTENT_LENGTH", "0")
		msg = msg + "\r\n"
		msg = msg + sdp
		return msg

	def create_invite(self, sdp, extra_headers=""):
		self.inv_branch = str(random.random())
		return self.create_req("INVITE", sdp, branch=self.inv_branch, extra_headers=extra_headers)

	def create_ack(self, sdp="", extra_headers=""):
		return self.create_req("ACK", sdp, extra_headers=extra_headers, branch=self.inv_branch)

	def create_bye(self, extra_headers=""):
		return self.create_req("BYE", "", extra_headers)

	def send_msg(self, msg):
		if (is_request(msg)):
			self.last_request = msg.split(" ", 1)[0]
		self.trace("============== TX MSG ============= \n" + msg)
		self.sock.sendto(msg, 0, (self.dst_addr, self.dst_port))

	def wait_msg(self, timeout):
		endtime = time.time() + timeout
		msg = ""
		while time.time() < endtime:
			readset = select([self.sock], [], [], timeout)
			if len(readset) < 1 or not self.sock in readset[0]:
				if len(readset) < 1:
					print "select() returns " + str(len(readset))
				elif not self.sock in readset[0]:
					print "select() alien socket"
				else:
					print "select other error"
				continue
			try:
				msg = self.sock.recv(2048)
			except:
				print "recv() exception: ", sys.exc_info()[0]
				continue

		if msg=="":
			return ""
		if self.last_request=="INVITE" and self.rem_tag=="":
			self.rem_tag = get_tag(msg, "To")
			self.rem_tag = self.rem_tag.rstrip("\r\n;")
			if self.rem_tag != "":
				self.rem_tag = ";tag=" + self.rem_tag
			self.trace("=== rem_tag:" + self.rem_tag)
		self.trace("=========== RX MSG ===========\n" + msg)
		return msg
	
	# Send request and wait for final response
	def send_request_wait(self, msg, timeout):
		t1 = 1.0
		endtime = time.time() + timeout
		resp = ""
		code = 0
		for i in range(0,5):
			self.send_msg(msg)
			resp = self.wait_msg(t1)
			if resp!="" and is_response(resp):
				code = get_code(resp)
				break
		last_resp = resp
		while code < 200 and time.time() < endtime:
			resp = self.wait_msg(endtime - time.time())
			if resp != "" and is_response(resp):
				code = get_code(resp)
				last_resp = resp
			elif resp=="":
				break
		return last_resp
	
	def hangup(self, last_code=0):
		self.trace("====== hangup =====")
		if last_code!=0:
			self.last_resp_code = last_code
		if self.last_resp_code>0 and self.last_resp_code<200:
			msg = self.create_req("CANCEL", "", branch=self.inv_branch, extra_headers="")
			self.send_request_wait(msg, 5)
			msg = self.create_ack()
			self.send_msg(msg)
		elif self.last_resp_code>=200 and self.last_resp_code<300:
			msg = self.create_ack()
			self.send_msg(msg)
			msg = self.create_bye()
			self.send_request_wait(msg, 5)
		else:
			msg = self.create_ack()
			self.send_msg(msg)


class SendtoCfg:
	# Test name
	name = ""
	# pjsua InstanceParam
	inst_param = None
	# Complete INVITE message. If this is not empty, then this
	# message will be sent instead and the "sdp" and "extra_headers"
	# settings will be ignored.
	complete_msg = ""
	# Initial SDP
	sdp = ""
	# Extra headers to add to request
	extra_headers = ""
	# Expected code
	resp_code = 0
	# Use TCP?
	use_tcp = False
	# List of RE patterns that must exist in response
	resp_include = []
	# List of RE patterns that must NOT exist in response
	resp_exclude = []
	# Constructor
	def __init__(self, name, pjsua_args, sdp, resp_code, 
		     resp_inc=[], resp_exc=[], use_tcp=False,
		     extra_headers="", complete_msg=""):
	 	self.complete_msg = complete_msg
		self.sdp = sdp
		self.resp_code = resp_code
		self.resp_include = resp_inc
		self.resp_exclude = resp_exc
		self.use_tcp = use_tcp
		self.extra_headers = extra_headers
		self.inst_param = cfg.InstanceParam("pjsua", pjsua_args)

