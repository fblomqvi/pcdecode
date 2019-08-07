/*
 * Convenience functions for printing error messages.
 * Copyright (C) 2016 Ferdinand Blomqvist
 *
 * This file is part of pcdecode.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Ferdinand Blomqvist.
 */

#include "dbg.h"
#include <stdarg.h>

void fprintf_we(FILE *file, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	fprintf(file, "; %s\n", errno ? strerror(errno) : "None");
}
