# guppi2_utils.py
# Some useful funcs for coherent dedispersion setup

def dm_from_parfile(parfile):
    """
    dm_from_parfile(parfile):
        Read DM value out of a parfile and return it.
    """
    pf = open(parfile, 'r')
    for line in pf:
        fields = line.split()
        key = fields[0]
        val = fields[1]
        if key == 'DM':
            pf.close()
            return float(val)
    pf.close()
    return 0.0

def fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):
    """
    fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):
        Returns a tuple of size parameters (fftlen, overlap, blocsize)
        given the input rf (center of band), bw, nchan, 
        DM, and optional max databuf size in MB.
    """
    round_fac = 8192 # Overlap rounding factor in samples
    rf_ghz = (rf - abs(bw)/2.0)/1.0e3
    chan_bw = bw / nchan
    overlap_samp = 8.3 * dm * chan_bw**2 / rf_ghz**3
    overlap_r = round_fac * (int(overlap_samp)/round_fac + 1)
    # Rough FFT length optimization based on GPU testing
    fftlen = 16*1024
    if overlap_r<=1024: fftlen=32*1024
    elif overlap_r<=2048: fftlen=64*1024
    elif overlap_r<=16*1024: fftlen=128*1024
    elif overlap_r<=64*1024: fftlen=256*1024
    while fftlen<2*overlap_r: fftlen *= 2
    # Calculate blocsize to hold an integer number of FFTs
    # Assumes 8-bit 2-pol data (4 bytes per sample)
    # Also assumes total nchan is distributed to 8 nodes
    node_nchan = nchan / 8
    bytes_per_samp = 4
    max_npts_per_chan = max_databuf_mb*1024*1024/bytes_per_samp/node_nchan
    nfft = (max_npts_per_chan - overlap_r)/(fftlen - overlap_r)
    npts_per_chan = nfft*(fftlen-overlap_r) + overlap_r
    blocsize = int(npts_per_chan*node_nchan*bytes_per_samp)
    return (fftlen, overlap_r, blocsize)

