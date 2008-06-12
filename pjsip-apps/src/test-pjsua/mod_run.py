# $Id$
import imp
import sys
import inc_param as param


# Read configuration
cfg_file = imp.load_source("cfg_file", sys.argv[2])

# Test title
title = "Basic pjsua"

# Param to spawn pjsua
p1 = param.Pjsua("pjsua", args=cfg_file.config.arg, 
		 echo=cfg_file.config.echo_enabled, 
		 trace=cfg_file.config.trace_enabled)

# Here where it all comes together
test = param.Test(title, run=[p1])
