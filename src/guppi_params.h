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
    int n_pol;                  /* Number of polarizations recorded */
    int n_bit;                  /* Number of bits in output data */
    
    /* Time, pointing coordinates */
    int mjd_i;                  /* Epoch of first sample (MJD) int part   */
    double mjd_f;               /* Epoch of first sample (MJD) frac part  */
    int ra_h;                   /* Right ascension hours (J2000)         */
    int ra_m;                   /* Right ascension minutes (J2000)       */
    double ra_s;                /* Right ascension seconds (J2000)       */
    int dec_d;                  /* Declination degrees (J2000)           */
    int dec_m;                  /* Declination minutes (J2000)           */
    double dec_s;               /* Declination seconds (J2000)           */
    double az;                  /* Telescope azimuth (deg)               */
    double el;                  /* Telescope elevation (deg)             */

    /* Observer info */
    char source[100];           /* Object being observed                 */
    char observer[100];         /* Observer[s] for the data set          */
    char project[100];          /* Project ID                            */
    char notes[500];            /* Any additional notes                  */

    /* Telescope info */
    char telescope[40];         /* Telescope used                        */
    char instrument[100];       /* Instrument used                       */
    char receiver[40];          /* Receiver name                         */
    char pol_mode[40];          /* Lin or Circ                           */
    double feed_angle;          /* Feed rotation angle                   */

    /* Backend hardware info */
    int decimation_factor;      /* Number of raw spectra integrated      */
    int n_bit_adc;              /* Number of bits sampled by ADCs        */
    int pfb_overlap;            /* PFB overlap factor                    */
    float scale[16*1024];       /* Per-channel scale factor              */

    /* Cal info */
    int cal_state;              /* 1=on, 0=off                           */
    double cal_freq;            /* Pulsed cal frequency (Hz)             */

    /* Leftovers from old struct.. delete if not needed! */
    double dm;                  /* Radio -- Dispersion Measure (cm-3 pc) */
    double freq;                /* Radio -- Low chan central freq (Mhz)  */
    double freqband;            /* Radio -- Total Bandwidth (Mhz)        */
    double chan_wid;            /* Radio -- Channel Bandwidth (Mhz)      */
    int bary;                   /* Barycentered?  1=yes, 0=no            */
    int numonoff;               /* The number of onoff pairs in the data */
    char name[200];             /* Data file name without suffix         */
    char analyzer[100];         /* Who analyzed the data                 */
    char band[40];              /* Type of observation (EM band)         */
    char filt[7];               /* IR,Opt,UV -- Photometric Filter       */
};

/* Write info from param struct into fits-style buffer */
void write_params(char *buf, struct guppi_params *p);

/* Parse info from buffer into param struct */
int read_params(char *buf, struct guppi_params *p);

#endif
