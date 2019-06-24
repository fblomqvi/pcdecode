/* A program that generates random points in Euclidian space.
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

#include "dbg.h"
#include "version.h"
#include "rng.h"
#include "simulate.h"
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

static int print_help(FILE* file)
{
    static const char* formatstr =
"Usage: %s [OPTION]...\n"
"  or:  %s [OPTION]... OUTPUT\n\n%s\n";

    static const char* helpstr =
"Generates random points in Euclidean space. Outputs to stdout\n"
"if no output file is given.\n\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -n, --num-points=NUM         The number of codewords to generate. Zero (0) makes the\n"
"                                 encoder run until it is killed.\n"
"  -r, --rng=RNG                The random number generator to use. To see a list of all\n"
"                                 available generators give 'list' as argument.\n"
"  -S, --seed=SEED              The seed for the random number generator.\n"
"      --help                   Display this help and exit.\n"
"      --version                Output version information and exit.\n\n";

    return (fprintf(file, formatstr, PROGRAM_NAME, PROGRAM_NAME, helpstr) < 0)
                ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void parse_cmdline(int argc, char* const argv[], struct options* opt)
{
	static const char* optstring = "a:n:c:r:R:S:s:b:e:t:E:f:T:";
	static struct option longopt[] = {
		{"algorithm", required_argument, NULL, 'a'},
		{"num-words", required_argument, NULL, 'n'},
		{"min-errors", required_argument, NULL, 'E'},
		{"fer-cutoff", required_argument, NULL, 'f'},
		{"threads", required_argument, NULL, 'T'},
		{"cols", required_argument, NULL, 'c'},
		{"rows", required_argument, NULL, 'r'},
		{"r-nroots", required_argument, NULL, 'U'},
		{"c-nroots", required_argument, NULL, 'u'},
		{"p-begin", required_argument, NULL, 'b'},
		{"p-end", required_argument, NULL, 'e'},
		{"p-step", required_argument, NULL, 't'},
		{"rng", required_argument, NULL, 'R'},
		{"seed", required_argument, NULL, 'S'},
		{"sym-size", required_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{0, 0, 0, 0}
	};

	// Setting default options
	*opt = (struct options) {
		.alg.std = pc_decode_new, .list = 0,
		.nthreads = 1,
		.symsize = 0, .gfpoly = 0,
		.rows = 0, .cols = 0,
		.r_nroots = 0, .c_nroots = 0,
		.r_fcr = 1, .c_fcr = 1,
		.r_prim = 1, .c_prim = 1,
		.cword_num = 0, .seed = 0,
		.min_errs = 50, .fer_cutoff = 10E-8,
		.p_start = 0.1, .p_stop = 0.01,
		.p_step = 0.01,
		.rng_type = gsl_rng_default
	};

	// Parsing the command line
	int ch;
	char* endptr;
	while((ch = getopt_long(argc, argv, optstring, longopt, NULL)) != -1)
	{
		switch(ch)
		{
		case 'a':
		{
			if (!strcmp(optarg, "new")) {
				opt->alg.std = pc_decode_new;
				opt->list = 0;
			} else if (!strcmp(optarg, "gmd")) {
				opt->alg.std = pc_decode_gmd;
				opt->list = 0;
			} else if (!strcmp(optarg, "gd")) {
				opt->alg.std = pc_decode_gd;
				opt->list = 0;
			} else if (!strcmp(optarg, "iter")) {
				opt->alg.std = pc_decode_iter;
				opt->list = 0;
			} else if (!strcmp(optarg, "comb")) {
				opt->alg.std = pc_decode_comb;
				opt->list = 0;
			} else if (!strcmp(optarg, "eras")) {
				opt->alg.std = pc_decode_iter_eras;
				opt->list = 0;
			} else if (!strcmp(optarg, "erasgd")) {
				opt->alg.std = pc_decode_eras_gd;
				opt->list = 0;
			} else if (!strcmp(optarg, "nlist")) {
				opt->alg.list = pc_decode_new_list;
				opt->list = 1;
			} else if (!strcmp(optarg, "clist1")) {
				opt->alg.list = pc_decode_comb_list1;
				opt->list = 1;
			} else if (!strcmp(optarg, "clist2")) {
				opt->alg.list = pc_decode_comb_list2;
				opt->list = 1;
			} else if (!strcmp(optarg, "elist")) {
				opt->alg.list = pc_decode_eras_list;
				opt->list = 1;
			} else {
				check(0, "invalid argument to option '%c': '%s'", ch, optarg);
			}

			opt->alg_name = optarg;
			break;
		}
		case 'c':
			opt->cols = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->cols == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'n':
			opt->cword_num = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->cword_num == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'E':
			opt->min_errs = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->min_errs == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'f':
			opt->fer_cutoff = strtod(optarg, &endptr);
			check(*endptr == '\0' && opt->fer_cutoff >= 0
				&& !(errno == ERANGE && opt->fer_cutoff == HUGE_VAL),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'r':
			opt->rows = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->rows == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'b':
			opt->p_start = strtod(optarg, &endptr);
			check(*endptr == '\0' && opt->p_start >= 0
				&& !(errno == ERANGE && opt->p_start == HUGE_VAL),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'e':
			opt->p_stop = strtod(optarg, &endptr);
			check(*endptr == '\0' && opt->p_stop >= 0
				&& !(errno == ERANGE && opt->p_stop == HUGE_VAL),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 't':
			opt->p_step = strtod(optarg, &endptr);
			check(*endptr == '\0' && opt->p_step >= 0
				&& !(errno == ERANGE && opt->p_step == HUGE_VAL),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'R':
		{
			const gsl_rng_type** rng_types = gsl_rng_types_setup();
			if(!strcmp(optarg, "list"))
				exit(print_rngs(stdout, rng_types));
			else
			{
				opt->rng_type = get_rng_type(optarg, rng_types);
				check(opt->rng_type, "invalid random number generator");
			}
			break;
		}
		case 'S':
			opt->seed = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->seed == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 's':
			opt->symsize = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->symsize == ULONG_MAX)
				&& opt->symsize <= 16,
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'U':
			opt->r_nroots = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->r_nroots == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'u':
			opt->c_nroots = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->c_nroots == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'T':
			opt->nthreads = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->nthreads == ULONG_MAX),
				"invalid argument to option '%c': '%s'", ch, optarg);
			break;
		case 'h':
			exit(print_help(stdout));
		case 'V':
			exit(print_version(stdout));
		default:
			goto error;
		}
	}


	// Check for mandatory arguments.
	check(opt->symsize > 0, "missing mandatory option -- '%c'", 's');
	check(opt->rows > 0, "missing mandatory option -- '%c'", 'r');
	check(opt->cols > 0, "missing mandatory option -- '%c'", 'c');
	check(opt->r_nroots > 0, "missing mandatory option -- '%s'", "r-nroots");
	check(opt->c_nroots > 0, "missing mandatory option -- '%s'", "c-nroots");

	// Checking that arguments are sane
	check(opt->p_start >= opt->p_stop, "p-begin must be larger than p-end");

	if (opt->gfpoly == 0)
		opt->gfpoly = get_gfpoly(opt->symsize);

	return;

error:
	fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
	exit(EXIT_FAILURE);
}


int main(int argc, char* argv[])
{
	struct options opt;
	int ret = EXIT_FAILURE;
	argv[0] = PROGRAM_NAME = "rnd-point";

	gsl_set_error_handler_off();
	parse_cmdline(argc, argv, &opt);

	opt.seed = opt.seed ? opt.seed : get_random_seed();

	ret = run_simulation(&opt);
	return ret;
}

