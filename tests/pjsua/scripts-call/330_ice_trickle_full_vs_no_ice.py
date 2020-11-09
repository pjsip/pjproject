# $Id$
#
from inc_cfg import *

# Trickle ICE full vs no ICE, should be okay (as there are host candidates)
test_param = TestParam(
		"Callee=no ICE, caller=Trickle ICE (full)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-trickle=2 --stun-srv stun.pjsip.org")
		]
		)
