#!/usr/bin/env python
import os, sys, time
from guppi_utils import *
from astro_utils import current_MJD

# Attach to status shared mem
g = guppi_status()

while (1):
    g.read()
    g.update_with_gbtstatus()

    # Current time
    MJD = current_MJD()
    MJDd = int(MJD)
    MJDf = MJD - MJDd
    MJDs = int(MJDf * 86400 + 1e-6)
    offs = (MJD - MJDd - MJDs/86400.0) * 86400.0
    g.update("STT_IMJD", MJDd)
    g.update("STT_SMJD", MJDs)
    if offs < 2e-6: offs = 0.0
    g.update("STT_OFFS", offs)
    
    # Apply to shared mem
    g.write()
    
    # Sleep for 1 second
    time.sleep(1)
