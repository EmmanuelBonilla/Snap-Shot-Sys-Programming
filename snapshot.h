/* $Revision: 1.2 $ */

/*
 *  CHECK_SYSCALL_TRACE is a handy macro to wrap system calls that
 *  follow the POSIX return value protocol.  (i.e., They return 0 if
 *  they succeed and -1 and set errno if they fail.)
 *
 *  The first argument is the system call you want to make and the
 *  second is a boolean value that determines whether or not you want
 *  to note successful system calls, e.g.
 *
 *      CHECK_SYSCALL_TRACE(stat("afile", &buf), 0)
 *
 *  These #includes are needed by the macro:
 */
#include <stdio.h>  // for snprintf(), perror(), fflush(), and fprintf()
#include <stdlib.h> // for exit()
#include <limits.h> // for PATH_MAX

#define CHECK_SYSCALL_TRACE(call, trace) \
{ \
   int _stat = (call); \
   char msg[PATH_MAX + 1024]; /* should be big enough! */ \
\
   if (_stat < 0) { \
       snprintf(msg, sizeof(msg), \
                "debug: call of %s\n  in %s() (%s:%d) failed -- exiting\n  error", \
                #call, __FUNCTION__, __FILE__, __LINE__); \
       perror(msg); \
       fflush(stderr); \
       exit(EXIT_FAILURE); \
   } else if (trace) { \
       fprintf(stderr, "debug: call of %s\n  in %s() (%s:%d) succeeded\n", \
                #call, __FUNCTION__, __FILE__, __LINE__); \
   } \
}

/*
 *  This should be declared (global) in the `main()` function. Its use is
 *  optional. It could (for example) by a "-d" on the command line. (See the
 *  example `main()` for how to do this.
 */
extern int snapshot_debug;

/*
 *  This is the prototype for the snapshot function.
 */
extern int snapshot(char *fn, char *progpath, char *readme);
