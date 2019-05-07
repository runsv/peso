/*
 * Initialize and serve a login terminal
 *
 * public domain code
 */

//include "utmp.c"
#include "getty.c"

#ifndef _PATH_LOGIN
#  define _PATH_LOGIN  "/bin/login"
#endif

#define print(s) (void) write( STDOUT_FILENO, s, strlen( s ) )

