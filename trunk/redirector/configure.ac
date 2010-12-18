dnl
dnl This file is part of Sylverant PSO Server.
dnl
dnl Copyright (C) 2010 Lawrence Sebald
dnl
dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU Affero General Public License version 3
dnl as published by the Free Software Foundation.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU Affero General Public License for more details.
dnl
dnl You should have received a copy of the GNU Affero General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ(2.61)
AC_INIT([sylvernt_redirector], [0.1.0])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AC_PROG_INSTALL

AC_LANG(C)

AC_CONFIG_SRCDIR([src/redirector.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([memset select socket])

ADD_CFLAGS([-Wall])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
