/*
 * tries to read the previous and the current runlevel
 * from a given or the default utmp file.
 * it prints out the first RUN_LVL entry it finds in that file
 * and terminates.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <utmp.h>
#include <utmpx.h>

int main ( const int argc, char ** argv )
{
  struct utmp * utp = NULL ;

  if ( ( 1 < argc ) && argv [ 1 ] && * argv [ 1 ] ) {
    (void) utmpname ( argv [ 1 ] ) ;
  }

  setutent () ;

  while ( NULL != ( utp = getutent () ) ) {
    if ( RUN_LVL == utp -> ut_type ) {
      char prev = utp -> ut_pid / 256 ;
      prev = prev ? prev : 'N' ;
      (void) printf ( "%c %c\n", prev, utp -> ut_pid % 256 ) ;
      endutent () ;
      return 0 ;
      break ;
    }
  }

  endutent () ;
  (void) puts ( "unknown" ) ;

  return 1 ;
}

