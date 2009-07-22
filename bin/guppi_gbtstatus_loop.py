#!/usr/bin/env python
import os, sys, time
from guppi_utils import *
from astro_utils import current_MJD

# Attach to status shared mem
g = guppi_status()

while (1):
    g.read()
    g.update_with_gbtstatus()
    g.write()
    time.sleep(1)
