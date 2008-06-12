# $Id$
#
# Just about the simple pjsua command line parameter, which should
# never fail in any circumstances
import inc_cfg

config = inc_cfg.Config(arg = "--null-audio --local-port 0 --rtp-port 0")

