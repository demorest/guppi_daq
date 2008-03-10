/* guppi_rawdisk_thread.c
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
#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"

#define STATUS_KEY "DISKSTAT"
#include "guppi_threads.h"

void guppi_rawdisk_thread(void *args) {

    /* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    int rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) { 
        guppi_error("guppi_rawdisk_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }

    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, 0);
    if (rv<0) {
        guppi_error("guppi_rawdisk_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_rawdisk_thread", 
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
    guppi_status_lock_safe(&st);
    guppi_read_params(st.buf, &gp);
    guppi_status_unlock_safe(&st);

    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(1);
    if (db==NULL) {
        guppi_error("guppi_rawdisk_thread",
                "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }

    /* Open output file */
    FILE *fraw = fopen("guppi_raw.dat", "w");
    FILE *fhdr = fopen("guppi_hdr.dat", "w");
    if ((fraw==NULL) || (fhdr==NULL)) { 
        guppi_error("guppi_rawdisk_thread",
                "Error opening output file.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)fclose, fhdr);
    pthread_cleanup_push((void *)fclose, fraw);

    /* Write header to hdr file */
    fprintf(fhdr, "# pktidx pktsize npkt ndrop\n");

    /* Loop */
    int packetidx=0, npacket=0, ndrop=0, packetsize=0;
    int curblock=0, total_status=0;
    char *ptr, *hend;
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
        guppi_read_params(ptr, &gp);

        /* Parse packet size, npacket from header */
        hgeti4(ptr, "PKTIDX", &packetidx);
        hgeti4(ptr, "PKTSIZE", &packetsize);
        hgeti4(ptr, "NPKT", &npacket);
        hgeti4(ptr, "NDROP", &ndrop);

        /* See how full databuf is */
        total_status = guppi_databuf_total_status(db);

        /* Write stats to separate file */
        fprintf(fhdr, "%d %d %d %d %d\n", packetidx, packetsize, npacket,
                ndrop, total_status);

        /* Write header to file */
        hend = ksearch(ptr, "END");
        for (ptr=ptr; ptr<hend; ptr+=80) {
            fwrite(ptr, 80, 1, fraw);
        }

        /* Write data */
        ptr = guppi_databuf_data(db, curblock);
        rv = fwrite(ptr, packetsize, npacket, fraw);
        if (rv != npacket) { 
            guppi_error("guppi_rawdisk_thread", 
                    "Error writing data.");
        }

        /* flush output */
        fflush(fraw);
        fflush(fhdr);

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);

        /* Go to next block */
        curblock = (curblock + 1) % db->n_block;

        /* Check for cancel */
        pthread_testcancel();

    }

    pthread_exit(NULL);

    pthread_cleanup_pop(0); /* Closes push(fclose) */
    pthread_cleanup_pop(0); /* Closes push(fclose) */
    pthread_cleanup_pop(0); /* Closes set_exit_status */

}
