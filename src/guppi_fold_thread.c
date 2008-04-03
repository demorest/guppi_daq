/* guppi_fold_thread.c
 *
 * Fold data, etc.
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "fitshead.h"
#include "write_psrfits.h"
#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"
#include "polyco.h"
#include "fold.h"

#define STATUS_KEY "FOLDSTAT"
#include "guppi_threads.h"

// Read a status buffer all of the key observation paramters
extern void guppi_read_obs_params(char *buf, 
                                  struct guppi_params *g, 
                                  struct psrfits *p);

/* Parse info from buffer into param struct */
extern void guppi_read_subint_params(char *buf, 
                                     struct guppi_params *g,
                                     struct psrfits *p);


void guppi_fold_thread(void *args) {

    /* Set priority */
    int rv;
    rv = setpriority(PRIO_PROCESS, 0, 0);
    if (rv<0) {
        guppi_error("guppi_fold_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_fold_thread", 
                "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)set_exit_status, &st);

    /* Init status */
    guppi_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEY, "init");
    guppi_status_unlock_safe(&st);

    /* Read in general parameters */
    struct guppi_params gp;
    struct psrfits pf;

    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(1);
    if (db==NULL) {
        guppi_error("guppi_fold_thread",
                "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }

    /* Load polycos */
    int imjd;
    double fmjd;
    int npc=1;
    struct polyco pc;
    FILE *polyco_file=NULL;

    /* Open output file */
    FILE *fraw = fopen("guppi_raw.dat", "w");
    if (fraw==NULL) { 
        guppi_error("guppi_fold_thread",
                "Error opening output file.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)fclose, fraw);

    /* Buffers */
    double *foldbuf=NULL;
    long long *cntbuf=NULL;

    /* Loop */
    char errmsg[256];
    int nbin=2048;
    int curblock=0;
    char *ptr;
    signal(SIGINT,cc);
    while (run) {

        /* Note waiting status */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        guppi_status_unlock_safe(&st);

        /* Wait for buf to have data */
        guppi_databuf_wait_filled(db, curblock);

        /* Note waiting status */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "writing");
        guppi_status_unlock_safe(&st);

        /* Read param struct for this block */
        ptr = guppi_databuf_header(db, curblock);
        guppi_read_obs_params(ptr, &gp, &pf); // XXX first time only??
        guppi_read_subint_params(ptr, &gp, &pf);

        /* Set up, clear out buffers */
        // TODO integrate for longer than one block
        foldbuf = (double *)realloc(foldbuf, 
                sizeof(double)*nbin*pf.hdr.nchan*pf.hdr.npol);
        cntbuf = (long long *)realloc(cntbuf, sizeof(long long)*nbin);
        memset(foldbuf, 0, sizeof(double)*nbin*pf.hdr.nchan*pf.hdr.npol);
        memset(cntbuf, 0, sizeof(long long)*nbin);

        /* Check src, get correct polycos */
        // XXX Do this better!
        imjd = pf.hdr.start_day;
        fmjd = pf.hdr.start_sec 
            + pf.hdr.dt*gp.packetindex*gp.packetsize/pf.hdr.nchan/pf.hdr.npol;
        fmjd /= 86400.0;
        polyco_file = fopen("polyco.dat", "r");
        if (polyco_file==NULL) { 
            guppi_error("guppi_fold_thread", "Couldn't open polyco.dat");
            pthread_exit(NULL);
        }
        rv = read_pc(polyco_file, &pc, pf.hdr.source, imjd, fmjd);
        if (rv!=0) { 
            sprintf(errmsg, "No matching polycos (src=%s, imjd=%d fmjd=%f)",
                    pf.hdr.source, imjd, fmjd);
            guppi_error("guppi_fold_thread", errmsg);
            pthread_exit(NULL);
        }
        fclose(polyco_file);

        /* fold */
        ptr = guppi_databuf_data(db, curblock);
        rv = fold_8bit_power(&pc, npc, imjd, fmjd, ptr,
                pf.hdr.nsblk, pf.hdr.nchan, pf.hdr.npol, 
                pf.hdr.dt, foldbuf, cntbuf, nbin);
        if (rv!=0) {
            guppi_error("guppi_fold_thread", "fold error");
        }

        /* Write output */
        normalize_folds(foldbuf, cntbuf, nbin, pf.hdr.nchan, pf.hdr.npol);
#if 0 
        rv = fwrite(foldbuf, sizeof(double), nbin*pf.hdr.nchan*pf.hdr.npol, 
                fraw);
        if (rv != nbin*pf.hdr.nchan*pf.hdr.npol) { 
            guppi_error("guppi_fold_thread", 
                    "Error writing data.");
        }

        /* flush output */
        fflush(fraw);
#endif

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);

        /* Go to next block */
        curblock = (curblock + 1) % db->n_block;

        /* Check for cancel */
        pthread_testcancel();

    }

    pthread_exit(NULL);

    pthread_cleanup_pop(0); /* Closes push(fclose) */
    pthread_cleanup_pop(0); /* Closes set_exit_status */

}
