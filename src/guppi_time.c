/* guppi_time.c
 *
 * Routines dealing with time conversion.
 */
#include <time.h>
#include <sys/time.h>
#include "slalib.h"
#include "guppi_error.h"

int get_current_mjd(int *stt_imjd, int *stt_smjd, double *stt_offs) {
    int rv;
    struct timeval tv;
    struct tm gmt;
    double mjd;

    rv = gettimeofday(&tv,NULL);
    if (rv) { return(GUPPI_ERR_SYS); }

    if (gmtime_r(&tv.tv_sec, &gmt)==NULL) { return(GUPPI_ERR_SYS); }

    slaCaldj(gmt.tm_year+1900, gmt.tm_mon+1, gmt.tm_mday, &mjd, &rv);
    if (rv!=0) { return(GUPPI_ERR_GEN); }

    if (stt_imjd!=NULL) { *stt_imjd = (int)mjd; }
    if (stt_smjd!=NULL) { *stt_smjd = gmt.tm_hour*3600 + gmt.tm_min*60 
        + gmt.tm_sec; }
    if (stt_offs!=NULL) { *stt_offs = tv.tv_usec*1e6; }

    return(GUPPI_OK);
}

