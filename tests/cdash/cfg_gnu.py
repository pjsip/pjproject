#
# cfg_gnu.py - GNU target configurator
#
# Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
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
