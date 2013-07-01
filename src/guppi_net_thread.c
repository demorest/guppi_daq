/* guppi_net_thread.c
 *
 * Routine to read packets from network and put them
 * into shared memory blocks.
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "fitshead.h"
#include "guppi_params.h"
#include "psrfits.h"
#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"
#include "guppi_udp.h"
#include "guppi_time.h"

#define STATUS_KEY "NETSTAT"  /* Define before guppi_threads.h */
#include "guppi_threads.h"

// Read a status buffer all of the key observation paramters
extern void guppi_read_obs_params(char *buf, 
                                  struct guppi_params *g, 
                                  struct psrfits *p);

/* This thread is passed a single arg, pointer
 * to the guppi_udp_params struct.  This thread should 
 * be cancelled and restarted if any hardware params
 * change, as this potentially affects packet size, etc.
 */
void *guppi_net_thread(void *_args) {

    /* Get arguments */
    struct guppi_thread_args *args = (struct guppi_thread_args *)_args;

    /* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    //CPU_SET(2, &cpuset);
    CPU_SET(3, &cpuset);
    int rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) { 
        guppi_error("guppi_net_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }

    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, args->priority);
    if (rv<0) {
        guppi_error("guppi_net_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct guppi_status st;
    rv = guppi_status_attach(&st);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_net_thread", 
                "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)guppi_status_detach, &st);
    pthread_cleanup_push((void *)set_exit_status, &st);

    /* Init status, read info */
    guppi_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEY, "init");
    guppi_status_unlock_safe(&st);

    /* Read in general parameters */
    struct guppi_params gp;
    struct psrfits pf;
    pf.sub.dat_freqs = NULL;
    pf.sub.dat_weights = NULL;
    pf.sub.dat_offsets = NULL;
    pf.sub.dat_scales = NULL;
    char status_buf[GUPPI_STATUS_SIZE];
    guppi_status_lock_safe(&st);
    memcpy(status_buf, st.buf, GUPPI_STATUS_SIZE);
    guppi_status_unlock_safe(&st);
    guppi_read_obs_params(status_buf, &gp, &pf);
    pthread_cleanup_push((void *)guppi_free_psrfits, &pf);

    /* Read network params */
    struct guppi_udp_params up;
    guppi_read_net_params(status_buf, &up);

    /* Attach to databuf shared mem */
    struct guppi_databuf *db;
    db = guppi_databuf_attach(args->output_buffer); 
    if (db==NULL) {
        guppi_error("guppi_net_thread",
                "Error attaching to databuf shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)guppi_databuf_detach, db);

    /* Set up UDP socket */
    rv = guppi_udp_init(&up);
    if (rv!=GUPPI_OK) {
        guppi_error("guppi_net_thread",
                "Error opening UDP socket.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)guppi_udp_close, &up);

    /* Time parameters */
    int stt_imjd=0, stt_smjd=0;
    double stt_offs=0.0;

    /* See which packet format to use */
    int use_parkes_packets=0;
    int nchan=0, npol=0, acclen=0;
    if (strncmp(up.packet_format, "PARKES", 6)==0) { use_parkes_packets=1; }
    if (use_parkes_packets) {
        printf("guppi_net_thread: Using Parkes UDP packet format.\n");
        nchan = pf.hdr.nchan;
        npol = pf.hdr.npol;
        acclen = gp.decimation_factor;
        if (acclen==0) { 
            guppi_error("guppi_net_thread", 
                    "ACC_LEN must be set to use Parkes format");
            pthread_exit(NULL);
        }
#if 0 
        if (npol!=2) {
            char msg[256];
            sprintf(msg, 
                    "Parkes packet format not implemented for npol=%d", 
                    npol);
            guppi_error("guppi_net_thread", msg);
            pthread_exit(NULL);
        }
#endif
        /* TODO : check packet_size, npol, chan for consistency? */
    }

    /* Figure out size of data in each packet, number of packets
     * per block, etc.
     * TODO : Figure out how/if to deal with packet size changing.
     */
    int block_size;
    struct guppi_udp_packet p;
    size_t packet_data_size = guppi_udp_packet_datasize(up.packet_size); 
    if (use_parkes_packets) 
        packet_data_size = parkes_udp_packet_datasize(up.packet_size);
    unsigned packets_per_block; 
    if (hgeti4(status_buf, "BLOCSIZE", &block_size)==0) {
            block_size = db->block_size;
            hputi4(status_buf, "BLOCSIZE", block_size);
    } else {
        if (block_size > db->block_size) {
            guppi_error("guppi_net_thread", "BLOCSIZE > databuf block_size");
            block_size = db->block_size;
            hputi4(status_buf, "BLOCSIZE", block_size);
        }
    }
    packets_per_block = block_size / packet_data_size;

    /* Counters */
    unsigned long long npacket_total=0, npacket_block=0;
    unsigned long long ndropped_total=0, ndropped_block=0;
    unsigned long long nbogus_total=0, nbogus_block=0;
    unsigned long long curblock_seq_num=0, nextblock_seq_num=0;
    unsigned long long seq_num, last_seq_num=2048;
    int curblock=-1;
    char *curheader=NULL, *curdata=NULL;
    unsigned block_packet_idx=0, last_block_packet_idx=0;
    double drop_frac_avg=0.0;
    const double drop_lpf = 0.25;

    /* Main loop */
    unsigned i, force_new_block=0, waiting=-1;
    char *dataptr;
    long long seq_num_diff;
    signal(SIGINT,cc);
    while (run) {

        /* Wait for data */
        rv = guppi_udp_wait(&up);
        if (rv!=GUPPI_OK) {
            if (rv==GUPPI_TIMEOUT) { 
                /* Set "waiting" flag */
                if (waiting!=1) {
                    guppi_status_lock_safe(&st);
                    hputs(st.buf, STATUS_KEY, "waiting");
                    guppi_status_unlock_safe(&st);
                    waiting=1;
                }
                continue; 
            } else {
                guppi_error("guppi_net_thread", 
                        "guppi_udp_wait returned error");
                perror("guppi_udp_wait");
                pthread_exit(NULL);
            }
        }

        /* Read packet */
        rv = guppi_udp_recv(&up, &p);
        if (rv!=GUPPI_OK) {
            if (rv==GUPPI_ERR_PACKET) {
                /* Unexpected packet size, ignore? */
                nbogus_total++;
                nbogus_block++;
                continue; 
            } else {
                guppi_error("guppi_net_thread", 
                        "guppi_udp_recv returned error");
                perror("guppi_udp_recv");
                pthread_exit(NULL);
            }
        }

        /* Update status if needed */
        if (waiting!=0) {
            guppi_status_lock_safe(&st);
            hputs(st.buf, STATUS_KEY, "receiving");
            guppi_status_unlock_safe(&st);
            waiting=0;
        }

        /* Convert packet format if needed */
        if (use_parkes_packets) 
            parkes_to_guppi(&p, acclen, npol, nchan);

        /* Check seq num diff */
        seq_num = guppi_udp_packet_seq_num(&p);
        seq_num_diff = seq_num - last_seq_num;
        if (seq_num_diff<=0 && curblock>=0) { 
            if (seq_num_diff<-128) { 
                printf("guppi_net_thread:  Packet sequence number reset\n");
                force_new_block=1; 
            } else if (seq_num_diff==0) {
                char msg[256];
                sprintf(msg, "Received duplicate packet (seq_num=%lld)", 
                        seq_num);
                guppi_warn("guppi_net_thread", msg);
            }
            else  { 
                /* No going backwards */
                char msg[256];
                sprintf(msg, 
                        "Received out-of-order packet (seq_num=%lld, diff=%lld)",
                        seq_num, seq_num_diff);
                guppi_warn("guppi_net_thread", msg);
                continue; 
            } 
        } else { force_new_block=0; }

        /* Determine if we go to next block */
        if ((seq_num>=nextblock_seq_num) || force_new_block) {

            if (curblock>=0) { 
                /* Close out current block */
                hputi4(curheader, "PKTIDX", curblock_seq_num);
                hputi4(curheader, "PKTSIZE", packet_data_size);
                hputi4(curheader, "NPKT", npacket_block);
                hputi4(curheader, "NDROP", ndropped_block);
                guppi_databuf_set_filled(db, curblock);
            }

            if (npacket_block) { 
                drop_frac_avg = (1.0-drop_lpf)*drop_frac_avg 
                    + drop_lpf*(double)ndropped_block/(double)npacket_block;
            }

            /* Put drop stats in general status area */
            guppi_status_lock_safe(&st);
            hputr8(st.buf, "DROPAVG", drop_frac_avg);
            hputr8(st.buf, "DROPTOT", 
                    npacket_total ? 
                    (double)ndropped_total/(double)npacket_total 
                    : 0.0);
            hputr8(st.buf, "DROPBLK", 
                    npacket_block ? 
                    (double)ndropped_block/(double)npacket_block 
                    : 0.0);
            guppi_status_unlock_safe(&st);

            /* Reset block counters */
            npacket_block=0;
            ndropped_block=0;
            nbogus_block=0;

            /* If new obs started, reset total counters, get start
             * time.  Start time is rounded to nearest integer
             * second, with warning if we're off that by more
             * than 100ms. */
            if (force_new_block) {
                npacket_total=0;
                ndropped_total=0;
                nbogus_total=0;
                get_current_mjd(&stt_imjd, &stt_smjd, &stt_offs);
                if (stt_offs>0.5) { stt_smjd+=1; stt_offs-=1.0; }
                if (fabs(stt_offs)>0.1) { 
                    char msg[256];
                    sprintf(msg, 
                            "Second fraction = %3.1f ms > +/-100 ms",
                            stt_offs*1e3);
                    guppi_warn("guppi_net_thread", msg);
                }
                stt_offs = 0.0;
                /* Warn if 1st packet number is not zero */
                if (seq_num!=0) {
                    char msg[256];
                    sprintf(msg, "First packet number is not 0 (seq_num=%lld)",
                            seq_num);
                    guppi_warn("guppi_net_thread", msg);
                }
            }

            /* Read/update current status shared mem */
            guppi_status_lock_safe(&st);
            if (stt_imjd!=0) {
                hputi4(st.buf, "STT_IMJD", stt_imjd);
                hputi4(st.buf, "STT_SMJD", stt_smjd);
                hputr8(st.buf, "STT_OFFS", stt_offs);
                hputi4(st.buf, "STTVALID", 1);
            } else {
                hputi4(st.buf, "STTVALID", 0);
            }
            memcpy(status_buf, st.buf, GUPPI_STATUS_SIZE);
            guppi_status_unlock_safe(&st);

            /* block size possibly changed on new obs */
            if (force_new_block) {
                if (hgeti4(status_buf, "BLOCSIZE", &block_size)==0) {
                        block_size = db->block_size;
                } else {
                    if (block_size > db->block_size) {
                        guppi_error("guppi_net_thread", 
                                "BLOCSIZE > databuf block_size");
                        block_size = db->block_size;
                    }
                }
                packets_per_block = block_size / packet_data_size;
            }
            hputi4(status_buf, "BLOCSIZE", block_size);

            /* Advance to next block when free, update its header */
            curblock = (curblock + 1) % db->n_block;
            curheader = guppi_databuf_header(db, curblock);
            curdata = guppi_databuf_data(db, curblock);
            last_block_packet_idx = 0;
            curblock_seq_num = seq_num - (seq_num % packets_per_block);
            nextblock_seq_num = curblock_seq_num + packets_per_block;
            while ((rv=guppi_databuf_wait_free(db,curblock)) != GUPPI_OK) {
                if (rv==GUPPI_TIMEOUT) {
                    waiting=1;
                    guppi_status_lock_safe(&st);
                    hputs(st.buf, STATUS_KEY, "blocked");
                    guppi_status_unlock_safe(&st);
                    continue;
                } else {
                    guppi_error("guppi_net_thread", 
                            "error waiting for free databuf");
                    run=0;
                    pthread_exit(NULL);
                    break;
                }
            }
            memcpy(curheader, status_buf, GUPPI_STATUS_SIZE);
        }

        /* Skip dropped blocks, put packet in right spot */
        block_packet_idx = seq_num - curblock_seq_num;
        dataptr = curdata + last_block_packet_idx*packet_data_size;
        for (i=last_block_packet_idx; i<block_packet_idx; i++) {
            memset(dataptr, 0, packet_data_size);
            dataptr += packet_data_size;
            ndropped_block++;
            ndropped_total++;
            npacket_total++;
            npacket_block++;
        }
        // TODO replace memcpy with special func that expands out
        // 1SFA packets, etc.
        //memcpy(dataptr, guppi_udp_packet_data(&p), packet_data_size);
        guppi_udp_packet_data_copy(dataptr, &p);
        npacket_total++;
        npacket_block++;
        last_block_packet_idx = block_packet_idx + 1;
        last_seq_num = seq_num;

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    pthread_exit(NULL);

    /* Have to close all push's */
    pthread_cleanup_pop(0); /* Closes push(guppi_udp_close) */
    pthread_cleanup_pop(0); /* Closes set_exit_status */
    pthread_cleanup_pop(0); /* Closes guppi_free_psrfits */
    pthread_cleanup_pop(0); /* Closes guppi_status_detach */
    pthread_cleanup_pop(0); /* Closes guppi_databuf_detach */
}
