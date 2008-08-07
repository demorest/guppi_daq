from guppi_utils import guppi_status, guppi_databuf
import shm_wrapper as shm
import numpy as n
import pylab as p
import time
import math
from optparse import OptionParser

# Cmd line opts
par = OptionParser()
par.add_option("-p", "--npol", type="int", dest="npol_disp",
        help="Number of polarizations to display (0=all)", default=0)
par.add_option("-u", "--unsigned", action="store_true", 
        dest="data_unsigned", default=False,
        help="Raw data is unsigned bytes (default=signed)")
par.add_option("--ymin", type="float", dest="ymin",
        help="Fixed minimum for y-axis (default=autoscale)")
par.add_option("--ymax", type="float", dest="ymax",
        help="Fixed maximum for y-axis (default=autoscale)")
(opts,args) = par.parse_args()

# Get all of the useful values
g = guppi_status()
g.read()
nchan = g["OBSNCHAN"]
npoln = g["NPOL"]
BW = g["OBSBW"]
fctr = g["OBSFREQ"]
fctr = BW/2.0

#Figure range of pols to display
if opts.npol_disp==0:
    opts.npol_disp = npoln

# Connect to the GUPPI data buffer
d = guppi_databuf()
if npoln==2 or opts.data_unsigned:
    d.dtype = n.uint8

# Define "convert to dB" function that can 
# deal with zeros in the data.
def db(x,min=-80.0):
    y = n.array(x,dtype=n.float)
    for i in range(len(y)):
        if (y[i]<=0.0):
            y[i] = min
        else:
            y[i] = 10.0*math.log10(y[i]/255.0)
    return y

p.ion()  # turn interactive mode on
p.figure(1)
x = p.arange(nchan)/float(nchan)*BW + (fctr-0.5*BW)

do_log = 0

data = d.data(0)

line=[]
hi_line=[]
lo_line=[]

nspec_sum = 1024

p.ioff() 

for pol in range(opts.npol_disp):
    avg_spec = data[0:nspec_sum,pol,:].mean(0)
    min_spec = data[0:nspec_sum,pol,:].min(0)
    max_spec = data[0:nspec_sum,pol,:].max(0)
    if opts.ymin!=None:
        ymin = opts.ymin
    else:
        ymin = float(min_spec.min())
    if opts.ymax!=None:
        ymax = opts.ymax
    else:
        ymax = float(max_spec.max())
    p.subplot(opts.npol_disp,1,pol+1)
    if pol==opts.npol_disp-1: 
        p.xlabel("Frequency (MHz)")
    p.ylabel("Power")
    p.title("Pol %d" % (pol))
    line.append(p.plot(x, avg_spec)[0])
    hi_line.append(p.plot(x, max_spec, 'b:')[0])
    lo_line.append(p.plot(x, min_spec, 'b:')[0])
    p.axis([x.min(),x.max(),ymin,ymax])
    p.draw()


run=1
while (run):
    try:
        print "updating..."
        g.read()
        try:
            curblock = g["CURBLOCK"]
        except KeyError:
            curblock = 1

        data = d.data(curblock)

        for pol in range(opts.npol_disp):
            avg_spec = data[0:nspec_sum,pol,:].mean(0)
            min_spec = data[0:nspec_sum,pol,:].min(0)
            max_spec = data[0:nspec_sum,pol,:].max(0)
            p.subplot(opts.npol_disp,1,pol+1)
            if opts.ymin!=None:
                ymin = opts.ymin
            else:
                ymin = float(min_spec.min())
            if opts.ymax!=None:
                ymax = opts.ymax
            else:
                ymax = float(max_spec.max())
            p.axis([x.min(),x.max(),ymin,ymax])
            line[pol].set_ydata(avg_spec)
            hi_line[pol].set_ydata(max_spec)
            lo_line[pol].set_ydata(min_spec)
            idx = abs(avg_spec).argmax()
            print "Block %2d, pol %1d: Max chan=%d freq=%.3fMHz value=%.3f" %\
                    (curblock, pol, idx, x[idx], avg_spec[idx]) 

        p.draw()
        time.sleep(1)
    except KeyboardInterrupt:
        print "Exiting.."
        run = 0
