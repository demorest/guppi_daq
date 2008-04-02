/* guppi_psrfits_thread.c
 *
 * Write databuf blocks out to disk.
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

#define STATUS_KEY "DISKSTAT"
#include "guppi_threads.h"

// Read a status buffer all of the key observation paramters
extern void guppi_read_obs_params(char *buf, 
                                  struct guppi_params *g, 
                                  struct psrfits *p);

/* Parse info from buffer into param struct */
extern void guppi_read_subint_params(char *buf, 
                                     struct guppi_params *g,
                                     struct psrfits *p);

void guppi_psrfits_thread(void *args) {
    
    /* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    int rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) { 
        guppi_error("guppi_psrfits_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }
    
    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, 0);
    if (rv<0) {
        guppi_error("guppi_psrfits_thread", "Error setting priority level.");
        perror("set_priority");
    }
    
    /* Attach to status shared mem area */
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_psrfits_thread", 
                    "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)set_exit_status, &st);
    
    /* Init status */
    guppi_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEY, "init");
    guppi_status_unlock_safe(&st);
    
    /* Initialize some key parameters */
    struct guppi_params gp;
    struct psrfits pf;
    pf.sub.dat_freqs = pf.sub.dat_weights = NULL;
    pf.sub.dat_offsets = pf.sub.dat_scales = NULL;
    pf.filenum = 0; // This is crucial
    pthread_cleanup_push((void *)psrfits_close, &pf);
    
    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(1);
    if (db==NULL) {
        guppi_error("guppi_psrfits_thread",
                    "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }
    
    /* Loop */
    int curblock=0, total_status=0, firsttime=1, run=1;
    char *ptr;
    signal(SIGINT, cc);
    do {
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
        
        /* See how full databuf is */
        total_status = guppi_databuf_total_status(db);
        
        /* Read param structs for this block */
        ptr = guppi_databuf_header(db, curblock);
        if (firsttime) {
            guppi_read_obs_params(ptr, &gp, &pf);
            firsttime = 0;
        } else {
            guppi_read_subint_params(ptr, &gp, &pf);
        }
        
        /* Get the pointer to the current data */
        pf.sub.data = (unsigned char *)guppi_databuf_data(db, curblock);
        
        /* Write the data */
        psrfits_write_subint(&pf);
            
        /* Is the scan complete? */
        if ((pf.hdr.scanlen > 0.0) && 
            (pf.T > pf.hdr.scanlen)) run = 0;
        
        /* For debugging... */
        if (gp.drop_frac > 0.0) {
           printf("Block %d dropped %.3g%% of the packets\n", 
                  pf.tot_rows, gp.drop_frac*100.0);
        }

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);
        
        /* Go to next block */
        curblock = (curblock + 1) % db->n_block;
        
        /* Check for cancel */
        pthread_testcancel();
        
    } while (run);
    
    /* Cleanup */
    
    free(pf.sub.dat_freqs);
    free(pf.sub.dat_weights);
    free(pf.sub.dat_offsets);
    free(pf.sub.dat_scales);
    
    pthread_exit(NULL);
    
    pthread_cleanup_pop(0); /* Closes psrfits_close */
    pthread_cleanup_pop(0); /* Closes set_exit_status */
}
