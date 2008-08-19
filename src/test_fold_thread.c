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
#include "guppi_status.h"
#include "guppi_databuf.h"
#include "guppi_params.h"

#include "guppi_thread_main.h"

void usage() {
    fprintf(stderr,
            "Usage: test_net_thread [options] sender_hostname\n"
            "Options:\n"
            "  -p n, --port=n    Port number\n"
            "  -h, --help        This message\n"
           );
}

/* Thread declarations */
void *guppi_net_thread(void *_up);
void *guppi_fold_thread(void *args);

int main(int argc, char *argv[]) {

    struct guppi_udp_params p;

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {0,0,0,0}
    };
    int opt, opti;
    p.port = 50000;
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

    /* Default to bee2 if no hostname given */
    if (optind==argc) {
        strcpy(p.sender, "bee2_10");
    } else {
        strcpy(p.sender, argv[optind]);
    }

    /* Init udp params */
    p.packet_size = 8208; /* Expected 8k + 8 byte seq num + 8 byte flags */

    /* Data buffer ids */
    struct guppi_thread_args fold_args;
    p.output_buffer = 1;
    fold_args.input_buffer = p.output_buffer;
    fold_args.output_buffer = 2;

    /* Init shared mem */
    struct guppi_status stat;
    struct guppi_databuf *dbuf_net=NULL, *dbuf_fold=NULL;
    int rv = guppi_status_attach(&stat);
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to guppi_status\n");
        exit(1);
    }

    dbuf_net = guppi_databuf_attach(p.output_buffer);
    if (dbuf_net==NULL) {
        fprintf(stderr, "Error connecting to guppi_databuf\n");
        exit(1);
    }
    guppi_databuf_clear(dbuf_net);

    dbuf_fold = guppi_databuf_attach(fold_args.output_buffer);
    if (dbuf_fold==NULL) {
        fprintf(stderr, "Error connecting to guppi_databuf\n");
        exit(1);
    }
    guppi_databuf_clear(dbuf_fold);


    run=1;
    signal(SIGINT, cc);

    /* Launch net thread */
    pthread_t net_thread_id;
    rv = pthread_create(&net_thread_id, NULL, guppi_net_thread,
            (void *)&p);
    if (rv) { 
        fprintf(stderr, "Error creating net thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Launch raw fold thread */
    pthread_t fold_thread_id;
    rv = pthread_create(&fold_thread_id, NULL, guppi_fold_thread, 
            (void *)&fold_args);
    if (rv) { 
        fprintf(stderr, "Error creating fold thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Wait for end */
    while (run) { sleep(1); }
    pthread_cancel(fold_thread_id);
    pthread_cancel(net_thread_id);
    pthread_kill(fold_thread_id,SIGINT);
    pthread_kill(net_thread_id,SIGINT);
    pthread_join(net_thread_id,NULL);
    printf("Joined net thread\n"); fflush(stdout);
    pthread_join(fold_thread_id,NULL);
    printf("Joined fold thread\n"); fflush(stdout);

    exit(0);
}
