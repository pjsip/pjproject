# $Id$
#
from inc_cfg import *

# Trickle ICE full vs regular, should be okay (as there are host candidates)
test_param = TestParam(
		"Callee=Regular ICE, caller=Trickle ICE (full)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=0 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-trickle=2 --stun-srv stun.pjsip.org")
		]
		)
