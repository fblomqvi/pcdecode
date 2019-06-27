/* Complexity simulations with product codes.
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
#include "simulate_complexity.h"
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>

static int print_help(FILE* file)
{
    static const char* formatstr =
"Usage: %s [OPTION]...\n\n%s\n";

    static const char* helpstr =
"Run complexity simulations for product codes with different algorithms.\n"
"The component codes are Reed-Solomon codes over fields of size 2^m.\n"
"Outputs to stdout.\n\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -a, --algorithm=ALG		The decoding algorithm to use. Valid values\n"
"                                 are gmd, gd, iter, itergd, eras, erasgd.\n"
"  -c, --cols=NUM               The number of columns in the codeword.\n"
"  -r, --rows=NUM               The number of rows in the codeword.\n"
"      --c-nroots=NUM           The number of roots in the oulumn code.\n"
"                                 The minimum distance of the column code\n"
"                                 is NUM + 1.\n"
"      --r-nroots=NUM           The number of roots in the row code. The\n"
"                                 minimum distance of the row code is\n"
"                                 NUM + 1.\n"
"  -n, --num-words=NUM          The minimum number of words to decode.\n"
"  -R, --rng=RNG                The random number generator to use. To see a list of all\n"
"                                 available generators give 'list' as argument.\n"
"  -s, --sym-size=NUM           The symbol size in bits.\n"
"  -S, --seed=SEED              The seed for the random number generator.\n"
"  -T, --threads=NUM            Number of computational threads to use.\n"
"      --help                   Display this help and exit.\n"
"      --version                Output version information and exit.\n";

    return (fprintf(file, formatstr, PROGRAM_NAME, helpstr) < 0)
                ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void parse_cmdline(int argc, char* const argv[], struct options* opt)
{
	static const char* optstring = "a:n:c:r:R:S:s:T:";
	static struct option longopt[] = {
		{"algorithm", required_argument, NULL, 'a'},
		{"num-words", required_argument, NULL, 'n'},
		{"threads", required_argument, NULL, 'T'},
		{"cols", required_argument, NULL, 'c'},
		{"rows", required_argument, NULL, 'r'},
		{"r-nroots", required_argument, NULL, 'U'},
		{"c-nroots", required_argument, NULL, 'u'},
		{"rng", required_argument, NULL, 'R'},
		{"seed", required_argument, NULL, 'S'},
		{"sym-size", required_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{0, 0, 0, 0}
	};

	// Setting default options
	*opt = (struct options) {
		.alg = pc_decode_gmd,
		.nthreads = 1,
		.symsize = 0, .gfpoly = 0,
		.rows = 0, .cols = 0,
		.r_nroots = 0, .c_nroots = 0,
		.r_fcr = 1, .c_fcr = 1,
		.r_prim = 1, .c_prim = 1,
		.cword_num = 0, .seed = 0,
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
			if (!strcmp(optarg, "gmd"))
				opt->alg = pc_decode_gmd;
			else if (!strcmp(optarg, "gd"))
				opt->alg = pc_decode_gd;
			else if (!strcmp(optarg, "iter"))
				opt->alg = pc_decode_iter;
			else if (!strcmp(optarg, "itergd"))
				opt->alg = pc_decode_iter_gd;
			else if (!strcmp(optarg, "eras"))
				opt->alg = pc_decode_eras;
			else if (!strcmp(optarg, "erasgd"))
				opt->alg = pc_decode_eras_gd;
			else
				check(0, "invalid argument to option '%c': '%s'", ch, optarg);
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
		case 'r':
			opt->rows = strtoul(optarg, &endptr, 10);
			check(*endptr == '\0'
				&& !(errno == ERANGE && opt->rows == ULONG_MAX),
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
	check(opt->cword_num > 0, "missing mandatory option -- '%c'", 'n');
	check(opt->symsize > 0, "missing mandatory option -- '%c'", 's');
	check(opt->rows > 0, "missing mandatory option -- '%c'", 'r');
	check(opt->cols > 0, "missing mandatory option -- '%c'", 'c');
	check(opt->r_nroots > 0, "missing mandatory option -- '%s'", "r-nroots");
	check(opt->c_nroots > 0, "missing mandatory option -- '%s'", "c-nroots");

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
	PROGRAM_NAME = argv[0];

	gsl_set_error_handler_off();
	parse_cmdline(argc, argv, &opt);

	opt.seed = opt.seed ? opt.seed : get_random_seed();

	ret = run_complexity(&opt);
	return ret;
}
