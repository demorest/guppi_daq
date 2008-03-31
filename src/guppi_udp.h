/* guppi_udp.h
 *
 * Functions dealing with setting up and 
 * receiving data through a UDP connection.
 */
#ifndef _GUPPI_UDP_H
#define _GUPPI_UDP_H

#include <sys/types.h>
#include <netdb.h>
#include <poll.h>

#define GUPPI_MAX_PACKET_SIZE 9000

/* Struct to hold connection parameters */
struct guppi_udp_params {

    /* Info needed from outside: */
    char sender[80];  /* Sender hostname */
    int port;         /* Receive port */
    size_t packet_size; /* Expected packet size, 0 = don't care */

    /* Derived from above: */
    int sock;                       /* Receive socket */
    struct addrinfo sender_addr;    /* Sender hostname/IP params */
    struct pollfd pfd;              /* Use to poll for avail data */

};

/* Basic structure of a packet.  This struct, functions should 
 * be used to get the various components of a data packet.   The
 * internal packet structure is:
 *   1. sequence number (64-bit unsigned int)
 *   2. data bytes (typically 8kB)
 *   3. status flags (64 bits)
 */
struct guppi_udp_packet {
    size_t packet_size;  /* packet size, bytes */
    char data[GUPPI_MAX_PACKET_SIZE]; /* packet data */
};
unsigned long long guppi_udp_packet_seq_num(struct guppi_udp_packet *p);
char *guppi_udp_packet_data(struct guppi_udp_packet *p);
size_t guppi_udp_packet_datasize(size_t packet_size);
unsigned long long guppi_udp_packet_flags(struct guppi_udp_packet *p);

/* Use sender and port fields in param struct to init
 * the other values, bind socket, etc.
 */
int guppi_udp_init(struct guppi_udp_params *p);

/* Wait for available data on the UDP socket */
int guppi_udp_wait(struct guppi_udp_params *p); 

/* Read a packet */
int guppi_udp_recv(struct guppi_udp_params *p, struct guppi_udp_packet *b);

/* Close out socket, etc */
int guppi_udp_close(struct guppi_udp_params *p);

#endif
