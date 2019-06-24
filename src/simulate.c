#include "simulate.h"
#include "gen_errors.h"
#include "rng.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct wspace {
	uint16_t *c;		/* sent codeword */
	uint16_t *r;		/* received word */
	uint16_t* list_buff;
	uint16_t** list;
};

struct thread_args {
	struct pc* pc;
	struct wspace* ws;
	gsl_rng* rng;
	union decoder alg;
	struct stats s;
	size_t trials;
	size_t min_errs;
	double p;
	int list;
	int retval;
};

static struct wspace *alloc_ws(int len, int nstrat)
{
	struct wspace *ws;

	ws = calloc(1, sizeof(*ws));
	if (!ws)
		return NULL;

	size_t buf_size = nstrat ? nstrat + 3 : 2;
	ws->c = malloc(buf_size * len * sizeof(*ws->c));
	if (!ws->c)
		goto err;

	ws->r = ws->c + len;

	if (nstrat) {
		ws->list_buff = ws->r + len;
		ws->list = malloc((nstrat + 1) * sizeof(*ws->list));
		if (!ws->list)
			goto err;

		for (int i = 0; i <= nstrat; i++)
			ws->list[i] = ws->list_buff + (i * len);
	}

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

	free(ws->list);
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
		"strategies used",
		"viable strategies",
		"max strategies",
		"row decoder actual",
		"row decoder worst case",
		"column decoder actual",
		"decoding failures",
		"reported failures",
		"critical failures",
	};

	print_pc(file, pc, prefix);
	fprintf(file, "%sAlgorithm: %s\n", prefix, alg);
	fprintf(file, "%sSeed: %lu\n", prefix, seed);
	fprintf(file, "%sThreads: %zu\n", prefix, nthreads);
	for (size_t i = 0; i < ARRAY_SIZE(col_heads); i++)
		fprintf(file, "%s(%zu) %s\n", prefix, i + 1, col_heads[i]);
}

static void print_stats(FILE* file, struct stats* s, double p)
{
	fprintf(file, "%f %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
		p, s->nwords, s->alg2, s->alg3, s->used,
		s->viable, s->max, s->rdec, s->rdec_max,
		s->cdec, s->dwrong, s->rfail, s->cfail);
	fflush(file);
}

/* Test up to error correction capacity */
static int test_normal(struct pc *pc, double p,
		size_t trials, size_t min_errs, struct wspace *ws,
		struct stats* s, const gsl_rng* rng,
		int (*decode)(struct pc*, uint16_t*, struct stats*))
{
	uint16_t *c = ws->c;
	uint16_t *r = ws->r;
	int len = pc_len(pc);
	int t = (pc_mind(pc) - 1) / 2;

	memset(s, 0, sizeof(*s));

	size_t j;
	for (j = 0; j < trials || s->dwrong < min_errs; j++) {
		int errs = get_rcw_channel(pc, c, r, p, rng);
		int derrs = decode(pc, r, s);

		if (derrs < 0)
			s->rfail++;

		if (memcmp(r, c, len * sizeof(*r))) {
			s->dwrong++;
			if (errs <= t)
				s->cfail++;
		}
	}

	s->nwords = j;
	return s->cfail;
}

static int decodes_correct_list(const uint16_t* c, size_t len,
				uint16_t* const* list, size_t list_len)
{
	for (size_t i = 0; i < list_len; i++) {
		if (!memcmp(c, list[i], len * sizeof(*c)))
			return 1;
	}

	return 0;
}

static int test_list(struct pc *pc, double p,
		size_t trials, size_t min_errs, struct wspace *ws,
		struct stats* s, const gsl_rng* rng,
		int (*decode)(struct pc*, const uint16_t*, uint16_t**, struct stats*))
{
	uint16_t *c = ws->c;
	uint16_t *r = ws->r;
	uint16_t **list = ws->list;
	int len = pc_len(pc);
	int t = (pc_mind(pc) - 1) / 2;

	memset(s, 0, sizeof(*s));

	size_t j;
	for (j = 0; j < trials || s->dwrong < min_errs; j++) {
		int errs = get_rcw_channel(pc, c, r, p, rng);
		int list_len = decode(pc, r, list, s);

		if (!decodes_correct_list(c, len, list, list_len)) {
			s->dwrong++;
			if (errs <= t)
				s->cfail++;
		}
	}

	s->nwords = j;
	return s->cfail;
}

static void* test_uc_thread(void* arg)
{
	struct thread_args* args = arg;
	if (args->list) {
		args->retval = test_list(args->pc, args->p, args->trials,
					args->min_errs, args->ws, &args->s,
					args->rng, args->alg.list);
	} else {
		args->retval = test_normal(args->pc, args->p, args->trials,
					args->min_errs, args->ws, &args->s,
					args->rng, args->alg.std);
	}
	return NULL;
}

static void free_stuff(struct thread_args* args, int nthreads)
{
	for (int i = 0; i < nthreads; i++) {
		free_pc(args[i].pc);
		free_ws(args[i].ws);
		gsl_rng_free(args[i].rng);
	}
}

static int alloc_stuff(struct thread_args* args, struct options* opt)
{
	size_t nthreads = opt->nthreads;
	memset(args, 0, nthreads * sizeof(*args));

	for (size_t i = 0; i < nthreads; i++) {
		args[i].pc = init_pc(opt->symsize, opt->gfpoly, opt->r_fcr,
				opt->r_prim, opt->r_nroots, opt->c_fcr,
				opt->c_prim, opt->c_nroots, opt->rows, opt->cols);
		if (!args[i].pc)
			goto err;

		int len = pc_len(args[i].pc);
		args[i].ws = alloc_ws(len * len, opt->list ? args[i].pc->nstrat : 0);
		if (!args[i].ws)
			goto err;

		args[i].rng = rng_alloc_and_seed(opt->rng_type, opt->seed + i);
		if (!args[i].rng)
			goto err;

		args[i].alg = opt->alg;
		args[i].list = opt->list;
	}

	return 0;

err:
	free_stuff(args, nthreads);
	return -1;
}

static void consolidate_stats(struct thread_args* args, int nthreads)
{
	for (int i = 1; i < nthreads; i++) {
		args[0].s.nwords += args[i].s.nwords;
		args[0].s.used += args[i].s.used;
		args[0].s.viable += args[i].s.viable;
		args[0].s.max += args[i].s.max;
		args[0].s.rdec += args[i].s.rdec;
		args[0].s.rdec_max += args[i].s.rdec_max;
		args[0].s.cdec += args[i].s.cdec;
		args[0].s.dwrong += args[i].s.dwrong;
		args[0].s.rfail += args[i].s.rfail;
		args[0].s.cfail += args[i].s.cfail;
	}

	print_stats(stdout, &args[0].s, args[0].p);
}

static int test_mt(struct thread_args* args, size_t nthreads, double p,
		size_t trials, size_t min_errs, double fer_cutoff)
{
	pthread_t thr[nthreads-1];
	for (size_t i = 0; i < nthreads-1; i++) {
		args[i].p = p;
		args[i].trials = trials;
		args[i].min_errs = min_errs;
		int rc = pthread_create(thr + i, NULL, test_uc_thread, &args[i]);
		if (rc)
			fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
	}

	args[nthreads-1].p = p;
	args[nthreads-1].trials = trials;
	args[nthreads-1].min_errs = min_errs;
	test_uc_thread(&args[nthreads-1]);
	for (size_t i = 0; i < nthreads-1; i++) {
		pthread_join(thr[i], NULL);
	}

	consolidate_stats(args, nthreads);

	struct stats* s = &args[0].s;
	if (((double) s->dwrong) / s->nwords < fer_cutoff)
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
	size_t min_errs = opt->min_errs / opt->nthreads;
	print_start(stdout, args[0].pc, "# ", opt->seed,
			opt->nthreads, opt->alg_name);

	for (double p = opt->p_start; p >= opt->p_stop - 10E-10; p -= opt->p_step) {
		if (test_mt(args, opt->nthreads, p, trials,
				min_errs, opt->fer_cutoff))
		    break;
	}

	free_stuff(args, opt->nthreads);
	return 0;
}

