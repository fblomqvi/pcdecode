#ifndef FB_PCDECODE_PRODUCT_CODE_H
#define FB_PCDECODE_PRODUCT_CODE_H

#include <stddef.h>
#include <stdint.h>
#include <librs.h>
#include <stdio.h>

struct estrat;

struct pc {
    struct rs_control* row_code;
    struct rs_control* col_code;
    size_t rows;
    size_t cols;
    size_t nstrat;
    size_t nstrat_bound;

    struct estrat* es;
    int* es_buffer;

    uint16_t* x_buf;
    uint16_t* y_buf;
};

struct stats {
    size_t nwords;
    size_t viable;
    size_t max;
    size_t rdec;
    size_t rdec_max;
    size_t cdec;
    size_t dwrong;
    size_t rfail;
    size_t cfail;
    size_t alg2;
    size_t alg3;
};

static inline void stats_add(struct stats* l, const struct stats* r)
{
	l->nwords += r->nwords;
	l->viable += r->viable;
	l->max += r->max;
	l->rdec += r->rdec;
	l->rdec_max += r->rdec_max;
	l->cdec += r->cdec;
	l->dwrong += r->dwrong;
	l->rfail += r->rfail;
	l->cfail += r->cfail;
	l->alg2 += r->alg2;
	l->alg3 += r->alg3;
}

size_t get_gfpoly(size_t symsize);

/* Initialize a Reed-Solomon control block
 * symsize = symbol size, bits
 * gfpoly = Field generator polynomial coefficients
 * fcr = first root of RS code generator polynomial, index form
 * prim = primitive element to generate polynomial roots
 * nroots = RS code generator polynomial degree (number of roots)
 */
struct pc* init_pc(size_t symsize, size_t gfpoly,
			size_t r_fcr, size_t r_prim, size_t r_nroots,
			size_t c_fcr, size_t c_prim, size_t c_nroots,
			size_t rows, size_t cols);

void free_pc(struct pc* pc);

void encode_pc(struct pc* pc, uint16_t* data);

int pc_decode_gmd(struct pc* pc, uint16_t* data, struct stats* s);
int pc_decode_gd(struct pc* pc, uint16_t* data, struct stats* s);
int pc_decode_iter(struct pc* pc, uint16_t* data, struct stats* s);
int pc_decode_iter_gd(struct pc* pc, uint16_t* data, struct stats* s);
int pc_decode_eras(struct pc* pc, uint16_t* data, struct stats* s);
int pc_decode_eras_gd(struct pc* pc, uint16_t* data, struct stats* s);

void print_pc(FILE* file, const struct pc* pc, const char* prefix);

static inline size_t pc_len(const struct pc* pc)
{ return pc->rows * pc->cols; }

static inline size_t pc_dim(const struct pc* pc)
{ return (pc->cols - pc->row_code->code->nroots) * (pc->rows - pc->col_code->code->nroots); }

static inline size_t pc_mind(const struct pc* pc)
{ return rs_mind(pc->row_code) * rs_mind(pc->col_code); }

#endif /* FB_PCDECODE_PRODUCT_CODE_H */
