/*
    This file is part of Sylverant PSO Server.

    Copyright (C) 2009 Lawrence Sebald

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
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "sylverant/debug.h"

static int min_level = DBG_LOG;

void debug_set_threshold(int level) {
    min_level = level;
}

void debug(int level, const char *fmt, ...) {
    va_list args;
    FILE *fp = stdout;
    struct timeval rawtime;
    struct tm cooked;

    /* Make sure we want to receive messages at this level. */
    if(level < min_level) {
        return;
    }

    /* Should we be printing to stderr? */
    if(level >= DBG_STDERR_THRESHOLD) {
        fp = stderr;
    }

    /* Get the timestamp */
    gettimeofday(&rawtime, NULL);

    /* Get UTC */
    gmtime_r(&rawtime.tv_sec, &cooked);

    /* Print the timestamp */
    fprintf(fp, "[%u:%02u:%02u: %02u:%02u:%02u.%03u]: ", cooked.tm_year + 1900,
            cooked.tm_mon + 1, cooked.tm_mday, cooked.tm_hour, cooked.tm_min,
            cooked.tm_sec, (unsigned int)(rawtime.tv_usec / 1000));

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fflush(fp);
}