/* guppi_rawdisk_thread.c
 *
 * Write databuf blocks out to disk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "fitshead.h"
#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"

void guppi_rawdisk_thread(void *args) {

    /* Attach to status shared mem area */
    int rv;
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_rawdisk_thread", 
                "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }

    /* Read in general parameters */
    struct guppi_params gp;
    pthread_cleanup_push((void *)guppi_status_unlock, &st);
    guppi_status_lock(&st);
    //guppi_read_params(st.buf, &gp);
    guppi_status_unlock(&st);
    pthread_cleanup_pop(0);

    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(1);
    if (db==NULL) {
        guppi_error("guppi_rawdisk_thread",
                "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }

    /* Loop */
    int curblock=0;
    while (1) {

        /* Wait for buf to have data */
        guppi_databuf_wait_filled(db, curblock);

        /* TODO : do stuff */

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);

    }

}
