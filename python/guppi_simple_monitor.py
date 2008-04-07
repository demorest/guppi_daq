from guppi_utils import guppi_status
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
nspec = 1000  # number of spectra to read and average

# Connect to the GUPPI data buffer
data_buf = shm.SharedMemoryHandle(12987498)
packed = data_buf.read(NumberOfBytes=3*8+3*4, offset=64)
struct_size, block_size, header_size = \
             n.fromstring(packed[0:24], dtype=n.int64)
shmid, semid, n_block = \
             n.fromstring(packed[24:36], dtype=n.int32)
data_offset = struct_size + n_block*header_size
offset = data_offset
curblock = 0
spec_size = npoln * nchan
print struct_size, block_size, header_size, shmid, semid, n_block, offset

p.ion()  # turn unteractive mode on
x = p.arange(nchan)/float(nchan)*BW + (fctr-0.5*BW)
#x = n.arange(nchan * npoln)

poln = 0
data = n.fromstring(data_buf.read(spec_size*nspec, offset), dtype=n.uint8)
data.shape = (nspec, spec_size)
avg_spec = data[:,nchan*poln:nchan*(poln+1)].mean(0)

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
        offset = data_offset + block_size*curblock
        data = n.fromstring(data_buf.read(spec_size*nspec, offset), dtype=n.uint8)
        data.shape = (nspec, spec_size)
        avg_spec = data[:,nchan*poln:nchan*(poln+1)].mean(0)
        line.set_ydata(avg_spec)
        print "Max chan=%d freq=%.3fMHz value=%.3f" %\
                (avg_spec.argmax(), x[avg_spec.argmax()], avg_spec.max())
        p.draw()
    except KeyboardInterrupt:
        print "Exiting.."
        run = 0
