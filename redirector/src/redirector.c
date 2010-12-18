/*
    Sylverant Server Redirector
    Copyright (C) 2009, 2010 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define NUM_DCSOCKS 2
#define NUM_GCSOCKS 3

/* Stuff that would normally be in packets.h, here for brevity. */
#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__)
#define LE16(x) (((x >> 8) & 0xFF) | ((x & 0xFF) << 8))
#else
#define LE16(x) x
#endif

typedef struct dc_pkt_hdr {
    uint8_t pkt_type;
    uint8_t flags;
    uint16_t pkt_len;
} __attribute__((packed)) dc_pkt_hdr_t;

typedef struct pc_pkt_hdr {
    uint16_t pkt_len;
    uint8_t pkt_type;
    uint8_t flags;
} __attribute__((packed)) pc_pkt_hdr_t;

/* The packet sent to redirect clients */
typedef struct dc_redirect {
    union {
        dc_pkt_hdr_t dc;
        pc_pkt_hdr_t pc;
    } hdr;
    uint32_t ip_addr;       /* Big-endian */
    uint16_t port;          /* Little-endian */
    uint8_t padding[2];
} __attribute__((packed)) dc_redirect_pkt;

#define REDIRECT_TYPE                   0x0019
#define DC_REDIRECT_LENGTH              0x000C

static uint8_t sendbuf[0xB0];               /* Largest is selective redirect */
static in_addr_t server_addr = INADDR_ANY;

/* Print information about this program to stdout. */
static void print_program_info() {
    printf("Sylverant Server Redirector version %s\n", VERSION);
    printf("Copyright (C) 2009, 2010 Lawrence Sebald\n\n");
    printf("This program is free software: you can redistribute it and/or\n"
           "modify it under the terms of the GNU Affero General Public\n"
           "License version 3 as published by the Free Software Foundation.\n\n"
           "This program is distributed in the hope that it will be useful,\n"
           "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
           "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
           "GNU General Public License for more details.\n\n"
           "You should have received a copy of the GNU Affero General Public\n"
           "License along with this program.  If not, see\n"
           "<http://www.gnu.org/licenses/>.\n");
}

/* Print help to the user to stdout. */
static void print_help(const char *bin) {
    printf("Usage: %s [arguments]\n"
           "-----------------------------------------------------------------\n"
           "--version       Print version info and exit\n"
           "--S ipaddr      Set the server to redirect to (required)\n"
           "--help          Print this help and exit\n", bin);
}

/* Parse any command-line arguments passed in. */
static void parse_command_line(int argc, char *argv[]) {
    int i;

    for(i = 1; i < argc; ++i) {
        if(!strcmp(argv[i], "--version")) {
            print_program_info();
            exit(EXIT_SUCCESS);
        }
        else if(!strcmp(argv[i], "-S")) {
            if(i == argc - 1) {
                printf("Need address argument for -S\n");
                exit(EXIT_FAILURE);
            }

            if(inet_pton(AF_INET, argv[i + 1], &server_addr) != 1) {
                printf("Invalid address for -S\n");
                exit(EXIT_FAILURE);
            }

            ++i;
        }
        else if(!strcmp(argv[i], "--help")) {
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else {
            printf("Illegal command line argument: %s\n", argv[i]);
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* Make sure a server address was set */
    if(server_addr == INADDR_ANY) {
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
}

/* Send a raw packet away. */
static int send_raw(int sock, int len) {
    ssize_t rv, total = 0;

    /* Keep trying until the whole thing's sent. */
    while(total < len) {
        if((rv = send(sock, sendbuf + total, len - total, 0)) < 0) {
            return -1;
        }

        total += rv;
    }

    return 0;
}

static int send_selective_redirect(int sock) {
    dc_redirect_pkt *pkt = (dc_redirect_pkt *)sendbuf;
    dc_pkt_hdr_t *hdr2 = (dc_pkt_hdr_t *)(sendbuf + 0x19);

    /* Wipe the packet */
    memset(pkt, 0, 0xB0);

    /* Fill in the redirect packet. PC users will parse this out as a type 0x19
       (Redirect) with size 0xB0. GC/DC users would parse it out as a type 0xB0
       (Ignored) with a size of 0x19. The second header takes care of the rest
       of the 0xB0 size. */
    pkt->hdr.pc.pkt_type = REDIRECT_TYPE;
    pkt->hdr.pc.pkt_len = LE16(0x00B0);
    pkt->ip_addr = server_addr;
    pkt->port = LE16(9300);

    /* Fill in the secondary header */
    hdr2->pkt_type = 0xB0;
    hdr2->pkt_len = LE16(0x0097);

    /* Send it away */
    return send_raw(sock, 0xB0);
}

static int send_redirect(int sock, uint16_t port, int pc) {
    dc_redirect_pkt *pkt = (dc_redirect_pkt *)sendbuf;

    /* Wipe the packet */
    memset(pkt, 0, DC_REDIRECT_LENGTH);

    /* Fill in the header */
    if(!pc) {
        pkt->hdr.dc.pkt_type = REDIRECT_TYPE;
        pkt->hdr.dc.pkt_len = LE16(DC_REDIRECT_LENGTH);
    }
    else {
        pkt->hdr.pc.pkt_type = REDIRECT_TYPE;
        pkt->hdr.pc.pkt_len = LE16(DC_REDIRECT_LENGTH);
    }

    /* Fill in the IP and port */
    pkt->ip_addr = server_addr;
    pkt->port = LE16(port);

    /* Send the packet away */
    return send_raw(sock, DC_REDIRECT_LENGTH);
}

static void run_server(int dcsocks[NUM_DCSOCKS], int pcsock,
                       int gcsocks[NUM_GCSOCKS]) {
    fd_set readfds;
    struct timeval timeout;
    socklen_t len;
    struct sockaddr_in addr;
    int nfds, asock, j;

    for(;;) {        
        FD_ZERO(&readfds);
        timeout.tv_sec = 9001;
        timeout.tv_usec = 0;
        nfds = 0;

        /* Add the listening sockets for incoming connections to the fd_set. */
        for(j = 0; j < NUM_DCSOCKS; ++j) {
            FD_SET(dcsocks[j], &readfds);
            nfds = nfds > dcsocks[j] ? nfds : dcsocks[j];
        }

        for(j = 0; j < NUM_GCSOCKS; ++j) {
            FD_SET(gcsocks[j], &readfds);
            nfds = nfds > gcsocks[j] ? nfds : gcsocks[j];
        }

        FD_SET(pcsock, &readfds);
        nfds = nfds > pcsock ? nfds : pcsock;

        if(select(nfds + 1, &readfds, NULL, NULL, &timeout) > 0) {
            for(j = 0; j < NUM_DCSOCKS; ++j) {
                if(FD_ISSET(dcsocks[j], &readfds)) {
                    len = sizeof(struct sockaddr_in);

                    if((asock = accept(dcsocks[j], (struct sockaddr *)&addr,
                                       &len)) < 0) {
                        perror("accept");
                    }
                    else {
                        send_redirect(asock, 9200 + j, 0);
                        close(asock);
                    }
                }
            }

            if(FD_ISSET(pcsock, &readfds)) {
                len = sizeof(struct sockaddr_in);

                if((asock = accept(pcsock, (struct sockaddr *)&addr,
                                   &len)) < 0) {
                    perror("accept");
                }
                else {
                    send_redirect(asock, 9300, 1);
                    close(asock);
                }
            }

            for(j = 0; j < NUM_GCSOCKS; ++j) {
                if(FD_ISSET(gcsocks[j], &readfds)) {
                    len = sizeof(struct sockaddr_in);

                    if((asock = accept(gcsocks[j], (struct sockaddr *)&addr,
                                       &len)) < 0) {
                        perror("accept");
                    }
                    else {
                        /* Sort out any PC users */
                        send_selective_redirect(asock);

                        if(!j) {
                            send_redirect(asock, 9100, 0);
                        }
                        else {
                            send_redirect(asock, 9000 + j - 1, 0);
                        }

                        close(asock);
                    }
                }
            }
        }
    }
}

static int open_sock(uint16_t port) {
    int sock = -1;
    struct sockaddr_in addr;

    /* Create the socket and listen for connections. */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock < 0) {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    memset(addr.sin_zero, 0, 8);

    if(bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        perror("bind");
        close(sock);
        return -1;
    }

    if(listen(sock, 10)) {
        perror("listen");
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[]) {
    int pcsock, dcsocks[NUM_DCSOCKS], gcsocks[NUM_GCSOCKS];

    parse_command_line(argc, argv);

    dcsocks[0] = open_sock(9200);
    dcsocks[1] = open_sock(9201);
    pcsock = open_sock(9300);
    gcsocks[0] = open_sock(9100);
    gcsocks[1] = open_sock(9000);
    gcsocks[2] = open_sock(9001);

    if(dcsocks[0] < 0 || dcsocks[1] < 0 || pcsock < 0 || gcsocks[0] < 0 ||
       gcsocks[1] < 0 || gcsocks[2] < 0) {
        exit(EXIT_FAILURE);
    }

    /* Run the login server. */
    run_server(dcsocks, pcsock, gcsocks);

    return 0;
}
