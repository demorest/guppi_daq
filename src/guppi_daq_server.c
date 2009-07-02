/* guppi_daq_server.c
 *
 * Persistent process that will await commands on a FIFO
 * and spawn datataking threads as appropriate.  Meant for
 * communication w/ guppi controller, etc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <errno.h>

#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"
#include "guppi_params.h"

#include "guppi_thread_main.h"

#define GUPPI_DAQ_CONTROL "/tmp/guppi_daq_control"

void usage() {
    fprintf(stderr,
            "Usage: guppi_daq_server [options]\n"
            "Options:\n"
            "  -h, --help        This message\n"
           );
}

/* Thread declarations */
void *guppi_net_thread(void *args);
void *guppi_fold_thread(void *args);
void *guppi_psrfits_thread(void *args);
void *guppi_null_thread(void *args);

int main(int argc, char *argv[]) {

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {0,0,0,0}
    };
    int use_null_thread = 0;
    int opt, opti;
    while ((opt=getopt_long(argc,argv,"hn",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'n':
                use_null_thread = 1;
                break;
            default:
            case 'h':
                usage();
                exit(0);
                break;
        }
    }

    /* Open command FIFO for read */
#define MAX_CMD_LEN 1024
    char cmd[MAX_CMD_LEN];
    int command_fifo;
    command_fifo = open(GUPPI_DAQ_CONTROL, O_RDONLY | O_NONBLOCK);
    if (command_fifo<0) {
        fprintf(stderr, "guppi_daq_server: Error opening control fifo\n");
        perror("open");
        exit(1);
    }

    /* Attach to shared memory buffers */
    struct guppi_status stat;
    struct guppi_databuf *dbuf_net=NULL, *dbuf_fold=NULL;
    int rv = guppi_status_attach(&stat);
    const int netbuf_id = 1;
    const int foldbuf_id = 2;
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to guppi_status\n");
        exit(1);
    }
    dbuf_net = guppi_databuf_attach(netbuf_id);
    if (dbuf_net==NULL) {
        fprintf(stderr, "Error connecting to guppi_databuf\n");
        exit(1);
    }
    guppi_databuf_clear(dbuf_net);
    dbuf_fold = guppi_databuf_attach(foldbuf_id);
    if (dbuf_fold==NULL) {
        fprintf(stderr, "Error connecting to guppi_databuf\n");
        exit(1);
    }
    guppi_databuf_clear(dbuf_fold);

    /* Thread setup */
#define MAX_THREAD 8
    struct guppi_thread_args args[MAX_THREAD];
    pthread_t thread_id[MAX_THREAD];
    char thread_type[MAX_THREAD][256];

    /* hmm.. keep this old signal stuff?? */
    run=1;
    signal(SIGINT, cc);

    /* Loop over recv'd commands, process them */
    int cmd_wait=1;
    while (cmd_wait && run) {

        // Wait for data on fifo
        struct pollfd pfd;
        pfd.fd = command_fifo;
        pfd.events = POLLIN;
        rv = poll(&pfd, 1, 1000);
        if (rv==0) { continue; }
        else if (rv<0) {
            perror("poll");
            continue;
        }

        // If we got POLLHUP, it means the other side closed its
        // connection.  Close and reopen the FIFO to clear this
        // condition.  Is there a better/recommended way to do this?
        if (pfd.revents==POLLHUP) { 
            close(command_fifo);
            command_fifo = open(GUPPI_DAQ_CONTROL, O_RDONLY | O_NONBLOCK);
            if (command_fifo<0) {
                fprintf(stderr, 
                        "guppi_daq_server: Error opening control fifo\n");
                perror("open");
                break;
            }
            continue;
        }

        // Read the command
        memset(cmd, 0, MAX_CMD_LEN);
        rv = read(command_fifo, cmd, MAX_CMD_LEN-1);
        if (rv==0) { continue; }
        else if (rv<0) {
            if (errno==EAGAIN) { continue; }
            else { perror("read");  continue; }
        } 

        // Truncate at newline
        // TODO: allow multiple commands in one read?
        char *ptr = strchr(cmd, '\n');
        if (ptr!=NULL) *ptr='\0'; 

        // Process the command 
        if (strncasecmp(cmd,"QUIT",MAX_CMD_LEN)==0) {
            // Exit program
            // TODO : decide how to behave if observations are running
            cmd_wait=0;
            continue;
        } 
        
        else if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0) {
            // Start observations
            // TODO : decide how to behave if observations are running
            printf("Start observations\n");

            // Figure out which mode to start
            char obs_mode[32];
            guppi_status_lock(&stat);
            guppi_read_obs_mode(stat.buf, obs_mode);
            guppi_status_unlock(&stat);

            // Do it
        } 
        
        else if (strncasecmp(cmd,"STOP",MAX_CMD_LEN)==0) {
            // Stop observations
            printf("Stop observations\n");
        } 
        
        else if (strncasecmp(cmd,"MONITOR",MAX_CMD_LEN)==0) {
            // Start monitor mode
            // TODO : decide how to behave if observations are running
            printf("Start monitor mode\n");
        } 
        
        else {
            // Unknown command
            printf("Unrecognized command '%s'\n", cmd);
        }
    }

    if (command_fifo>0) close(command_fifo);
    exit(0);
}
