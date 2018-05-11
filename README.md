# getcwd-autoretry-preload
A library that shadows getcwd to prevent program crashes from a Lustre bug.

The Lustre filesystem seems to have a rare but recurring bug in several versions in which `getcwd` fails unexpectedly, usually crashing the calling code.

This library shadows the `getcwd` call, wrapping the original in a function which retries on failure, and logs the failure and retries to the syslog.

It is intended to be used with the `LD_PRELOAD` environment variable, for example:

```
export LD_PRELOAD=/full/path/to/getcwd-autoretry-preload.so
mpirun my_program
```

It's only the one C file, but so that you don't have to remember the flags for building a library, there's also a Makefile.

```
$ make
gcc -O2 -shared -fPIC -o getcwd-autoretry-preload.so getcwd-autoretry-preload.c

$ make INSTALL_PATH=/some/path install
install -d /some/path
install getcwd-autoretry-preload.so /some/path
```

The C code was sourced from a copy provided by Nathan Dauchy to the Intel HPDD JIRA tracker, here: <https://jira.hpdd.intel.com/browse/LU-9735>


