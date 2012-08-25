/* check_guppi_status.c
 *
 * Basic prog to test status shared mem routines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "fitshead.h"
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

    guppi_status_unlock(&s);

    exit(0);
}
