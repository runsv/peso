/*
 * init stage 2
 */

#include "feat.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/environ.h>
#include <s6/config.h>
#include <s6/s6-supervise.h>
#include "os.h"

#if defined (OSLinux)
#  include <sys/prctl.h>
#  include <linux/vt.h>
#  include <linux/kd.h>
#endif

#include "version.h"

#define DIR_RETRY_TIMEOUT	3
#define CHECK_RETRY_TIMEOUT	4
#define FINISH_PROG		S6_SVSCAN_CTLDIR "/finish"
#define CRASH_PROG		S6_SVSCAN_CTLDIR "/crash"
#define SIGNAL_PROG		S6_SVSCAN_CTLDIR "/SIG"
#define SIGNAL_PROG_LEN		(sizeof( SIGNAL_PROG ) - 1)
#define USAGE			"s6-svscan [ -S | -s ] [ -c maxservices ] [ -t timeout ] [ -d notif ] [ dir ]"
#define dieusage()		strerr_dieusage( 100, USAGE )

/* integer constants */
emum {
  DIR_RETRY_TIMEOUT			= 3,
  CHECK_RETRY_TIMEOUT			= 4,
  FLAG_DIVERT_SIGNALS_OFF		= 0x01,
  FLAG_USE_MQUEUE			= 0x02,
  FLAG_USE_ABSTRACT			= 0x04,
  WANT_KILL				= 0x01,
} ;

struct svinfo_s {
  dev_t dev ;
  ino_t ino ;
  tain_t restartafter [ 2 ] ;
  pid_t pid [ 2 ] ;
  int p [ 2 ] ;
  unsigned int flagactive : 1 ;
  unsigned int flaglog : 1 ;
} ;

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

static size_t max = 500, n = 0 ;
static int wantreap = 1 ;
static int wantscan = 1 ;
static unsigned int wantkill = 0 ;
static int cont = 1 ;
static unsigned long int what = 0, got_sig = 0 ;
static char const * finish_arg = "reboot" ;
static tain_t deadline, defaulttimeout ;
static struct svinfo_s * services ;

static void panicnosp ( const char * ) gccattr_noreturn ;

static void panicnosp ( const char * errmsg )
{
  char const * eargv [ 2 ] = { CRASH_PROG, 0 } ;

  strerr_warnwu1sys ( errmsg ) ;
  strerr_warnw2x ( "executing into ", eargv [ 0 ] ) ;
  //(void) execve ( eargv [ 0 ], (char * const *) eargv, (char * const *) environ ) ;
  (void) execv ( eargv [ 0 ], (char * const *) eargv ) ;
  /* and if that execve fails, screw it and just die */
  strerr_dieexec ( 111, eargv [ 0 ] ) ;
}

static void panic ( char const * ) gccattr_noreturn ;

static void panic ( char const * errmsg )
{
  const int e = errno ;

  selfpipe_finish () ;
  errno = e ;
  panicnosp ( errmsg ) ;
}

static void killthem ( void )
{
  unsigned int i = 0 ;

  if ( ! wantkill ) { return ; }

  for ( i = 0 ; i < n ; ++ i ) {
    if ( ! ( wantkill & 1 ) && services [ i ] . flagactive ) { continue ; }

    if ( services [ i ] . pid [ 0 ] ) {
      (void) kill ( services [ i ] . pid [ 0 ], ( wantkill & 2 ) ? SIGTERM : SIGHUP ) ;
    }

    if ( services [ i ] . flaglog && services [ i ] . pid [ 1 ] ) {
      (void) kill ( services [ i ] . pid [ 1 ], ( wantkill & 4 ) ? SIGTERM : SIGHUP ) ;
    }
  }

  wantkill = 0 ;
}

static void term ( void )
{
  cont = 0 ;
  wantkill = 3 ;
}

static void hup ( void )
{
  cont = 0 ;
  wantkill = 1 ;
}

static void quit ( void )
{
  cont = 0 ;
  wantkill = 7 ;
}

static void intr ( void )
{
  finish_arg = "reboot" ;
  term () ;
}

static void usr1 ( void )
{
  finish_arg = "poweroff" ;
  term () ;
}

static void handle_signals ( void )
{
  while ( 1 ) {
    switch ( selfpipe_read () ) {
      case -1 : panic ( "selfpipe_read" ) ;
      case 0 : return ; break ;
      case SIGCHLD : wantreap = 1 ; break ;
      case SIGALRM : wantscan = 1 ; break ;
      case SIGABRT : cont = 0 ; break ;
      case SIGTERM : term() ; break ;
      case SIGHUP : hup() ; break ;
      case SIGQUIT : quit() ; break ;
      case SIGINT : intr() ; break ;
    }
  }
}

static void handle_diverted_signals ( void )
{
  while ( 1 ) {
    const int sig = selfpipe_read () ;

    switch ( sig ) {
      case -1 : panic ( "selfpipe_read" ) ;
      case 0 : return ; break ;
      case SIGCHLD : wantreap = 1 ; break ;
      case SIGALRM : wantscan = 1 ; break ;
      case SIGABRT : cont = 0 ; break ;
      default :
        {
          char const * const name = sig_name ( sig ) ;
          const size_t len = strlen ( name ) ;
          char fn [ SIGNAL_PROG_LEN + len + 1 ] ;
          const char * const newargv [ 2 ] = { fn, 0 } ;

          (void) memcpy ( fn, SIGNAL_PROG, SIGNAL_PROG_LEN ) ;
          (void) memcpy ( fn + SIGNAL_PROG_LEN, name, len + 1 ) ;

          if  ( ! child_spawn0 ( newargv [ 0 ], newargv, (char const **) environ ) )
            strerr_warnwu2sys ( "spawn ", newargv [ 0 ] ) ;
        }
        break ;
    }
  }
}

static void handle_control ( const int fd )
{
  while ( 1 ) {
    char c = 0 ;
    const ssize_t r = sanitize_read ( fd_read ( fd, & c, 1 ) ) ;

    if ( r < 0 ) panic ( "read control pipe" ) ;
    else if ( ! r ) break ;
    else switch ( c ) {
      case 'p' : finish_arg = "poweroff" ; break ;
      case 'h' : hup () ; return ;
      case 'r' : finish_arg = "reboot" ; break ;
      case 'a' : wantscan = 1 ; break ;
      case 't' : term () ; return ;
      case 's' : finish_arg = "halt" ; break ;
      case 'z' : wantreap = 1 ; break ;
      case 'b' : cont = 0 ; return ;
      case 'n' : wantkill = 2 ; break ;
      case 'N' : wantkill = 6 ; break ;
      case '6' :
      case 'i' : intr () ; return ;
      case 'q' : quit () ; return ;
      case '0' : finish_arg = "halt" ; term () ; return ;
      case '7' : usr1 () ; return ;
      case '8' : finish_arg = "other" ; term () ; return ;
      default : {
          char s [ 2 ] = { c, 0 } ;
          strerr_warnw2x ( "received unknown control command: ", s ) ;
        }
        break ;
    }
  }
}

/* First essential function: the reaper.
 * s6-svscan must wait() for all children,
 * including ones it doesn't know it has.
 * Dead active services are flagged to be restarted in 1 second.
 */
static void reap ( void )
{
  tain_t nextscan ;

  if ( ! wantreap ) return ;

  wantreap = 0 ;
  tain_addsec_g ( & nextscan, 1 ) ;

  while ( 1 ) {
    int wstat = 0 ;
    pid_t r = wait_nohang ( & wstat ) ;

    if ( r < 0 )
      if ( errno != ECHILD ) panic ( "wait_nohang" ) ;
      else break ;
    else if ( ! r ) break ;
    else {
      unsigned int i = 0 ;

      for ( i = 0 ; i < n ; ++ i ) {
        if ( services [ i ] . pid [ 0 ] == r ) {
          services [ i ] . pid [ 0 ] = 0 ;
          services [ i ] . restartafter [ 0 ] = nextscan ;
          break ;
        } else if ( services [ i ] . pid [ 1 ] == r ) {
          services [ i ] . pid [ 1 ] = 0 ;
          services [ i ] . restartafter [ 1 ] = nextscan ;
          break ;
        }
      }

      if ( i == n ) continue ;

      if ( services [ i ] . flagactive ) {
        if (tain_less(&nextscan, &deadline)) deadline = nextscan ;
      } else {
        if ( services [ i ] . flaglog ) {
 /*
    BLACK MAGIC:
     - we need to close the pipe early:
       * as soon as the writer exits so the logger can exit on EOF
       * or as soon as the logger exits so the writer can crash on EPIPE
     - but if the same service gets reactivated before the second
       supervise process exits, ouch: we've lost the pipe
     - so we can't reuse the same service even if it gets reactivated
     - so we're marking a dying service with a closed pipe
     - if the scanner sees a service with p[0] = -1 it won't flag
       it as active (and won't restart the dead supervise)
     - but if the service gets reactivated we want it to restart
       as soon as the 2nd supervise process dies
     - so the scanner marks such a process with p[0] = -2
     - and the reaper triggers a scan when it finds a -2.
 */
          if (services[i].p[0] >= 0) {
            fd_close(services[i].p[1]) ; services[i].p[1] = -1 ;
            fd_close(services[i].p[0]) ; services[i].p[0] = -1 ;
          } else if (services[i].p[0] == -2) wantscan = 1 ;
        }

        if (!services[i].pid[0] && (!services[i].flaglog || !services[i].pid[1]))
          services [ i ] = services [ -- n ] ;
      }
    }
  }
}


/* Second essential function: the scanner.
   It monitors the service directories and spawns a supervisor
   if needed. */

static void trystart ( unsigned int i, char const * name, int islog )
{
  const pid_t pid = fork () ;

  switch ( pid ) {
    case -1 :
      tain_addsec_g(&services[i].restartafter[islog], CHECK_RETRY_TIMEOUT) ;
      strerr_warnwu2sys("fork for ", name) ;
      return ;
    case 0 :
    {
      char const *cargv[3] = { "s6-supervise", name, 0 } ;
      PROG = "s6-svscan (child)" ;
      selfpipe_finish() ;
      if (services[i].flaglog)
        if (fd_move(!islog, services[i].p[!islog]) == -1)
          strerr_diefu2sys(111, "set fds for ", name) ;
      xpathexec_run(S6_BINPREFIX "s6-supervise", cargv, (char const **)environ) ;
    }
  }

  services[i].pid[islog] = pid ;
}

static void retrydirlater ( void )
{
  tain_t a ;
  tain_addsec_g ( & a, DIR_RETRY_TIMEOUT ) ;
  if ( tain_less ( & a, & deadline ) ) deadline = a ;
}

static void check ( char const * name )
{
  struct stat st ;
  size_t namelen ;
  unsigned int i = 0 ;

  if (name[0] == '.') return ;

  if (stat(name, &st) == -1) {
    strerr_warnwu2sys("stat ", name) ;
    retrydirlater() ;
    return ;
  }

  if ( ! S_ISDIR( st . st_mode ) ) return ;

  namelen = strlen(name) ;

  for (; i < n ; i++) if ((services[i].ino == st.st_ino) && (services[i].dev == st.st_dev)) break ;

  if ( i < n ) {
    if (services[i].flaglog && (services[i].p[0] < 0)) {
     /* See BLACK MAGIC above. */
      services[i].p[0] = -2 ;
      return ;
    }
  } else {
    if (n >= max) {
      strerr_warnwu3x("start supervisor for ", name, ": too many services") ;
      return ;
    } else {
      struct stat su ;
      char tmp[namelen + 5] ;
      memcpy(tmp, name, namelen) ;
      memcpy(tmp + namelen, "/log", 5) ;
      if (stat(tmp, &su) < 0)
        if (errno == ENOENT) services[i].flaglog = 0 ;
        else {
          strerr_warnwu2sys("stat ", tmp) ;
          retrydirlater() ;
          return ;
        }
      else if (!S_ISDIR(su.st_mode))
        services[i].flaglog = 0 ;
      else {
        if (pipecoe(services[i].p) < 0) {
          strerr_warnwu1sys("pipecoe") ;
          retrydirlater() ;
          return ;
        }
        services[i].flaglog = 1 ;
      }
      services[i].ino = st.st_ino ;
      services[i].dev = st.st_dev ;
      tain_copynow(&services[i].restartafter[0]) ;
      tain_copynow(&services[i].restartafter[1]) ;
      services[i].pid[0] = 0 ;
      services[i].pid[1] = 0 ;
      ++ n ;
    }
  }
  
  services[i].flagactive = 1 ;

  if ( services [ i ] . flaglog && ! services [ i ] . pid [ 1 ] ) {
    if ( ! tain_future( & services [ i ] . restartafter [ 1 ] ) ) {
      char tmp [ namelen + 5 ] ;
      memcpy(tmp, name, namelen) ;
      memcpy(tmp + namelen, "/log", 5) ;
      trystart(i, tmp, 1) ;
    } else if (tain_less(&services[i].restartafter[1], &deadline))
      deadline = services[i].restartafter[1] ;
  }

  if ( ! services[i].pid[0]) {
    if (!tain_future(&services[i].restartafter[0]))
      trystart(i, name, 0) ;
    else if (tain_less(&services[i].restartafter[0], &deadline))
      deadline = services[i].restartafter[0] ;
  }
}

static void scan ( void )
{
  unsigned int i = 0 ;
  DIR * dir = NULL ;

  if ( ! wantscan ) return ;

  wantscan = 0 ;
  tain_add_g ( & deadline, & defaulttimeout ) ;
  dir = opendir ( "." ) ;

  if ( NULL == dir ) {
    strerr_warnwu1sys ( "opendir ." ) ;
    retrydirlater () ;
    return ;
  }

  for ( ; i < n ; ++ i ) services [ i ] . flagactive = 0 ;

  while ( 1 ) {
    direntry * d = NULL ;

    errno = 0 ;
    d = readdir ( dir ) ;

    if ( d ) { check ( d -> d_name ) ; }
    else { break ; }
  }

  if ( errno ) {
    strerr_warnwu1sys ( "readdir ." ) ;
    retrydirlater () ;
  }

  dir_close ( dir ) ;

  for ( i = 0 ; i < n ; ++ i )
    if ( ! services [ i ] . flagactive && ! services [ i ] . pid [ 0 ] ) {
    if ( services [ i ] . flaglog ) {
      if ( services [ i ] . pid [ 1 ] ) continue ;

      if ( services [ i ] . p [ 0 ] >= 0 ) {
        fd_close ( services [ i ] . p [ 1 ] ) ; services [ i ] . p [ 1 ] = -1 ;
        fd_close ( services [ i ] . p [ 0 ] ) ; services [ i ] . p [ 0 ] = -1 ;
      }
    }

    services [ i ] = services [ -- n ] ;
  }
}

static void sig_setup ( void )
{
}

int main ( int argc, char const * const * argv )
{
  char divertsignals = 0 ;
  unsigned int notif = 0 ;
  unsigned long int f = 0 ;
  const pid_t mypid = getpid () ;
  const uid_t myuid = getuid () ;
  iopause_fd x [ 2 ] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;

  /* initialize global variables */
  PROG = "s6-svscan" ;
  /* drop possible privileges we should not have */
  (void) seteuid ( myuid ) ;
  (void) setegid ( getgid () ) ;

  if ( 1 < mypid ) {
    (void) umask ( umask ( 00077 ) ) ;
  } else {
    if ( myuid || geteuid () ) {
      (void) fprintf ( stderr, "\r%s:\tmust be super user\r\n", pname ) ;
      (void) fflush ( NULL ) ;
      return 100 ;
    }

    (void) umask ( 00022 ) ;
    (void) chdir ( "/" ) ;
  }

  (void) setsid () ;
  (void) setpgid ( 0, 0 ) ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;

    while ( 1 ) {
      const int opt = subgetopt_r ( argc, argv, "Sst:c:d:", & l ) ;

      if ( 1 > opt ) { break ; }

      switch ( opt ) {
        case 'S' :
          if ( 1 < mypid ) { f |= FLAG_DIVERT_SIGNALS_OFF ; }
          break ;
        case 's' :
          divertsignals = 1 ;
          break ;
        case 't' :
          if ( uint0_scan ( l . arg, & t ) ) { break ; }
        case 'c' :
          if ( uint0_scan ( l . arg, & max ) ) { break ; }
        case 'd' :
          if ( 0 == uint0_scan ( l . arg, & notif ) ) {
            dieusage () ;
            return 100 ;
          }

          if ( 3 > notif ) {
            strerr_dief1x ( 100, "notification fd must be 3 or more" ) ;
            return 100 ;
          }

          if ( 0 > fcntl ( notif, F_GETFD ) ) {
            strerr_dief1sys ( 100, "invalid notification fd" ) ;
            return 111 ;
          }

          break ;
        default :
          dieusage () ;
          return 100 ;
          break ;
      }
    }

    argc -= l . ind ;
    argv += l . ind ;

    if ( t ) tain_from_millisecs ( & defaulttimeout, t ) ;
    else defaulttimeout = tain_infinite_relative ;

    if ( max < 2 ) max = 2 ;
  }

  /* Init phase.
   * If something fails here, we can die, because it means that
   * something is seriously wrong with the system, and we can't
   * run correctly anyway.
   */
  if ( 0 < argc && 0 != chdir ( argv [ 0 ] ) ) strerr_diefu1sys ( 111, "chdir" ) ;

  x [ 1 ] . fd = s6_supervise_lock ( S6_SVSCAN_CTLDIR ) ;
  x [ 0 ] . fd = selfpipe_init () ;

  if ( x [ 0 ] . fd < 0 ) strerr_diefu1sys ( 111, "selfpipe_init" ) ;

  if ( sig_ignore ( SIGPIPE ) < 0 ) strerr_diefu1sys ( 111, "ignore SIGPIPE" ) ;

  {
    sigset_t set ;
    sigemptyset ( & set ) ;
    sigaddset ( & set, SIGCHLD ) ;
    sigaddset ( & set, SIGALRM ) ;
    sigaddset ( & set, SIGTERM ) ;
    sigaddset ( & set, SIGHUP ) ;
    sigaddset ( & set, SIGQUIT ) ;
    sigaddset ( & set, SIGABRT ) ;
    sigaddset ( & set, SIGINT ) ;

    if ( divertsignals ) {
      sigaddset ( & set, SIGUSR1 ) ;
      sigaddset ( & set, SIGUSR2 ) ;
    }

    if ( selfpipe_trapset ( & set ) < 0 ) strerr_diefu1sys ( 111, "trap signals" ) ;
  }

  if ( notif ) {
    fd_write ( notif, "\n", 1 ) ;
    fd_close ( notif ) ;
    notif = 0 ;
  }

  {
    struct svinfo_s blob [ max ] ; /* careful with that stack, Eugene */
    services = blob ;
    tain_now_g () ;


    /* Loop phase.
     * From now on, we must not die.
     * Temporize on recoverable errors, and panic on serious ones.
     */
    while ( cont ) {
      int r = 0 ;

      reap () ;
      scan () ;
      killthem () ;
      r = iopause_g ( x, 2, & deadline ) ;

      if ( r < 0 ) panic ( "iopause" ) ;
      else if ( ! r ) wantscan = 1 ;
      else {
        if ( ( x [ 0 ] . revents | x [ 1 ] . revents ) & IOPAUSE_EXCEPT ) {
          errno = EIO ;
          panic("check internal pipes") ;
        }

        if ( x [ 0 ] . revents & IOPAUSE_READ )
          divertsignals ? handle_diverted_signals () : handle_signals () ;

        if ( x [ 1 ] . revents & IOPAUSE_READ ) handle_control ( x [ 1 ] . fd ) ;
      }
    }

    /* Finish phase */
    selfpipe_finish () ;
    killthem () ;
    reap () ;
  }

  {
    char const * eargv [ 3 ] = { FINISH_PROG, finish_arg, 0 } ;
    //(void) execve ( eargv [ 0 ], (char **) eargv, (char * const *) environ ) ;
    (void) execv ( eargv [ 0 ], (char **) eargv ) ;
  }

  panicnosp ( "exec finish script " FINISH_PROG ) ;

  return 111 ;
}

