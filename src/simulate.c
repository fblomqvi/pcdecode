/* simulate.c
   Copyright (C) 2019 Ferdinand Blomqvist

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License version 2 as published by
   the Free Software Foundation.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   this program. If not, see <http://www.gnu.org/licenses/>.

   Written by Ferdinand Blomqvist. */

#include "simulate.h"
#include "gen_errors.h"
#include "rng.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <omp.h>
#include <stdatomic.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct wspace {
	uint16_t *c;		/* sent codeword */
	uint16_t *r;		/* received word */
};

struct thread_args {
	int (*decode)(struct pc*, uint16_t*, struct stats*);
	struct pc* pc;
	struct wspace* ws;
	gsl_rng* rng;
	struct stats s;
	size_t trials;
	size_t min_errs;
	double p;
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
	return ws;

err:
	free(ws->c);
	free(ws);
	return NULL;
}

static void free_ws(struct wspace *ws)
{
	if (!ws)
		return;

	free(ws->c);
	free(ws);
}

static void print_start(FILE* file, struct pc* pc,
			const char* prefix, unsigned long seed,
			size_t nthreads, const char* alg)
{
	static const char* const col_heads[] = {
		"channel error probability",
		"number of codewords",
		"algorithm 2",
		"algorithm 3",
		"viable strategies",
		"max strategies",
		"row decoder actual",
		"row decoder worst case",
		"column decoder actual",
		"decoding failures",
		"reported failures",
		"critical failures",
	};

	pc_print(file, pc, prefix);
	fprintf(file, "%sAlgorithm: %s\n", prefix, alg);
	fprintf(file, "%sSeed: %lu\n", prefix, seed);
	fprintf(file, "%sThreads: %zu\n", prefix, nthreads);
	for (size_t i = 0; i < ARRAY_SIZE(col_heads); i++)
		fprintf(file, "%s(%zu) %s\n", prefix, i + 1, col_heads[i]);
}

static void print_stats(FILE* file, struct stats* s, double p, size_t ecount)
{
	fprintf(file, "%f %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
		p, s->nwords, s->alg2, s->alg3,
		s->viable, s->max, s->rdec, s->rdec_max,
		s->cdec, ecount, s->rfail, s->cfail);
	fflush(file);
}

/* Test up to error correction capacity */
static int test_normal(struct thread_args* args, _Atomic size_t* ecount)
{
	size_t min_errs = args->min_errs;
	size_t trials = args->trials;
	struct stats* s = &args->s;
	struct pc* pc = args->pc;
	struct wspace* ws = args->ws;
	uint16_t *c = ws->c;
	uint16_t *r = ws->r;
	int len = pc_len(pc);
	int t = (pc_mind(pc) - 1) / 2;

	memset(s, 0, sizeof(*s));

	size_t j;
	for (j = 0; j < trials || *ecount < min_errs; j++) {
		int errs = get_rcw_channel(pc, c, r, args->p, args->rng);
		int derrs = args->decode(pc, r, s);

		if (derrs < 0)
			s->rfail++;

		if (memcmp(r, c, len * sizeof(*r))) {
			(*ecount)++;
			if (errs <= t)
				s->cfail++;
		}
	}

	s->nwords = j;
	return s->cfail;
}

static void free_stuff(struct thread_args* args, int nthreads)
{
	for (int i = 0; i < nthreads; i++) {
		pc_free(args[i].pc);
		free_ws(args[i].ws);
		gsl_rng_free(args[i].rng);
	}
}

static int alloc_stuff(struct thread_args* args, struct options* opt)
{
	size_t nthreads = opt->nthreads;
	memset(args, 0, nthreads * sizeof(*args));

	for (size_t i = 0; i < nthreads; i++) {
		args[i].pc = pc_init(opt->symsize, opt->gfpoly, opt->r_fcr,
				opt->r_prim, opt->r_nroots, opt->c_fcr,
				opt->c_prim, opt->c_nroots, opt->rows, opt->cols);
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

static void consolidate_stats(struct thread_args* args, int nthreads, size_t ecount)
{
	for (int i = 1; i < nthreads; i++)
		stats_add(&args[0].s, &args[i].s);

	print_stats(stdout, &args[0].s, args[0].p, ecount);
}

static int test_mt(struct thread_args* args, size_t nthreads, double p,
		size_t trials, size_t min_errs, double fer_cutoff)
{
	_Atomic size_t ecount = 0;

	#pragma omp parallel for
	for (size_t i = 0; i < nthreads; i++) {
		args[i].p = p;
		args[i].trials = trials;
		args[i].min_errs = min_errs;
		test_normal(args + i, &ecount);
	}

	consolidate_stats(args, nthreads, ecount);
	if (((double) ecount) / args[0].s.nwords < fer_cutoff)
		return -1;

	return 0;
}

int run_simulation(struct options* opt)
{
	struct thread_args args[opt->nthreads];

	int ret = alloc_stuff(args, opt);
	if (ret)
		return -1;

	size_t trials = opt->cword_num / opt->nthreads;
	print_start(stdout, args[0].pc, "# ", opt->seed,
			opt->nthreads, opt->alg_name);

	omp_set_num_threads(opt->nthreads);
	for (double p = opt->p_start; p >= opt->p_stop - 10E-10; p -= opt->p_step) {
		if (test_mt(args, opt->nthreads, p, trials,
				opt->min_errs, opt->fer_cutoff))
		    break;

		if (opt->p_halve_at - p >= -10E-10) {
			opt->p_step /= 2;
			opt->p_halve_at = 0.0;
		}
	}

	free_stuff(args, opt->nthreads);
	return 0;
}

