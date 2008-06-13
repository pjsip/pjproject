# $Id:$
#
import inc_cfg

# Simple call
config = inc_cfg.CallConfig(
		title = "Basic call",
		callee_cfg = inc_cfg.Config(arg="--null-audio"),
		caller_cfg = inc_cfg.Config(arg="--null-audio")
		)
