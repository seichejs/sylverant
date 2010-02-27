/*
    Sylverant Ship Server
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
#include <time.h>
#include <sys/time.h>
#include <iconv.h>
#include <string.h>

#include "utils.h"

void print_packet(unsigned char *pkt, int len) {
    unsigned char *pos = pkt, *row = pkt;
    int line = 0, type = 0;

    /* Print the packet both in hex and ASCII. */
    while(pos < pkt + len) {
        if(type == 0) {
            printf("%02X ", *pos);
        }
        else {
            if(*pos >= 0x20 && *pos < 0x7F) {
                printf("%c", *pos);
            }
            else {
                printf(".");
            }
        }

        ++line;
        ++pos;

        if(line == 16) {
            if(type == 0) {
                printf("\t");
                pos = row;
                type = 1;
                line = 0;
            }
            else {
                printf("\n");
                line = 0;
                row = pos;
                type = 0;
            }
        }
    }

    /* Finish off the last row's ASCII if needed. */
    if(len & 0x1F) {
        /* Put spaces in place of the missing hex stuff. */
        while(line != 16) {
            printf("   ");
            ++line;
        }

        pos = row;
        printf("\t");

        /* Here comes the ASCII. */
        while(pos < pkt + len) {
            if(*pos >= 0x20 && *pos < 0x7F) {
                printf("%c", *pos);
            }
            else {
               printf(".");
            }

            ++pos;
        }

        printf("\n");
    }
}

int dc_bug_report(ship_client_t *c, dc_simple_mail_pkt *pkt) {
    struct timeval rawtime;
    struct tm cooked;
    char filename[64];
    char text[0x91];
    FILE *fp;

    /* Get the timestamp */
    gettimeofday(&rawtime, NULL);
    
    /* Get UTC */
    gmtime_r(&rawtime.tv_sec, &cooked);

    /* Figure out the name of the file we'll be writing to. */
    sprintf(filename, "bugs/%u.%02u.%02u.%02u.%02u.%02u.%03u-%d",
            cooked.tm_year + 1900, cooked.tm_mon + 1, cooked.tm_mday,
            cooked.tm_hour, cooked.tm_min, cooked.tm_sec,
            (unsigned int)(rawtime.tv_usec / 1000), c->guildcard);

    strncpy(text, pkt->stuff, 0x90);
    text[0x90] = '\0';

    /* Attempt to open up the file. */
    fp = fopen(filename, "w");

    if(!fp) {
        return -1;
    }

    /* Write the bug report out. */
    fprintf(fp, "Bug report from %s (%d) v%d @ %u.%02u.%02u %02u:%02u:%02u\n\n",
            c->pl->name, c->guildcard, c->version, cooked.tm_year + 1900,
            cooked.tm_mon + 1, cooked.tm_mday, cooked.tm_hour, cooked.tm_min,
            cooked.tm_sec);

    fprintf(fp, "%s", text);

    fclose(fp);

    return send_txt(c, "\tE\tC7Thank you for your report");
}

int pc_bug_report(ship_client_t *c, pc_simple_mail_pkt *pkt) {
    struct timeval rawtime;
    struct tm cooked;
    char filename[64];
    char text[0x91];
    FILE *fp;
    iconv_t ic = iconv_open("SHIFT_JIS", "UTF-16LE");
    char *inptr, *outptr;
    size_t in, out;

    if(ic == (iconv_t)-1) {
        return -1;
    }

    /* Get the timestamp */
    gettimeofday(&rawtime, NULL);

    /* Get UTC */
    gmtime_r(&rawtime.tv_sec, &cooked);

    /* Figure out the name of the file we'll be writing to. */
    sprintf(filename, "bugs/%u.%02u.%02u.%02u.%02u.%02u.%03u-%d",
            cooked.tm_year + 1900, cooked.tm_mon + 1, cooked.tm_mday,
            cooked.tm_hour, cooked.tm_min, cooked.tm_sec,
            (unsigned int)(rawtime.tv_usec / 1000), c->guildcard);

    in = 0x120;
    out = 0x90;
    inptr = pkt->stuff;
    outptr = text;
    iconv(ic, &inptr, &in, &outptr, &out);
    iconv_close(ic);

    text[0x90] = '\0';

    /* Attempt to open up the file. */
    fp = fopen(filename, "w");

    if(!fp) {
        return -1;
    }

    /* Write the bug report out. */
    fprintf(fp, "Bug report from %s (%d) v%d @ %u.%02u.%02u %02u:%02u:%02u\n\n",
            c->pl->name, c->guildcard, c->version, cooked.tm_year + 1900,
            cooked.tm_mon + 1, cooked.tm_mday, cooked.tm_hour, cooked.tm_min,
            cooked.tm_sec);

    fprintf(fp, "%s", text);

    fclose(fp);

    return send_txt(c, "\tE\tC7Thank you for your report");
}
