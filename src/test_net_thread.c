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
            "Usage: test_net_thread [options] [sender_hostname]\n"
            "Default hostname: bee2_10\n"
            "Options:\n"
            "  -h, --help        This message\n"
            "  -p n, --port=n    Port number (default 50000)\n"
            "  -d, --disk        Write raw data to disk (default no)\n"
            "  -s n, --size=n    Set packet size in bytes (8208)\n"
           );
}

/* Thread declarations */
void *guppi_net_thread(void *_up);
void *guppi_rawdisk_thread(void *args);
void *guppi_null_thread(void *args);

int main(int argc, char *argv[]) {

    struct guppi_udp_params p;

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"port",   1, NULL, 'p'},
        {"disk",   0, NULL, 'd'},
        {"size",   1, NULL, 's'},
        {0,0,0,0}
    };
    int opt, opti;
    int disk=0;
    p.port = 50000;
    p.packet_size = 8208; /* Expected 8k + 8 byte seq num + 8 byte flags */
    while ((opt=getopt_long(argc,argv,"hp:ds:",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'p':
                p.port = atoi(optarg);
                break;
            case 'd':
                disk=1;
                break;
            case 's':
                p.packet_size = atoi(optarg);
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

    /* Set the databuf destination id */
    p.output_buffer = 1;

    /* Init shared mem */
    struct guppi_status stat;
    struct guppi_databuf *dbuf=NULL;
    int rv = guppi_status_attach(&stat);
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to guppi_status\n");
        exit(1);
    }
    dbuf = guppi_databuf_attach(p.output_buffer);
    /* If attach fails, first try to create the databuf */
    if (dbuf==NULL) 
        dbuf = guppi_databuf_create(24, 32*1024*1024, p.output_buffer);
    /* If that also fails, exit */
    if (dbuf==NULL) {
        fprintf(stderr, "Error connecting to guppi_databuf\n");
        exit(1);
    }
    guppi_databuf_clear(dbuf);

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

    /* Launch raw disk (or null) thread */
    struct guppi_thread_args null_args;
    null_args.input_buffer = p.output_buffer;
    pthread_t disk_thread_id;
    if (disk)
        rv = pthread_create(&disk_thread_id, NULL, guppi_rawdisk_thread, 
                (void *)&null_args);
    else
        rv = pthread_create(&disk_thread_id, NULL, guppi_null_thread, 
                (void *)&null_args);
    if (rv) { 
        fprintf(stderr, "Error creating null thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Wait for end */
    while (run) { sleep(1); }
    pthread_cancel(disk_thread_id);
    pthread_cancel(net_thread_id);
    pthread_kill(disk_thread_id,SIGINT);
    pthread_kill(net_thread_id,SIGINT);
    pthread_join(net_thread_id,NULL);
    printf("Joined net thread\n"); fflush(stdout);
    pthread_join(disk_thread_id,NULL);
    printf("Joined disk thread\n"); fflush(stdout);

    exit(0);
}
