/* guppi_status.c
 *
 * Implementation of the status routines described 
 * in guppi_status.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>

#include "guppi_status.h"
#include "guppi_error.h"

int guppi_status_attach(struct guppi_status *s) {

    /* Look for key file, create it if not existing */
    FILE *key_file=NULL;
    key_file = fopen(GUPPI_STATUS_KEYFILE, "r");
    if (key_file==NULL) {
        key_file = fopen(GUPPI_STATUS_KEYFILE, "w");
        if (key_file==NULL) {
            /* Couldn't create keyfile */
            guppi_error("guppi_status_attach", 
                    "Couldn't read or create shmem keyfile.");
            return(GUPPI_ERR_SYS);
        }
        fclose(key_file);
    }

    /* Create shmem key */
    key_t status_shm_key = ftok(GUPPI_STATUS_KEYFILE, GUPPI_STATUS_SHMID);
    if (status_shm_key==-1) {
        guppi_error("guppi_status_attach", "ftok error");
        return(GUPPI_ERR_SYS);
    }

    /* Get shared mem id (creating it if necessary) */
    s->shmid = shmget(status_shm_key, GUPPI_STATUS_SIZE, 0644 | IPC_CREAT);
    if (s->shmid==-1) { 
        guppi_error("guppi_status_attach", "shmget error");
        return(GUPPI_ERR_SYS);
    }

    /* Now attach to the segment */
    s->buf = shmat(s->shmid, NULL, 0);
    if (s->buf == (void *)-1) {
        printf("shmid=%d\n", s->shmid);
        guppi_error("guppi_status_attach", "shmat error");
        return(GUPPI_ERR_SYS);
    }

    /* Get the locking semaphore.
     * Final arg (1) means create in unlocked state (0=locked).
     */
    s->lock = sem_open(GUPPI_STATUS_SEMID, O_CREAT, 00644, 1);
    if (s->lock==SEM_FAILED) {
        guppi_error("guppi_status_attach", "sem_open");
        return(GUPPI_ERR_SYS);
    }

    /* Init buffer if needed */
    guppi_status_chkinit(s);

    return(GUPPI_OK);
}

/* TODO: put in some (long, ~few sec) timeout */
int guppi_status_lock(struct guppi_status *s) {
    return(sem_wait(s->lock));
}

int guppi_status_unlock(struct guppi_status *s) {
    return(sem_post(s->lock));
}

/* Return pointer to END key */
char *guppi_find_end(char *buf) {
    /* Loop over 80 byte cards */
    int offs;
    char *out=NULL;
    for (offs=0; offs<GUPPI_STATUS_SIZE; offs+=GUPPI_STATUS_CARD) {
        if (strncmp(&buf[offs], "END", 3)==0) { out=&buf[offs]; break; }
    }
    return(out);
}

/* So far, just checks for existence of "END" in the proper spot */
void guppi_status_chkinit(struct guppi_status *s) {

    /* Lock */
    guppi_status_lock(s);

    /* If no END, clear it out */
    if (guppi_find_end(s->buf)==NULL) {
        /* Zero bufer */
        memset(s->buf, 0, GUPPI_STATUS_SIZE);
        /* Fill first card w/ spaces */
        memset(s->buf, ' ', GUPPI_STATUS_CARD);
        /* add END */
        strncpy(s->buf, "END", 3);
    }

    /* Unlock */
    guppi_status_unlock(s);
}