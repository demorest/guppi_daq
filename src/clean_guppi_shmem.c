/* clean_guppi_shmem.c
 *
 * Mark all GUPPI shmem segs for deletion.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>

#include "guppi_status.h"
#include "guppi_databuf.h"
#include "guppi_error.h"

int main(int argc, char *argv[]) {
    int rv,ex=0;

    /* Status shared mem, force unlock first */
    int sidx=0;
    char semid[1024];
    struct guppi_status s;
    for (sidx=0; sidx<2; sidx++) {
        s.idx = sidx;
        sprintf(semid, "%s_%d", GUPPI_STATUS_SEMID, sidx);
        sem_unlink(semid);
        rv = guppi_status_attach(&s);
        if (rv!=GUPPI_OK) {
            fprintf(stderr, "Error connecting to status shared mem.\n");
            perror(NULL);
            exit(1);
        }
        rv = shmctl(s.shmid, IPC_RMID, NULL);
        if (rv==-1) {
            fprintf(stderr, "Error deleting status segment.\n");
            perror("shmctl");
            ex=1;
        }
        rv = sem_unlink(s.semid);
        if (rv==-1) {
            fprintf(stderr, "Error unlinking status semaphore.\n");
            perror("sem_unlink");
            ex=1;
        }
    }

    /* Databuf shared mem */
    struct guppi_databuf *d=NULL;
    int i=0, j=0;
    for (j=0; j<2; j++) {
        for (i=1; i<=2; i++) {
            d = guppi_databuf_attach(10*j+i); // Repeat for however many needed ..
            if (d==NULL) continue;
            if (d->semid) { 
                rv = semctl(d->semid, 0, IPC_RMID); 
                if (rv==-1) {
                    fprintf(stderr, "Error removing databuf semaphore\n");
                    perror("semctl");
                    ex=1;
                }
            }
            rv = shmctl(d->shmid, IPC_RMID, NULL);
            if (rv==-1) {
                fprintf(stderr, "Error deleting databuf segment.\n");
                perror("shmctl");
                ex=1;
            }
        }
    }

    exit(ex);
}

