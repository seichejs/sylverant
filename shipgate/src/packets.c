/*
    Sylverant Shipgate
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "shipgate.h"
#include "ship.h"

static uint8_t sendbuf[65536];

/* Send a raw packet away. */
static int send_raw(ship_t *c, int len) {
    ssize_t rv, total = 0;
    void *tmp;

    /* Keep trying until the whole thing's sent. */
    if(!c->sendbuf_cur) {
        while(total < len) {
            rv = send(c->sock, sendbuf + total, len - total, 0);

            if(rv == -1 && errno != EAGAIN) {
                return -1;
            }
            else if(rv == -1) {
                break;
            }

            total += rv;
        }
    }

    rv = len - total;

    if(rv) {
        /* Move out any already transferred data. */
        if(c->sendbuf_start) {
            memmove(c->sendbuf, c->sendbuf + c->sendbuf_start,
                    c->sendbuf_cur - c->sendbuf_start);
            c->sendbuf_cur -= c->sendbuf_start;
        }

        /* See if we need to reallocate the buffer. */
        if(c->sendbuf_cur + rv > c->sendbuf_size) {
            tmp = realloc(c->sendbuf, c->sendbuf_cur + rv);

            /* If we can't allocate the space, bail. */
            if(tmp == NULL) {
                return -1;
            }

            c->sendbuf_size = c->sendbuf_cur + rv;
            c->sendbuf = (unsigned char *)tmp;
        }

        /* Copy what's left of the packet into the output buffer. */
        memcpy(c->sendbuf + c->sendbuf_cur, sendbuf + total, rv);
        c->sendbuf_cur += rv;
    }

    return 0;
}

/* Encrypt a packet, and send it away. */
static int send_crypt(ship_t *c, int len) {
    /* Make sure its at least a header in length. */
    if(len < 8) {
        return -1;
    }

    if(c->key_set)
        RC4(&c->gate_key, len, sendbuf, sendbuf);

    return send_raw(c, len);
}

int forward_dreamcast(ship_t *c, dc_pkt_hdr_t *dc, uint32_t sender) {
    shipgate_fw_pkt *pkt = (shipgate_fw_pkt *)sendbuf;
    int dc_len = LE16(dc->pkt_len);
    int full_len = sizeof(shipgate_fw_pkt) + dc_len;

    /* Round up the packet size, if needed. */
    if(full_len & 0x07)
        full_len = (full_len + 8) & 0xFFF8;

    /* Scrub the buffer */
    memset(pkt, 0, full_len);

    /* Fill in the shipgate header */
    pkt->hdr.pkt_len = htons(full_len);
    pkt->hdr.pkt_type = htons(SHDR_TYPE_DC);
    pkt->hdr.pkt_unc_len = htons(full_len);
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE);

    /* Add the metadata */
    pkt->ship_id = htonl(sender);

    /* Copy in the packet, unchanged */
    memcpy(&pkt->pkt, dc, dc_len);

    /* Send the packet away */
    return send_crypt(c, full_len);
}

int forward_pc(ship_t *c, dc_pkt_hdr_t *pc, uint32_t sender) {
    shipgate_fw_pkt *pkt = (shipgate_fw_pkt *)sendbuf;
    int pc_len = LE16(pc->pkt_len);
    int full_len = sizeof(shipgate_fw_pkt) + pc_len;

    /* Round up the packet size, if needed. */
    if(full_len & 0x07)
        full_len = (full_len + 8) & 0xFFF8;

    /* Scrub the buffer */
    memset(pkt, 0, full_len);

    /* Fill in the shipgate header */
    pkt->hdr.pkt_len = htons(full_len);
    pkt->hdr.pkt_type = htons(SHDR_TYPE_PC);
    pkt->hdr.pkt_unc_len = htons(full_len);
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE);

    /* Add the metadata */
    pkt->ship_id = htonl(sender);

    /* Copy in the packet, unchanged */
    memcpy(&pkt->pkt, pc, pc_len);

    /* Send the packet away */
    return send_crypt(c, full_len);
}

/* Send a welcome packet to the given ship. */
int send_welcome(ship_t *c) {
    shipgate_login_pkt *pkt = (shipgate_login_pkt *)sendbuf;

    /* Scrub the buffer */
    memset(pkt, 0, sizeof(shipgate_login_pkt));

    /* Fill in the header */
    pkt->hdr.pkt_len = htons(sizeof(shipgate_login_pkt));
    pkt->hdr.pkt_type = htons(SHDR_TYPE_LOGIN);
    pkt->hdr.pkt_unc_len = htons(sizeof(shipgate_login_pkt));
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE);

    /* Fill in the required message */
    strcpy(pkt->msg, shipgate_login_msg);

    /* Fill in the version information */
    pkt->ver_major = VERSION_MAJOR;
    pkt->ver_minor = VERSION_MINOR;
    pkt->ver_micro = VERSION_MICRO;

    /* Fill in the nonces */
    memcpy(pkt->ship_nonce, c->ship_nonce, 4);
    memcpy(pkt->gate_nonce, c->gate_nonce, 4);

    /* Send the packet away */
    return send_raw(c, sizeof(shipgate_login_pkt));
}

/* Send a ship up/down message to the given ship. */
int send_ship_status(ship_t *c, ship_t *o, uint16_t status) {
    shipgate_ship_status_pkt *pkt = (shipgate_ship_status_pkt *)sendbuf;

    /* Scrub the buffer */
    memset(pkt, 0, sizeof(shipgate_ship_status_pkt));

    /* Fill in the header */
    pkt->hdr.pkt_len = htons(sizeof(shipgate_ship_status_pkt));
    pkt->hdr.pkt_type = htons(SHDR_TYPE_SSTATUS);
    pkt->hdr.pkt_unc_len = htons(sizeof(shipgate_ship_status_pkt));
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE);

    /* Fill in the info */
    strcpy(pkt->name, o->name);
    pkt->ship_id = htonl(o->key_idx);
    pkt->ship_addr = o->remote_addr;
    pkt->int_addr = o->local_addr;
    pkt->ship_port = htons(o->port);
    pkt->status = htons(status);
    pkt->flags = htonl(o->flags);
    pkt->clients = htons(o->clients);
    pkt->games = htons(o->games);
    pkt->menu_code = htons(o->menu_code);

    /* Send the packet away */
    return send_crypt(c, sizeof(shipgate_ship_status_pkt));
}

/* Send a ping packet to a client. */
int send_ping(ship_t *c, int reply) {
    shipgate_hdr_t *pkt = (shipgate_hdr_t *)sendbuf;

    /* Fill in the header. */
    pkt->pkt_len = htons(sizeof(shipgate_hdr_t));
    pkt->pkt_type = htons(SHDR_TYPE_PING);
    pkt->pkt_unc_len = htons(sizeof(shipgate_hdr_t));

    if(reply) {
        pkt->flags = htons(SHDR_NO_DEFLATE | SHDR_RESPONSE);
    }
    else {
        pkt->flags = htons(SHDR_NO_DEFLATE);
    }

    /* Send it away. */
    return send_crypt(c, sizeof(shipgate_hdr_t));
}

/* Send the ship a character data restore. */
int send_cdata(ship_t *c, uint32_t gc, uint32_t slot, void *cdata) {
    shipgate_char_data_pkt *pkt = (shipgate_char_data_pkt *)sendbuf;

    /* Fill in the header. */
    pkt->hdr.pkt_len = htons(sizeof(shipgate_char_data_pkt));
    pkt->hdr.pkt_type = htons(SHDR_TYPE_CREQ);
    pkt->hdr.pkt_unc_len = htons(sizeof(shipgate_char_data_pkt));
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE | SHDR_RESPONSE);

    /* Fill in the body. */
    pkt->guildcard = htonl(gc);
    pkt->slot = htonl(slot);
    pkt->padding = 0;
    memcpy(pkt->data, cdata, 1052); 

    /* Send it away. */
    return send_crypt(c, sizeof(shipgate_char_data_pkt));
}

/* Send a reply to a GM login request. */
int send_gmreply(ship_t *c, uint32_t gc, uint32_t block, int good, uint8_t p) {
    shipgate_gmlogin_reply_pkt *pkt = (shipgate_gmlogin_reply_pkt *)sendbuf;
    uint16_t flags = good ? SHDR_RESPONSE : SHDR_FAILURE;

    /* Clear the packet first */
    memset(pkt, 0, sizeof(shipgate_gmlogin_reply_pkt));

    /* Fill in the response. */
    pkt->hdr.pkt_len = htons(sizeof(shipgate_gmlogin_reply_pkt));
    pkt->hdr.pkt_type = htons(SHDR_TYPE_GMLOGIN);
    pkt->hdr.pkt_unc_len = htons(sizeof(shipgate_gmlogin_reply_pkt));
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE | flags);

    pkt->guildcard = htonl(gc);
    pkt->block = htonl(block);
    pkt->priv = p;

    return send_crypt(c, sizeof(shipgate_gmlogin_reply_pkt));
}

/* Send a client/game update packet. */
int send_counts(ship_t *c, uint32_t ship_id, uint16_t clients, uint16_t games) {
    shipgate_cnt_pkt *pkt = (shipgate_cnt_pkt *)sendbuf;

    /* Clear the packet first */
    memset(pkt, 0, sizeof(shipgate_cnt_pkt));

    /* Fill in the response. */
    pkt->hdr.pkt_len = htons(sizeof(shipgate_cnt_pkt));
    pkt->hdr.pkt_type = htons(SHDR_TYPE_COUNT);
    pkt->hdr.pkt_unc_len = htons(sizeof(shipgate_cnt_pkt));
    pkt->hdr.flags = htons(SHDR_NO_DEFLATE);

    pkt->clients = htons(clients);
    pkt->games = htons(games);
    pkt->ship_id = htonl(ship_id);

    return send_crypt(c, sizeof(shipgate_cnt_pkt));
}

/* Send an error packet to a ship */
int send_error(ship_t *c, uint16_t type, uint16_t flags, uint32_t err,
               uint8_t *data, int data_sz) {
    shipgate_error_pkt *pkt = (shipgate_error_pkt *)sendbuf;
    uint16_t sz;

    /* These were first added in protocol version 1. */
    if(c->proto_ver < 1) {
        return 0;
    }

    /* Make sure the data size is valid */
    if(data_sz > 65536 - sizeof(shipgate_error_pkt)) {
        return -1;
    }

    /* Clear the header of the packet */
    memset(pkt, 0, sizeof(shipgate_error_pkt));
    sz = sizeof(shipgate_error_pkt) + data_sz;

    /* Fill it in */
    pkt->hdr.pkt_len = htons(sz);
    pkt->hdr.pkt_type = htons(type);
    pkt->hdr.pkt_unc_len = pkt->hdr.pkt_len;
    pkt->hdr.flags = htons(flags);
    pkt->error_code = htonl(err);
    memcpy(pkt->data, data, data_sz);

    return send_crypt(c, sz);
}
