# $Id$
#
from inc_cfg import *

# No ICE vs trickle ICE full, should be okay (as ICE will be disabled)
# Note: somehow the test fails sometime when callee has STUN candidate, looks like the first DTMF digit is lost
test_param = TestParam(
		"Callee=Trickle ICE (full), caller=no ICE",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-trickle=2"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
