# $Id:$
import imp
import sys


# Read configuration
cfg_file = imp.load_source("cfg_file", sys.argv[2])

# Here where it all comes together
test = cfg_file.test_param
