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
#include <time.h>

#include "fitshead.h"
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

// Index of this instance, if multiple copies are running
int daq_idx=0;

/* Override "usual" SIGINT stuff */
int srv_run=1;
void srv_cc(int sig) { printf("guppi_daq_server: srv_cc\n"); fflush(stdout);srv_run=0; run=0; }
void srv_quit(int sig) { printf("guppi_daq_server: srv_quit\n"); fflush(stdout); srv_run=0; }

/* Thread declarations */
void *guppi_net_thread(void *args);
void *guppi_vdif_thread(void *args);
void *guppi_fold_thread(void *args);
void *guppi_psrfits_thread(void *args);
void *guppi_null_thread(void *args);

/* Useful thread functions */

int check_thread_exit(struct guppi_thread_args *args, int nthread) {
    int i, rv=0;
    for (i=0; i<nthread; i++) 
        rv += args[i].finished;
    return(rv);
}

void init_search_mode(struct guppi_thread_args *args, int *nthread) {
    guppi_thread_args_init(&args[0]); // net
    guppi_thread_args_init(&args[1]); // disk
    args[0].daq_idx = daq_idx;
    args[1].daq_idx = daq_idx;
    args[0].output_buffer = 10*daq_idx+1;
    args[1].input_buffer = args[0].output_buffer;
    *nthread = 2;
}

void init_fold_mode(struct guppi_thread_args *args, int *nthread) {
    guppi_thread_args_init(&args[0]); // net
    guppi_thread_args_init(&args[1]); // fold
    guppi_thread_args_init(&args[2]); // disk
    args[0].daq_idx = daq_idx;
    args[1].daq_idx = daq_idx;
    args[2].daq_idx = daq_idx;
    args[0].output_buffer = 10*daq_idx+1;
    args[1].input_buffer = args[0].output_buffer;
    args[1].output_buffer = 10*daq_idx+2;
    args[2].input_buffer = args[1].output_buffer;
    *nthread = 3;
}

void init_monitor_mode(struct guppi_thread_args *args, int *nthread) {
    guppi_thread_args_init(&args[0]); // net
    guppi_thread_args_init(&args[1]); // null
    args[0].daq_idx = daq_idx;
    args[1].daq_idx = daq_idx;
    args[0].output_buffer = 10*daq_idx+1;
    args[1].input_buffer = args[0].output_buffer;
    *nthread = 2;
}

void init_vdif_mode(struct guppi_thread_args *args, int *nthread) {
    guppi_thread_args_init(&args[0]);
    args[0].daq_idx = daq_idx;
    args[0].output_buffer = 10*daq_idx+1;
    *nthread = 1;
}

void start_search_mode(struct guppi_thread_args *args, pthread_t *ids) {
    // TODO error checking...
    int rv;
    rv = pthread_create(&ids[0], NULL, guppi_net_thread, (void*)&args[0]);
    rv = pthread_create(&ids[1], NULL, guppi_psrfits_thread, (void*)&args[1]);
}

void start_fold_mode(struct guppi_thread_args *args, pthread_t *ids) {
    // TODO error checking...
    int rv;
    rv = pthread_create(&ids[0], NULL, guppi_net_thread, (void*)&args[0]);
    rv = pthread_create(&ids[1], NULL, guppi_fold_thread, (void*)&args[1]);
    rv = pthread_create(&ids[2], NULL, guppi_psrfits_thread, (void*)&args[2]);
}

void start_monitor_mode(struct guppi_thread_args *args, pthread_t *ids) {
    // TODO error checking...
    int rv;
    //rv = pthread_create(&ids[0], NULL, guppi_net_thread, (void*)&args[0]);
    rv = pthread_create(&ids[0], NULL, guppi_vdif_thread, (void*)&args[0]);
    rv = pthread_create(&ids[1], NULL, guppi_null_thread, (void*)&args[1]);
}

void start_vdif_mode(struct guppi_thread_args *args, pthread_t *ids) {
    int rv;
    rv = pthread_create(&ids[0], NULL, guppi_vdif_thread, (void*)&args[0]);
}

void stop_threads(struct guppi_thread_args *args, pthread_t *ids,
        unsigned nthread) {
    unsigned i;
    for (i=0; i<nthread; i++) pthread_cancel(ids[i]);
    for (i=0; i<nthread; i++) pthread_kill(ids[i], SIGINT);
    for (i=0; i<nthread; i++) pthread_join(ids[i], NULL);
    for (i=0; i<nthread; i++) guppi_thread_args_destroy(&args[i]);
    for (i=0; i<nthread; i++) ids[i] = 0;
}

int main(int argc, char *argv[]) {

    static struct option long_opts[] = {
        {"help", 0, NULL, 'h'},
        {"idx",  1, NULL, 'i'},
        {0,0,0,0}
    };
    int opt, opti;
    while ((opt=getopt_long(argc,argv,"hi:",long_opts,&opti))!=-1) {
        switch (opt) {
            default:
            case 'h':
                usage();
                exit(0);
                break;
            case 'i':
                daq_idx = atoi(optarg);
                break;
        }
    }

    /* create FIFO */
    int ierr;
    struct stat daq_buf;

    errno = 0;

    char guppi_daq_control[256];
    sprintf(guppi_daq_control, "%s_%d", GUPPI_DAQ_CONTROL, daq_idx);

    ierr = stat(guppi_daq_control, &daq_buf);
    if ( ierr == 0 ) /* FIFO exists.  Can we get rid of it? */
    {
#if 0 
        /* Hope that the following does the sensible thing */
        /* for a FIFO.                                     */
        ierr = unlink(guppi_daq_control);
        if ( ierr != 0)
        {
            perror(guppi_daq_control);
            exit(errno);
        }
#endif
    }
    else if ( errno == EACCES ) /* Can't even get to the directory! */
    {
        perror(guppi_daq_control);
        exit(errno);
    }
    else
    {

        ierr = mkfifo(guppi_daq_control, O_CREAT | O_NONBLOCK | O_RDWR );
        if ( ierr != 0 )
        {
            perror(guppi_daq_control);
            exit(errno);
        }

        ierr = chmod(guppi_daq_control, 00666);
        if ( ierr != 0 )
        {
            perror(guppi_daq_control);
            exit(errno);
        }

    }

    /* Open command FIFO for read */
#define MAX_CMD_LEN 1024
    char cmd[MAX_CMD_LEN];
    int command_fifo;
    command_fifo = open(guppi_daq_control, O_RDONLY | O_NONBLOCK);
    if (command_fifo<0) {
        fprintf(stderr, "guppi_daq_server: Error opening control fifo\n");
        perror("open");
        exit(1);
    }

    /* Attach to shared memory buffers */
    struct guppi_status stat;
    struct guppi_databuf *dbuf_net=NULL, *dbuf_fold=NULL;
    stat.idx = daq_idx;
    int rv = guppi_status_attach(&stat);
    const int netbuf_id = 10*daq_idx+1;
    // XXX we don't need a foldbuf at VLA...
    //const int foldbuf_id = 10*daq_idx+2;
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to guppi_status\n");
        exit(1);
    }
    dbuf_net = guppi_databuf_attach(netbuf_id);
    if (dbuf_net==NULL) {
        dbuf_net = guppi_databuf_create(8,128*1024*1024,netbuf_id);
        if (dbuf_net==NULL) {
            fprintf(stderr, "Error creating guppi_databuf\n");
            exit(1);
        }
    }
    guppi_databuf_clear(dbuf_net);
    //dbuf_fold = guppi_databuf_attach(foldbuf_id);
    //if (dbuf_fold==NULL) {
    //    fprintf(stderr, "Error connecting to guppi_databuf\n");
    //    exit(1);
    //}
    //guppi_databuf_clear(dbuf_fold);

    /* Thread setup */
#define MAX_THREAD 8
    int i;
    int nthread_cur = 0;
    struct guppi_thread_args args[MAX_THREAD];
    pthread_t thread_id[MAX_THREAD];
    for (i=0; i<MAX_THREAD; i++) thread_id[i] = 0;

    /* Print start time for logs */
    time_t curtime = time(NULL);
    char tmp[256];
    printf("\nguppi_daq_server started at %s", ctime_r(&curtime,tmp));
    fflush(stdout);

    /* hmm.. keep this old signal stuff?? */
    run=1;
    srv_run=1;
    signal(SIGINT, srv_cc);
    signal(SIGTERM, srv_quit);

    /* Loop over recv'd commands, process them */
    int cmd_wait=1;
    while (cmd_wait && srv_run) {

        // Check to see if threads have exited, if so, stop them
        if (check_thread_exit(args, nthread_cur)) {
            run = 0;
            stop_threads(args, thread_id, nthread_cur);
            nthread_cur = 0;
        }

        // Heartbeat, status update
        time_t curtime;
        char timestr[32];
        char *ctmp;
        time(&curtime);
        ctime_r(&curtime, timestr);
        ctmp = strchr(timestr, '\n');
        if (ctmp!=NULL) { *ctmp = '\0'; } else { timestr[0]='\0'; }
        guppi_status_lock(&stat);
        hputs(stat.buf, "DAQPULSE", timestr);
        hputs(stat.buf, "DAQSTATE", nthread_cur==0 ? "stopped" : "running");
        guppi_status_unlock(&stat);

        // Flush any status/error/etc for logfiles
        fflush(stdout);
        fflush(stderr);

        // Wait for data on fifo
        struct pollfd pfd;
        pfd.fd = command_fifo;
        pfd.events = POLLIN;
        rv = poll(&pfd, 1, 1000);
        if (rv==0) { continue; }
        else if (rv<0) {
            if (errno!=EINTR) perror("poll");
            continue;
        }

        // If we got POLLHUP, it means the other side closed its
        // connection.  Close and reopen the FIFO to clear this
        // condition.  Is there a better/recommended way to do this?
        if (pfd.revents==POLLHUP) { 
            close(command_fifo);
            command_fifo = open(guppi_daq_control, O_RDONLY | O_NONBLOCK);
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
            printf("Exit\n");
            run = 0;
            stop_threads(args, thread_id, nthread_cur);
            cmd_wait=0;
            continue;
        } 
        
        else if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0 ||
                strncasecmp(cmd,"MONITOR",MAX_CMD_LEN)==0) {
            // Start observations
            // TODO : decide how to behave if observations are running
            printf("Start observations\n");

            if (nthread_cur>0) {
                printf("  observations already running!\n");
            } else {

                // Figure out which mode to start
                char obs_mode[32];
                if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0) {
                    guppi_status_lock(&stat);
                    guppi_read_obs_mode(stat.buf, obs_mode);
                    guppi_status_unlock(&stat);
                } else {
                    strncpy(obs_mode, cmd, 32);
                }
                printf("  obs_mode = %s\n", obs_mode);

                // Clear out data bufs
                guppi_databuf_clear(dbuf_net);
                //guppi_databuf_clear(dbuf_fold);

                // Do it
                run = 1;
                if (strncasecmp(obs_mode, "SEARCH", 7)==0) {
                    init_search_mode(args, &nthread_cur);
                    start_search_mode(args, thread_id);
                } else if (strncasecmp(obs_mode, "FOLD", 5)==0) {
                    init_fold_mode(args, &nthread_cur);
                    start_fold_mode(args, thread_id);
                } else if (strncasecmp(obs_mode, "CAL", 4)==0) {
                    init_fold_mode(args, &nthread_cur);
                    start_fold_mode(args, thread_id);
                } else if (strncasecmp(obs_mode, "MONITOR", 8)==0) {
                    init_monitor_mode(args, &nthread_cur);
                    start_monitor_mode(args, thread_id);
                } else if (strncasecmp(obs_mode, "VDIF", 5)==0) {
                    init_vdif_mode(args, &nthread_cur);
                    start_vdif_mode(args, thread_id);
                } else {
                    printf("  unrecognized obs_mode!\n");
                }

            }

        } 
        
        else if (strncasecmp(cmd,"STOP",MAX_CMD_LEN)==0) {
            // Stop observations
            printf("Stop observations\n");
            run = 0;
            stop_threads(args, thread_id, nthread_cur);
            nthread_cur = 0;
            guppi_databuf_clear(dbuf_net);
        } 
        
        else {
            // Unknown command
            printf("Unrecognized command '%s'\n", cmd);
        }
    }

    /* Stop any running threads */
    run = 0;
    stop_threads(args, thread_id, nthread_cur);

    if (command_fifo>0) close(command_fifo);

    guppi_status_lock(&stat);
    hputs(stat.buf, "DAQSTATE", "exiting");
    guppi_status_unlock(&stat);

    curtime = time(NULL);
    printf("guppi_daq_server exiting cleanly at %s\n", ctime_r(&curtime,tmp));

    fflush(stdout);
    fflush(stderr);

    /* remove FIFO */
    ierr = unlink(guppi_daq_control);

    exit(0);
}
