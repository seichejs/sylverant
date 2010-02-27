/*
    Sylverant Login Server
    Copyright (C) 2009, 2010 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sylverant/debug.h>
#include <sylverant/database.h>
#include <sylverant/quest.h>

#include "login.h"
#include "login_packets.h"

extern sylverant_quest_list_t qlist;

/* Handle a client's login request packet. */
static int handle_login(login_client_t *c, login_dclogin_pkt *pkt) {
    uint32_t gc;
    char query[256], dc_id[32], serial[32], access[32];
    void *result;
    char **row;

    c->language_code = pkt->language_code;

    /* Escape all the important strings. */
    sylverant_db_escape_str(&conn, dc_id, pkt->dc_id, 8);
    sylverant_db_escape_str(&conn, serial, pkt->serial, 8);
    sylverant_db_escape_str(&conn, access, pkt->access_key, 8);

    sprintf(query, "SELECT guildcard FROM dreamcast_clients WHERE dc_id='%s' "
            "AND serial_number='%s' AND access_key='%s'", dc_id, serial,
            access);

    /* If we can't query the database, fail. */
    if(sylverant_db_query(&conn, query)) {
        return -1;
    }

    result = sylverant_db_result_store(&conn);

    if((row = sylverant_db_result_fetch(result))) {
        /* We have seen this client before, save their guildcard for use. */
        gc = (uint32_t)strtoul(row[0], NULL, 0);
        sylverant_db_result_free(result);
    }
    else {
        sylverant_db_result_free(result);

        /* Assign a nice fresh new guildcard number to the client. */
        sprintf(query, "INSERT INTO guildcards (account_id) VALUES (NULL)");

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        /* Grab the new guildcard for the user. */
        gc = (uint32_t)sylverant_db_insert_id(&conn);

        /* Add the client into our database. */
        sprintf(query, "INSERT INTO dreamcast_clients (guildcard, "
                "serial_number, access_key, dc_id) VALUES ('%u', '%s', '%s', "
                "'%s')", gc, serial, access, dc_id);

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }
    }

    return send_dc_security(c, gc, NULL, 0);
}

/* Handle a client's login request packet (yes, this function is the same as the
   one above, but it uses a different structure). */
static int handle_v2login(login_client_t *c, login_dcv2login_pkt *pkt) {
    uint32_t gc;
    char query[256], dc_id[32], serial[32], access[32];
    void *result;
    char **row;

    /* Escape all the important strings. */
    sylverant_db_escape_str(&conn, dc_id, pkt->dc_id, 8);
    sylverant_db_escape_str(&conn, serial, pkt->serial, 8);
    sylverant_db_escape_str(&conn, access, pkt->access_key, 8);

    if(c->type != CLIENT_TYPE_PC) {
        sprintf(query, "SELECT guildcard FROM dreamcast_clients WHERE "
                "dc_id='%s' AND serial_number='%s' AND access_key='%s'", dc_id,
                serial, access);
    }
    else {
        sprintf(query, "SELECT guildcard FROM dreamcast_clients WHERE "
                "serial_number='%s' AND access_key='%s'", serial, access);
    }

    /* If we can't query the database, fail. */
    if(sylverant_db_query(&conn, query)) {
        return -1;
    }

    result = sylverant_db_result_store(&conn);

    if((row = sylverant_db_result_fetch(result))) {
        /* We have seen this client before, save their guildcard for use. */
        gc = (uint32_t)strtoul(row[0], NULL, 0);
        sylverant_db_result_free(result);
    }
    else if(c->type == CLIENT_TYPE_PC) {
        sprintf(query, "SELECT guildcard FROM dreamcast_clients WHERE "
                "access_key='%s' AND serial_number='0'", access);

        /* If we can't query the database, fail. */
        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        result = sylverant_db_result_store(&conn);

        if((row = sylverant_db_result_fetch(result))) {
            /* We have seen this client before, save their guildcard for
               use and set the serial number. */
            gc = (uint32_t)strtoul(row[0], NULL, 0);
            sylverant_db_result_free(result);

            sprintf(query, "UPDATE dreamcast_clients SET serial_number='%s' "
                    "WHERE guildcard='%u'", serial, gc);

            if(sylverant_db_query(&conn, query)) {
                return -1;
            }
        }
        else {
            /* Disconnect the unregistered user. */
            return -1;
        }
    }
    else {
        sylverant_db_result_free(result);

        /* Assign a nice fresh new guildcard number to the client. */
        sprintf(query, "INSERT INTO guildcards (account_id) VALUES (NULL)");

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        /* Grab the new guildcard for the user. */
        gc = (uint32_t)sylverant_db_insert_id(&conn);

        /* Add the client into our database. */
        sprintf(query, "INSERT INTO dreamcast_clients (guildcard, "
                "serial_number, access_key, dc_id) VALUES ('%u', '%s', '%s', "
                "'%s')", gc, serial, access, dc_id);

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }
    }

    c->guildcard = gc;

    /* Force them to send us a 0x9D so we have their language code, since this
       packet doesn't have it. */
    return send_simple(c, LOGIN_DCV2_LOGINA_TYPE, 2);
}

/* The next few functions look the same pretty much... All added for gamecube
   support. */
static int handle_gchlcheck(login_client_t *c, login_gc_hlcheck_pkt *pkt) {
    uint32_t account, time, gc;
    char query[256], serial[32], access[32];
    void *result;
    char **row;
    unsigned char hash[16];
    int i;

    /* Escape all the important strings. */
    sylverant_db_escape_str(&conn, serial, pkt->serial, 8);
    sylverant_db_escape_str(&conn, access, pkt->access_key, 12);

    sprintf(query, "SELECT guildcard FROM gamecube_clients WHERE "
            "serial_number='%s' AND access_key='%s'", serial, access);

    /* If we can't query the database, fail. */
    if(sylverant_db_query(&conn, query)) {
        return -1;
    }

    result = sylverant_db_result_store(&conn);

    if((row = sylverant_db_result_fetch(result))) {
        /* The client has at least registered, check the password. */
        gc = (uint32_t)strtoul(row[0], NULL, 0);

        sylverant_db_result_free(result);

        /* We need the account to do that though... */
        sprintf(query, "SELECT account_id FROM guildcards WHERE guildcard='%u'",
                gc);

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        result = sylverant_db_result_store(&conn);

        if(!(row = sylverant_db_result_fetch(result))) {
            return -1;
        }

        account = (uint32_t)strtoul(row[0], NULL, 0);
        sylverant_db_result_free(result);

        sprintf(query, "SELECT password, regtime FROM account_data WHERE "
                "account_id='%u'", account);

        /* If we can't query the DB, fail. */
        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        result = sylverant_db_result_store(&conn);

        if((row = sylverant_db_result_fetch(result))) {
            /* Check the password. */
            sprintf(query, "%s_%s_salt", pkt->password, row[1]);
            md5(query, strlen(query), hash);

            query[0] = '\0';
            for(i = 0; i < 16; ++i) {
                sprintf(query, "%s%02x", query, hash[i]);
            }

            for(i = 0; i < strlen(row[0]); ++i) {
                row[0][i] = tolower(row[0][i]);
            }

            if(!strcmp(row[0], query)) {
                sylverant_db_result_free(result);
                return send_simple(c, LOGIN_DCV2_LOGINA_TYPE, 1);
            }
        }
    }

    sylverant_db_result_free(result);

    /* If we get here, we didn't find them, bail out. */
    return -1;
}

static int handle_gcloginc(login_client_t *c, login_gc_loginc_pkt *pkt) {
    uint32_t account, time, gc;
    char query[256], serial[32], access[32];
    void *result;
    char **row;
    unsigned char hash[16];
    int i;

    /* Escape all the important strings. */
    sylverant_db_escape_str(&conn, serial, pkt->serial, 8);
    sylverant_db_escape_str(&conn, access, pkt->access_key, 12);

    sprintf(query, "SELECT guildcard FROM gamecube_clients WHERE "
            "serial_number='%s' AND access_key='%s'", serial, access);

    /* If we can't query the database, fail. */
    if(sylverant_db_query(&conn, query)) {
        return -1;
    }

    result = sylverant_db_result_store(&conn);

    if((row = sylverant_db_result_fetch(result))) {
        /* The client has at least registered, check the password. */
        gc = (uint32_t)strtoul(row[0], NULL, 0);

        sylverant_db_result_free(result);

        /* We need the account to do that though... */
        sprintf(query, "SELECT account_id FROM guildcards WHERE guildcard='%u'",
                gc);

        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        result = sylverant_db_result_store(&conn);

        if(!(row = sylverant_db_result_fetch(result))) {
            return -1;
        }

        account = (uint32_t)strtoul(row[0], NULL, 0);
        sylverant_db_result_free(result);

        sprintf(query, "SELECT password, regtime FROM account_data WHERE "
                "account_id='%u'", account);

        /* If we can't query the DB, fail. */
        if(sylverant_db_query(&conn, query)) {
            return -1;
        }

        result = sylverant_db_result_store(&conn);

        if((row = sylverant_db_result_fetch(result))) {
            /* Check the password. */
            sprintf(query, "%s_%s_salt", pkt->password, row[1]);
            md5(query, strlen(query), hash);

            query[0] = '\0';
            for(i = 0; i < 16; ++i) {
                sprintf(query, "%s%02x", query, hash[i]);
            }

            for(i = 0; i < strlen(row[0]); ++i) {
                row[0][i] = tolower(row[0][i]);
            }

            if(!strcmp(row[0], query)) {
                sylverant_db_result_free(result);
                return send_simple(c, LOGIN_GC_LOGINC_TYPE, 1);
            }
        }
    }

    sylverant_db_result_free(result);

    /* If we get here, we didn't find them, bail out. */
    return -1;
}

static int handle_gclogine(login_client_t *c, login_login_de_pkt *pkt) {
    uint32_t gc;
    char query[256], serial[32], access[32];
    void *result;
    char **row;

    c->language_code = pkt->language_code;

    /* Escape all the important strings. */
    sylverant_db_escape_str(&conn, serial, pkt->serial, 8);
    sylverant_db_escape_str(&conn, access, pkt->access_key, 12);

    sprintf(query, "SELECT guildcard FROM gamecube_clients WHERE "
            "serial_number='%s' AND access_key='%s'", serial, access);

    /* If we can't query the database, fail. */
    if(sylverant_db_query(&conn, query)) {
        return -1;
    }

    result = sylverant_db_result_store(&conn);

    if((row = sylverant_db_result_fetch(result))) {
        /* Grab the client's guildcard number. */
        gc = (uint32_t)strtoul(row[0], NULL, 0);
        sylverant_db_result_free(result);

        return send_dc_security(c, gc, NULL, 0);
    }

    sylverant_db_result_free(result);

    /* If we get here, we didn't find them, bail out. */
    return -1;
}

static int handle_logind(login_client_t *c, login_login_de_pkt *pkt) {
    /* We made clients send this packet just specifically to grab the language
       code... All the real checking has been done elsewhere. */
    c->language_code = pkt->language_code;

    return send_dc_security(c, c->guildcard, NULL, 0);
}

/* Handle a client's ship select packet. */
static int handle_ship_select(login_client_t *c,
                              dc_login_ship_select_pkt *pkt) {
    extern int ship_transfer(login_client_t *c, uint32_t shipid);

    /* Were we on a ship select or the offline quest menu? */
    if(LE32(pkt->menu_id) == 0x00120000) {
        /* Check if they picked the "Offline Quests" entry. */
        if(LE32(pkt->item_id) == 0xDEADBEEF && qlist.cat_count == 1) {
            return send_quest_list(c, &qlist.cats[0]);
        }
        else {
            return ship_transfer(c, LE32(pkt->item_id));
        }
    }
    else {
        return send_quest(c, &qlist.cats[0].quests[LE32(pkt->item_id)]);
    }
}

/* Process one login packet. */
int process_dclogin_packet(login_client_t *c, void *pkt) {
    dc_pkt_header_t *dc = (dc_pkt_header_t *)pkt;
    pc_pkt_header_t *pc = (pc_pkt_header_t *)pkt;
    uint8_t type;

    if(c->type == CLIENT_TYPE_DC || c->type == CLIENT_TYPE_GC) {
        type = dc->pkt_type;
    }
    else {
        type = pc->pkt_type;
    }

    debug(DBG_LOG, "DC/PC/GC: Receieved type 0x%02X\n", type);

    switch(type) {
        case LOGIN_DC_LOGIN0_TYPE:
            /* XXXX: Do something with this sometime. */
            return send_simple(c, LOGIN_DC_LOGIN0_TYPE, 1);

        case LOGIN_DC_LOGIN2_TYPE:
            /* XXXX: Do something with this sometime. */
            return send_simple(c, LOGIN_DC_LOGIN2_TYPE, 1);

        case LOGIN_CLIENT_LOGIN_TYPE:
            /* XXXX: Figure this all out sometime. */
            return handle_login(c, (login_dclogin_pkt *)pkt);

        case LOGIN_DCV2_LOGINA_TYPE:
            /* XXXX: You had to switch packets on me, didn't you Sega? */
            return handle_v2login(c, (login_dcv2login_pkt *)pkt);

        case LOGIN_DC_CHECKSUM_TYPE:
            /* XXXX: ??? */
            return send_simple(c, LOGIN_DC_CHECKSUM_REPLY_TYPE, 1);

        case LOGIN_TIMESTAMP_TYPE:
            /* XXXX: Actually, I've got nothing here. */
            return send_timestamp(c);

        case LOGIN_DC_SHIP_LIST_REQ_TYPE:
        case LOGIN_SHIP_LIST_TYPE:
            /* XXXX: I don't have anything here either, but thought I'd be
               funny anyway. */
            return send_ship_list(c);

        case LOGIN_INFO_REQUEST_TYPE:
            /* XXXX: Actually send something relevant! */
            return send_info_reply(c, "Nothing here.");

        case LOGIN_SHIP_SELECT_TYPE:
            /* XXXX: This might actually work, at least if there's a ship. */
            return handle_ship_select(c, (dc_login_ship_select_pkt *)pkt);

        case LOGIN_GC_VERIFY_LICENSE_TYPE:
            /* XXXX: Why in the world do they duplicate so much data here? */
            return handle_gchlcheck(c, (login_gc_hlcheck_pkt *)pkt);

        case LOGIN_GC_LOGINC_TYPE:
            /* XXXX: Yep... check things here too. */
            return handle_gcloginc(c, (login_gc_loginc_pkt *)pkt);

        case LOGIN_GC_LOGINE_TYPE:
            /* XXXX: One final check, and give them their guildcard. */
            return handle_gclogine(c, (login_login_de_pkt *)pkt);

        case LOGIN_LOGIND_TYPE:
            /* XXXX: All this work for a language code... */
            return handle_logind(c, (login_login_de_pkt *)pkt);

        default:
            return -3;
    }

    return 0;
}