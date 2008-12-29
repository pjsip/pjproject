import builder
import os
import sys

# Each configurator must export this function
def create_builder(args):
    # (optional) args format:
    #   site configuration module. If not specified, "cfg_site" is implied

    if len(args)>0:
        file = args[0]
    else:
        file = "cfg_site"

    if os.access(file+".py", os.F_OK) == False:
	print "Error: file '%s.py' doesn't exist." % (file)
	sys.exit(1)

    cfg_site = __import__(file)
    test_cfg = builder.BaseConfig(cfg_site.BASE_DIR, \
                                  cfg_site.URL, \
                                  cfg_site.SITE_NAME, \
                                  cfg_site.GROUP, \
                                  cfg_site.OPTIONS)

    builders = [
        builder.GNUTestBuilder(test_cfg, build_config_name="default",
                               user_mak="export CFLAGS+=-Wall\n",
                               config_site="#define PJ_TODO(x)\n",
                               exclude=cfg_site.EXCLUDE,
                               not_exclude=cfg_site.NOT_EXCLUDE)
        ]

    return builders
