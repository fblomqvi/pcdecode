/*
 * product_code.c
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

#include "product_code.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

struct estrat {
	int *strat;
	size_t size;
	int viable;
};

size_t get_gfpoly(size_t symsize)
{
	static size_t gfpolys[] = {
		0x7,	0xb,	0x13,	0x25,	0x43,
		0x89,	0x11d,	0x211,	0x409,	0x805,
		0x1053,	0x201b,	0x4443,	0x8003,	0x1100
	};

	return gfpolys[symsize - 2];
}

struct pc *pc_init(size_t symsize, size_t gfpoly,
		   size_t r_fcr, size_t r_prim, size_t r_nroots,
		   size_t c_fcr, size_t c_prim, size_t c_nroots,
		   size_t rows, size_t cols)
{
	if (cols <= r_nroots || rows <= c_nroots)
		return NULL;

	struct pc *pc = calloc(1, sizeof(*pc));
	if (!pc)
		return NULL;

	pc->row_code = rs_init(symsize, gfpoly, r_fcr, r_prim, r_nroots);
	if (!pc->row_code)
		goto err;

	pc->col_code = rs_init(symsize, gfpoly, c_fcr, c_prim, c_nroots);
	if (!pc->col_code)
		goto err;

	pc->nstrat = (rs_mind(pc->col_code) + 1) / 2;
	pc->es = malloc(pc->nstrat * sizeof(*pc->es));
	if (!pc->es)
		goto err;

	size_t slen = pc->row_code->nroots;
	pc->es_buffer = malloc(pc->nstrat * slen * sizeof(*pc->es_buffer));
	if (!pc->es_buffer)
		goto err;

	for (size_t i = 0; i < pc->nstrat; i++)
		pc->es[i].strat = pc->es_buffer + (i * slen);

	pc->x_buf = malloc(2 * rows * cols * sizeof(*pc->x_buf));
	if (!pc->x_buf)
		goto err;

	pc->y_buf = pc->x_buf + rows * cols;
	pc->rows = rows;
	pc->cols = cols;

	size_t tmp = (rs_mind(pc->row_code) + 1) / 2;
	pc->nstrat_bound = tmp < pc->nstrat ? tmp : pc->nstrat;

	return pc;

err:
	free(pc->es_buffer);
	free(pc->es);
	rs_free(pc->col_code);
	rs_free(pc->row_code);
	free(pc);
	return NULL;
}

void pc_free(struct pc *pc)
{
	if (!pc)
		return;

	free(pc->x_buf);
	free(pc->es_buffer);
	free(pc->es);
	rs_free(pc->col_code);
	rs_free(pc->row_code);
	free(pc);
}

void pc_encode(struct pc *pc, uint16_t *data)
{
	size_t row_dlen = pc->cols - pc->row_code->nroots;
	for (size_t i = 0; i < row_dlen; i++)
		rs_encode(pc->col_code, &data[i], pc->rows, pc->cols);

	uint16_t *end = data + pc_len(pc);
	for (uint16_t *ptr = data; ptr < end; ptr += pc->cols)
		rs_encode(pc->row_code, ptr, pc->cols, 1);
}

static void reset_estrat(struct pc *pc)
{
	for (size_t i = 0; i < pc->nstrat; i++) {
		struct estrat *es = &pc->es[i];
		es->size = 0;
		es->viable = 1;
	}
}

static void add_to_estrat(struct pc *pc, size_t col, int weight)
{
	size_t slen = pc->row_code->nroots;

	if (weight < 0)
		weight = pc->nstrat;

	for (int i = 0; i < weight; i++) {
		struct estrat *es = &pc->es[i];
		if (es->size < slen)
			es->strat[es->size++] = col;
		else
			es->viable = 0;
	}
}

static void estrat_disable_duplicates(struct pc *pc)
{
	for (size_t i = 0; i < pc->nstrat - 1; i++) {
		if (!pc->es[i].viable)
			continue;

		/* ps->es[i] is a superset of ps->es[i+1] so both are
		 * equal if they have the same size */
		if (pc->es[i].size == pc->es[i + 1].size)
			pc->es[i].viable = 0;
	}
}

static void estrat_remove_unnecessary(struct pc *pc)
{
	size_t d = rs_mind(pc->row_code);

	int i = pc->nstrat - 1;
	do {
		while (!pc->es[i].viable || (d - pc->es[i].size) % 2)
			if (--i == 0)
				return;

		int j = i - 1;
		while (!pc->es[j].viable)
			if (--j < 0)
				return;

		if (pc->es[i].size == pc->es[j].size - 1)
			pc->es[i].viable = 0;

		i = j;
	} while (i > 0);
}

static size_t estrat_count_viable(struct pc *pc)
{
	size_t w = 0;

	for (long i = pc->nstrat - 1; i >= 0; i--)
		if (pc->es[i].viable)
			w++;

	return w;
}

static inline double calc_weight(int e, int t, size_t d)
{ return e < 0 || e > t ? 0 : ((double) d - 2 * e) / d; }

static void decode_columns_gmd(struct pc *pc, uint16_t *data, double *weights)
{
	struct rs_code *rs = pc->col_code;
	size_t d = rs->nroots + 1;
	int t = rs->nroots / 2;

	reset_estrat(pc);

	for (size_t i = 0; i < pc->cols; i++) {
		int ret = rs_decode(rs, &data[i], pc->rows,
				    pc->cols, NULL, 0, NULL);
		add_to_estrat(pc, i, ret);
		weights[i] = calc_weight(ret, t, d);
	}

	estrat_disable_duplicates(pc);
	estrat_remove_unnecessary(pc);
}

static double calc_gdm(const double *weights, size_t len,
		       const int *errpos, int nerr)
{
	uint16_t errs[len];

	memset(errs, 0, len * sizeof(*errs));
	for (int i = 0; i < nerr; i++)
		errs[errpos[i]] = 1;

	double sum = len;
	for (size_t i = 0; i < len; i++)
		sum += errs[i] ? weights[i] : -weights[i];

	return sum;
}

static size_t update_stats_and_check_if_viable(struct pc *pc,
					       struct stats *s, int gmd)
{
	size_t viable = estrat_count_viable(pc);
	s->viable += viable;
	s->cdec += pc->cols;
	s->max += pc->nstrat_bound;
	if (gmd)
		s->rdec_max += (pc->nstrat_bound - 1) + pc->rows;
	else
		s->rdec_max += pc->nstrat_bound * pc->rows;

	return viable;
}

/* Returns zero on success and 1 on failure */
static int gmd_decode_row(struct pc *pc, int r, int *i, uint16_t *data,
			  uint16_t *x, double *weights, struct stats *s)
{
	struct rs_code *rs = pc->row_code;
	uint16_t *y = pc->y_buf;
	int errors[rs->nroots];
	int fail = 1;

	for (; *i >= 0; (*i)--) {
		struct estrat *es = &pc->es[*i];

		if (!es->viable)
			continue;

		// Copy row before decoding
		memcpy(y, x + r * pc->cols, pc->cols * sizeof(*x));
		s->rdec++;

		int ret = rs_decode(rs, y, pc->cols, 1,
				    es->strat, es->size, errors);
		if (ret < 0)
			continue;

		double dist = calc_gdm(weights, pc->cols, errors, ret);
		if (dist < rs->nroots + 1) {
			memcpy(data + r * pc->cols, y, pc->cols * sizeof(*y));
			fail = 0;
			break;
		}
	}

	return fail;
}

int pc_decode_gmd(struct pc *pc, uint16_t *data, struct stats *s)
{
	double weights[pc->cols];
	uint16_t *x = pc->x_buf;

	memcpy(x, data, pc_len(pc) * sizeof(*x));
	decode_columns_gmd(pc, x, weights);

	size_t viable = update_stats_and_check_if_viable(pc, s, 1);
	if (viable == 0)
		return -1;

	int i = pc->nstrat - 1;

	for (size_t r = 0; r < pc->rows; r++) {
		int fail = gmd_decode_row(pc, r, &i, data, x, weights, s);
		if (fail)
			return -1;
	}

	return 0;
}

static void gd_decode_row(struct pc *pc, int r, uint16_t *data,
			  uint16_t *x, double *weights, struct stats *s)
{
	struct rs_code *rs = pc->row_code;
	uint16_t *y = pc->y_buf;
	uint16_t *tmp = y + pc->cols;
	double min_dist = INFINITY;
	int errors[rs->nroots];
	int fail = 1;

	for (int i = pc->nstrat - 1; i >= 0; i--) {
		struct estrat *es = &pc->es[i];

		if (!es->viable)
			continue;

		// Copy row before decoding
		memcpy(y, x + r * pc->cols, pc->cols * sizeof(*x));
		s->rdec++;

		int ret = rs_decode(rs, y, pc->cols, 1,
				    es->strat, es->size, errors);
		if (ret < 0)
			continue;

		double dist = calc_gdm(weights, pc->cols, errors, ret);
		if (dist < rs->nroots + 1) {
			memcpy(data + r * pc->cols, y, pc->cols * sizeof(*y));
			fail = 0;
			break;
		} else if (dist < min_dist) {
			memcpy(tmp, y, pc->cols * sizeof(*y));
		}
	}

	if (fail)
		memcpy(data + r * pc->cols, tmp, pc->cols * sizeof(*y));
}

int pc_decode_gd(struct pc *pc, uint16_t *data, struct stats *s)
{
	double weights[pc->cols];
	uint16_t *x = pc->x_buf;

	memcpy(x, data, pc_len(pc) * sizeof(*x));
	decode_columns_gmd(pc, x, weights);

	size_t viable = update_stats_and_check_if_viable(pc, s, 0);
	if (viable == 0)
		return -1;

	for (size_t r = 0; r < pc->rows; r++)
		gd_decode_row(pc, r, data, x, weights, s);

	return 0;
}

int pc_decode_iter(struct pc *pc, uint16_t *data, struct stats *s)
{
	struct rs_code *rs_c = pc->col_code;
	struct rs_code *rs_r = pc->row_code;
	size_t len = pc_len(pc);
	uint16_t *prev = pc->x_buf;
	uint16_t *y = pc->y_buf;
	size_t rounds = 0;
	int fail;

	memcpy(y, data, len * sizeof(*y));

	do {
		memcpy(prev, y, len * sizeof(*y));
		rounds++;
		fail = 0;

		// Decode columns
		for (size_t i = 0; i < pc->cols; i++) {
			fail |= rs_decode(rs_c, &y[i], pc->rows,
					  pc->cols, NULL, 0, NULL);
		}

		// Decode rows
		uint16_t *end = y + len;
		for (uint16_t *ptr = y; ptr < end; ptr += pc->cols) {
			fail |= rs_decode(rs_r, ptr, pc->cols,
					  1, NULL, 0, NULL);
		}
	} while (memcmp(y, prev, len * sizeof(*y)) != 0);

	if (!fail)
		memcpy(data, y, len * sizeof(*y));

	s->cdec += pc->cols * rounds;
	s->rdec += pc->rows * rounds;
	return fail;
}

int pc_decode_eras(struct pc *pc, uint16_t *data, struct stats *s)
{
	int ret = pc_decode_iter(pc, data, s);
	if (!ret)
		return ret;

	s->alg2++;

	struct rs_code *rs_c = pc->col_code;
	struct rs_code *rs_r = pc->row_code;
	size_t len = pc_len(pc);
	uint16_t *prev = pc->x_buf;
	uint16_t *y = pc->y_buf;
	size_t rounds = 1;
	int fail;
	int col_eras[pc->cols], col_eras_idx[pc->cols];
	int row_eras[pc->cols], row_eras_idx[pc->cols];
	int col_eras_count = 0;
	int row_eras_count = 0;

	// Decode columns
	for (size_t i = 0; i < pc->cols; i++) {
		col_eras[i] = rs_decode(rs_c, &y[i], pc->rows,
					pc->cols, NULL, 0, NULL);
		if (col_eras[i])
			col_eras_count++;
	}

	// Decode rows
	for (size_t i = 0; i < pc->rows; i++) {
		row_eras[i] = rs_decode(rs_r, &y[i * pc->cols], pc->cols,
					1, NULL, 0, NULL);
		if (row_eras[i])
			row_eras_idx[row_eras_count++] = i;
	}

	do {
		memcpy(prev, y, len * sizeof(*y));
		rounds++;
		fail = 0;

		// Decode columns
		for (size_t i = 0; i < pc->cols; i++) {
			int eras_count = col_eras[i] ? row_eras_count : 0;
			ret = rs_decode(rs_c, &y[i], pc->rows, pc->cols,
					row_eras_idx, eras_count, NULL);
			fail |= ret;
			if (eras_count && ret >= 0) {
				col_eras[i] = 0;
				col_eras_count--;
			}
		}

		if (col_eras_count) {
			// Build col_eras
			col_eras_count = 0;
			for (size_t i = 0; i < pc->cols; i++)
				if (col_eras[i])
					col_eras_idx[col_eras_count++] = i;
		}

		// Decode rows
		for (size_t i = 0; i < pc->rows; i++) {
			int eras_count = row_eras[i] ? col_eras_count : 0;
			ret = rs_decode(rs_r, &y[i * pc->cols], pc->cols, 1,
					col_eras_idx, eras_count, NULL);
			fail |= ret;
			if (eras_count && ret >= 0) {
				row_eras[i] = 0;
				row_eras_count--;
			}
		}

		if (row_eras_count) {
			// Build col_eras
			row_eras_count = 0;
			for (size_t i = 0; i < pc->rows; i++)
				if (row_eras[i])
					row_eras_idx[row_eras_count++] = i;
		}
	} while (memcmp(y, prev, len * sizeof(*y)) != 0);

	if (!fail)
		memcpy(data, y, len * sizeof(*y));

	s->cdec += pc->cols * rounds;
	s->rdec += pc->rows * rounds;
	return fail;
}

int pc_decode_iter_gd(struct pc *pc, uint16_t *data, struct stats *s)
{
	int ret = pc_decode_iter(pc, data, s);
	if (ret) {
		s->alg2++;
		ret = pc_decode_gd(pc, data, s);
	}

	return ret;
}

int pc_decode_eras_gd(struct pc *pc, uint16_t *data, struct stats *s)
{
	int ret = pc_decode_eras(pc, data, s);
	if (ret) {
		s->alg3++;
		ret = pc_decode_gd(pc, data, s);
	}

	return ret;
}

void pc_print(FILE *file, const struct pc *pc, const char *prefix)
{
	size_t nn = pc->row_code->nn;

	fprintf(file, "%s(%zu, %zu, %zu)_%zu code...\n", prefix,
		pc_len(pc), pc_dim(pc), pc_mind(pc), nn + 1);
	fprintf(file, "%s  Row code: (%zu, %zu, %d)\n", prefix,
		pc->cols, pc->cols - pc->row_code->nroots,
		rs_mind(pc->row_code));
	fprintf(file, "%s  Col code: (%zu, %zu, %d)\n", prefix,
		pc->rows, pc->rows - pc->col_code->nroots,
		rs_mind(pc->col_code));
	fprintf(file, "%s  gfpoly: 0x%.2x\n", prefix, pc->row_code->gfpoly);
}
