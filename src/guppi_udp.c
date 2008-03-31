/* guppi_udp.c
 *
 * UDP implementations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

#include "guppi_udp.h"
#include "guppi_databuf.h"
#include "guppi_error.h"

int guppi_udp_init(struct guppi_udp_params *p) {

    /* Resolve sender hostname */
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    int rv = getaddrinfo(p->sender, NULL, &hints, &result);
    if (rv!=0) { 
        guppi_error("guppi_udp_init", "getaddrinfo failed");
        return(GUPPI_ERR_SYS);
    }

    /* Set up socket */
    p->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (p->sock==-1) { 
        guppi_error("guppi_udp_init", "socket error");
        freeaddrinfo(result);
        return(GUPPI_ERR_SYS);
    }

    /* bind to local address */
    struct sockaddr_in local_ip;
    local_ip.sin_family =  AF_INET;
    local_ip.sin_port = htons(p->port);
    local_ip.sin_addr.s_addr = INADDR_ANY;
    rv = bind(p->sock, (struct sockaddr *)&local_ip, sizeof(local_ip));
    if (rv==-1) {
        guppi_error("guppi_udp_init", "bind");
        return(GUPPI_ERR_SYS);
    }

    /* Set up socket to recv only from sender */
    for (rp=result; rp!=NULL; rp=rp->ai_next) {
        if (connect(p->sock, rp->ai_addr, rp->ai_addrlen)==0) { break; }
    }
    if (rp==NULL) { 
        guppi_error("guppi_udp_init", "connect error");
        close(p->sock); 
        freeaddrinfo(result);
        return(GUPPI_ERR_SYS);
    }
    memcpy(&p->sender_addr, rp, sizeof(struct addrinfo));
    freeaddrinfo(result);

    /* Non-blocking recv */
    fcntl(p->sock, F_SETFL, O_NONBLOCK);

    /* Increase recv buffer for this sock */
    int bufsize = 64*1024*1024;
    socklen_t ss = sizeof(int);
    rv = setsockopt(p->sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(int));
    if (rv<0) { 
        guppi_error("guppi_udp_init", "Error setting rcvbuf size.");
        perror("setsockopt");
    } 
    rv = getsockopt(p->sock, SOL_SOCKET, SO_RCVBUF, &bufsize, &ss); 
    if (rv==0) { 
        printf("guppi_udp_init: SO_RCVBUF=%d\n", bufsize);
    }

    /* Poll command */
    p->pfd.fd = p->sock;
    p->pfd.events = POLLIN;

    return(GUPPI_OK);
}

int guppi_udp_wait(struct guppi_udp_params *p) {
    int rv = poll(&p->pfd, 1, 1000); /* Timeout 1sec */
    if (rv==1) { return(GUPPI_OK); } /* Data ready */
    else if (rv==0) { return(GUPPI_TIMEOUT); } /* Timed out */
    else { return(GUPPI_ERR_SYS); }  /* Other error */
}

int guppi_udp_recv(struct guppi_udp_params *p, struct guppi_udp_packet *b) {
    int rv = recv(p->sock, b->data, GUPPI_MAX_PACKET_SIZE, 0);
    b->packet_size = rv;
    if (rv==-1) { return(GUPPI_ERR_SYS); }
    else if (p->packet_size) {
        if (rv!=p->packet_size) { return(GUPPI_ERR_PACKET); }
        else { return(GUPPI_OK); }
    } else { 
        p->packet_size = rv;
        return(GUPPI_OK); 
    }
}

unsigned long long guppi_udp_packet_seq_num(struct guppi_udp_packet *p) {
    return(*(unsigned long long *)(p->data));
}

size_t guppi_udp_packet_datasize(size_t packet_size) {
    return(packet_size - 2*sizeof(unsigned long long));
}

char *guppi_udp_packet_data(struct guppi_udp_packet *p) {
    return((char *)(p->data) + sizeof(unsigned long long));
}

unsigned long long guppi_udp_packet_flags(struct guppi_udp_packet *p) {
    return(*(unsigned long long *)((char *)(p->data) 
                + p->packet_size - sizeof(unsigned long long)));
}

int guppi_udp_close(struct guppi_udp_params *p) {
    close(p->sock);
    return(GUPPI_OK);
}
