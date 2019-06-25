#ifndef FB_PCDECODE_SIMULATE_H
#define FB_PCDECODE_SIMULATE_H

#include "product_code.h"
#include "rng.h"

union decoder {
	int (*std)(struct pc*, uint16_t*, struct stats*);
	int (*list)(struct pc*, const uint16_t*, uint16_t**, struct stats*);
};

struct options {
	const gsl_rng_type* rng_type;
	union decoder alg;
	const char* alg_name;
	int list;
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


int run_simulation(struct options* opt);

#endif /* FB_PCDECODE_SIMULATE_H */
