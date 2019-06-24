#ifndef FB_PCDECODE_SIMULATE_COMPLEXITY_H
#define FB_PCDECODE_SIMULATE_COMPLEXITY_H

#include "product_code.h"
#include "rng.h"

struct options {
	const gsl_rng_type* rng_type;
	int (*alg)(struct pc*, uint16_t*, struct stats*);
	const char* alg_name;
	size_t cword_num;
	size_t nthreads;
	unsigned long seed;
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


int run_complexity(struct options* opt);

#endif /* FB_PCDECODE_SIMULATE_COMPLEXITY_H */

