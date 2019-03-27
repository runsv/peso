/*
 * This code execs into "busybox init" and hence it is not required
 * to call the compiled binary "(/sbin/)init".
 * One can use the path to this binary as init kernel parameter
 * (i. e. boot the Linux kernel with init=/path/to/bin) instead.
 * Can be used to exec into "toybox init" aswell when compiling with
 * "-DBBX=/path/to/the/toybox/bin".
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

/* default command search PATH */
#define PATH		"/bin:/sbin:/usr/bin:/usr/sbin"

/* default shell */
#define SHELL		"/bin/sh"

/* default hostname */
#define HOSTNAME	"darkstar"

/* path to the BusyBox binary
 * make sure it exists !!
 */
#define BBX		"/bin/busybox"

/* default environment to use */
static char * Env [ 6 ] = {
  "HOME=/",
  "SHELL=" SHELL,
  "PATH=" PATH,
  (char *) NULL,
  (char *) NULL,
  (char *) NULL,
} ;

static void setup_env ( void )
{
  /* see if the kernel has already set the TERM env var for us */
  char * s = getenv ( "TERM" ) ;
  /* (re)use that one (strlen( "TERM=" ) == 5) or a default value */
  Env [ 3 ] = ( s && * s ) ? s - 5 : "TERM=linux" ;
  /* see if the kernel has set up the CONSOLE env var for us */
  s = getenv ( "CONSOLE" ) ;
  Env [ 4 ] = ( s && * s ) ? s - 8 : "CONSOLE=/dev/console" ;
}

static int setup_rlimits ( void )
{
  struct rlimit rlim ;
  rlim . rlim_max = RLIM_INFINITY ;
  (void) getrlimit ( RLIMIT_CORE, & rlim ) ;
  rlim . rlim_cur = 0 ;
  return setrlimit ( RLIMIT_CORE, & rlim ) ;
}

int main ( const int argc, char ** argv )
{
  if ( 1 < getpid () ) {
    (void) fputs ( "\ninit: Must run as init process (PID 1).\n\n", stderr ) ;
    return 100 ;
  }

  if ( getuid () || geteuid () ) {
    (void) fputs ( "\ninit: Must be super-user.\n\n", stderr ) ;
    return 100 ;
  }

  if ( 0 < argc ) {
    argv [ 0 ] = "init" ;
  }

  /* set up the process environment */
  setup_env () ;
  (void) umask ( 00022 ) ;
  (void) setup_rlimits () ;
  (void) setsid () ;
  (void) setpgid ( 0, 0 ) ;
  (void) chdir ( "/" ) ;
  /* set a default hostname that can be changed later */
  (void) sethostname ( HOSTNAME, strlen ( HOSTNAME ) ) ;

  /* execve(2) into the real BusyBox binary */
  (void) execve ( BBX, argv, Env ) ;
  (void) execve ( "/bin/busybox", argv, Env ) ;
  (void) execve ( "/sbin/busybox", argv, Env ) ;
  (void) execve ( "/usr/bin/busybox", argv, Env ) ;
  (void) execve ( "/usr/sbin/busybox", argv, Env ) ;
  /* not too helpful when using our default PATH :-\ */
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
  (void) execvpe ( "busybox", argv, Env ) ;
#endif
  (void) execvp ( "busybox", argv ) ;
  perror ( "execve(2) failed" ) ;

  /* BusyBox binary was not found, we are hosed now.
   * do something to prevent kernel panicking:
   * exec into shell or sulogin ?
   */
  (void) fprintf ( stderr,
    "\ninit: Could not find the \"busybox\" binary in PATH\n\t%s\ninit: Execing into sulogin ...\n\n"
    , PATH ) ;
  (void) fflush ( NULL ) ;
  (void) execle ( "/sbin/sulogin", "sulogin", (char *) NULL, Env ) ;
  (void) execle ( "/bin/sulogin", "sulogin", (char *) NULL, Env ) ;
  perror ( "execve(2) failed" ) ;
  (void) fprintf ( stderr,
    "\ninit: Could not find the \"sulogin\" utility.\ninit: Execing into the \"%s\" shell ...\n\n"
    , SHELL ) ;
  (void) fflush ( NULL ) ;
  (void) execle ( SHELL, SHELL, (char *) NULL, Env ) ;
  (void) execlp ( "bash", "bash", "-i", (char *) NULL ) ;
  (void) execlp ( "mksh", "mksh", "-i", (char *) NULL ) ;
  (void) execlp ( "ksh", "ksh", "-i", (char *) NULL ) ;
  (void) execle ( "/bin/sh", "sh", (char *) NULL, Env ) ;
  (void) execlp ( "sh", "sh", (char *) NULL ) ;
  perror ( "execve(2) failed" ) ;

  /* very deep shit: kernel panic ahead */
  return 111 ;
}

