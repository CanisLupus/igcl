# -*- coding: cp1252 -*-
import sys, os
import time

argv = sys.argv
argc = len(argv)

nPeers = int(argv[1])
useCoord = int(argv[2])
config = argv[3] # Debug or Release

if useCoord:
	os.system(r'(mate-terminal -e "./%s/thesis 1 44100" &)' % (config))

for i in xrange(1, nPeers+1):
	os.system(r'(mate-terminal -e "./%s/thesis 0 12%03d 127.0.0.1 44100" &)' % (config, i))
	#os.system(r'(mate-terminal -e "./%s/thesis 0 12%03d 193.137.203.94 44100" &)' % (config, i))
	time.sleep(0.1)
