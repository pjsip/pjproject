# $Id$
#
from inc_cfg import *

# Trickle ICE half vs half
test_param = TestParam(
		"Callee=Trickle ICE (half), caller=Trickle ICE (half)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=1 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-trickle=1 --stun-srv stun.pjsip.org")
		]
		)
