# $Id$
#
from inc_cfg import *

# Trickle ICE full vs full
test_param = TestParam(
		"Callee=Trickle ICE (full), caller=Trickle ICE (full)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=2 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-trickle=2 --stun-srv stun.pjsip.org")
		]
		)
