import os
from guppi_utils import *
from astro_utils import current_MJD
from optparse import OptionParser

# Parse command line
par = OptionParser()
par.add_option("-c", "--cal", dest="cal",
               help="Setup for cal scan (folding mode)",
               action="store_true", default=False)
par.add_option("-n", "--scannum", dest="scan",
               help="Set scan number",
               action="store", type="int", default=1);
par.add_option("-i", "--increment_scan", dest="inc",
               help="Increment scan num",
               action="store_true", default=False);
par.add_option("-T", "--tscan", dest="tscan",
               help="Scan length (sec)",
               action="store", type="float", default=8*3600.0)
par.add_option("-P", "--parfile", dest="parfile", 
               help="Use this parfile for folding",
               action="store", default="");
par.add_option("-t", "--tfold", dest="tfold",
               help="Fold dump time (sec)",
               action="store", type="float", default=30.0)
par.add_option("-b", "--bins", dest="nbin",
               help="Number of profile bins for folding",
               action="store", type="int", default=256)
par.add_option("-I", "--onlyI", dest="onlyI",
               help="Only record total intensity",
               action="store_true", default=False)
par.add_option("--dstime", dest="dstime",
               help="Downsample in time (int, power of 2)",
               action="store", type="int", default=1)
par.add_option("--dsfreq", dest="dsfreq",
               help="Downsample in freq (int, power of 2)",
               action="store", type="int", default=1)
par.add_option("--obs", dest="obs",
               help="Set observers name",
               action="store", default="unknown")
par.add_option("--src", dest="src",
               help="Set observed source name",
               action="store", default="Fake_PSR")
par.add_option("--ra", dest="ra",
               help="Set source R.A. (hh:mm:ss.s)",
               action="store", default="12:34:56.7")
par.add_option("--dec", dest="dec",
               help="Set source Dec (+/-dd:mm:ss.s)",
               action="store", default="+12:34:56.7")
par.add_option("--freq", dest="freq",
               help="Set center freq (MHz)",
               action="store", type="float", default=1200.0)
par.add_option("--acc_len", dest="acc_len",
               help="Hardware accumulation length",
               action="store", type="int", default=16)
par.add_option("--gb43m", dest="gb43m", 
               help="Set params for 43m observing",
               action="store_true", default=False)
par.add_option("--gbt", dest="gbt", 
               help="Use values from gbtstatus (the default)",
               action="store_true", default=True)
par.add_option("--fake", dest="fake",
               help="Set params for fake psr",
               action="store_true", default=False)
(opt,arg) = par.parse_args()

g = guppi_status()

g.read()

if (opt.inc):
    opt.scan = g["SCANNUM"] + 1

if opt.obs=="unknown":
    try:
        username = os.environ['LOGNAME']
    except KeyError:
        username = os.getlogin()
    else:
        username = "unknown"

g.update("SCANNUM", opt.scan)

if (opt.gb43m or opt.fake):
    opt.gbt = False
    g.update("TELESCOP", "GB43m")
    g.update("OBSBW", -800.0)

if (opt.gbt):
    g.update_with_gbtstatus()
    g.update("OBSBW", 800.0)
else:
    g.update("FRONTEND", "None")
    g.update("PROJID", "GUPPI tests")
    g.update("FD_POLN", "LIN")
    g.update("TRK_MODE", "TRACK")
    g.update("SRC_NAME", opt.src)
    g.update("RA_STR", opt.ra)
    g.update("DEC_STR", opt.dec)
    g.update("OBSFREQ", opt.freq)

if g['OBSERVER']=='unknown':
    g.update("OBSERVER", username)

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

g.write()

g.update("TFOLD", opt.tfold)
if (opt.cal):
    g.update("OBS_MODE", "CAL")
    if opt.tscan > 7.9*3600:
        opt.tscan = 60.0
    g.update("TFOLD", 10.0) # override for cal scans
    g.update("BASENAME", "guppi_%5d_%s_%04d_cal"%(g['STT_IMJD'], g['SRC_NAME'], g['SCANNUM']))
    g.update("CAL_MODE", "ON")
else:
    g.update("OBS_MODE", "SEARCH")
    g.update("BASENAME", "guppi_%5d_%s_%04d"%(g['STT_IMJD'], g['SRC_NAME'], g['SCANNUM']))
    g.update("CAL_MODE", "OFF")

g.update("SCANLEN", opt.tscan)
g.update("BACKEND", "GUPPI")
g.update("PKTFMT", "GUPPI")
g.update("DATAHOST", "bee2_10")
g.update("DATAPORT", 50000)
g.update("POL_TYPE", "IQUV")

g.update("CAL_FREQ", 25.0)
g.update("CAL_DCYC", 0.5)
g.update("CAL_PHS", 0.0)

g.update("OBSNCHAN", 2048)
g.update("NPOL", 4)
g.update("NBITS", 8)
g.update("PFB_OVER", 4)
g.update("NBITSADC", 8)
g.update("ACC_LEN", opt.acc_len)

g.update("NRCVR", 2)

if opt.onlyI:
    g.update("ONLY_I", 1)
else:
    g.update("ONLY_I", 0)
g.update("DS_TIME", opt.dstime)
g.update("DS_FREQ", opt.dsfreq)

g.update("NBIN", opt.nbin)
g.update("PARFILE", opt.parfile)

#g.update("BLOCSIZE", )
g.update("OFFSET0", 0.0)
g.update("SCALE0", 1.0)
g.update("OFFSET1", 0.0)
g.update("SCALE1", 1.0)
g.update("OFFSET2", 0.5)
g.update("SCALE2", 1.0)
g.update("OFFSET3", 0.5)
g.update("SCALE3", 1.0)

g.update("TBIN", abs(g['ACC_LEN']*g['OBSNCHAN']/g['OBSBW']*1e-6))
g.update("CHAN_BW", g['OBSBW']/g['OBSNCHAN'])

g.update_azza()
g.write()
