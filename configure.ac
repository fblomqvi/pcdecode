# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([pcdecode], [0.1], [https://github.com/fblomqvi/pcdecode])
AC_CONFIG_SRCDIR([src/product_code.c])
AC_CONFIG_HEADERS([config.h])

# Put autotools auxiliary files in subdirectories to reduce clutter:
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# Init automake, and specify this program use relaxed structures.
# i.e. this program doesn't follow the gnu coding standards, and doesn't have
# ChangeLog, COPYING, AUTHORS, INSTALL, README etc. files.
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects silent-rules])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

# Checks for libraries.
PKG_CHECK_MODULES([GSL], [gsl], [],
	     [AC_MSG_ERROR([Cannot find the GSL library!])])

AC_CHECK_LIB([rs], [rs_decode], [],
	     [AC_MSG_ERROR([Cannot find librs!])])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stddef.h stdint.h stdlib.h string.h])

if [test -z "$HAVE_limits.h"] ; then
    AC_MSG_ERROR([Cannot find limits.h!])
fi
if [test -z "$HAVE_stddef.h"] ; then
    AC_MSG_ERROR([Cannot find stddef.h!])
fi
if [test -z "$HAVE_stdint.h"] ; then
    AC_MSG_ERROR([Cannot find stdint.h!])
fi
if [test -z "$HAVE_stdlib.h"] ; then
    AC_MSG_ERROR([Cannot find stdlib.h!])
fi
if [test -z "$HAVE_string.h"] ; then
    AC_MSG_ERROR([Cannot find string.h])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime memset strerror strtoul])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
