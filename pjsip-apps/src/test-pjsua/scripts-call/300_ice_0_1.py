# $Id:$
#
import inc_cfg

# ICE mismatch
config = inc_cfg.CallConfig(
		title = "Callee=no ICE, caller=use ICE",
		callee_cfg = inc_cfg.Config(arg="--null-audio"),
		caller_cfg = inc_cfg.Config(arg="--null-audio --use-ice")
		)
