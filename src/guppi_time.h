/* guppi_time.h
 *
 * Routines to deal with time convsrsions, etc.
 */
#ifndef _GUPPI_TIME_H
#define _GUPPI_TIME_H

/* Return current time using PSRFITS-style integer MJD, integer 
 * second time of day, and fractional second offset. */
int get_current_mjd(int *stt_imjd, int *stt_smjd, double *stt_offs);

/* Return the LST (in sec) for the GBT at a specific MJD (UTC) */
int get_current_lst(double mjd, int *lst_secs);

#endif
