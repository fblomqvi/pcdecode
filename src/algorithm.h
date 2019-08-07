/* algorithm.h
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

#ifndef FB_PCDECODE_ALGORITHM_H
#define FB_PCDECODE_ALGORITHM_H

#include "product_code.h"

typedef int (*alg_ptr)(struct pc *, uint16_t *, struct stats *);

/*
 * Returns a pointer to the decoding function of the specified algorithm.
 * Returns NULL if the algorithm name is invalid.
 */
alg_ptr algorithm_by_name(const char *name);

/* Returns the name associated with the given decoding algorithm. */
const char *algorithm_get_name(alg_ptr alg);

/* Prints the names of all the available algorithms; one on each line */
int algorithm_print_names(FILE *file);

#endif /* FB_PCDECODE_ALGORITHM_H */
