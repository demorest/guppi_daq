/* guppi_params.h 
 *
 * Defines structure used internally to represent observation 
 * parameters.  Includes routines to read/write this info to
 * a "FITS-style" shared memory buffer.
 */
#ifndef _GUPPI_PARAMS_H
#define _GUPPI_PARAMS_H

struct guppi_params {
    /* Packet information for the current block */
    long long packetindex;      // Index of first packet in raw data block
    int packetsize;             // Size in bytes of data portion of each packet
    int n_packets;              // Total number of packets in current block
    int n_dropped;              // Number of packets dropped in current block
    /* Backend hardware info */
    int decimation_factor;      // Number of raw spectra integrated
    int n_bits_adc;             // Number of bits sampled by ADCs
    int pfb_overlap;            // PFB overlap factor
    float scale[16*1024];       // Per-channel scale factor
    float offset[16*1024];      // Per-channel offset
};

#endif
