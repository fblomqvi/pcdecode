ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = -Wall -Wextra -pedantic -fopenmp -I$(srcdir)/src/

bin_PROGRAMS = complexity simulate

COMMON_SOURCES = src/dbg.c src/dbg.h src/gen_errors.c src/gen_errors.h \
		 src/product_code.c src/product_code.h src/prog_name.c \
		 src/prog_name.h src/rng.c src/rng.h src/version.c \
		 src/version.h src/algorithm.c src/algorithm.h

complexity_SOURCES = src/complexity_main.c src/complexity.c \
		     src/complexity.h $(COMMON_SOURCES)
complexity_LDFLAGS = $(GSL_LIBS)

simulate_SOURCES = src/simulate_main.c src/simulate.c \
		  src/simulate.h $(COMMON_SOURCES)
simulate_LDFLAGS = $(GSL_LIBS)

EXTRA_DIST = LICENSE
dist-hook:
	cp $(srcdir)/README.md $(distdir)/README.md
