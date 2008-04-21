from guppi_utils import guppi_status, guppi_databuf
import shm_wrapper as shm
import numpy as n
import pylab as p
import time
import math

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
x = p.arange(nchan)/float(nchan)*BW + (fctr-0.5*BW)

do_log = 0

poln = 0
data = d.data(0)
avg_spec = data[:,poln,:].mean(0)
min_spec = data[:,poln,:].min(0)
max_spec = data[:,poln,:].max(0)

if do_log:
    avg_spec = db(avg_spec)
    min_spec = db(min_spec)
    max_spec = db(max_spec)

print len(x), len(avg_spec)

p.xlabel("Frequency (MHz)")
p.ylabel("Power")
line, = p.plot(x, avg_spec)
hi_line, = p.plot(x, max_spec, 'b:')
lo_line, = p.plot(x, min_spec, 'b:')

if do_log:
    p.axis([x.min(),x.max(),-30.0,0.0])
else:
    p.axis('auto')

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
        avg_spec = data[:,poln,:].mean(0)
        min_spec = data[:,poln,:].min(0)
        max_spec = data[:,poln,:].max(0)
        if do_log:
            avg_spec = db(avg_spec)
            min_spec = db(min_spec)
            max_spec = db(max_spec)
        line.set_ydata(avg_spec)
        hi_line.set_ydata(max_spec)
        lo_line.set_ydata(min_spec)
        p.axis('auto')
        print "Block %2d: Max chan=%d freq=%.3fMHz value=%.3f" %\
                (curblock, avg_spec.argmax(), x[avg_spec.argmax()],\
                avg_spec.max())
        p.draw()
        time.sleep(1)
    except KeyboardInterrupt:
        print "Exiting.."
        run = 0
