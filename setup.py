#!/usr/bin/env python

"""GUPPI DAQ package



"""

from distutils.core import setup, Extension
from distutils.command.build import build as _build
import os
import sys

srcdir = 'python'
doclines = __doc__.split("\n")

class build(_build):
    def run(self):
        if os.system('make -C %s' % srcdir) != 0:
            print >>sys.stderr, "call to 'make' failed..."
            print >>sys.stderr, "unable to build dependencies. aborting..."
            sys.exit(1)
        _build.run(self)

setup(
    name        = 'guppi_daq'
  , version     = '0.1'
  , packages    = ['guppi_daq']
  , package_dir = {'guppi_daq' : srcdir}
  , package_data = {'guppi_daq': ['*.so']}
  , cmdclass = {'build': build}
  , maintainer = "NRAO"
  # , maintainer_email = ""
  # , url = ""
  , license = "http://www.gnu.org/copyleft/gpl.html"
  , platforms = ["any"]
  , description = doclines[0]
  , long_description = "\n".join(doclines[2:])
  )
