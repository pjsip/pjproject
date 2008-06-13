# $Id:$
#
import inc_cfg

# Simple call
config = inc_cfg.CallConfig(
		title = "Callee=no SRTP, caller=optional SRTP",
		callee_cfg = inc_cfg.Config(arg="--null-audio"),
		caller_cfg = inc_cfg.Config(arg="--null-audio --use-srtp=1 --srtp-secure=0")
		)
