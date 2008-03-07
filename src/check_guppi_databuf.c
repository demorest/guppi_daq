/* check_guppi_databuf.c
 *
 * Basic prog to test dstabuf shared mem routines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "fitshead.h"
#include "guppi_error.h"
#include "guppi_databuf.h"

void usage() { 
    fprintf(stderr, "check_guppi_databuf\n");
}

int main(int argc, char *argv[]) {

    int rv;

    /* Loop over cmd line to fill in params */
    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"quiet",  0, NULL, 'q'},
        {"create", 0, NULL, 'c'},
        {"id",     1, NULL, 'i'},
        {0,0,0,0}
    };
    int opt,opti;
    int quiet=0;
    int create=0;
    int db_id=1;
    while ((opt=getopt_long(argc,argv,"hqci",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'c':
                create=1;
                break;
            case 'q':
                quiet=1;
                break;
            case 'i':
                db_id = atoi(optarg);
                break;
            case 'h':
            default:
                usage();
                exit(0);
                break;
        }
    }

    /* Create mem if asked, otherwise attach */
    struct guppi_databuf *db=NULL;
    if (create) { 
        db = guppi_databuf_create(8, 16*1024*1024, 2880*8, db_id);
        if (db==NULL) {
            fprintf(stderr, "Error creating databuf (may already exist).\n");
            exit(1);
        }
    } else {
        db = guppi_databuf_attach(db_id);
        if (db==NULL) { 
            fprintf(stderr, 
                    "Error attaching to databuf (may not exist).\n");
            exit(1);
        }
    }

    /* Print basic info */
    printf("databuf %d stats:\n", db_id);
    printf("  shmid=%d\n", db->shmid);
    printf("  semid=%d\n", db->semid);
    printf("  n_block=%d\n", db->n_block);
    printf("  struct_size=%ld\n", db->struct_size);
    printf("  block_size=%ld\n", db->block_size);
    printf("  header_size=%ld\n\n", db->header_size);

    /* loop over blocks */
    int i;
    char buf[81];
    char *ptr, *hend;
    for (i=0; i<db->n_block; i++) {
        printf("block %d status=%d\n", i, 
                guppi_databuf_block_status(db, i));
        hend = ksearch(db->header[i], "END");
        if (hend==NULL) {
            printf("header not initialized\n");
        } else {
            printf("header:\n");
            for (ptr=db->header[i]; ptr<hend; ptr+=80) {
                strncpy(buf, ptr, 80);
                buf[79]='\0';
                printf("%s\n", buf);
            }
        }

    }

    exit(0);
}
