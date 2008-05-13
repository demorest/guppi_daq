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
#include "guppi_error.h"
#include "slalib.h"

#ifndef DEGTORAD
#define DEGTORAD 0.017453292519943295769236907684886127134428718885417
#endif
#ifndef RADTODEG
#define RADTODEG 57.29577951308232087679815481410517033240547246656
#endif
#ifndef SOL
#define SOL 299792458.0
#endif

#define DEBUGOUT 1

#define get_dbl(key, param, def) {                                      \
        if (hgetr8(buf, (key), &(param))==0) {                          \
            if (DEBUGOUT)                                               \
                printf("Warning:  %s not in status shm!\n", (key));     \
            (param) = (def);                                            \
        }                                                               \
    }

#define get_int(key, param, def) {                                      \
        if (hgeti4(buf, (key), &(param))==0) {                          \
            if (DEBUGOUT)                                               \
                printf("Warning:  %s not in status shm!\n", (key));     \
            (param) = (def);                                            \
        }                                                               \
    }

#define get_lon(key, param, def) {                                      \
        {                                                               \
            double dtmp;                                                \
            if (hgetr8(buf, (key), &dtmp)==0) {                         \
                if (DEBUGOUT)                                           \
                    printf("Warning:  %s not in status shm!\n", (key)); \
                (param) = (def);                                        \
            } else {                                                    \
                (param) = (long long)(rint(dtmp));                      \
            }                                                           \
        }                                                               \
    }

#define get_str(key, param, len, def) {                                 \
        if (hgets(buf, (key), (len), (param))==0) {                     \
            if (DEBUGOUT)                                               \
                printf("Warning:  %s not in status shm!\n", (key));     \
            strcpy((param), (def));                                     \
        }                                                               \
    }

#define exit_on_missing(key, param, val) {                              \
        if ((param)==(val)) {                                           \
            char errmsg[100];                                           \
            sprintf(errmsg, "%s is required!\n", (key));                \
            guppi_error("guppi_read_obs_params", errmsg);               \
            exit(1);                                                    \
        }                                                               \
    }


// Return the beam FWHM in degrees for obs_freq in MHz 
// and dish_diam in m
double beam_FWHM(double obs_freq, double dish_diam)
{
    double lambda = SOL/(obs_freq*1e6);
    return 1.2 * lambda / dish_diam * RADTODEG;
}


// Read a status buffer all of the key observation paramters
void guppi_read_subint_params(char *buf, 
                              struct guppi_params *g, 
                              struct psrfits *p)
{
    // Parse packet size, # of packets, etc.
    get_lon("PKTIDX", g->packetindex, -1L);
    get_int("PKTSIZE", g->packetsize, 0);
    get_int("NPKT", g->n_packets, 0);
    get_int("NDROP", g->n_dropped, 0);
    get_dbl("DROPAVG", g->drop_frac_avg, 0.0);
    get_dbl("DROPTOT", g->drop_frac_tot, 0.0);
    g->drop_frac = (double) g->n_dropped / (double) g->n_packets;
    get_str("PKTFMT", g->packet_format, 32, "GUPPI");

    // Observation params
    get_dbl("AZ", p->sub.tel_az, 0.0);
    if (p->sub.tel_az < 0.0) p->sub.tel_az += 360.0;
    get_dbl("ZA", p->sub.tel_zen, 0.0);
    get_dbl("RA", p->sub.ra, 0.0);
    get_dbl("DEC", p->sub.dec, 0.0);

    // Backend HW parameters
    get_int("ACC_LEN", g->decimation_factor, 0);
    get_int("NBITSADC", g->n_bits_adc, 8);
    get_int("PFB_OVER", g->pfb_overlap, 4);

    // Midtime of this block (relative to obs start)
    int bytes_per_dt = p->hdr.nchan * p->hdr.npol * p->hdr.nbits / 8;
    p->sub.offs = p->hdr.dt * 
        (double)(g->packetindex * g->packetsize / bytes_per_dt)
        + 0.5 * p->sub.tsubint;

    { // MJD and LST calcs
        int imjd, smjd, lst_secs;
        double offs, mjd;
        get_current_mjd(&imjd, &smjd, &offs);
        mjd = (double) imjd + ((double) smjd + offs) / 86400.0;
        get_current_lst(mjd, &lst_secs);
        p->sub.lst = (double) lst_secs;
    }

    // Until we need them...
    p->sub.feed_ang = 0.0;
    p->sub.pos_ang = 0.0;
    p->sub.par_ang = 0.0;
    
    // Galactic coords
    slaEqgal(p->sub.ra*DEGTORAD, p->sub.dec*DEGTORAD,
             &p->sub.glon, &p->sub.glat);
    p->sub.glon *= RADTODEG;
    p->sub.glat *= RADTODEG;
}


// Read a status buffer all of the key observation paramters
void guppi_read_obs_params(char *buf, 
                           struct guppi_params *g, 
                           struct psrfits *p)
{
    // Freq, BW, etc.
    get_dbl("OBSFREQ", p->hdr.fctr, 0.0);
    get_dbl("OBSBW", p->hdr.BW, 0.0);
    exit_on_missing("OBSBW", p->hdr.BW, 0.0);
    get_int("OBSNCHAN", p->hdr.nchan, 0);
    exit_on_missing("OBSNCHAN", p->hdr.nchan, 0);
    get_int("NPOL", p->hdr.npol, 0);
    exit_on_missing("NPOL", p->hdr.npol, 0);
    get_int("NBITS", p->hdr.nbits, 0);
    exit_on_missing("NBITS", p->hdr.nbits, 0);
    get_dbl("TBIN", p->hdr.dt, 0.0);
    exit_on_missing("TBIN", p->hdr.dt, 0.0);
    get_dbl("CHAN_BW", p->hdr.df, 0.0);
    if (p->hdr.df==0.0) p->hdr.df = p->hdr.BW/p->hdr.nchan;
    get_dbl("SCANLEN", p->hdr.scanlen, 0.0);
    get_int("NRCVR", p->hdr.rcvr_polns, 2);
    p->hdr.orig_df = p->hdr.df;
    p->hdr.orig_nchan = p->hdr.nchan;

    // Observation information
    get_str("TELESCOP", p->hdr.telescope, 24, "GBT");
    get_str("OBSERVER", p->hdr.observer, 24, "Unknown");
    get_str("SRC_NAME", p->hdr.source, 24, "Unknown");
    get_str("FRONTEND", p->hdr.frontend, 24, "Unknown");
    get_str("BACKEND", p->hdr.backend, 24, "GUPPI");
    get_str("PROJID", p->hdr.project_id, 24, "Unknown");
    get_str("FD_POLN", p->hdr.poln_type, 8, "Unknown");
    get_str("POL_TYPE", p->hdr.poln_order, 16, "Unknown");
    get_int("SCANNUM", p->hdr.scan_number, 1);
    get_str("BASENAME", p->basefilename, 200, "guppi");
    if (strcmp("POL_TYPE", "AA+BB")==0 ||
        strcmp("POL_TYPE", "INTEN")==0)
        p->hdr.summed_polns = 1;
    else
        p->hdr.summed_polns = 0;
    get_str("TRK_MODE", p->hdr.track_mode, 16, "Unknown");
    get_str("RA_STR", p->hdr.ra_str, 16, "Unknown");
    get_str("DEC_STR", p->hdr.dec_str, 16, "Unknown");
    // Should set other cal values if CAL_MODE is on
    get_str("CAL_MODE", p->hdr.cal_mode, 8, "Unknown");
    if (!(strcmp(p->hdr.cal_mode, "OFF")==0)) {  // Cals not off
        get_dbl("CAL_FREQ", p->hdr.cal_freq, 25.0);
        get_dbl("CAL_DCYC", p->hdr.cal_dcyc, 0.5);
        get_dbl("CAL_PHS", p->hdr.cal_phs, 0.0);
    }
    get_str("OBS_MODE", p->hdr.obs_mode, 8, "Unknown");

    // Fold mode specific stuff
    int fold=0;
    if (strcmp("FOLD", p->hdr.obs_mode)==0) { fold=1; }
    if (fold) {
        get_int("NBIN", p->hdr.nbin, 1024);
    } else {
        p->hdr.nbin = 1;
    }
    
    { // Start time, MJD
        int mjd_d, mjd_s;
        double mjd_fs;
        get_int("STT_IMJD", mjd_d, 0);
        get_int("STT_SMJD", mjd_s, 0);
        get_dbl("STT_OFFS", mjd_fs, 0.0);
        p->hdr.MJD_epoch = (long double) mjd_d;
        p->hdr.MJD_epoch += ((long double) mjd_s + mjd_fs) / 86400.0;
        p->hdr.start_day = mjd_d;
        p->hdr.start_sec = mjd_s + mjd_fs;
    }
    
    { // Date and time of start
        int YYYY, MM, DD, h, m;
        double s;
        datetime_from_mjd(p->hdr.MJD_epoch, &YYYY, &MM, &DD, &h, &m, &s);
        sprintf(p->hdr.date_obs, "%04d-%02d-%02dT%02d:%02d:%06.3f", 
                YYYY, MM, DD, h, m, s);
    }
    
    // The beamwidth
    if (strcmp("GBT", p->hdr.telescope)==0)
        p->hdr.beam_FWHM = beam_FWHM(p->hdr.fctr, 100.0);
    else if (strcmp("GB43m", p->hdr.telescope)==0)
        p->hdr.beam_FWHM = beam_FWHM(p->hdr.fctr, 43.0);
    else
        p->hdr.beam_FWHM = 0.0;

    // Now bookkeeping information
    {
        int ii, jj, kk;
        int bytes_per_dt = p->hdr.nchan * p->hdr.npol * p->hdr.nbits / 8;
        char key[10];
        double offset, scale, dtmp;
        long long max_bytes_per_file;

        get_int("BLOCSIZE", p->sub.bytes_per_subint, 0);
        p->hdr.nsblk = p->sub.bytes_per_subint / bytes_per_dt;
        p->sub.FITS_typecode = TBYTE;
        p->sub.tsubint = p->hdr.nsblk * p->hdr.dt;
        if (fold) { 
            // TODO fix this up
            p->hdr.nsblk = 1;
            p->sub.FITS_typecode = TFLOAT;
            p->sub.tsubint *= 128;
            p->sub.bytes_per_subint = sizeof(float) * p->hdr.nbin *
                p->hdr.nchan * p->hdr.npol;
        }
        max_bytes_per_file = PSRFITS_MAXFILELEN * 1073741824L;
        // Will probably want to tweak this so that it is a nice round number
        if (p->sub.bytes_per_subint!=0)
            p->rows_per_file = max_bytes_per_file / p->sub.bytes_per_subint;

        // Free the old ones in case we've changed the params
        if (p->sub.dat_freqs) free(p->sub.dat_freqs);
        if (p->sub.dat_weights) free(p->sub.dat_weights);
        if (p->sub.dat_offsets) free(p->sub.dat_offsets);
        if (p->sub.dat_scales) free(p->sub.dat_scales);

        // Allocate the subband arrays
        p->sub.dat_freqs = (float *)malloc(sizeof(float) *  p->hdr.nchan);
        p->sub.dat_weights = (float *)malloc(sizeof(float) *  p->hdr.nchan);
        dtmp = p->hdr.fctr - 0.5 * p->hdr.BW + 0.5 * p->hdr.df;
        for (ii = 0 ; ii < p->hdr.nchan ; ii++) {
            p->sub.dat_freqs[ii] = dtmp + ii * p->hdr.df;
            p->sub.dat_weights[ii] = 1.0;
        }
        p->sub.dat_offsets = (float *)malloc(sizeof(float) *  
                                             p->hdr.nchan * p->hdr.npol);
        p->sub.dat_scales = (float *)malloc(sizeof(float) *  
                                            p->hdr.nchan * p->hdr.npol);
        for (ii = 0 ; ii < p->hdr.npol ; ii++) {
            sprintf(key, "OFFSET%d", ii);
            get_dbl(key, offset, 0.0);
            sprintf(key, "SCALE%d", ii);
            get_dbl(key, scale, 1.0);
            for (jj = 0, kk = ii*p->hdr.nchan ; jj < p->hdr.nchan ; jj++, kk++) {
                p->sub.dat_offsets[kk] = offset;
                p->sub.dat_scales[kk] = scale;
            }
        }
    }
    
    // Read information that is appropriate for the subints
    guppi_read_subint_params(buf, g, p);
    p->hdr.azimuth = p->sub.tel_az;
    p->hdr.zenith_ang = p->sub.tel_zen;
    p->hdr.ra2000 = p->sub.ra;
    p->hdr.dec2000 = p->sub.dec;
    p->hdr.start_lst = p->sub.lst;
    p->hdr.feed_angle = p->sub.feed_ang;
}
