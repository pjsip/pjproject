#
from inc_cfg import *

# ICE with STUN but no host candidates (srflx candidates only).
# Both sides suppress host candidates with --ice-max-hosts 0, so the only
# candidates gathered are the server reflexive ones obtained from STUN.
test_param = TestParam(
		"Callee=use ICE+STUN (no host), caller=use ICE+STUN (no host)",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-ice --ice-max-hosts 0 --stun-srv stun.pjsip.org"),
			InstanceParam("caller", "--null-audio --max-calls=1 --use-ice --ice-max-hosts 0 --stun-srv stun.pjsip.org")
		]
		)
