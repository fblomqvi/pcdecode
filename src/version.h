/*
 * version.h
 * Copyright (C) 2019 Ferdinand Blomqvist
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

#ifndef FB_PCDECODE_VERSION_H
#define FB_PCDECODE_VERSION_H

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#define RELEASE_YEAR 2019
#define RELEASE_MONTH 8
#define RELEASE_DAY 2

#define PACKAGE_NAME "pcdecode"

struct version {
	unsigned major;
	unsigned minor;
	unsigned patch;
};

extern const struct version g_current_version;

int print_version(FILE *file);

#endif /* FB_PCDECODE_VERSION_H */
