/*
 * gen_errors.h
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

#ifndef FB_PCDECODE_GEN_ERRORS_H
#define FB_PCDECODE_GEN_ERRORS_H

#include "product_code.h"
#include "rng.h"

void get_rcw_we(struct pc *pc, uint16_t *c, uint16_t *r,
		int errs, int *errlocs, const gsl_rng *rng);

int get_rcw_channel(struct pc *pc, uint16_t *c, uint16_t *r,
		    double p, const gsl_rng *rng);

#endif /* FB_PCDECODE_GEN_ERRORS_H */
