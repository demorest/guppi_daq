/* test_udp_recv.c
 *
 * Simple UDP recv tester.  Similar to net_test code.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
            "Usage: guppi_udp [options] sender_hostname\n"
            "Options:\n"
            "  -p n, --port=n    Port number\n"
            "  -h, --help        This message\n"
           );
}

/* control-c handler */
int run=1;
void stop_running(int sig) { run=0; }

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
    rv = guppi_udp_init(&p);
    if (rv!=GUPPI_OK) { 
        fprintf(stderr, "Error setting up networking\n");
        exit(1);
    }
    printf("sock=%d\n", p.sock);

    unsigned long long packet_count=0, max_id=0;
    const size_t max_packet = 8192;
    char *buf = (char *)malloc(max_packet);
    struct guppi_udp_packet *packet = 
        (struct guppi_udp_packet *)buf;
    size_t psize, init_psize;
    int first=1;
    signal(SIGINT, stop_running);
    printf("Waiting for data (sock=%d).\n", p.sock);
    while (run) {
        rv = guppi_udp_wait(&p);
        if (rv==GUPPI_OK) {
            /* recv data ,etc */
            psize = recv(p.sock, buf, max_packet, 0);
            if (psize==-1) { 
                if (errno!=EAGAIN) {
                    printf("sock=%d\n", p.sock);
                    perror("recv");
                    exit(1);
                }
            } else {
                if (first) { 
                    printf("Receiving (packet_size=%d).\n", (int)psize);
                    init_psize=psize;
                    first=0;
                } else {
                    if (psize!=init_psize) { 
                        fprintf(stderr, "psize %d!=%d\n", 
                                (int)psize, (int)init_psize);
                    }
                }
                packet_count++;
                if (packet->seq_num>max_id) { max_id=packet->seq_num; }
            }
        } else if (rv==GUPPI_TIMEOUT) {
            if (first==0) { run=0; }
        } else {
            if (run) {
                perror("poll");
                guppi_udp_close(&p);
                exit(1);
            } else {
                printf("Caught SIGINT, exiting.\n");
            }
        }
    }

    printf("Received %lld packets, dropped %lld (%.3e)\n",
            packet_count, max_id-packet_count, 
            (double)(max_id-packet_count)/(double)max_id);



    guppi_udp_close(&p);
    exit(0);
}
