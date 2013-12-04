import urllib2
import sys

def fetch_rst(url):
	print 'Fetching %s..' % url
	req = urllib2.Request(url)
		
	fd = urllib2.urlopen(req, timeout=30)
	body = fd.read()		

	pos = body.find("{{{")
	if pos >= 0:
		body = body[pos+4:]
	
	pos = body.find("}}}")
	if pos >= 0:
		body = body[:pos]

	pos = body.find("#!rst")
	if pos >= 0:
		body = body[pos+6:]

	pos = url.rfind("/")
	if pos >= 0:
		filename = url[pos+1:]
	else:
		filename = url
	
	pos = filename.find('?')
	if pos >= 0:
		filename = filename[:pos]
		
	filename += ".rst"
	f = open(filename, 'w')
	f.write(body)
	f.close()


def process_index(index):
	pages = []
	
	f = open(index + '.rst', 'r')
	line = f.readline()
	while line:
		if line.find('toctree::') >= 0:
			break
		line = f.readline()
		
	if line.find('toctree::') < 0:
		return []
	# Skip directive (or whatever it's called
	line = f.readline().strip()
	while line and line[0] == ':':
		line = f.readline().strip()
	# Skip empty lines
	line = f.readline().strip()
	while not line:
		line = f.readline().strip()
	# Parse names
	while line:
		pages.append(line)
		line = f.readline().strip()
		
	f.close()
	
	return pages


if __name__ == '__main__':
	print "** Warning: This will overwrite ALL RST files in current directory. Continue? [n] ",
	if sys.stdin.readline().strip() != 'y':
		sys.exit(0)
		
	url_format = 'http://trac.pjsip.org/repos/wiki/pjsip-doc/%s?format=txt'
	
	index = url_format % ('index')
	fetch_rst(index)
	
	pages = process_index('index')
	for page in pages:
		url = url_format % (page)
		fetch_rst(url)
		
	print 'Done.'
	