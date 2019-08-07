/* simulate.h
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

#ifndef FB_PCDECODE_SIMULATE_H
#define FB_PCDECODE_SIMULATE_H

#include "product_code.h"
#include "rng.h"

struct options {
	const gsl_rng_type *rng_type;
	int (*alg)(struct pc *, uint16_t *, struct stats *);
	size_t cword_num;
	size_t min_errs;
	size_t nthreads;
	unsigned long seed;
	double fer_cutoff;
	double p_start;
	double p_stop;
	double p_step;
	double p_halve_at;
	size_t rows;
	size_t cols;
	size_t symsize;
	size_t gfpoly;
	size_t r_fcr;
	size_t r_prim;
	size_t r_nroots;
	size_t c_fcr;
	size_t c_prim;
	size_t c_nroots;
};


int run_simulation(struct options *opt);

#endif /* FB_PCDECODE_SIMULATE_H */
