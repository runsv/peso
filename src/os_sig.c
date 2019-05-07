/*
 * wrapper functions to process related syscalls
 *
 * public domain code
 */

/*
 * strsignal, psignal, psiginfo verwenden, wo passend
 */

/* needed by strsignal(3) ? */
/* already declared in "common.h"
extern const char * const sys_siglist [] ;
*/

static int Sstrsignal ( lua_State * L )
{
  const int i = luaL_checkinteger ( L, 1 ) ;
  char * str = strsignal ( i ) ;

  if ( str && * str ) { (void) lua_pushstring ( L, str ) ; }
  else { (void) lua_pushfstring ( L, "unknown signal number %d", i ) ; }

  return 1 ;
}

static int Lpause_forever ( lua_State * L )
{
  while ( 1 ) { (void) pause () ; }

  return 0 ;
}

/* reset signal handling to default behaviour and unblock all signals */
static int Lreset_sigs ( lua_State * L )
{
  /* restore default signal handling behaviour */
  reset_sigs () ;
  return 0 ;
}

/* blocks all signals */
static int Lblock_all_sigs ( lua_State * L )
{
  sigset_t ss ;

  (void) sigfillset ( & ss ) ;
  (void) sigdelset ( & ss, SIGBUS ) ;
  (void) sigdelset ( & ss, SIGFPE ) ;
  (void) sigdelset ( & ss, SIGILL ) ;
  (void) sigdelset ( & ss, SIGSEGV ) ;
#if defined (OSLinux)
  /* those 2 rt signals are use by the NPTL thread library */
  (void) sigdelset ( & ss, 32 ) ;
  (void) sigdelset ( & ss, 33 ) ;
#endif
  NOINTR( sigprocmask ( SIG_SETMASK, & ss, NULL ) )

  return 0 ;
}

/* unblocks all signals */
static int Lunblock_all_sigs ( lua_State * L )
{
  sigset_t ss ;

  (void) sigemptyset ( & ss ) ;
  NOINTR( sigprocmask ( SIG_SETMASK, & ss, NULL ) )

  return 0 ;
}

/*
static int init_sigmask ( lua_State * L )
{
}
*/

/* blocks a given signal */
static int Lblock_sig ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < i && NSIG > i ) {
    sigset_t ss ;

    (void) sigemptyset ( & ss ) ;
    (void) sigaddset ( & ss, i ) ;
    NOINTR( sigprocmask ( SIG_BLOCK, & ss, NULL ) )
  }

  return 0 ;
}

/* unblocks a given signal */
static int Lunblock_sig ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < i && NSIG > i ) {
    sigset_t ss ;

    (void) sigemptyset ( & ss ) ;
    (void) sigaddset ( & ss, i ) ;
    NOINTR( sigprocmask ( SIG_UNBLOCK, & ss, NULL ) )
  }

  return 0 ;
}

/* waits for all pending blocked signals */
static int Lsig_wait_all ( lua_State * L )
{
  int e = 0, i ;
  sigset_t ss ;
  siginfo_t si ;

  (void) sigfillset ( & ss ) ;
  i = sigwaitinfo ( & ss, & si ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;

  return 2 ;
}

/* waits for a given number of seconds for all pending blocked signals */
static int Lsig_timed_wait_all ( lua_State * L )
{
  int e = 0, i ;
  sigset_t ss ;
  siginfo_t si ;
  struct timespec ts ;
  long s = luaL_checkinteger ( L, 1 ) ;

  if ( 0 > s ) { return 0 ; }

  ts . tv_nsec = 0 ;
  ts . tv_sec = s ;
  (void) sigfillset ( & ss ) ;
  i = sigtimedwait ( & ss, & si, & ts ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;

  return 2 ;
}

/* reset signal handling of given signal to default behaviour */
static int Lsig_reset ( lua_State * L ) {
  int e = 0, r = 0, sig = 0 ;
  struct sigaction sa ;

  sa . sa_flags = 0 ;
  sa . sa_handler = SIG_DFL ;
  sig = luaL_checkinteger ( L, 1 ) ;

  if ( 1 > sig || NSIG < sig ) { return 0 ; }

  if ( SIGKILL == sig || SIGSTOP == sig ) {
    return 0 ;
  }

  r = sigemptyset ( & sa . sa_mask ) ;
  r = sigaction ( sig, & sa, NULL ) ;
  e = errno ;
  lua_pushinteger ( L, r ) ;
  lua_pushinteger ( L, e ) ;

  return 2 ;
}

/* sends a given signal to ALL running processes (except our own) */
static int Lkillall5 ( lua_State * L )
{
  int s = luaL_optinteger ( L, 1, SIGTERM ) ;

  s = ( 0 < s && NSIG > s ) ? s : SIGTERM ;
  killall5 ( s ) ;

  return 0 ;
}

/* sends a given signal to ALL running processes (except our own) */
static int Lkill_all ( lua_State * L )
{
  int e = 0, i = 0, s = SIGTERM ;
  sighandler_t sh = signal ( SIGHUP, SIG_IGN ) ;
  const int n = lua_gettop ( L ) ;

#if defined (OSsolaris) || defined (OSLinux) || defined (OSbsd)
#else
  return 0 ;
#endif

  s = luaL_optinteger ( L, 1, SIGTERM ) ;
  if ( 1 > s || NSIG < s ) { return 0 ; }

  if ( SIGKILL != s && SIGSTOP != s ) {
    sh = signal ( s, SIG_IGN ) ;
  }

#if defined (OSsolaris)
  i = kill_all_sol ( s ) ;
#elif defined (OSLinux) || defined (OSbsd)
  i = kill ( -1, s ) ;
#else
#endif

  e = errno ;
  /* restore old signal handler ? */
  if ( 1 < n ) {
    if ( lua_isboolean ( L, 2 ) && lua_toboolean ( L, 2 ) )
    {
      sh = signal ( s, sh ) ;
    }
  }

  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;
  return 2 ;
}

/* nukes all processes */
static int Lnuke ( lua_State * L )
{
  int i = 0, s = -3;
  const int n = lua_gettop ( L ) ;
  sighandler_t st = signal ( SIGTERM, SIG_IGN ) ;
  sighandler_t sh = signal ( SIGHUP, SIG_IGN ) ;

  s = luaL_optinteger ( L, 1, -3 ) ;
#if defined (OSsolaris)
  i += kill_all_sol ( SIGTERM ) ;
  i += kill_all_sol ( SIGHUP ) ;
  i += kill_all_sol ( SIGCONT ) ;
#else
  i += kill ( -1, SIGTERM ) ;
  i += kill ( -1, SIGHUP ) ;
  i += kill ( -1, SIGCONT ) ;
#endif

  sync () ;
#if defined (OSsolaris) || defined (OSLinux) || defined (OSbsd)
  if ( 0 < s ) {
    i += sleep ( s ) ;
  } else {
    i += usleep ( 800 * 1000 ) ;
    i += usleep ( 800 * 1000 ) ;
  }
#endif

#if defined (OSsolaris)
  i += kill_all_sol ( SIGKILL ) ;
#elif defined (OSLinux) || defined (OSbsd)
  i += kill ( -1, SIGKILL ) ;
#else
#endif

  while ( 0 < waitpid ( -1, NULL, WNOHANG ) ) ;
  sync () ;

  /* restore old signal handler ? */
  if ( 1 < n ) {
    if ( lua_isboolean ( L, 2 ) && lua_toboolean ( L, 2 ) )
    {
      st = signal ( SIGTERM, st ) ;
      sh = signal ( SIGTERM, sh ) ;
    }
  }

  lua_pushboolean ( L, 0 == i ) ;
  return 1 ;
}

/* trap a given signal in a pretty dumb way */
static volatile int Lsig = 0 ;
static volatile unsigned char Lsigv [ NSIG ] = { 0 } ;

static int Lgot_sig ( lua_State * L )
{
  int r = 0 ;
  int s = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < s && NSIG > s ) {
    r = ( s == Lsig ) || Lsigv [ s ] ;
  }

  lua_pushinteger( L, r ) ;

  return 1 ;
}

static void Lsig_handler ( const int s )
{
  if ( 0 < s && NSIG > s ) {
    Lsig = s ;
    ++ Lsigv [ s ] ;
  }
}

static int Ltrap_sig ( lua_State * L ) {
  int s = 0 ;
  struct sigaction sa ;

  sa . sa_flags = SA_RESTART | SA_NOCLDSTOP ;
  sa . sa_handler = Lsig_handler ;
  s = luaL_checkinteger ( L, 1 ) ;

  if ( 1 > s || NSIG <= s ) { return 0 ; }

  if ( SIGKILL == s || SIGSTOP == s ) {
    return 0 ;
  }
#ifdef OSLinux
  else if ( 32 == s || 33 == s ) {
    return 0 ;
  }
#endif

  Lsigv [ s ] = 0 ;
  (void) sigemptyset ( & sa . sa_mask ) ;
  NOINTR( sigaction ( s, & sa, NULL ) )

  return 0 ;
}

/* get a new signal fd (Linux only) */
#if defined (OSLinux)
static int Lsignalfd ( lua_State * L )
{
  sigset_t ss ;
  int i = signalfd ( -1, & ss, SFD_NONBLOCK | SFD_CLOEXEC ) ;

  if ( 0 > i ) {
    i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  } else {
    lua_pushinteger ( L, i ) ;
    return 1 ;
  }

  return 0 ;
}
#endif

/*
static lua_State * SL = NULL ;

static int sig_init ( lua_State * L )
{
  SL = L ;

  return 0 ;
}
*/

/* set up our current sigmask */
static void setup_sigmask ( sigset_t * ssp )
{
  /* block all signals */
  (void) sigfillset ( ssp ) ;
  NOINTR( sigprocmask ( SIG_BLOCK, ssp, NULL ) )
  /* remove some signals from the current sigmask again */
  (void) sigdelset ( ssp, SIGSEGV ) ;
  (void) sigdelset ( ssp, SIGILL ) ;
  (void) sigdelset ( ssp, SIGFPE ) ;
  (void) sigdelset ( ssp, SIGBUS ) ;
#  if defined (OSLinux)
  (void) sigdelset ( ssp, 32 ) ;
  (void) sigdelset ( ssp, 33 ) ;
#  endif
  /* and unblock those again */
  NOINTR( sigprocmask ( SIG_SETMASK, ssp, NULL ) )
}

#if 0
/* global variables to store the signal number of received signals */
static volatile int got_sig = 0 ;
static volatile sig_atomic_t got_sigs = 0 ;

/* simple and dumb signal handlers that just save
 * the received signals into global variables
 */
static void handle_sig ( const int s )
{
  if ( 0 < s && NSIG > s ) {
    got_sig = s ;
    if ( 0 < s && 32 > s ) { ADDSET( got_sigs, s ) ; }
  }
}

#if defined (SIGRTMIN)
static volatile int got_rtsig = 0 ;
static volatile sig_atomic_t got_rtsigs = 0 ;

static void handle_rtsig ( const int s )
{
  if ( 0 < s && NSIG > s ) {
    if ( SIGRTMIN <= s
#  if defined (SIGRTMAX)
      && s <= SIGRTMAX
#  endif
    ) {
      const int i = s - SIGRTMIN ;
      got_rtsig = s ;
      if ( 0 <= i && 32 > i ) { ADDSET( got_rtsigs, i ) ; }
    }
  }
}
#endif

/* signal handler for the segv, ill, fpe and bus signals */
static void handle_sigsegv ( const int sig, siginfo_t * sip, void * ucon )
{
  if ( 0 < sig && NSIG > sig ) {
    const int code = sip -> si_code ;
    /*
    int fd = -3 ;
    char * tt = NULL ;
    char * what = NULL ;
    */

    got_sig = sig ;
    ADDSET( got_sigs, sig ) ;
    /* for signals generated by the kernel si_code is > 0
     * (not only on Solaris)
     */
#if defined (OSsolaris) || defined (OSsunos5)
    if ( 1 > code ) { return ; }
#endif
    if ( 0 == ( SI_KERNEL & code ) ) { return ; }
  }
}

/* set up signal handling behaviour */
static void setup_sigs ( void )
{
  int i = 0 ;
  struct sigaction sa ;

  /* initialize global variables that hold the received signals */
  got_sig = got_sigs = 0 ;

  /* zero out the contents of the sigaction struct first */
  (void) memset ( & sa, 0, sizeof ( sa ) ) ;
  sa . sa_flags = SA_RESTART | SA_NOCLDSTOP ;
  sa . sa_handler = handle_sig ;
  (void) sigemptyset ( & sa . sa_mask ) ;
  /* handle some signals via signal handler procedure */
  NOINTR( sigaction ( SIGCHLD, & sa, NULL ) )
  NOINTR( sigaction ( SIGHUP, & sa, NULL ) )
  NOINTR( sigaction ( SIGINT, & sa, NULL ) )
  NOINTR( sigaction ( SIGTERM, & sa, NULL ) )
  NOINTR( sigaction ( SIGUSR1, & sa, NULL ) )
  NOINTR( sigaction ( SIGUSR2, & sa, NULL ) )
#ifdef SIGCLD
  NOINTR( sigaction ( SIGCLD, & sa, NULL ) )
#endif
#ifdef SIGWINCH
  NOINTR( sigaction ( SIGWINCH, & sa, NULL ) )
#endif
#ifdef SIGPWR
  NOINTR( sigaction ( SIGPWR, & sa, NULL ) )
#endif
  /* catch all real time signals where possible */
#if defined (SIGRTMIN)
  got_rtsig = got_rtsigs = 0 ;
  sa . sa_handler = handle_rtsig ;
#  if defined (SIGRTMAX)
  i = SIGRTMAX ;
#  else
  i = NSIG - 1 ;
#  endif
  while ( SIGRTMIN <= i ) {
    NOINTR( sigaction ( i, & sa, NULL ) )
    -- i ;
  }
#endif

  i = getpid () ;
  if ( 2 > i ) {
    /* running as process #1 */
#ifdef OSbsd
    /* things common to ALL (!!) of the BSDs */
#endif
#if defined (OSLinux)
    /* trap SIGKILL on Linux ? this seems to be allowed for process #1 */
    /* make the kernel not reboot and send us the SIGINT signal instead
     * when it receives the crtl-alt-del key sequence
     */
#  if defined (RB_DISABLE_CAD)
    (void) reboot ( RB_DISABLE_CAD ) ;
#  endif

    /* handle the signal sent by the keyboard request keysequence */
#  if defined (SIGWINCH)
    i = open ( VT_MASTER, O_RDWR | O_NONBLOCK | O_CLOEXEC | O_NOCTTY ) ;
    if ( 0 > i ) {
      (void) ioctl ( STDIN_FILENO, KDSIGACCEPT, SIGWINCH ) ;
    } else {
      if ( isatty ( i ) ) { (void) ioctl ( i, KDSIGACCEPT, SIGWINCH ) ; }
      close_fd ( i ) ;
    }
#  endif /* SIGWINCH */
    /* end of Linux part */
#elif defined (OSsolaris) || defined (OSsunos5)
#elif defined (OSfreebsd)
#elif defined (OSdragonfly)
#elif defined (OSnetbsd)
#elif defined (OSopenbsd)
#endif
  } /* end if process #1 */

  /* set up a (non pseudo) handler for segv et al */
  sa . sa_sigaction = handle_sigsegv ;
  sa . sa_flags = SA_SIGINFO ;
  (void) sigfillset ( & sa . sa_mask ) ;
  NOINTR( sigaction ( SIGSEGV, & sa, NULL ) )
  NOINTR( sigaction ( SIGILL, & sa, NULL ) )
  NOINTR( sigaction ( SIGFPE, & sa, NULL ) )
  NOINTR( sigaction ( SIGBUS, & sa, NULL ) )

  /* block all signals as we are at it */
  NOINTR( sigprocmask ( SIG_BLOCK, & sa . sa_mask, NULL ) )
}

/* check pending signals */
static int Lcheck_sigs ( lua_State * L )
{
  static char mask_ok = 0 ;
  static sigset_t ss ;
  int i, s = -3 ;
  siginfo_t si ;
  struct timespec ts ;

  si . si_signo = 0 ;
  i = luaL_optinteger ( L, 1, 2 ) ;
  /* time to wait for a pending signal */
  ts . tv_nsec = 0 ; /* nanoseconds */
  ts . tv_sec = ( 0 < i ) ? i : 2 ; /* seconds */

  if ( 0 == mask_ok ) {
    setup_sigs () ;
    (void) sigfillset ( & ss ) ;
    NOINTR( sigprocmask ( SIG_BLOCK, & ss, NULL ) )
    setup_sigmask ( & ss ) ;
    mask_ok = 3 ;
  }

  s = sigtimedwait ( & ss, & si, & ts ) ;
  if ( 0 < s && NSIG > s ) {
    got_sig = s ;
    if ( 32 > s ) { ADDSET( got_sigs, s ) ; }
#if defined (SIGRTMIN)
    else if ( ( 0 + SIGRTMIN ) <= s &&
#  if defined (SIGRTMAX)
      ( 0 + SIGRTMAX )
#  else
      ( NSIG - 1 )
#  endif
        >= s && 32 > ( s - SIGRTMIN ) )
    { got_rtsig = s ; ADDSET( got_rtsigs, ( s - SIGRTMIN )) ; }
#endif

    lua_pushinteger ( L, s ) ;
    lua_pushinteger ( L, si . si_signo ) ;
    lua_pushinteger ( L, si . si_code ) ;

    if ( SI_USER & si . si_code ) {
      lua_pushinteger ( L, si . si_pid ) ;
      lua_pushinteger ( L, si . si_uid ) ;
      return 5 ;
    } else if ( SI_QUEUE & si . si_code ) {
      /* the signal was sent using sigqueue(2) along with a possible payload */
      lua_pushinteger ( L, si . si_pid ) ;
      lua_pushinteger ( L, si . si_uid ) ;
      i = si . si_value . sival_int ;
      return 6 ;
    }

    return 3 ;
  } /* end if */

  return 0 ;
}
#endif

