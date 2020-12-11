# $Id$
#
from inc_cfg import *

# Trickle ICE half vs regular, should be okay (just like regular vs regular)
test_param = TestParam(
		"Callee=Regular ICE, caller=Trickle ICE (half)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=0 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-trickle=1 --stun-srv stun.pjsip.org")
		]
		)
