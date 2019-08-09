__pcdecode__ contains tools for doing simulations with product codes built with
Reed-Solomon codes. The main use case is simulations for (academic) research.

BUILDING
--------

If building from a release tarball, the standard

    ./configure
    make

will suffice. If you're building from git then run

    autoreconf -i --force
    ./configure
    make

Optionally you can install everything with

    make install


Dependencies:

* [librs](https://github.com/fblomqvi/librs)
* [GSL](https://www.gnu.org/software/gsl/)
