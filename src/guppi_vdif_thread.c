/* guppi_vdif_thread.c
 *
 * Routine to read packets from network and put them
 * into shared memory blocks.  This one is based on
 * guppi_net_thread.c but also handles merging two vdif streams.
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

#include "vdifio.h"

// Read a status buffer all of the key observation paramters
extern void guppi_read_obs_params(char *buf, 
                                  struct guppi_params *g, 
                                  struct psrfits *p);

/* Struct with per-stream counters, info */
struct vdif_stream {
    int thread_id; // Expected thread id
    int curblock;  // Block that data will go to
    long long curblock_seq_num;   // Starting seq num of the current block
    long long nextblock_seq_num;  // Starting seq num of the next block
    long long seq_num; // Seq num of current packet
    long long last_seq_num;  // Previous seq num
    long long npacket_block; // # of packets this block
    long long npacket_total; // # of packets total
    long long ndropped_block; // # of missed packets this block
    long long ndropped_total; // # of missed packets total
};

void init_stream(struct vdif_stream *v, int thread_id) {
    v->thread_id = thread_id;
    v->curblock = -1;
    v->curblock_seq_num = 0;
    v->nextblock_seq_num = 0;
    v->seq_num = 0;
    v->last_seq_num = -1;
    v->npacket_block = 0;
    v->npacket_total = 0;
    v->ndropped_block = 0;
    v->ndropped_total = 0;
}

/* Check whether the specified block is active in any of the streams */
int check_block_active(const struct vdif_stream *stream, 
        int nstream, int block) {
    int i, result=0;
    for (i=0; i<nstream; i++)
        if (stream[i].curblock == block) 
            result=1;
    return result;
}

/* Copy the latest packet belonging to vdif stream into the data block
 * area.  This interleaves samples depending on index and nstream.
 */
void packet_data_copy(struct vdif_stream *v, struct guppi_udp_packet *p,
        int index, int nstream, char *block_data) {
    long long block_idx = v->seq_num - v->curblock_seq_num;
    /* NOTE, we will use bps=1 here even for 2-bit data, to avoid
     * having to do any sub-byte data rearranging.  The relevant
     * code (dspsr) will need to know that each byte contains 4
     * samples from a single polarization.
     */
    //const int bps = 2; // 8-bit complex data
    const int bps = 1; // 8-bit real data
    const int nbytes = p->packet_size - VDIF_HEADER_BYTES;
    const int nsamp = nbytes / bps;

    char *optr = block_data + block_idx*nbytes*nstream + index*bps;
    char *iptr = p->data + VDIF_HEADER_BYTES;

    int i;
    for (i=0; i<nsamp; i++) {
        memcpy(optr, iptr, bps);
        iptr += bps;
        optr += nstream*bps;
    }

    /* update counters */
    v->ndropped_block += (v->seq_num - v->last_seq_num - 1);
    v->ndropped_total += (v->seq_num - v->last_seq_num - 1);
    v->npacket_total++;
    v->npacket_block++;
    v->last_seq_num = v->seq_num;
}

/* Copy the entire VDIF packet (including headers) into the data
 * block.  Packets are interleaved by stream.
 */
void packet_full_copy(struct vdif_stream *v, struct guppi_udp_packet *p,
        int index, int nstream, char *block_data) {

    long long block_idx = v->seq_num - v->curblock_seq_num;
    const int nbytes = p->packet_size;

    char *optr = block_data + block_idx*nbytes*nstream + index*nbytes;
    char *iptr = p->data;
    memcpy(optr, iptr, nbytes);

    /* update counters */
    v->ndropped_block += (v->seq_num - v->last_seq_num - 1);
    v->ndropped_total += (v->seq_num - v->last_seq_num - 1);
    v->npacket_total++;
    v->npacket_block++;
    v->last_seq_num = v->seq_num;
}

/* This thread is passed a single arg, pointer
 * to the guppi_udp_params struct.  This thread should 
 * be cancelled and restarted if any hardware params
 * change, as this potentially affects packet size, etc.
 */
void *guppi_vdif_thread(void *_args) {

    /* Get arguments */
    struct guppi_thread_args *args = (struct guppi_thread_args *)_args;

    int rv;
#if 0 
    /* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    //CPU_SET(2, &cpuset);
    CPU_SET(3, &cpuset); // TODO figure out if this even makes sense
    rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) { 
        guppi_error("guppi_net_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }
#endif

    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, args->priority);
    if (rv<0) {
        guppi_error("guppi_net_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct guppi_status st;
    st.idx = args->daq_idx;
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
    int use_vdif_packets=0;
    if (strncmp(up.packet_format, "VDIF", 6)==0) { use_vdif_packets=1; }
    else {
        guppi_error("guppi_vdif_thread", "packet format is not VDIF");
        pthread_exit(NULL);
    }

    /* Do we copy the packet headers (for raw VDIF writing)? */
    unsigned copy_full_packet = 0;
    char raw_fmt[8];
    guppi_read_raw_mode(status_buf, raw_fmt);
    if (strncmp(raw_fmt,"VDIF",4)==0) { copy_full_packet=1; }

    /* Set up VDIF stream info */
    unsigned i;
    const unsigned nstream = 2;
    struct vdif_stream stream[nstream];
    struct vdif_stream *cs; // Use for pointer to active stream each time
    for (i=0; i<nstream; i++) init_stream(&stream[i], 0);
    // old values
    //stream[0].thread_id = 0;
    //stream[1].thread_id = 896;
    if (hgeti4(status_buf, "VDIFTIDA", &stream[0].thread_id)==0) 
        stream[0].thread_id=2;
    if (hgeti4(status_buf, "VDIFTIDB", &stream[1].thread_id)==0) 
        stream[1].thread_id=3;

    /* Figure out size of data in each packet, number of packets
     * per block, etc.
     * Round down to an integer # of packets per block if necessary.
     */
    int block_size;
    struct guppi_udp_packet p, p0;
    //size_t packet_data_size = guppi_udp_packet_datasize(up.packet_size); 
    size_t packet_data_size = up.packet_size - VDIF_HEADER_BYTES; 
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
    int block_size_rounded;
    if (copy_full_packet) {
        packets_per_block = block_size / nstream / up.packet_size;
        block_size_rounded = packets_per_block * nstream * up.packet_size;
    } else {
        packets_per_block = block_size / nstream / packet_data_size;
        block_size_rounded = packets_per_block * nstream * packet_data_size;
    }
    if (block_size_rounded != block_size) {
        char msg[256];
        sprintf(msg, "Rounding BLOCSIZE from %d to %d", block_size, 
                block_size_rounded);
        guppi_warn("guppi_net_thread", msg);
        block_size = block_size_rounded;
        hputi4(status_buf, "BLOCSIZE", block_size);
    }

    /* Get number of bits */
    int nbit;
    if (hgeti4(status_buf, "NBITS", &nbit)==0) nbit=8;

    /* For VDIF, we need to calculate packets per second */
    int vdif_packets_per_second = 0;
    if (use_vdif_packets) {
        /* This assumes single-pol per VDIF thread */
        //vdif_packets_per_second = fabs(pf.hdr.BW) * 1e6 * 2 / packet_data_size;
        vdif_packets_per_second = fabs(pf.hdr.BW) * 1e6 * 2 
            / (packet_data_size * 8 / nbit);
        printf("guppi_net_thread: VDIF packets per sec = %d\n",
                vdif_packets_per_second);
    }

    /* Counters */
    int first=1;
    char *curheader=NULL;
    long long nbogus_total=0, nbogus_block=0;
    double drop_frac_avg=0.0;
    const double drop_lpf = 0.25;

    /* Main loop */
    int istream, thread_id;
    unsigned force_new_block=0, waiting=-1;
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
                /* Unexpected packet size, ignore */
                nbogus_total++;
                nbogus_block++;
                printf("Bad pkt size (want=%d got=%d)\n",up.packet_size, 
                        p.packet_size);
                continue; 
            } else {
                guppi_error("guppi_net_thread", 
                        "guppi_udp_recv returned error");
                perror("guppi_udp_recv");
                pthread_exit(NULL);
            }
        }

        /* Figure out which stream this packet is in */
        thread_id = getVDIFThreadID(p.data);
        for (istream=0; istream<nstream; istream++) {
            if (thread_id == stream[istream].thread_id) {
                cs = &stream[istream];
                break;
            }
        }
        if (istream==nstream) {
            // Did not match any expected thread ids
            nbogus_total++;
            nbogus_block++;
            printf("Bad pkt stream (thread_id=%d)\n", thread_id);
            continue;
        }

        /* Update status if needed */
        if (waiting!=0) {
            guppi_status_lock_safe(&st);
            hputs(st.buf, STATUS_KEY, "receiving");
            guppi_status_unlock_safe(&st);
            waiting=0;
        }

        /* Grab some necessary info from the first vdif packet */
        if (first && use_vdif_packets) {
            memcpy(&p0, &p, sizeof(struct guppi_udp_packet));
            mjd_from_vdif(p0.data, vdif_packets_per_second, 
                    &stt_imjd, &stt_smjd, &stt_offs);
            // Make start time the next integer second
            stt_smjd++;
            if (stt_smjd>=86400) { stt_imjd++; stt_smjd-=86400; }
            stt_offs=0.0;
            first=0;
        }

        /* Skip if we haven't reached start time yet */
        if (getVDIFFrameMJD(p.data) < stt_imjd) { continue; }
        if (getVDIFFrameSecond(p.data) < stt_smjd) { continue; }

        /* Check seq num diff */
        //cs->seq_num = guppi_vdif_packet_seq_num(&p,&p0,vdif_packets_per_second);
        cs->seq_num = guppi_vdif_packet_seq_num(&p, stt_imjd, stt_smjd,
                vdif_packets_per_second);
        seq_num_diff = cs->seq_num - cs->last_seq_num;
        if (seq_num_diff<=0 && cs->curblock>=0) { 
            if (seq_num_diff==0) {
                char msg[256];
                sprintf(msg, "Received duplicate packet (seq_num=%lld)", 
                        cs->seq_num);
                guppi_warn("guppi_net_thread", msg);
            }
            else  { 
                /* No going backwards */
                char msg[256];
                sprintf(msg, 
                        "Received out-of-order packet (thread=%d, seq_num=%lld, diff=%lld)",
                        cs->thread_id, cs->seq_num, seq_num_diff);
                guppi_warn("guppi_net_thread", msg);
                continue; 
            } 
        } else { force_new_block=0; }

        /* Determine if we go to next block */
        if ((cs->seq_num>=cs->nextblock_seq_num) || force_new_block) {

            /* Debug info */
            printf("New Block(%2d):", cs->curblock);
            printf("  istream=%d", istream);
            printf("  seq_num=%lld", cs->seq_num);
            printf("  npacket=%lld", cs->npacket_block);
            printf("  ndrop=%lld", cs->ndropped_block);
            printf("\n");

            if (cs->curblock>=0) { 
                /* TODO: check if all streams are done with this yet */
                /* Close out current block */
                hputi4(curheader, "NPKT", cs->npacket_block);
                hputi4(curheader, "NDROP", cs->ndropped_block);
            }

            if (cs->npacket_block) { 
                drop_frac_avg = (1.0-drop_lpf)*drop_frac_avg 
                    + drop_lpf*(double)cs->ndropped_block/(double)cs->npacket_block;
            }

            /* Put drop stats in general status area */
            /* How to merge the different stream info.. */
#if 1 
            guppi_status_lock_safe(&st);
            hputr8(st.buf, "DROPAVG", drop_frac_avg);
            hputr8(st.buf, "DROPTOT", 
                    cs->npacket_total ? 
                    (double)cs->ndropped_total/(double)cs->npacket_total 
                    : 0.0);
            hputr8(st.buf, "DROPBLK", 
                    cs->npacket_block ? 
                    (double)cs->ndropped_block/(double)cs->npacket_block 
                    : 0.0);
            guppi_status_unlock_safe(&st);
#endif

            /* Reset block counters */
            cs->npacket_block=0;
            cs->ndropped_block=0;
            nbogus_block=0;

            /* Read/update current status shared mem */
            guppi_status_lock_safe(&st);
            if (stt_imjd!=0) {
                hputi4(st.buf, "STT_IMJD", stt_imjd);
                hputi4(st.buf, "STT_SMJD", stt_smjd);
                hputnr8(st.buf, "STT_OFFS", 15, stt_offs);
                hputi4(st.buf, "STTVALID", 1);
            } else {
                hputi4(st.buf, "STTVALID", 0);
            }
            memcpy(status_buf, st.buf, GUPPI_STATUS_SIZE);
            guppi_status_unlock_safe(&st);

            /* Advance to next block when it's free */
            int last_block = cs->curblock;
            int next_block = (cs->curblock + 1) % db->n_block;
            int next_block_active = check_block_active(stream, nstream,
                    next_block);
            cs->curblock = next_block;
            curheader = guppi_databuf_header(db, cs->curblock);
            cs->curblock_seq_num = cs->seq_num 
                - (cs->seq_num % packets_per_block);
            cs->nextblock_seq_num = cs->curblock_seq_num + packets_per_block;
            if (last_block>=0 &&
                    check_block_active(stream, nstream,last_block)==0)
                guppi_databuf_set_filled(db, last_block);
            while ((rv=guppi_databuf_wait_free(db,cs->curblock)) != GUPPI_OK) {
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

            /* If this block has not been updated yet, zero its
             * data area and update the header.
             */
            if (next_block_active == 0) {
                memset(guppi_databuf_data(db, cs->curblock), 0, block_size);
                memcpy(curheader, status_buf, GUPPI_STATUS_SIZE);
                hputi4(curheader, "PKTIDX", cs->curblock_seq_num);
                hputi4(curheader, "PKTSIZE", packet_data_size);
            }
        }

        /* Copy the packet data into the right place, interleaving 
         * streams.
         */
        if (copy_full_packet) {
            packet_full_copy(cs, &p, istream, nstream,
                    guppi_databuf_data(db, cs->curblock));
        } else {
            packet_data_copy(cs, &p, istream, nstream,
                    guppi_databuf_data(db, cs->curblock));
        }

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
