/*
 * This code execs into "busybox init" and hence it is not required
 * to call the compiled binary "(/sbin/)init".
 * One can use the path to this binary as init kernel parameter
 * (i. e. boot the Linux kernel with init=/path/to/bin) instead.
 * Can be used to exec into "toybox init" aswell when compiling with
 * "-DBB=/path/to/the/toybox/bin".
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
#include <libgen.h>
#include <paths.h>

/* path to the BusyBox binary
 * better make sure it exists !!
 */
#define BB		"/bin/busybox"
/* default hostname */
#define HOSTNAME	"darkstar"
/* default shell */
#define SHELL		"/bin/sh"
/* default command search PATH */
#define PATH		"/bin:/sbin:/usr/bin:/usr/sbin"
/* default TERM value (probably "linux") */
#define TERM		"linux"
/* system console device (probably "/dev/console") */
#ifdef _PATH_CONSOLE
# define CONSOLE	_PATH_CONSOLE
#else
# define CONSOLE	"/dev/console"
#endif

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
  Env [ 3 ] = ( s && * s ) ? s - 5 : "TERM=" TERM ;
  /* see if the kernel has set up the CONSOLE env var for us */
  s = getenv ( "CONSOLE" ) ;
  /* (re)use that one (strlen( "CONSOLE=" ) == 8) or a default value */
  Env [ 4 ] = ( s && * s ) ? s - 8 : "CONSOLE=" CONSOLE ;
}

/* execve(2) into a given executable */
static void exec_into ( char * const x, char ** av )
{
  char * b = (char *) NULL ;

  (void) fflush ( NULL ) ;
  (void) execve ( x, av, Env ) ;
  perror ( "execve(2) failed" ) ;
  (void) fflush ( NULL ) ;
  b = basename ( x ) ;

  if ( b && ( x != b ) && * b ) {
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
    (void) execvpe ( b, av, Env ) ;
#endif
    (void) execvp ( b, av ) ;
    perror ( "execve(2) failed" ) ;
    (void) fflush ( NULL ) ;
  }

#if defined (__GLIBC__) && defined (_GNU_SOURCE)
  (void) execvpe ( * av, av, Env ) ;
#endif
  (void) execvp ( * av, av ) ;
  perror ( "execve(2) failed" ) ;
  (void) fflush ( NULL ) ;
}

static int setup_rlimits ( void )
{
  struct rlimit rlim ;
  rlim . rlim_max = RLIM_INFINITY ;
  (void) getrlimit ( RLIMIT_CORE, & rlim ) ;
  rlim . rlim_cur = 0 ;
  return setrlimit ( RLIMIT_CORE, & rlim ) ;
}

/* execve(2) into the real init binary */
static void run_init ( const int argc, char ** argv )
{
  if ( 2 > argc ) {
    char * av [ 2 ] = { "init", (char *) NULL } ;
    exec_into ( BB, av ) ;
    exec_into ( "/bin/busybox", av ) ;
    exec_into ( "/bin/toybox", av ) ;
    /*
    exec_into ( "/sbin/sysv-init", av ) ;
    */
  } else {
    argv [ 0 ] = "init" ;
    exec_into ( BB, argv ) ;
    exec_into ( "/bin/busybox", argv ) ;
    exec_into ( "/bin/toybox", argv ) ;
    /*
    exec_into ( "/sbin/sysv-init", argv ) ;
    */
  }
}

static void sulogin ( void )
{
  char * av [ 2 ] = { "sulogin", (char *) NULL } ;
  exec_into ( "/sbin/sulogin", av ) ;
}

static void sh ( void )
{
  char * av [ 3 ] = { (char *) NULL } ;

  av [ 1 ] = "-i" ;
  av [ 0 ] = "zsh" ;
  exec_into ( "/usr/bin/zsh", av ) ;
  av [ 0 ] = "bash" ;
  /*
  exec_into ( "/usr/bin/bash", av ) ;
  */
  exec_into ( "/bin/bash", av ) ;
  av [ 0 ] = "ksh" ;
  exec_into ( "/bin/mksh", av ) ;
  exec_into ( "/bin/lksh", av ) ;
  exec_into ( "/bin/ksh93", av ) ;
  exec_into ( "/bin/ksh", av ) ;
  av [ 0 ] = "tcsh" ;
  exec_into ( "/usr/bin/tcsh", av ) ;
  av [ 0 ] = "sh" ;
  exec_into ( "/bin/ash", av ) ;
  exec_into ( "/bin/dash", av ) ;
  av [ 1 ] = (char *) NULL ;
  exec_into ( "/bin/sh", av ) ;
}

static int rescue ( void )
{
  (void) fputs (
    "\ninit: Failed to start the real \"init\".\ninit: Trying to exec into \"sulogin\" ...\n\n"
    , stderr ) ;
  (void) fflush ( NULL ) ;
  sulogin () ;
  (void) fputs (
    "\ninit: Failed to start \"sulogin\".\ninit: Trying to exec into a shell ...\n\n"
    , stderr ) ;
  sh () ;
  (void) fflush ( NULL ) ;
  /* very deep shit: kernel panic ahead */
  return 111 ;
}

int main ( const int argc, char ** argv )
{
  if ( 1 < getpid () ) {
    (void) fputs ( "\ninit: Must run as process #1.\n\n", stderr ) ;
    (void) fflush ( NULL ) ;
    return 100 ;
  }

  if ( getuid () || geteuid () ) {
    (void) fputs ( "\ninit: Must be super-user.\n\n", stderr ) ;
    (void) fflush ( NULL ) ;
    return 100 ;
  }

  /* sane process environment */
  setup_env () ;
  (void) umask ( 00022 ) ;
  (void) setup_rlimits () ;
  (void) setsid () ;
  (void) setpgid ( 0, 0 ) ;
  (void) chdir ( "/" ) ;
  /* set a preliminary default hostname that can be easily changed later */
#ifdef HOSTNAME
  (void) sethostname ( HOSTNAME, strlen ( HOSTNAME ) ) ;
#endif
  run_init ( argc, argv ) ;

  return rescue () ;
}

