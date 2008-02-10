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
#include <fitsio2.h>

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

/* Helper func to find location of keyword in buffer */
char *guppi_find_key(char *buf, const char *key) {
    char *out=NULL, keytmp[80];
    int offs=0, match, exact, len, status=0;
    for (offs=0; offs<GUPPI_STATUS_SIZE; offs+=GUPPI_STATUS_CARD) {
        fits_test_record(&buf[offs], &status);
        if (status) { break; }
        fits_get_keyname(&buf[offs], keytmp, &len, &status);
        fits_compare_str((char *)key, keytmp, CASEINSEN, &match, &exact);
        if (match==TRUE) {
            out = &buf[offs];
            break;
        }
        fits_compare_str("END", keytmp, CASEINSEN, &match, &exact);
        if (match==TRUE) { break; }
    }
    return(out);
}

int guppi_get_param(struct guppi_status *s, int datatype, const char *key,
        void *value, int lock) {

    /* Check that input keyname is legal */
    int status=0;
    fits_test_keyword((char*)key, &status);
    if (status) { 
        guppi_error("guppi_get_param", "Illegal key name");
        return(GUPPI_ERR_PARAM);
    }

    /* Lock buffer for reads */
    if (lock==GUPPI_LOCK) guppi_status_lock(s);

    /* Look for key */
    char *ptr;
    char valtmp[80];
    valtmp[0]='\0';
    ptr = guppi_find_key(s->buf, key);
    if (ptr==NULL) { return(GUPPI_ERR_KEY); }
    fits_parse_value(ptr, valtmp, NULL, &status);

    /* Unlock */
    if (lock==GUPPI_LOCK) guppi_status_unlock(s);

    /* If didn't get val, return error */
    if (valtmp[0]=='\0') { return(GUPPI_ERR_KEY); }

    /* Parse val into appropriate data type */
    /* Use fitsio conversion, or stdlib ones? */
    char dtype;
    long long inttmp;
    double flttmp;
    if (datatype==TSTRING) {
        /* If string was requested, we're done */
        strncpy((char *)value, valtmp, 80);
        return(GUPPI_OK);
    }

    fits_get_keytype(valtmp, &dtype, &status);
    if ((dtype=='L') || (dtype=='I')) {

        /* Int types */
        inttmp = atoll(valtmp);

        switch (datatype) {
            case TBYTE:
                *((unsigned char *)value) = inttmp;
                break;
            case TSHORT:
                *((short *)value) = inttmp;
                break;
            case TLONG:
                *((long *)value) = inttmp;
                break;
            case TINT:
                *((int *)value) = inttmp;
                break;
            case TUINT:
                *((unsigned int *)value) = inttmp;
                break;
            case TUSHORT:
                *((unsigned short *)value) = inttmp;
                break;
            case TULONG:
                *((unsigned long *)value) = inttmp;
                break;
            case TLONGLONG:
                *((long long *)value) = inttmp;
                break;
            case TFLOAT:
                *((float *)value) = inttmp;
                break;
            case TDOUBLE:
                *((double *)value) = inttmp;
                break;
            default:
                return(GUPPI_ERR_PARAM);
                break;
        }

    } else if (dtype=='F') {

        /* Floating point types */
        flttmp = atof(valtmp);

        switch (datatype) {
            case TBYTE:
                *((unsigned char *)value) = flttmp;
                break;
            case TSHORT:
                *((short *)value) = flttmp;
                break;
            case TLONG:
                *((long *)value) = flttmp;
                break;
            case TINT:
                *((int *)value) = flttmp;
                break;
            case TUINT:
                *((unsigned int *)value) = flttmp;
                break;
            case TUSHORT:
                *((unsigned short *)value) = flttmp;
                break;
            case TULONG:
                *((unsigned long *)value) = flttmp;
                break;
            case TLONGLONG:
                *((long long *)value) = flttmp;
                break;
            case TFLOAT:
                *((float *)value) = flttmp;
                break;
            case TDOUBLE:
                *((double *)value) = flttmp;
                break;
            default:
                return(GUPPI_ERR_PARAM);
                break;
        }

    } else {
        /* Unrecognized, complex, string */
        return(GUPPI_ERR_KEY);
    }

    return(GUPPI_OK);
}

int guppi_set_param(struct guppi_status *s, int datatype, const char *key,
        const void *value, int lock) {

    /* Check that input keyname is legal */
    int status=0;
    fits_test_keyword((char*)key, &status);
    if (status) { 
        guppi_error("guppi_get_param", "Illegal key name");
        return(GUPPI_ERR_PARAM);
    }

    /* Convert value to an appropriate string */
    char valstr[69], dtype='\0';
    long long inttmp;
    double dbltmp;
    int ndigit=-7;
    switch (datatype) {
        case TSTRING:
            strncpy(valstr, (char *)value, 68);
            dtype = 'C';
            break;
        case TBYTE:
            inttmp = *(unsigned char *)value;
            dtype = 'I';
            break;
        case TINT:
            inttmp = *(int *)value;
            dtype = 'I';
            break;
        case TUINT:
            inttmp = *(unsigned int *)value;
            dtype = 'I';
            break;
        case TLONGLONG:
            inttmp = *(long long *)value;
            dtype = 'I';
            break;
        case TFLOAT:
            dbltmp = *(float *)value;
            dtype = 'F';
            ndigit = -7;
            break;
        case TDOUBLE:
            dbltmp = *(double *)value;
            dtype = 'F';
            ndigit = -15;
            break;
        default:
            return(GUPPI_ERR_PARAM);
            break;
    }
    if (dtype=='I') sprintf(valstr, "%lld", inttmp);
    else if (dtype=='F') sprintf(valstr, "%.*G", ndigit, dbltmp);

    /* Lock  */
    if (lock==GUPPI_LOCK) guppi_status_lock(s);

    /* Look for key */
    char *ptr;
    ptr = guppi_find_key(s->buf, key);
    if (ptr==NULL) { 
        /* Key doesn't exist, move END up to make space */
        ptr = guppi_find_end(s->buf);
        memset(&ptr[GUPPI_STATUS_CARD], ' ', GUPPI_STATUS_CARD);
        ptr[2*GUPPI_STATUS_CARD] = '\0';
        strncpy(&ptr[GUPPI_STATUS_CARD], "END", 3);
    } 

    /* Fill in updated card */
    int i, len;
    char card[81];
    ffmkky((char *)key, valstr, NULL, card, &status);
    len = strlen(card);
    for (i=len; i<GUPPI_STATUS_CARD; i++) { card[i]=' '; }
    card[80] = '\0';
    strncpy(ptr, card, GUPPI_STATUS_CARD);

    /* Unlock  */
    if (lock==GUPPI_LOCK) guppi_status_unlock(s);

    return(GUPPI_OK);

}
