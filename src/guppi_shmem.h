/* guppi_shmem.h
 *
 * Defines shared mem structure for data passing.
 * Includes routines to allocate / attach to shared
 * memory.
 */
#ifndef _GUPPI_SHMEM_H
#define _GUPPI_SHMEM_H

struct guppi_shmem {
    int n_block;        /* Number of data blocks in buffer */
    size_t block_size;  /* Size of each data block (bytes) */
    char data_type[40]; /* Type of data in buffer */
    int *status;        /* Points to n_block*int status array */
    char **data;        /* n_block pointers to data blocks */
}

/* Create a new shared mem area with given params.  Returns 
 * pointer to the new area on success, or NULL on error.  Returns
 * error if an existing shmem area exists with the given shmid (or
 * if other errors occured trying to allocate it).
 */
struct guppi_shmem *guppi_shmem_create(int n_block, size_t block_size,
        int shmid);

/* Return a pointer to a existing shmem segment with given shmid.
 * Returns error if segment does not exist 
 */
struct guppi_shmem *guppi_shmem_attach(int shmid);

#endif
