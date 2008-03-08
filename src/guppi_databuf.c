/* guppi_databuf.c
 *
 * Routines for creating and accessing main data transfer
 * buffer in shared memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "guppi_databuf.h"
#include "guppi_error.h"

struct guppi_databuf *guppi_databuf_create(int n_block, size_t block_size,
        size_t header_size, int databuf_id) {

    /* Create keyfile if it doesn't exist */
    FILE *key_file=NULL;
    key_file = fopen(GUPPI_DATABUF_KEYFILE, "r");
    if (key_file==NULL) {
        key_file = fopen(GUPPI_DATABUF_KEYFILE, "w");
        if (key_file==NULL) {
            /* Couldn't create keyfile */
            guppi_error("guppi_databuf_attach", 
                    "Couldn't read or create shmem keyfile.");
            return(NULL);
        }
        fclose(key_file);
    }

    /* Create shmem key */
    key_t shm_key = ftok(GUPPI_DATABUF_KEYFILE, databuf_id);
    if (shm_key==-1) {
        guppi_error("guppi_databuf_create", "ftok error");
        return(NULL);
    }

    /* Calc databuf size */
    size_t struct_size = sizeof(struct guppi_databuf);
    struct_size = 8192 * (1 + struct_size/8192); /* round up */
    size_t databuf_size = (block_size+header_size) * n_block + struct_size;

    /* Get shared memory block, error if it already exists */
    int shmid;
    shmid = shmget(shm_key, databuf_size, 0644 | IPC_CREAT | IPC_EXCL);
    if (shmid==-1) {
        guppi_error("guppi_databuf_create", "shmget error");
        return(NULL);
    }

    /* Attach */
    struct guppi_databuf *d;
    d = shmat(shmid, NULL, 0);
    if (d==(void *)-1) {
        guppi_error("guppi_databuf_create", "shmat error");
        return(NULL);
    }

    /* Try to lock in memory */
    int rv = shmctl(shmid, SHM_LOCK, NULL);
    if (rv==-1) {
        guppi_error("guppi_databuf_create", "Error locking shared memory.");
        perror("shmctl");
    }

    /* Zero out memory */
    memset(d, 0, databuf_size);

    /* Fill params into databuf */
    int i;
    char end_key[81];
    memset(end_key, ' ', 80);
    strncpy(end_key, "END", 3);
    end_key[80]='\0';
    d->shmid = shmid;
    d->semid = 0;
    d->n_block = n_block;
    d->struct_size = struct_size;
    d->block_size = block_size;
    d->header_size = header_size;
    sprintf(d->data_type, "unknown");
    for (i=0; i<n_block; i++) { 
        memcpy(guppi_databuf_header(d,i), end_key, 80); 
    }

    /* Get semaphores set up */
    d->semid = semget(shm_key, n_block, 0644 | IPC_CREAT);
    if (d->semid==-1) { 
        guppi_error("guppi_databuf_create", "semget error");
        return(NULL);
    }

    /* Init semaphores to 0 */
    union semun arg;
    arg.array = (unsigned short *)malloc(sizeof(unsigned short)*n_block);
    memset(arg.array, 0, sizeof(unsigned short)*n_block);
    rv = semctl(d->semid, 0, SETALL, arg);
    free(arg.array);

    return(d);
}

char *guppi_databuf_header(struct guppi_databuf *d, int block_id) {
    return((char *)d + d->struct_size + block_id*d->header_size);
}

char *guppi_databuf_data(struct guppi_databuf *d, int block_id) {
    return((char *)d + d->struct_size + d->n_block*d->header_size
            + block_id*d->block_size);
}

struct guppi_databuf *guppi_databuf_attach(int databuf_id) {

    /* Get key */
    key_t shm_key = ftok(GUPPI_DATABUF_KEYFILE, databuf_id);
    if (shm_key==-1) {
        // Doesn't exist, exit quietly
        //guppi_error("guppi_databuf_attach", "ftok error");
        return(NULL);
    }

    /* Get shmid */
    int shmid;
    shmid = shmget(shm_key, 0, 0644);
    if (shmid==-1) {
        // Doesn't exist, exit quietly
        //guppi_error("guppi_databuf_create", "shmget error");
        return(NULL);
    }

    /* Attach */
    struct guppi_databuf *d;
    d = shmat(shmid, NULL, 0);
    if (d==(void *)-1) {
        guppi_error("guppi_databuf_create", "shmat error");
        return(NULL);
    }

    return(d);

}

int guppi_databuf_block_status(struct guppi_databuf *d, int block_id) {
    return(semctl(d->semid, block_id, GETVAL));
}

int guppi_databuf_wait_free(struct guppi_databuf *d, int block_id) {
    int rv;
    struct sembuf op;
    op.sem_num = block_id;
    op.sem_op = 0;
    op.sem_flg = 0;
    rv = semop(d->semid, &op, 1);
    if (rv==-1) { 
        guppi_error("guppi_databuf_wait_free", "semop error");
        return(GUPPI_ERR_SYS);
    }
    return(0);
}

int guppi_databuf_wait_filled(struct guppi_databuf *d, int block_id) {
    /* This needs to wait for the semval of the given block
     * to become > 0, but NOT immediately decrement it to 0.
     * Probably do this by giving an array of semops, since
     * (afaik) the whole array happens atomically:
     * step 1: wait for val=1 then decrement (semop=-1)
     * step 2: increment by 1 (semop=1)
     */
    int rv;
    struct sembuf op[2];
    op[0].sem_num = op[1].sem_num = block_id;
    op[0].sem_flg = op[1].sem_flg = 0;
    op[0].sem_op = -1;
    op[1].sem_op = 1;
    rv = semop(d->semid, op, 2);
    if (rv==-1) { 
        guppi_error("guppi_databuf_wait_filled", "semop error");
        return(GUPPI_ERR_SYS);
    }
    return(0);
}

int guppi_databuf_set_free(struct guppi_databuf *d, int block_id) {
    /* TODO : check that buf is filled? use NOWAIT? */
    int rv;
    struct sembuf op;
    op.sem_num = block_id;
    op.sem_op = -1;
    op.sem_flg = 0;
    rv = semop(d->semid, &op, 1);
    if (rv==-1) { 
        guppi_error("guppi_databuf_set_free", "semop error");
        return(GUPPI_ERR_SYS);
    }
    return(0);
}

int guppi_databuf_set_filled(struct guppi_databuf *d, int block_id) {
    /* TODO : check that buf is free? use NOWAIT? */
    int rv;
    struct sembuf op;
    op.sem_num = block_id;
    op.sem_op = 1;
    op.sem_flg = 0;
    rv = semop(d->semid, &op, 1);
    if (rv==-1) { 
        guppi_error("guppi_databuf_set_filled", "semop error");
        return(GUPPI_ERR_SYS);
    }
    return(0);
}
