#include "gen_errors.h"
#include <string.h>
#include <stdlib.h>

static void gen_random_cword(struct pc *pc, uint16_t* c,
			const gsl_rng* rng)
{
	int nn = pc->row_code->code->nn;
	int rdlen = pc->cols - pc->row_code->code->nroots;
	int cdlen = pc->rows - pc->col_code->code->nroots;

	/* Load c with random data and encode */
	for (int j = 0; j < cdlen; j++)
		for (int i = 0; i < rdlen; i++)
			c[j * pc->cols + i] = gsl_rng_get(rng) & nn;

	pc_encode(pc, c);
}

/*
 * Generates a random codeword and stores it in c. Generates random errors and
 * erasures, and stores the random word with errors in r. Error positions are
 * encoded into errlocs so that errlocs has one of two values in every position:
 *
 * 0 if there is no error in this position;
 * 1 if there is a symbol error in this position;
 */
void get_rcw_we(struct pc *pc, uint16_t* c, uint16_t* r,
		int errs, int* errlocs, const gsl_rng* rng)
{
	int nn = pc->row_code->code->nn;
	int len = pc_len(pc);

	gen_random_cword(pc, c, rng);

	/* Make copy and add errors and erasures */
	memcpy(r, c, len * sizeof(*r));
	memset(errlocs, 0, len * sizeof(*errlocs));

	/* Generating random errors */
	for (int i = 0; i < errs; i++) {
		int errval, errloc;

		do {
			/* Error value must be nonzero */
			errval = gsl_rng_get(rng) & nn;
		} while (errval == 0);

		do {
			/* Must not choose the same location twice */
			errloc = gsl_rng_uniform_int(rng, len);
		} while (errlocs[errloc] != 0);

		errlocs[errloc] = 1;
		r[errloc] ^= errval;
	}
}


/* Returns the number of errors */
int get_rcw_channel(struct pc *pc, uint16_t* c, uint16_t* r,
			double p, const gsl_rng* rng)
{
	int nn = pc->row_code->code->nn;
	int len = pc_len(pc);
	int errs = 0;

	gen_random_cword(pc, c, rng);

	/* Make copy and add errors and erasures */
	memcpy(r, c, len * sizeof(*r));

	/* Generating random errors */
	for (int i = 0; i < len; i++) {
		double tmp = gsl_rng_uniform(rng);
		if (tmp > p)
			continue;

		int errval;

		do {
			/* Error value must be nonzero */
			errval = gsl_rng_get(rng) & nn;
		} while (errval == 0);

		r[i] ^= errval;
		errs++;
	}

	return errs;
}
