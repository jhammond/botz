#!/usr/bin/env python
import simplejson, urllib2, pprint

f = urllib2.urlopen("http://localhost:9901/status")
pprint.pprint(simplejson.load(f))




