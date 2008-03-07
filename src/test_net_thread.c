/* test_net_thread.c
 *
 * Test run net thread.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <errno.h>

#include "guppi_udp.h"
#include "guppi_error.h"

void usage() {
    fprintf(stderr,
            "Usage: test_net_thread [options] sender_hostname\n"
            "Options:\n"
            "  -p n, --port=n    Port number\n"
            "  -h, --help        This message\n"
           );
}

/* net thread */
void *guppi_net_thread(void *_up);

int main(int argc, char *argv[]) {

    int rv;
    struct guppi_udp_params p;

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {0,0,0,0}
    };
    int opt, opti;
    p.port = 5000;
    p.packet_size=0; /* Initially don't care */
    while ((opt=getopt_long(argc,argv,"hp:",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'p':
                p.port = atoi(optarg);
                break;
            default:
            case 'h':
                usage();
                exit(0);
                break;
        }
    }

    /* Need sender hostname */
    if (optind==argc) {
        usage();
        exit(1);
    }

    /* Init udp params */
    strcpy(p.sender, argv[optind]);
    p.packet_size = 8200; /* Expected 8k + 8 byte seq num */

    /* Launch net thread */
    pthread_t net_thread_id;
    int rv = pthread_create(&net_thread_id, NULL, guppi_net_thread,
            (void *)p);
    if (rv) { 
        fprintf(stderr, "Error creating net thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Wait for end */
    rv = pthread_join(net_thread_id, NULL);

    exit(0);
}
