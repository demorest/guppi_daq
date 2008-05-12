from guppi_utils import *
from astro_utils import current_MJD
g = guppi_status()

g.update("SCANNUM", 4)
g.update("SRC_NAME", "B0329+54")
#g.update("SRC_NAME", "B1937+21")
#g.update("SRC_NAME", "test_tone")
#g.update("RA_STR", "19:39:38.560")
#g.update("DEC_STR", "21:34:59.143")
g.update("RA_STR", "03:32:59.36")
g.update("DEC_STR", "+54:34:43.6")

g.update("SCANLEN", 600.0)
g.update("BASENAME", "guppi_test_%s_%04d"%(g['SRC_NAME'], g['SCANNUM']))

g.update("TELESCOP", "GB43m")
g.update("OBSERVER", "GUPPI Crew")
g.update("FRONTEND", "None")
g.update("BACKEND", "GUPPI")
g.update("PROJID", "first light tests")
g.update("FD_POLN", "LIN")
g.update("POL_TYPE", "IQUV")


g.update("OBSFREQ", 960.0)
g.update("OBSBW", 400.0)
g.update("OBSNCHAN", 2048)
g.update("NPOL", 4)
g.update("NBITS", 8)
g.update("ACC_LEN", 16)
g.update("TBIN", g['ACC_LEN']*g['OBSNCHAN']/g['OBSBW']*1e-6)
g.update("CHAN_BW", g['OBSBW']/g['OBSNCHAN'])

# Correct for 4-bin offset problem
#g.update("OBSFREQ", g['OBSFREQ']+4*g['CHAN_BW'])

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

if (1): # Use for parkes spectrometer
    g.update("BACKEND", "Parspec");
    g.update("OBSNCHAN", 1024);
    g.update("NPOL", 2);
    g.update("POL_TYPE", "AABB");
    g.update("ACC_LEN", 13)
    g.update("TBIN", g['ACC_LEN']*g['OBSNCHAN']/g['OBSBW']*1e-6)

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
