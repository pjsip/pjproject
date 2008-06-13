# $Id$
#
import inc_cfg

# TCP call
config = inc_cfg.CallConfig(
		title = "TCP transport",
		callee_cfg = inc_cfg.Config(arg="--null-audio --no-udp"),
		caller_cfg = inc_cfg.Config(arg="--null-audio --no-udp")
		)
config.uri_param = ";transport=tcp"
