/* algorithm.c
 * Copyright (C) 2019 Ferdinand Blomqvist
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
 * Written by Ferdinand Blomqvist. */

#include "dbg.h"
#include "algorithm.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct algorithm {
	const char *name;
	alg_ptr ptr;
};

static struct algorithm algs[] = {
	{ "gmd",    pc_decode_gmd     },
	{ "gd",	    pc_decode_gd      },
	{ "iter",   pc_decode_iter    },
	{ "eras",   pc_decode_eras    },
	{ "itergd", pc_decode_iter_gd },
	{ "erasgd", pc_decode_eras_gd }
};

alg_ptr algorithm_by_name(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(algs); i++)
		if (!strcmp(name, algs[i].name))
			return algs[i].ptr;

	return NULL;
}

const char *algorithm_get_name(alg_ptr alg)
{
	for (size_t i = 0; i < ARRAY_SIZE(algs); i++)
		if (alg == algs[i].ptr)
			return algs[i].name;

	return NULL;
}

int algorithm_print_names(FILE *file)
{
	libcheck(fprintf(file, "Available algorithms are:\n") > 0, "printing error");
	for (size_t i = 0; i < ARRAY_SIZE(algs); i++)
		libcheck(fprintf(file, "%s\n", algs[i].name) > 0, "printing error");

	return 0;

error:
	return -1;
}
