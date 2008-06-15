# $Id:$
#
# Just about the simple pjsua command line parameter, which should
# never fail in any circumstances
from inc_cfg import *

test_param = TestParam(
		"Basic run", 
		[
			InstanceParam("pjsua", "--null-audio --local-port 0 --rtp-port 0")
		]
		)

