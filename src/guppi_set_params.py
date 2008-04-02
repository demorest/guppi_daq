from guppi_utils import *
from astro_utils import current_MJD
g = guppi_status()

g.update("SRC_NAME", "net test")
g.update("RA_STR", "10:00:00.000")
g.update("DEC_STR", "+20:00:00.0000")

g.update("SCANLEN", 5.0)
g.update("SCANNUM", 1)
g.update("BASENAME", "guppi_nettests")

g.update("TELESCOP", "GB43m")
g.update("OBSERVER", "GUPPI Crew")
g.update("FRONTEND", "None")
g.update("BACKEND", "GUPPI")
g.update("PROJID", "first light tests")
g.update("SRC_NAME", "bitpatterns")
g.update("FD_POLN", "LIN")
g.update("POL_TYPE", "IQUV")

g.update("OBSFREQ", 1600.0)
g.update("OBSBW", 800.0)
g.update("OBSNCHAN", 4096)
g.update("NPOL", 4)
g.update("NBITS", 8)
g.update("TBIN", 0.000050)
g.update("CHAN_BW", g['OBSBW']/g['OBSNCHAN'])
g.update("NRCVR", 2)

g.update("TRK_MODE", "TRACK")
g.update("CAL_MODE", "OFF")
#g.update("BLOCSIZE", )
g.update("OFFSET0", 0.0)
g.update("SCALE0", 1.0)
g.update("OFFSET1", 0.0)
g.update("SCALE1", 1.0)
g.update("OFFSET2", 0.0)
g.update("SCALE2", 1.0)
g.update("OFFSET3", 0.0)
g.update("SCALE3", 1.0)

if (1):  # in case we don't get a real start time
    MJD = current_MJD()
    MJDd = int(MJD)
    MJDf = MJD - MJDd
    MJDs = int(MJDf * 86400 + 1e-6)
    offs = (MJD - MJDd - MJDs/86400.0) * 86400.0
    g.update("STT_IMJD", MJDd)
    g.update("STT_SMJD", MJDs)
    if offs < 2e-6: offs = 0.0
    g.update("STT_OFFS", offs)

g.update_azza()
g.write()
