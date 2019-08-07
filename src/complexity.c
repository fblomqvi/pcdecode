/* complexity.c
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

#include "complexity.h"
#include "gen_errors.h"
#include "algorithm.h"
#include "rng.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <omp.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct wspace {
	uint16_t *c;    /* sent codeword */
	uint16_t *r;    /* received word */
	int *errlocs;
};

struct thread_args {
	int (*decode)(struct pc *, uint16_t *, struct stats *);
	struct pc *pc;
	struct wspace *ws;
	gsl_rng *rng;
	struct stats s;
};

static struct wspace *alloc_ws(int len)
{
	struct wspace *ws;

	ws = calloc(1, sizeof(*ws));
	if (!ws)
		return NULL;

	ws->c = malloc(2 * len * sizeof(*ws->c));
	if (!ws->c)
		goto err;

	ws->r = ws->c + len;

	ws->errlocs = malloc(len * sizeof(*ws->errlocs));
	if (!ws->errlocs)
		goto err;

	return ws;

err:
	free(ws->errlocs);
	free(ws->c);
	free(ws);
	return NULL;
}

static void free_ws(struct wspace *ws)
{
	if (!ws)
		return;

	free(ws->errlocs);
	free(ws->c);
	free(ws);
}

static void print_start(FILE *file, struct pc *pc,
			const char *prefix, unsigned long seed,
			size_t nthreads, const char *alg)
{
	static const char *const col_heads[] = {
		"number of errors in codeword",
		"number of codewords",
		"viable strategies",
		"max strategies",
		"row decoder actual",
		"row decoder worst case",
		"column decoder actual",
		"decoding failures",
		"reported failures",
	};

	pc_print(file, pc, prefix);
	fprintf(file, "%sAlgorithm: %s\n", prefix, alg);
	fprintf(file, "%sSeed: %lu\n", prefix, seed);
	fprintf(file, "%sThreads: %zu\n", prefix, nthreads);
	for (size_t i = 0; i < ARRAY_SIZE(col_heads); i++)
		fprintf(file, "%s(%zu) %s\n", prefix, i + 1, col_heads[i]);
}

static void print_stats(FILE *file, struct stats *s, int errs)
{
	fprintf(file, "%d %zu %zu %zu %zu %zu %zu %zu %zu\n",
		errs, s->nwords, s->viable, s->max,
		s->rdec, s->rdec_max, s->cdec, s->dwrong, s->rfail);
	fflush(file);
}

/* Test up to error correction capacity */
static int test_uc(struct thread_args *args, int trials, int errs)
{
	struct stats *s = &args->s;
	struct pc *pc = args->pc;
	struct wspace *ws = args->ws;
	int *errlocs = ws->errlocs;
	uint16_t *c = ws->c;
	uint16_t *r = ws->r;
	int len = pc_len(pc);

	memset(s, 0, sizeof(*s));

	for (int j = 0; j < trials; j++) {
		get_rcw_we(pc, c, r, errs, errlocs, args->rng);
		int derrs = args->decode(pc, r, s);

		if (derrs < 0)
			s->rfail++;

		if (memcmp(r, c, len * sizeof(*r)))
			s->dwrong++;
	}

	s->nwords = trials;
	return s->dwrong;
}

static void free_stuff(struct thread_args *args, int nthreads)
{
	for (int i = 0; i < nthreads; i++) {
		pc_free(args[i].pc);
		free_ws(args[i].ws);
		gsl_rng_free(args[i].rng);
	}
}

static int alloc_stuff(struct thread_args *args, int nthreads, struct options *opt)
{
	memset(args, 0, nthreads * sizeof(*args));

	for (int i = 0; i < nthreads; i++) {
		args[i].pc = pc_init(opt->symsize, opt->gfpoly, opt->r_fcr,
				     opt->r_prim, opt->r_nroots, opt->c_fcr,
				     opt->c_prim, opt->c_nroots, opt->rows,
				     opt->cols);
		if (!args[i].pc)
			goto err;

		int len = pc_len(args[i].pc);
		args[i].ws = alloc_ws(len * len);
		if (!args[i].ws)
			goto err;

		args[i].rng = rng_alloc_and_seed(opt->rng_type, opt->seed + i);
		if (!args[i].rng)
			goto err;

		args[i].decode = opt->alg;
	}

	return 0;

err:
	free_stuff(args, nthreads);
	return -1;
}

static void consolidate_stats(struct thread_args *args, int nthreads, int errs)
{
	for (int i = 1; i < nthreads; i++)
		stats_add(&args[0].s, &args[i].s);

	print_stats(stdout, &args[0].s, errs);
}

static void test_mt(struct thread_args *args, int nthreads, int errs, int trials)
{
	#pragma omp parallel for
	for (int i = 0; i < nthreads; i++)
		test_uc(args + i, trials, errs);

	consolidate_stats(args, nthreads, errs);
}

int run_complexity(struct options *opt)
{
	struct thread_args args[opt->nthreads];

	int ret = alloc_stuff(args, opt->nthreads, opt);
	if (ret)
		return -1;

	int t = (pc_mind(args[0].pc) - 1) / 2;
	int trials = opt->cword_num / opt->nthreads;
	print_start(stdout, args[0].pc, "# ", opt->seed,
		    opt->nthreads, algorithm_get_name(opt->alg));

	omp_set_num_threads(opt->nthreads);
	for (int errs = 0; errs <= t; errs++)
		test_mt(args, opt->nthreads, errs, trials);

	free_stuff(args, opt->nthreads);
	return 0;
}
