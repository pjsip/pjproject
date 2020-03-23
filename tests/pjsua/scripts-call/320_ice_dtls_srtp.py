# $Id$
#
from inc_cfg import *

# ICE mismatch
test_param = TestParam(
		"Callee=use ICE & SRTP, caller=use ICE & DTLS-SRTP",
		[
			InstanceParam("callee", "--null-audio --use-ice --max-calls=1 --use-srtp=2 --srtp-secure=0"),
			InstanceParam("caller", "--null-audio --use-ice --max-calls=1 --use-srtp=2 --srtp-secure=0 --srtp-keying=1")
		]
		)
