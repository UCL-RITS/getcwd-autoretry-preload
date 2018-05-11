
// Note from Ian Kirker:
//   This code was provided by Nathan Dauchy, here:
//     https://jira.hpdd.intel.com/browse/LU-9735
//   No additional copyright information was provided, 
//     besides the single comment below.

// rjh - Fri Oct  8 15:01:49 EST 2010


#include <dlfcn.h>     // for dlsym
#include <unistd.h>    // for getcwd, readlink
#include <errno.h>     // for errno
#include <stdio.h>     // for printf, fprintf
#include <string.h>    // for strerror, strlen, strncmp
#include <stdlib.h>    // for getenv, atol, gethostname
#include <syslog.h>    // for syslog

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1l)
#endif

#define REAL_LIBC RTLD_NEXT

#ifdef _GNU_SOURCE
# error - _GNU_SOURCE defined
#endif
#ifdef __USE_GNU
# error - __USE_GNU defined
#endif

/*
[root@vayu1 ~]# nm /lib64/libc-2.5.so  | grep getcwd
00000000000c69c0 t __getcwd
00000000000e7eb0 T __getcwd_chk
00000000000c69c0 W getcwd
*/

// man 2 getcwd
//   long getcwd(char *buf, unsigned long size);
// man 3 getcwd
//   char *getcwd(char *buf, size_t size);

#define no_USE_PROC_CWD

char syslogIdent[128];
int priority = LOG_ERR | LOG_USER;

int getRank(void);
char *getHostname(void);
void setupSyslog(int rank, char *hostname);
#ifdef USE_PROC_CWD
char *getProcSelfCwd(void);
#endif

// read OMPI_COMM_WORLD_RANK
int getRank(void) {
    char *b;
    int rank = -1;

    b = getenv( "OMPI_COMM_WORLD_RANK" );
    if (b) {
        long i = atol(b);
        if (i > 0)
            rank = i;
    }
    //printf( "rjh set rank to %d\n", rank);

    return rank;
}

#ifdef USE_PROC_CWD
// read the /proc/self/cwd link
char *getProcSelfCwd(void) {
    char *b;
    ssize_t len;

    if ( (b = malloc(4096*sizeof(char))) == 0 ) {
	syslog(priority, "NF: getcwd wrapper malloc of %ld failed\n", 4096*sizeof(char) );
	return NULL;
    }

    // expect eg.
    // [root@v1489 ttt]# ls -l /proc/self/cwd
    // lrwxrwxrwx 1 root root 0 Oct 15 14:52 /proc/self/cwd -> /short/z00/rjh/ttt (deleted)

    if ((len = readlink("/proc/self/cwd", b, 4096-1)) != -1) {
	b[len] = '\0';
	return b;
    }
    else
	free(b);

    return 0;
}
#endif

char *getHostname(void) {
    char *b;

    if ( (b = malloc(64*sizeof(char))) == NULL ) {
	syslog(priority, "NF: getcwd wrapper malloc of %ld failed\n", 64*sizeof(char) );
	return NULL;
    }

    if ( !gethostname(b, 64) )
	return b;
    else
	free(b);

    return 0;
}

void setupSyslog(int rank, char *hostname) {
   int logopt = LOG_PID | LOG_CONS | LOG_PERROR;
   int facility = LOG_USER;

   sprintf(syslogIdent, "NF: getcwd: mpi rank %d, host %s: ", rank, hostname);
   openlog(syslogIdent, logopt, facility);
}

char *getcwd(char *buf, size_t size) {
    static char * (*func) (char *, size_t) = NULL;
    char *ret;
#ifdef USE_PROC_CWD
    char *cwd;
#endif
    int tryAgain = 1, try = 0, N = 10;

    if (!func) {
	int rank;
	char *hostname;

        func = (char * (*) (char *, size_t)) dlsym(REAL_LIBC, "getcwd");

	rank = getRank();
	hostname = getHostname();
        setupSyslog( rank, hostname );
	free(hostname);
	//printf( "rjh - getcwd() - rank %d, hostname %s\n", rank, hostname );
    }

    //printf( "rjh - getcwd()\n" );

    // try the syscall N times, and print/syslog a message (once) if any of them fail
    while ( tryAgain && ( try < N ) ) {
        tryAgain = 0;

	//fprintf(stderr,"try %d\n", try);

        ret = func(buf, size);

        if (!ret) {
            int err = errno;
            if ( try == 0 )
		syslog( priority, "failed: size %ld, buf %p, ret %p: %s\n", size, buf, ret, strerror(err) );
            tryAgain = 1;
        }
        //else if (size < 10) {
        //    fprintf( stderr, "NF: getcwd likely screwy as size < 10: size %ld, buf %p, ret %p, buf string '%s'\n", size, buf, ret, buf );
        //}
        //else if (buf != ret && buf != 0) {
        //    fprintf( stderr, "NF: getcwd probably failed as buf != ret: size %ld, buf %p, ret %p, buf string '%s'\n", size, buf, ret, buf );
        //}
        //else if (buf == 0) {
        //    fprintf( stderr, "NF: getcwd WARNING: being called with non-standard buf = 0: size %ld, buf %p, ret %p\n", size, buf, ret );
	//}

        try++;
    }

    if (ret) {
	if ( try > 1 ) // eventually succeeded, but not on the first try
	    syslog( priority, "succeeded at try %d of %d: size %ld, buf %p, ret %p, path %s\n", try, N, size, buf, ret, buf );

	return ret;
    }

#ifdef USE_PROC_CWD
    // fallback to reading /proc/self/cwd
    cwd = getProcSelfCwd();
    if ( !cwd ) {
	syslog( priority, "reading /proc/self/cwd failed too\n" );
	ret = 0;
    }
    else {
	// succeeded, but we might need to remove the ' (deleted)' off the end if it's there
	size_t len = strlen(cwd);
	size_t lenDel = strlen(" (deleted)");
	//fprintf( stderr, "len %ld, lenDel %ld, ret %s\n", len, lenDel, cwd );

	if ( len > lenDel && strncmp( &(cwd[len-lenDel]), " (deleted)", lenDel ) == 0 ) {
	    syslog( priority, "/proc/self/cwd reports deleted dir. will be trimmed and returned: %s\n", cwd );
	    len -= lenDel;
	    cwd[len] = '\0';
	}
	else {
	    syslog( priority, "using /proc/self/cwd: %s\n", cwd );
	}

	// copy cwd string into buf
	if ( len > size )
	    ret = 0;
	else {
	    memcpy( buf, cwd, len );  // check this for off-by-one's and also use strcpy and strstr as per Andrey instead?
	    ret = buf;
	    buf[len] = '\0';
	}
    }
    if (cwd)
	free(cwd);

    // could fallback further to reading $PWD from the parent shell, which would be wrong if the app had done a chdir

#else
    syslog( priority, "failed after %d tries: size %ld, buf %p, ret %p\n", N, size, buf, ret );
#endif

    return ret;
}

/*
long getcwd(char *buf, unsigned long size) {
    static long (*func) (char *, unsigned long) = NULL;
    long ret;

    if (!func)
        func = (long (*) (char *, unsigned long)) dlsym(REAL_LIBC, "getcwd");
 
    printf( "rjh - getcwd()\n" );

    ret = func(buf, size);
    return ret;
}
*/

