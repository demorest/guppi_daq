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
    //struct guppi_params gp;
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

    /* Open output file */
    FILE *f = fopen("guppi_raw.dat", "w");
    if (f==NULL) { 
        guppi_error("guppi_rawdisk_thread",
                "Error opening output file.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)fclose, f);

    /* Loop */
    int curblock=0;
    char *ptr, *hend, buf[81];
    while (1) {

        /* Wait for buf to have data */
        guppi_databuf_wait_filled(db, curblock);

        /* Write header to file */
        ptr = guppi_databuf_header(db, curblock);
        hend = ksearch(ptr, "END");
        for (ptr=ptr; ptr<hend; ptr+=80) {
            memcpy(buf, ptr, 80);
            buf[79]='\0';
            fprintf(f, "%s\n", buf);
        }

        /* TODO : write data */

        /* flush output */
        fflush(f);

        /* Mark as free */
        guppi_databuf_set_free(db, curblock);

        /* Go to next block */
        curblock = (curblock + 1) % db->n_block;

        /* Check for cancel */
        pthread_testcancel();

    }

    pthread_cleanup_pop(0); /* Closes push(fclose) */

}
