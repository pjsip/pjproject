import sys
import inc_util as util

from inc_cfg import *

# Read configuration
cfg_file = util.load_module_from_file("cfg_file", ARGS[1])

# Here where it all comes together
test = cfg_file.test_param
