# $Id$
#
from inc_cfg import *

# No ICE vs trickle ICE full, should be okay (as ICE will be disabled)
test_param = TestParam(
		"Callee=Trickle ICE (full), caller=no ICE",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=2 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
