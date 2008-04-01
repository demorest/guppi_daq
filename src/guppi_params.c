/* guppi_params.c
 *
 * Routines to read/write basic system parameters.
 * Use PSRFITS style keywords as much as possible.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "fitshead.h"
#include "write_psrfits.h"
#include "guppi_params.h"
#include "guppi_time.h"
#include "slalib.h"

#ifndef DEGTORAD
#define DEGTORAD 0.017453292519943295769236907684886127134428718885417
#endif
#ifndef RADTODEG
#define RADTODEG 57.29577951308232087679815481410517033240547246656
#endif

#define DEBUG 1

#define get_dbl(key, param) {                                           \
        if (hgetr8(buf, (key), &(param))==0) {                          \
            if (DEBUG) printf("Warning:  %s not defined in status buffer!\n", (key)); \
            (param) = 0.0;                                              \
        }                                                               \
    }

#define get_int(key, param) {                                           \
        if (hgeti4(buf, (key), &(param))==0) {                          \
            if (DEBUG) printf("Warning:  %s not defined in status buffer!\n", (key)); \
            (param) = 0;                                                \
        }                                                               \
    }

#define get_lon(key, param) {                                           \
        {                                                               \
            double dtmp;                                                \
            if (hgetr8(buf, (key), &dtmp)==0) {                         \
                if (DEBUG) printf("Warning:  %s not defined in status buffer!\n", (key)); \
                (param) = 0L;                                           \
            } else {                                                    \
                (param) = (long long)(dtmp + 0.00000001);               \
            }                                                           \
        }                                                               \
    }

#define get_str(key, param, len) {                                      \
        if (hgets(buf, (key), (len), (param))==0) {                     \
            if (DEBUG) printf("Warning:  %s not defined in status buffer!\n", (key)); \
            strcpy(param, "");                                          \
        }                                                               \
    }


// Write to the status buffer all of the key paramters
void guppi_write_params(char *buf, 
                        struct guppi_params *g,
                        struct psrfits *p)
{
    { // Start time, MJD
        int mjd_i = (int)(p->hdr.MJD_epoch);
        double mjd_f = p->hdr.MJD_epoch - (long double) mjd_i;
        hputi4(buf, "STT_IMJD", mjd_i);
        hputi4(buf, "STT_SMJD", (int)(mjd_f*86400.0));
        hputr8(buf, "STT_OFFS", fmod(mjd_f*86400.0, 1.0));
    }
    
    // Freq, BW, Nchan, etc
    hputr8(buf, "OBSFREQ", p->hdr.fctr);
    hputr8(buf, "OBSBW", p->hdr.BW);
    hputi4(buf, "OBSNCHAN", p->hdr.nchan);
    hputr8(buf, "CHAN_BW", p->hdr.orig_df);
    hputi4(buf, "NPOL", p->hdr.npol);
    hputi4(buf, "NBITS", p->hdr.nbits);
    hputr8(buf, "TBIN", p->hdr.dt);
}

// Read from the status buffer all of the key paramters
void guppi_read_params(char *buf, 
                       struct guppi_params *g, 
                       struct psrfits *p)
{
    { // Start time, MJD
        int mjd_d, mjd_s;
        double mjd_fs;
        get_int("STT_IMJD", mjd_d);
        get_int("STT_SMJD", mjd_s);
        get_dbl("STT_OFFS", mjd_fs);
        p->hdr.MJD_epoch = (long double) mjd_d;
        p->hdr.MJD_epoch += ((long double) mjd_s + mjd_fs) / 86400.0;
    }

    // Freq, BW, etc.
    get_dbl("OBSFREQ", p->hdr.fctr);
    get_dbl("OBSBW", p->hdr.BW);
    get_int("OBSNCHAN", p->hdr.nchan);
    get_int("NPOL", p->hdr.npol);
    get_int("NBITS", p->hdr.nbits);
    get_dbl("TBIN", p->hdr.dt);
    get_dbl("CHAN_BW", p->hdr.df);

    // Parse packet size, # of packets, etc.
    get_lon("PKTIDX", g->packetindex);
    get_int("PKTSIZE", g->packetsize);
    get_int("NPKT", g->n_packets);
    get_int("NDROP", g->n_dropped);

    // Observation information
    get_str("OBSERVER", p->hdr.observer, 24);
    get_str("SRC_NAME", p->hdr.source, 24);
    get_str("FRONTEND", p->hdr.frontend, 24);
    get_str("PROJID", p->hdr.project_id, 24);
    get_str("FD_POLN", p->hdr.poln_type, 8);
    get_str("POL_TYPE", p->hdr.poln_order, 16);
    get_str("TRK_MODE", p->hdr.track_mode, 16);
    // Should set other cal values if CAL_MODE is on
    get_str("CAL_MODE", p->hdr.cal_mode, 8);
    get_str("RA_STR", p->hdr.ra_str, 16);
    get_str("DEC_STR", p->hdr.dec_str, 16);
    get_dbl("AZ", p->sub.tel_az);
    get_dbl("ZA", p->sub.tel_zen);
    get_dbl("RA", p->sub.ra);
    get_dbl("DEC", p->sub.dec);
    p->hdr.ra2000 = p->sub.ra;
    p->hdr.dec2000 = p->sub.dec;

    { // MJD and LST calcs
        int imjd, smjd, lst_secs;
        double offs, mjd;
        get_current_mjd(&imjd, &smjd, &offs);
        mjd = (double) imjd + ((double) smjd + offs) / 86400.0;
        get_current_lst(mjd, &lst_secs);
        p->sub.lst = (double) lst_secs;
    }

    // Galactic coords
    slaEqgal(p->hdr.ra2000*DEGTORAD, p->hdr.dec2000*DEGTORAD,
             &p->sub.glon, &p->sub.glat);
}
