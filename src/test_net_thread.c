/* test_net_thread.c
 *
 * Test run net thread.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

/* Control-C handler */
int run=1;
void cc(int sig) { run=0; }

/* net thread */
void *guppi_net_thread(void *_up);
void *guppi_rawdisk_thread(void *args);

int main(int argc, char *argv[]) {

    struct guppi_udp_params p;

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {0,0,0,0}
    };
    int opt, opti;
    p.port = 5000;
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

    /* Init shared mem */

    /* Launch net thread */
    pthread_t net_thread_id;
    int rv = pthread_create(&net_thread_id, NULL, guppi_net_thread,
            (void *)&p);
    if (rv) { 
        fprintf(stderr, "Error creating net thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Launch raw disk thread */
    pthread_t disk_thread_id;
    rv = pthread_create(&disk_thread_id, NULL, guppi_rawdisk_thread, NULL);
    if (rv) { 
        fprintf(stderr, "Error creating net thread.\n");
        perror("pthread_create");
        pthread_cancel(net_thread_id);
        exit(1);
    }

    /* Wait for end */
    run=1;
    signal(SIGINT, cc);
    while (run) { sleep(1); }
    pthread_cancel(disk_thread_id);
    pthread_cancel(net_thread_id);

    exit(0);
}
