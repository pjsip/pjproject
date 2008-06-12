# $Id$
#
import inc_cfg

# Simple call
config = inc_cfg.CallConfig(
		title = "Callee=mandatory SRTP, caller=mandatory SRTP",
		callee_cfg = inc_cfg.Config(arg="--null-audio --use-srtp=2 --srtp-secure=0"),
		caller_cfg = inc_cfg.Config(arg="--null-audio --use-srtp=2 --srtp-secure=0")
		)
