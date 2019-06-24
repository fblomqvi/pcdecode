#ifndef FB_PCDECODE_GEN_ERRORS_H
#define FB_PCDECODE_GEN_ERRORS_H

#include "product_code.h"
#include "rng.h"

void get_rcw_we(struct pc *pc, uint16_t* c, uint16_t* r,
		int errs, int* errlocs, const gsl_rng* rng);

int get_rcw_channel(struct pc *pc, uint16_t* c, uint16_t* r,
			double p, const gsl_rng* rng);

#endif /* FB_PCDECODE_GEN_ERRORS_H */
