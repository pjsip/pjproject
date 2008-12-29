import builder

# Your site name
SITE_NAME="Newham3"

# The URL where tests will be submitted to
URL = "http://192.168.0.2/dash/submit.php?project=PJSIP"

# Test group
GROUP = "Experimental"

# PJSIP base directory
BASE_DIR = "/root/project/pjproject"

# List of additional ccdash options
#OPTIONS = ["-o", "out.xml", "-y"]
OPTIONS = []

# List of regular expression of test patterns to be excluded
EXCLUDE = []

# List of regular expression of test patterns to be included (even
# if they match EXCLUDE patterns)
NOT_EXCLUDE = []
#"configure", "update", "build.*make", "build", "run.py mod_run.*100_simple"]
