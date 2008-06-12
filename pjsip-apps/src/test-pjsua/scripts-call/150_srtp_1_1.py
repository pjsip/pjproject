# $Id$
#
import inc_cfg

# Simple call
config = inc_cfg.CallConfig(
		title = "Callee=optional SRTP, caller=optional SRTP",
		callee_cfg = inc_cfg.Config(arg="--null-audio --use-srtp=1 --srtp-secure=0"),
		caller_cfg = inc_cfg.Config(arg="--null-audio --use-srtp=1 --srtp-secure=0")
		)
