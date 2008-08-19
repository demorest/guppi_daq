/* guppi_null_thread.c
 *
 * Marks databufs empty as soon as they're full
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

#define STATUS_KEY "NULLSTAT"
#include "guppi_threads.h"

void guppi_null_thread(void *_args) {

    int rv;
#if 0 
    /* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) { 
        guppi_error("guppi_null_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }
#endif

    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, 0);
    if (rv<0) {
        guppi_error("guppi_null_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Get args */
    struct guppi_thread_args *args = (struct guppi_thread_args *)_args;

    /* Attach to status shared mem area */
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_null_thread", 
                "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)set_exit_status, &st);

    /* Init status */
    guppi_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEY, "init");
    guppi_status_unlock_safe(&st);

    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(args->input_buffer);
    if (db==NULL) {
        guppi_error("guppi_null_thread",
                "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }

    /* Loop */
    int curblock=0;
    signal(SIGINT,cc);
    while (run) {

        /* Note waiting status */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        guppi_status_unlock_safe(&st);

        /* Wait for buf to have data */
        guppi_databuf_wait_filled(db, curblock);

        /* Note waiting status, current block */
        guppi_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "blanking");
        hputi4(st.buf, "CURBLOCK", curblock);
        guppi_status_unlock_safe(&st);

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);

        /* Go to next block */
        curblock = (curblock + 1) % db->n_block;

        /* Check for cancel */
        pthread_testcancel();

    }

    pthread_exit(NULL);

    pthread_cleanup_pop(0); /* Closes set_exit_status */

}
