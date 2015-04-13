# $Id$
#
from inc_cfg import *

# ICE mismatch
test_param = TestParam(
		"Callee=use ICE, caller=use ICE",
		[
			InstanceParam("callee", "--null-audio --use-ice --max-calls=1"),
			InstanceParam("caller", "--null-audio --use-ice --max-calls=1")
		]
		)
