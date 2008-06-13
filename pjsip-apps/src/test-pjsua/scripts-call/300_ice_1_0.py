# $Id:$
#
import inc_cfg

# ICE mismatch
config = inc_cfg.CallConfig(
		title = "Callee=use ICE, caller=no ICE",
		callee_cfg = inc_cfg.Config(arg="--null-audio --use-ice"),
		caller_cfg = inc_cfg.Config(arg="--null-audio")
		)
