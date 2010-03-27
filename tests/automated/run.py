import sys
import ccdash

if __name__ == "__main__":
	sys.argv[0] = "ccdash.py"
	rc = ccdash.main(sys.argv)
	sys.exit(rc)


