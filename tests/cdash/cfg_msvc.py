#
# cfg_msvc.py - MSVC/Visual Studio target configurator
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
    #   [cfg_site] [--vs-config VSCFG]
    #
    #   cfg_site:   site configuration module. If not specified, "cfg_site" 
    #               is implied
    #   VSCFG:      Visual Studio build configuration to build. Sample values:
    #               "Debug|Win32", "Release|Win32". If not specified then 
    #               "Release|Win32" is assumed

    cfg_site = "cfg_site"
    vs_cfg = "Release|Win32"
    in_option = ""
    
    for arg in args:
        if in_option=="--vs-config":
            vs_cfg = arg
            in_option = ""
        elif arg=="--vs-config":
            in_option = arg
        else:
            cfg_site = arg
        
    if os.access(cfg_site+".py", os.F_OK) == False:
        print "Error: file '%s.py' doesn't exist." % (cfg_site)
        sys.exit(1)

    cfg_site = __import__(cfg_site)
    test_cfg = builder.BaseConfig(cfg_site.BASE_DIR, \
                                  cfg_site.URL, \
                                  cfg_site.SITE_NAME, \
                                  cfg_site.GROUP, \
                                  cfg_site.OPTIONS)

    builders = [
        builder.MSVCTestBuilder(test_cfg, 
                                vs_config=vs_cfg,
                                build_config_name="default",
                                config_site="#define PJ_TODO(x)\n",
                                exclude=cfg_site.EXCLUDE,
                                not_exclude=cfg_site.NOT_EXCLUDE)
        ]

    return builders

