/* guppi_params.h 
 *
 * Defines structure used internally to represent observation 
 * parameters.  Includes routines to read/write this info to
 * a "FITS-style" shared memory buffer.
 */
#ifndef _GUPPI_PARAMS_H
#define _GUPPI_PARAMS_H

struct guppi_params {

    /* Basic observation params */
    double bandwidth;           /* Total BW in MHz at ADC (ie sample_rate/2) */
    double dt;                  /* Time per integrated spectrum (s) */
    int n_chan;                 /* Number of spectral channels */
    double f_ctr;               /* Band center frequency (MHz) */
    int band_dir;               /* +1=forward, -1=reversed freqs */
    int n_pol;                  /* Number of polarizations recorded (1 = summed) */
    int n_bits;                 /* Number of bits in output data */
    
    /* Time, pointing coordinates */
    int mjd_i;                  /* Epoch of first sample (MJD) int part   */
    double mjd_f;               /* Epoch of first sample (MJD) frac part  */
    double ra_deg;              /* Right ascension (degrees, J2000) */
    double dec_deg;             /* Declination (degrees, J2000) */
    double az;                  /* Telescope azimuth (degrees) */
    double el;                  /* Telescope elevation (degrees) */

    /* String observation info */
    char basefilenm[128];       /* base filename for raw data */
    char source[128];           /* Object being observed */
    char observer[128];         /* Observer[s] for the data set */
    char project[128];          /* Project ID */
    char notes[512];            /* Any additional notes */
    char guppi_vers[32];        /* guppi_dac version string */

    /* Telescope info */
    char telescope[128];        /* Telescope used */
    char instrument[128];       /* Instrument used */
    char receiver[32];          /* Receiver name */
    char pol_mode[16];          /* Linear or Circular */
    double feed_angle;          /* Feed rotation angle */
    int tracking;               /* 0 = driftscan, 1 = tracking */

    /* Backend hardware info */
    int decimation_factor;      /* Number of raw spectra integrated */
    int n_bits_adc;             /* Number of bits sampled by ADCs */
    int pfb_overlap;            /* PFB overlap factor */
    float scale[16*1024];       /* Per-channel scale factor */
    float offset[16*1024];      /* Per-channel offset */

    /* Cal info */
    int cal_state;              /* 1=on, 0=off */
    double cal_freq;            /* Pulsed cal frequency (Hz) */
};

/* Write info from param struct into fits-style buffer */
void write_params(char *buf, struct guppi_params *p);

/* Parse info from buffer into param struct */
int read_params(char *buf, struct guppi_params *p);

#endif
