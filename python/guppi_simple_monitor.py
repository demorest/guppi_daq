from guppi_utils import guppi_status, guppi_databuf
import shm_wrapper as shm
import numpy as n
import pylab as p
import time

# Get all of the useful values
g = guppi_status()
g.read()
nchan = g["OBSNCHAN"]
npoln = g["NPOL"]
BW = g["OBSBW"]
fctr = g["OBSFREQ"]
fctr = BW/2.0

# Connect to the GUPPI data buffer
d = guppi_databuf()

p.ion()  # turn unteractive mode on
x = p.arange(nchan)/float(nchan)*BW + (fctr-0.5*BW)
#x = n.arange(nchan * npoln)

poln = 0
avg_spec = d.data(0)[:,poln,:].mean(0)

print len(x), len(avg_spec)

line, = p.plot(x, avg_spec)

run=1
while (run):
    try:
        print "updating..."
        g.read()
        try:
            curblock = g["CURBLOCK"]
        except KeyError:
            curblock = 1
        avg_spec = d.data(curblock)[:,poln,:].mean(0)
        line.set_ydata(avg_spec)
        print "Block %2d: Max chan=%d freq=%.3fMHz value=%.3f" %\
                (curblock, avg_spec.argmax(), x[avg_spec.argmax()],\
                avg_spec.max())
        p.draw()
        time.sleep(1)
    except KeyboardInterrupt:
        print "Exiting.."
        run = 0
