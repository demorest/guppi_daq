/* guppi_params.c
 *
 * Routines to read/write basic system parameters.
 * Use PSRFITS style keywords as much as possible.
 */
#include <stdio.h>
#include <math.h>

#include "fitshead.h"
#include "guppi_params.h"

void guppi_write_params(char *buf, struct guppi_params *p) {

    /* Start time, MJD */
    hputi4(buf, "STT_IMJD", p->mjd_i);
    hputi4(buf, "STT_SMJD", (int)(p->mjd_f*86400.0));
    hputr8(buf, "STT_OFFS", fmod(p->mjd_f*86400.0,1.0));

    /* Freq, BW, Nchan, etc */
    hputr8(buf, "OBSFREQ", p->f_ctr);
    hputr8(buf, "OBSBW", (double)p->band_dir * p->bandwidth);
    hputi4(buf, "OBSNCHAN", p->n_chan);
    hputi4(buf, "NPOL", p->n_pol);
    hputi4(buf, "NBITS", p->n_bits);
    hputr8(buf, "TBIN", p->dt);

}

void guppi_read_params(char *buf, struct guppi_params *p) {

    double dtmp;

    /* Start time, MJD */
    hgeti4(buf, "STT_IMJD", &p->mjd_i);
    hgetr8(buf, "STT_SMJD", &p->mjd_f);
    hgetr8(buf, "STT_OFFS", &dtmp);
    p->mjd_f = (p->mjd_f + dtmp)/86400.0;

    /* Freq, BW, etc. */
    hgetr8(buf, "OBSFREQ", &p->f_ctr);
    hgetr8(buf, "OBSBW", &p->bandwidth);
    if (p->bandwidth<0.0) { p->band_dir=-1; p->bandwidth *= -1.0; }
    else { p->band_dir=1; }
    hgeti4(buf, "OBSNCHAN", &p->n_chan);
    hgeti4(buf, "NPOL", &p->n_pol);
    hgeti4(buf, "NBITS", &p->n_bits);
    hgetr8(buf, "TBIN", &p->dt);
}
