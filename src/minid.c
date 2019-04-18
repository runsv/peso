/*
 * minimalistic init (implements process #1)
 */

#include "feat.h"
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include "os.h"
#include "init.h"

/* set process resource (upper) limits */
static int set_rlimits ( void )
{
  struct rlimit rlim ;

  rlim . rlim_cur = 0 ;
  rlim . rlim_max = RLIM_INFINITY ;

  /* set the SOFT (!!) default upper limit of coredump sizes to zero
   * (i. e. disable coredumps). can raised again later in child/sub
   * processes as needed.
   */
  return setrlimit ( RLIMIT_CORE, & rlim ) ;
}

/* accept the signal (SIGWINCH) sent by the kernel when the kbrequest
 * key sequence was typed (on Linux)
 */
static void kbreq ( void )
{
  if ( is_c ( VT_MASTER ) ) {
    const int fd = open ( VT_MASTER, O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC ) ;

    if ( 0 <= fd ) {
      if ( isatty ( fd ) ) { (void) ioctl ( fd, KDSIGACCEPT, SIGWINCH ) ; }

      if ( 0 < fd ) { (void) close_fd ( fd ) ; }

      return ;
    }
  }

  if ( isatty ( 0 ) ) { (void) ioctl ( 0, KDSIGACCEPT, SIGWINCH ) ; }
}

/* minimal default environment used by subprocesses */
static char * Env [ 5 ] = {
  "HOME=/",
  "SHELL=" SHELL,
  "PATH=" PATH,
  (char *) NULL,
  (char *) NULL,
} ;

/* set up default environment for subprocesses */
static void setup_env ( void )
{
  /* see if the kernel has already set the TERM env var for us */
  char * s = getenv ( "TERM" ) ;

  if ( s && * s ) {
    /* The environment contains the TERM var set to a non-empty value,
     * so (re)use that one (strlen( "TERM=" ) == 5).
     */ 
    Env [ 3 ] = s - 5 ;
  } else {
    /* The TERM variable was not set by the kernel, hence use a (hopefully)
     * appropriate default value for it.
     */
    Env [ 3 ] =
#if defined (OSLinux)
      "TERM=linux"
#elif defined (OSfreebsd)
/*    "TERM=cons25"    */
      "TERM=xterm"
#elif defined (OSdragonfly)
/*    "TERM=cons25"    */
      "TERM=xterm"
#elif defined (OSnetbsd)
      "TERM=wsvt25"
#elif defined (OSopenbsd)
      "TERM=pccon"
#elif defined (OSsunos)
# if defined (__sparc__)
      "TERM=sun"
# else
      "TERM=pc"
# endif
#else
      "TERM=vt102"
#endif
      ;
  }
}

/* try to exec into a givan executable */
static void execit ( char ** av )
{
  (void) execve ( * av, av, Env ) ;
  perror ( "execve() failed" ) ;
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
  (void) execvpe ( * av, av, Env ) ;
  perror ( "execvpe() failed" ) ;
#endif
  (void) execvp ( * av, av ) ;
  perror ( "execvp() failed" ) ;
  (void) fflush ( NULL ) ;
}

/* spawn a subprocess, i. e. (v)fork and exec into executable */
static pid_t spawn ( const unsigned long int f, char ** av )
{
  pid_t p = 0 ;

  (void) fflush ( NULL ) ;

  /* vfork(2) instead of fork(2) */
  while ( 0 > ( p = vfork () ) && ENOSYS != errno ) {
    (void) msleep ( 5, 0 ) ;
  }

  if ( 0 == p ) {
    /* child process */
    (void) setsid () ;
    (void) setpgid ( 0, 0 ) ;
    /*
    (void) execve ( * av, av, Env ) ;
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
    (void) execvpe ( * av, av, Env ) ;
#endif
    (void) execvp ( * av, av ) ;
    */
    execit ( av ) ;
    _exit ( 127 ) ;
  } else if ( ( 0 < p ) && ( RUN_WAITPID & f ) ) {
    /* parent process */
    return reap ( 0, p ) ;
  }

  return p ;
}

/* pseudo signal handler for SIGCHILD */
static void sig_child ( const int s )
{
  (void) s ;
}

static volatile sig_atomic_t got_sig = 0 ;

static void sig_misc ( const int s )
{
  if ( 0 < s && NSIG > s && SIGCHLD != s ) {
    /* save the received signal by setting the corresponding bit */
    got_sig |= 1 << s ;
  }
}

static void setup_sigs ( void )
{
  struct sigaction sa ;

  /* initialize global variable */
  got_sig = 0 ;
  /* zero out the sigaction struct before use */
  (void) memset ( & sa, 0, sizeof ( struct sigaction ) ) ;

  sa . sa_flags = SA_RESTART | SA_NOCLDSTOP ;
  sa . sa_handler = sig_child ;
  (void) sigemptyset ( & sa . sa_mask ) ;
  /* catch SIGCHLD */
  (void) sigaction ( SIGCHLD, & sa, NULL ) ;

  /* catch other signals of interest */
  sa . sa_flags = SA_RESTART ;
  sa . sa_handler = sig_misc ;
  (void) sigaction ( SIGTERM, & sa, NULL ) ;
  (void) sigaction ( SIGHUP, & sa, NULL ) ;
  (void) sigaction ( SIGINT, & sa, NULL ) ;
  (void) sigaction ( SIGUSR1, & sa, NULL ) ;
  (void) sigaction ( SIGUSR2, & sa, NULL ) ;
#ifdef SIGWINCH
  (void) sigaction ( SIGWINCH, & sa, NULL ) ;
#endif
#ifdef SIGPWR
  (void) sigaction ( SIGPWR, & sa, NULL ) ;
#endif

  /* unblock all signals */
  (void) sigprocmask ( SIG_SETMASK, & sa . sa_mask, NULL ) ;
}

int main ( const int argc, char ** argv )
{
  (void) umask ( 00022 ) ;
  (void) set_rlimits () ;
  (void) chdir ( "/" ) ;
  /* run boot script */

  while ( 1 ) {
    if ( got_sig ) {
    }

    (void) pause () ;
  }

  return 0 ;
}

