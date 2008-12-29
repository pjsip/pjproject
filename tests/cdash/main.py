import sys

if len(sys.argv)==1:
    print "Usage: main.py cfg_file [cfg_site]"
    print "Example:"
    print "  main.py cfg_gnu"
    print "  main.py cfg_gnu custom_cfg_site"
    sys.exit(1)


args = []
args.extend(sys.argv)
args.remove(args[1])
args.remove(args[0])

cfg_file = __import__(sys.argv[1])
builders = cfg_file.create_builder(args)

for builder in builders:
    builder.execute()
