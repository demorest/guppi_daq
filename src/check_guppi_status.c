/* check_guppi_status.c
 *
 * Basic prog to test status shared mem routines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fitsio.h>

#include "guppi_error.h"
#include "guppi_status.h"

int main(int argc, char *argv[]) {

    int rv;
    struct guppi_status s;

    rv = guppi_status_attach(&s);
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to shared mem.\n");
        perror(NULL);
        exit(1);
    }

    guppi_status_lock(&s);

    /* Loop over cmd line to fill in params */
    static struct option long_opts[] = {
        {"key",    1, NULL, 'k'},
        {"string", 1, NULL, 's'},
        {"float",  1, NULL, 'f'},
        {"double", 1, NULL, 'd'},
        {"int",    1, NULL, 'i'},
        {"quiet",  0, NULL, 'q'},
        {0,0,0,0}
    };
    int opt,opti;
    char *key=NULL;
    float flttmp;
    double dbltmp;
    int inttmp;
    int quiet=0;
    while ((opt=getopt_long(argc,argv,"k:s:f:d:i:q",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'k':
                key = optarg;
                break;
            case 's':
                if (key) 
                    guppi_set_param(&s, TSTRING, key, optarg, GUPPI_NOLOCK);
                break;
            case 'f':
                flttmp = atof(optarg);
                if (key) 
                    guppi_set_param(&s, TFLOAT, key, &flttmp, GUPPI_NOLOCK);
                break;
            case 'd':
                dbltmp = atof(optarg);
                if (key) 
                    guppi_set_param(&s, TDOUBLE, key, &dbltmp, GUPPI_NOLOCK);
                break;
            case 'i':
                inttmp = atoi(optarg);
                if (key) 
                    guppi_set_param(&s, TINT, key, &inttmp, GUPPI_NOLOCK);
                break;
            case 'q':
                quiet=1;
                break;
            default:
                break;
        }
    }

    /* If not quiet, print out buffer */
    if (!quiet) { printf(s.buf); printf("\n"); }

    guppi_status_unlock(&s);
    exit(0);
}
