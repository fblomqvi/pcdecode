#include "simulate_complexity.h"
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
	uint16_t *r_cpy;
	int *errlocs;
};

struct thread_args {
	struct pc* pc;
	struct wspace* ws;
	gsl_rng* rng;
	int (*decode)(struct pc*, uint16_t*, struct stats*);
	struct stats s;
	int trials;
	int errs;
	int retval;
};

static struct wspace *alloc_ws(int len)
{
	struct wspace *ws;

	ws = calloc(1, sizeof(*ws));
	if (!ws)
		return NULL;

	ws->c = malloc(3 * len * sizeof(*ws->c));
	if (!ws->c)
		goto err;

	ws->r = ws->c + len;
	ws->r_cpy = ws->r + len;

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

static void print_start(FILE* file, struct pc* pc,
			const char* prefix, unsigned long seed,
			size_t nthreads, const char* alg)
{
	static const char* const col_heads[] = {
		"number of errors in codeword",
		"number of codewords",
		"strategies used",
		"viable strategies",
		"max strategies",
		"row decoder actual",
		"row decoder worst case",
		"column decoder actual",
		"decoding failures",
		"reported failures",
	};

	print_pc(file, pc, prefix);
	fprintf(file, "%sAlgorithm: %s\n", prefix, alg);
	fprintf(file, "%sSeed: %lu\n", prefix, seed);
	fprintf(file, "%sThreads: %zu\n", prefix, nthreads);
	for (size_t i = 0; i < ARRAY_SIZE(col_heads); i++)
		fprintf(file, "%s(%zu) %s\n", prefix, i + 1, col_heads[i]);
}

static void print_stats(FILE* file, struct stats* s, int errs)
{
	fprintf(file, "%d %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
		errs, s->nwords, s->used, s->viable, s->max,
		s->rdec, s->rdec_max, s->cdec, s->dwrong, s->rfail);
	fflush(file);
}

/*
static void print_matrix(uint16_t* data, size_t rows, size_t cols, int nn)
{
	int width = ceil(log10(nn));
	for (size_t r = 0; r < rows; r++) {
		printf("(");
		for (size_t c = 0; c < cols; c++) {
			printf(" %*u", width, data[r * cols + c]);
		}
		printf(" )\n");
	}
	printf("\n");
}
*/

/* Test up to error correction capacity */
static int test_uc(struct pc *pc, int errs,
		int trials, struct wspace *ws,
		struct stats* s, const gsl_rng* rng,
		int (*decode)(struct pc*, uint16_t*, struct stats*))
{
	uint16_t *c = ws->c;
	uint16_t *r = ws->r;
	int *errlocs = ws->errlocs;
	int len = pc_len(pc);

	memset(s, 0, sizeof(*s));

	for (int j = 0; j < trials; j++) {
		get_rcw_we(pc, c, r, errs, errlocs, rng);
		//memcpy(ws->r_cpy, r, len * sizeof(*r));
		int derrs = decode(pc, r, s);

		if (derrs < 0)
			s->rfail++;

		if (memcmp(r, c, len * sizeof(*r))) {
			s->dwrong++;
			/*
			printf("c:\n");
			print_matrix(c, pc->rows, pc->cols, pc->row_code->code->nn);
			printf("r:\n");
			print_matrix(ws->r_cpy, pc->rows, pc->cols, pc->row_code->code->nn);
			*/
		}
	}

	s->nwords = trials;
	return s->dwrong;
}

static void* test_uc_thread(void* arg)
{
	struct thread_args* args = arg;
	int retval = test_uc(args->pc, args->errs, args->trials,
				args->ws, &args->s, args->rng,
				args->decode);
	/*
	if (retval)
		printf("  FAIL: %d decoding failures!\n", retval);
	*/
	args->retval = retval;
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

static int alloc_stuff(struct thread_args* args, int nthreads, struct options* opt)
{
	memset(args, 0, nthreads * sizeof(*args));

	for (int i = 0; i < nthreads; i++) {
		args[i].pc = init_pc(opt->symsize, opt->gfpoly, opt->r_fcr,
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

	print_stats(stdout, &args[0].s, args[0].errs);
}

static void test_mt(struct thread_args* args, int nthreads, int errs, int trials)
{
	pthread_t thr[nthreads-1];
	for (int i = 0; i < nthreads-1; i++) {
		args[i].errs = errs;
		args[i].trials = trials;
		int rc = pthread_create(thr + i, NULL, test_uc_thread, &args[i]);
		if (rc)
			fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
	}

	args[nthreads-1].errs = errs;
	args[nthreads-1].trials = trials;
	test_uc_thread(&args[nthreads-1]);
	for (int i = 0; i < nthreads-1; i++) {
		pthread_join(thr[i], NULL);
	}

	consolidate_stats(args, nthreads);
}

int run_complexity(struct options* opt)
{
	struct thread_args args[opt->nthreads];

	int ret = alloc_stuff(args, opt->nthreads, opt);
	if (ret)
		return -1;

	int t = (pc_mind(args[0].pc) - 1) / 2;
	int trials = opt->cword_num / opt->nthreads;
	print_start(stdout, args[0].pc, "# ", opt->seed,
		    opt->nthreads, opt->alg_name);

	for (int errs = 0; errs <= t; errs++)
		test_mt(args, opt->nthreads, errs, trials);

	free_stuff(args, opt->nthreads);
	return 0;
}
