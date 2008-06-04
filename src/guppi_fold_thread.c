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
#include "psrfits.h"
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
    struct guppi_databuf *db_in, *db_out;
    db_in = guppi_databuf_attach(1);
    if (db_in==NULL) {
        guppi_error("guppi_fold_thread",
                "Error attaching to databuf(1) shared memory.");
        pthread_exit(NULL);
    }
    db_out = guppi_databuf_attach(2);
    if (db_out==NULL) {
        guppi_error("guppi_fold_thread",
                "Error attaching to databuf(2) shared memory.");
        pthread_exit(NULL);
    }

    /* Load polycos */
    int imjd;
    double fmjd;
    int npc=0, ipc;
    struct polyco *pc=NULL;
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
    struct foldbuf fb;
    fb.nbin = 2048;
    fb.nchan = 0;
    fb.npol = 0;
    fb.data = NULL;
    fb.count = NULL;

    /* Loop */
    char errmsg[256];
    int curblock_in=0, curblock_out=-1;
    int refresh_polycos=1, next_integration=1;
    int nblock_int=0, npacket=0, ndrop=0;
    char *ptr;
    signal(SIGINT,cc);
    while (run) {

        /* Note waiting status */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        guppi_status_unlock_safe(&st);

        /* Wait for buf to have data */
        guppi_databuf_wait_filled(db_in, curblock_in);

        /* Note waiting status */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "folding");
        guppi_status_unlock_safe(&st);

        /* Read param struct for this block */
        ptr = guppi_databuf_header(db_in, curblock_in);
        guppi_read_obs_params(ptr, &gp, &pf); // XXX first time only??
        guppi_read_subint_params(ptr, &gp, &pf);

        /* Set up, clear out buffers */
        if (next_integration) {

            /* Close out last integ */
            if (curblock_out>=0) { 
                guppi_databuf_set_filled(db_out, curblock_out);
            }

            fb.nbin = 2048;
            fb.nchan = pf.hdr.nchan;
            fb.npol = pf.hdr.npol;

            /* Wait for next output buf */
            curblock_out = (curblock_out + 1) % db_out->n_block;
            guppi_databuf_wait_free(db_out, curblock_out);
            memcpy(guppi_databuf_header(db_out, curblock_out),
                    guppi_databuf_header(db_in, curblock_in),
                    GUPPI_STATUS_SIZE);
            hputi4(guppi_databuf_header(db_out, curblock_out),
                    "NBIN", fb.nbin);

            fb.data = (float *)guppi_databuf_data(db_out, curblock_out);
            fb.count = (unsigned *)(fb.data + fb.nbin * fb.nchan * fb.npol);
            clear_foldbuf(&fb);

            nblock_int=0;
            npacket=0;
            ndrop=0;
            next_integration = 0;
        }

        /* Check src, get correct polycos */
        imjd = pf.hdr.start_day;
        fmjd = pf.hdr.start_sec 
            + pf.hdr.dt*gp.packetindex*gp.packetsize/pf.hdr.nchan/pf.hdr.npol;
        fmjd /= 86400.0;
        if (refresh_polycos) { 
            polyco_file = fopen("polyco.dat", "r");
            if (polyco_file==NULL) { 
                guppi_error("guppi_fold_thread", "Couldn't open polyco.dat");
                pthread_exit(NULL);
            }
            npc=0;
            do { 
                pc = (struct polyco *)realloc(pc, 
                        sizeof(struct polyco) * (npc+1));
                rv = read_one_pc(polyco_file, &pc[npc]);
                npc++;
            } while (rv==0);
            if (npc==1) { 
                guppi_error("guppi_fold_thread", "Error parsing polyco file.");
                pthread_exit(NULL);
            }
            fclose(polyco_file);
            refresh_polycos=0;
        }

        /* Select polyco set */
        ipc = select_pc(pc, npc, pf.hdr.source, imjd, fmjd);
        if (ipc<0) { 
            sprintf(errmsg, "No matching polycos (src=%s, imjd=%d, fmjd=%f)",
                    pf.hdr.source, imjd, fmjd);
            guppi_error("guppi_fold_thread", errmsg);
            pthread_exit(NULL);
        }

        /* fold */
        ptr = guppi_databuf_data(db_in, curblock_in);
        rv = fold_8bit_power(&pc[ipc], imjd, fmjd, ptr,
                pf.hdr.nsblk, pf.hdr.dt, &fb);
        if (rv!=0) {
            guppi_error("guppi_fold_thread", "fold error");
        }

        nblock_int++;
        npacket += gp.n_packets;
        ndrop += gp.n_dropped;
        hputi4(guppi_databuf_header(db_out, curblock_out),
                "NBLOCK", nblock_int);
        hputi4(guppi_databuf_header(db_out, curblock_out),
                "NPKT", npacket);
        hputi4(guppi_databuf_header(db_out, curblock_out),
                "NDROP", ndrop);

        /* Mark in as free */
        guppi_databuf_set_free(db_in, curblock_in);

        /* Go to next blocks */
        curblock_in = (curblock_in + 1) % db_in->n_block;

        /* Check for cancel */
        pthread_testcancel();

    }

    pthread_exit(NULL);

    pthread_cleanup_pop(0); /* Closes push(fclose) */
    pthread_cleanup_pop(0); /* Closes set_exit_status */

}
