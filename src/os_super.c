/*
 * event loop for the supervisor daemon
 */

static int sys_stage ( lua_State * L, const char * name )
{
  if ( name && * name ) {
    int i = -3 ;

    if ( LUA_TFUNCTION == lua_getglobal ( L, name ) ) {
      i = lua_pcall ( L, 0, 1, 0 ) ;

      if ( i ) {
        /* error */
        i = -1 ;
      } else {
        /* function call succeeded (0 == LUA_OK), check for results */
        i = 0 ;

        if ( lua_isboolean ( L, -1 ) ) {
          i = lua_toboolean ( L, -1 ) ;
        } else if ( lua_isinteger ( L, -1 ) ) {
          i = lua_tointeger ( L, -1 ) ;
          i = abs ( i ) ;
        }
      }
    }

    (void) clear_stack ( L ) ;
    return i ;
  }

  return -5 ;
}

/* pass data (consisting of an integer and a string) to Lua and
 * use the results we get from the Lua hook function.
 */
static int pass_data ( lua_State * L, int * iptr, char * buf,
  const size_t len )
{
  int i = -3 ;

  if ( LUA_TFUNCTION == lua_getglobal ( L, PESI_DATA_HOOK ) ) {
    lua_pushinteger ( L, iptr ? * iptr : 0 ) ;
    i = 1 ;

    if ( ( 0 < len ) && buf && * buf ) {
      (void) lua_pushlstring ( L, buf, str_nlen ( buf, len ) ) ;
      i = 2 ;
    }

    i = lua_pcall ( L, i, 2, 0 ) ;

    if ( i ) {
      /* error */
      i = -1 ;
    } else {
      /* function call succeeded (0 == LUA_OK), check for results */
      i = 0 ;

      if ( lua_isinteger ( L, -2 ) ) {
        * iptr = lua_tointeger ( L, -2 ) ;
        i = 1 ;
      }

      if ( lua_isstring ( L, -1 ) ) {
        size_t s = 0 ;
        const char * msg = lua_tolstring ( L, -1, & s ) ;

        if ( ( 0 < s ) && msg && * msg ) {
          (void) strcopy ( buf, msg, ( s < len ) ? s : len ) ;
          i = 2 ;
        }
      }
    }
  }

  (void) clear_stack ( L ) ;
  return i ;
}

/* pass the results of waitpid(2) via a hook function to Lua */
static int pass_pid ( lua_State * L, const pid_t pid, const int wstatus )
{
  int i = -3 ;

  if ( LUA_TFUNCTION == lua_getglobal ( L, PESI_PID_HOOK ) ) {
    lua_pushinteger ( L, pid ) ;
    i = 1 ;

    if ( WIFEXITED( wstatus ) ) {
      lua_pushinteger ( L, WEXITSTATUS( wstatus ) ) ;
      lua_pushinteger ( L, 1 ) ;
      i = 3 ;
    } else if ( WIFSIGNALED( wstatus ) ) {
      lua_pushinteger ( L, WTERMSIG( wstatus ) ) ;
      lua_pushinteger ( L, 2 ) ;
      i = 3 ;
    } else if ( WIFSTOPPED( wstatus ) ) {
      lua_pushinteger ( L, WSTOPSIG( wstatus ) ) ;
      lua_pushinteger ( L, 3 ) ;
      i = 3 ;
    }

    i = lua_pcall ( L, i, 0, 0 ) ;
    i = i ? -1 : 0 ;
  }

  (void) clear_stack ( L ) ;
  return i ;
}

/* sets up the server unix domain socket */
static int serv_init ( void )
{
  int i = 1, sd = -1 ;
  struct sockaddr_un serv_ad ;

  /* setup the socket address of the (abstract) unix domain
   * socket we will use for ipc
   */
  (void) memset ( & serv_ad, 0, sizeof ( serv_ad ) ) ;
  serv_ad . sun_family = AF_UNIX ;
  (void) strcopy ( 1 + serv_ad . sun_path, "initsock",
    sizeof ( serv_ad . sun_path ) - 1 ) ;
  /* use an abstract unix socket on Linux */
  serv_ad . sun_path [ 0 ] = '\0' ;
  sd = socket ( AF_UNIX,
    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0 ) ;

  if ( 0 > sd ) { return -1 ; }

  (void) fcntl ( sd, F_SETFD, FD_CLOEXEC ) ;
  (void) fcntl ( sd, F_SETFL, O_NONBLOCK ) ;

  if ( bind ( sd, (struct sockaddr *) & serv_ad,
    sizeof ( struct sockaddr_un ) ) ) { return -3 ; }

  if ( 0 > setsockopt ( sd, SOL_SOCKET, SO_PASSCRED, & i, sizeof ( i ) ) )
  { return -5 ; }

  if ( listen ( sd, 5 ) ) { return -5 ; }

  return sd ;
}

/* accept incoming client connections on the server's unix domain socket */
static int sock_acc ( const int serv_fd )
{
  if ( 0 <= serv_fd ) {
    struct sockaddr_un peer_ad ;
    socklen_t s = sizeof ( struct sockaddr_un ) ;
    const int i = accept ( serv_fd, (struct sockaddr *) & peer_ad, & s ) ;

    if ( 0 <= i ) {
      struct ucred uc ;

      s = sizeof ( struct ucred ) ;
      (void) memset ( & uc, 0, sizeof ( uc ) ) ;

      /* retrieve peer credentials */
      if ( 0 == getsockopt ( i, SOL_SOCKET, SO_PEERCRED, & uc, & s )
        && 0 == uc . uid && 0 == uc . gid && 1 < uc . pid
        && 0 == kill ( uc . pid, 0 ) )
      { return i ; }
      else { (void) close_fd ( i ) ; }
    }

    return -1 ;
  }

  return -3 ;
}

/* bring down the system */
static int sys_halt ( lua_State * L, int cmd )
{
  //int i = -3 ;
  pid_t p = 0 ;
  char * msg = NULL ;
  const pid_t mypid = getpid () ;
  cmd = ( 0 < cmd ) ? cmd : 'r' ;

  /* reap possible zombie processes */
  do { p = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

  /* never hurts for process #1 */
  (void) chdir ( "/" ) ;
  /* change the runlevel record in utmp from 3 to 0/6 */
  (void) utmp_set_runlevel ( ( 'r' == cmd || 'R' == cmd ) ? 6 : 0, 3 ) ;
  /* write shutdown time record to utmp */
  (void) utmp_set_halt () ;
  /* try to redirect std out/err to the system console device */
  //rediroe ( CONSOLE ) ;
  /* change active vt to tty1 */
  (void) chvt ( 1 ) ;

  /* start the stage 3 tasks now */
  msg =
    "\r\n\r\033[1;32;44m  [init] Running shutdown tasks  ...  \033[0m\r\n\r\n"
    ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
  if ( sys_stage ( L, PESI_SYS_DOWN ) ) {
  }

  /* change active vt to tty1 */
  (void) chvt ( 1 ) ;

  /* reap possible zombie processes */
  do { p = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

  /* kill all running processes */
#if defined (OSsunos)
  (void) kill_all_sol ( SIGTERM ) ;
  (void) kill_all_sol ( SIGCONT ) ;
#else
  (void) kill ( -1, SIGTERM ) ;
  (void) kill ( -1, SIGCONT ) ;
#endif
  msg = "\r[init] Sent the TERM signal to all processes\r\n" ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
  do_sleep ( 1, 0 ) ;
#if defined (OSsunos)
  (void) kill_all_sol ( SIGHUP ) ;
#else
  (void) kill ( -1, SIGHUP ) ;
#endif
  msg = "\r[init] Sent the HUP signal to all processes\r\n" ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
  do_sleep ( 0, 300 * 1000 ) ;
#if defined (OSsunos)
  (void) kill_all_sol ( SIGINT ) ;
#else
  (void) kill ( -1, SIGINT ) ;
#endif
  do_sleep ( 0, 600 * 1000 ) ;
#if defined (OSsunos)
  (void) kill_all_sol ( SIGKILL ) ;
#else
  (void) kill ( -1, SIGKILL ) ;
#endif
  msg = "\r[init] Sent the KILL signal to all processes\r\n" ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
  do_sleep ( 0, 10 * 1000 ) ;

  /* reap possible zombie processes */
  do { p = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

  sync () ;
  /* start the stage 3 child process now and wait for it's termination */
  msg =
    "\r\n\r\033[1;32;44m  [inid] Running shutdown tasks  ...  \033[0m\r\n\r\n"
    ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
  if ( sys_stage ( L, PESI_SYS_HALT ) ) {
  }

  /* reap possible zombie processes */
  do { p = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

  (void) clear_stack ( L ) ;
  if ( L ) { lua_close ( L ) ; }
  /* change active vt to tty1 */
  (void) chvt ( 1 ) ;
  sync () ;
  /* terminate all remaining processes (except our own)
   * Kill all remaining processes. it is safe to use -1 here
   * since we run as PID 1 and are thus exempt from receiving
   * the signal on Linux, BSD and even Solaris.
   */ 
  msg = "\r[inid] Killing all remaining processes ...\r\n" ;
  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
#if defined (OSsunos)
  (void) kill_all_sol ( SIGTERM ) ;
  (void) kill_all_sol ( SIGHUP ) ;
  (void) kill_all_sol ( SIGINT ) ;
  (void) kill_all_sol ( SIGCONT ) ;
  (void) kill_all_sol ( SIGKILL ) ;
#else
  (void) kill ( -1, SIGTERM ) ;
  (void) kill ( -1, SIGHUP ) ;
  (void) kill ( -1, SIGINT ) ;
  (void) kill ( -1, SIGKILL ) ;
  (void) kill ( -1, SIGCONT ) ;
#endif
  do_sleep ( 0, 10 * 1000 ) ;

  /* reap possible zombie processes */
  do { p = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

  sync () ;

  if ( 'h' == cmd || 'H' == cmd ) {
    msg = "\r\n\r\033[1;32;44m  [init] Requesting system halt ...  \033[0m\r\n\r\n" ;
  } else if ( 'p' == cmd || 'P' == cmd ) {
    msg = "\r\n\r\033[1;32;44m  [init] Requesting system poweroff ...  \033[0m\r\n\r\n" ;
  } else {
    msg = "\r\n\r\033[1;32;44m  [init] Requesting system reboot ...  \033[0m\r\n\r\n" ;
  }

  (void) write ( STDOUT_FILENO, msg, strlen ( msg ) ) ;
#if defined (OSLinux)
  (void) do_reboot ( 'C' ) ;
#endif
  close_all ( -3 ) ;

  if ( 1 < mypid ) {
    if ( 0 > do_reboot ( cmd ) ) {
      (void) do_reboot ( 'r' ) ;
    }
  } else {
    (void) fork_reboot ( cmd ) ;

    do { p = wait ( NULL ) ; }
    while ( ( 0 < p ) || ( 0 > p && EINTR == errno ) ) ;

    while ( 1 ) {
      (void) pause () ;
      (void) delay ( 3, 0 ) ;
    }
  }

  return 0 ;
}

/* pseudo signal handler for sigchild */
static void sig_child ( const int sig )
{
  (void) sig ;
}

static volatile sig_atomic_t got_sig = 0 ;

static void sig_misc ( const int sig )
{
  if ( 0 < sig && NSIG > sig && SIGCHLD != sig ) {
    got_sig |= 1 << sig ;
  }
}

#if defined (SIGRTMIN) && defined (SIGRTMAX)
static volatile sig_atomic_t got_rt_sig = 0 ;

static void sig_rt ( const int sig )
{
  if ( SIGRTMIN <= sig && SIGRTMAX >= sig ) {
    got_rt_sig |= 1 << sig ;
  }
}
#endif

static void sig_segv ( const int sig )
{
  (void) sig ;
}

static void sig_setup ( void )
{
  int i ;
  struct sigaction sa ;

  got_sig = 0 ;
  (void) memset ( & sa, 0, sizeof ( struct sigaction ) ) ;
  sa . sa_flags = SA_RESTART ;
  sa . sa_handler = SIG_DFL ;
  (void) sigemptyset ( & sa . sa_mask ) ;

  /* first restore default dispostions for all signals */
  for ( i = 1 ; NSIG > i ; ++ i ) {
    if ( SIGKILL != i && SIGSTOP != i && SIGCHLD != i ) {
      (void) sigaction ( i, & sa, NULL ) ;
    }
  }

  /* catch sigchild */
  sa . sa_handler = sig_child ;
  (void) sigaction ( SIGCHLD, & sa, NULL ) ;
  /* catch other signals of interest */
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

#if defined (SIGRTMIN) && defined (SIGRTMAX)
  got_rt_sig = 0 ;
  sa . sa_handler = sig_rt ;

  for ( i = SIGRTMIN ; SIGRTMAX >= i ; ++ i ) {
    (void) sigaction ( i, & sa, NULL ) ;
  }
#endif

  /* block all signals for the moment */
  /*
  (void) sigfillset ( & sa . sa_mask ) ;
  */
  (void) sigprocmask ( SIG_SETMASK, & sa . sa_mask, NULL ) ;
}

static int evLoopLinux ( lua_State * L )
{
  int i, sock, cfd, sfd ;
  pid_t p = 0 ;
  sigset_t mask ;
  struct pollfd pfdv [ 3 ] ;

  /* set up some variables first */
  pfdv [ 0 ] . events = POLLIN ;
  pfdv [ 1 ] . events = POLLIN ;
  pfdv [ 2 ] . events = POLLIN ;
  pfdv [ 0 ] . fd = -1 ;
  pfdv [ 1 ] . fd = -1 ;
  pfdv [ 2 ] . fd = -1 ;
  /* initialize global variables */
  got_sig = 0 ;
#if defined (SIGRTMIN) && defined (SIGRTMAX)
  got_rt_sig = 0 ;
#endif
  /* set up signal handling */
  setup_sigs () ;
  (void) sigemptyset ( & mask ) ;
  (void) sigaddset ( & mask, SIGCHLD ) ;
  cfd = signalfd ( -1, & mask, SFD_NONBLOCK | SFD_CLOEXEC ) ;
  pfdv [ 0 ] . fd = cfd ;
  /* block most signals */
  (void) sigfillset ( & mask ) ;
  (void) sigdelset ( & mask, SIGSEGV ) ;
  (void) sigdelset ( & mask, SIGILL ) ;
  (void) sigdelset ( & mask, SIGFPE ) ;
  (void) sigdelset ( & mask, SIGBUS ) ;
#if defined (OSLinux)
  (void) sigdelset ( & mask, 32 ) ;
  (void) sigdelset ( & mask, 33 ) ;
#endif
  (void) sigprocmask ( SIG_SETMASK, & mask, NULL ) ;
  sfd = signalfd ( -1, & mask, SFD_NONBLOCK | SFD_CLOEXEC ) ;
  pfdv [ 1 ] . fd = sfd ;
  sock = sock_init () ;
  pfdv [ 2 ] . fd = sock ;

  while ( 1 ) {
    if ( 0 < poll ( pfdv, 3, -1 ) ) {
    }
  }

  return sys_halt ( L, 'h' ) ;
}

static int evLoopSigWait ( lua_State * L )
{
  int i ;
  pid_t p = 0 ;
  sigset_t mask ;

  /* initialize global variables */
  got_sig = 0 ;
#if defined (SIGRTMIN) && defined (SIGRTMAX)
  got_rt_sig = 0 ;
#endif
  /* set up signal handling */
  setup_sigs () ;
  /* block most signals */
  (void) sigfillset ( & mask ) ;
  (void) sigdelset ( & mask, SIGSEGV ) ;
  (void) sigdelset ( & mask, SIGILL ) ;
  (void) sigdelset ( & mask, SIGFPE ) ;
  (void) sigdelset ( & mask, SIGBUS ) ;
#if defined (OSLinux)
  (void) sigdelset ( & mask, 32 ) ;
  (void) sigdelset ( & mask, 33 ) ;
#endif
  (void) sigprocmask ( SIG_SETMASK, & mask, NULL ) ;

  while ( 1 ) {
    i = sig_wait ( & mask ) ;

    if ( 1 > i || NSIG <= i ) {
      continue ;
    } else if ( SIGCHLD == i ) {
      do {
        p = waitpid ( -1, & i, WNOHANG ) ;

        if ( 0 < p ) {
          /* pass pid and wait status to Lua */
        }
      } while ( 0 < p || ( 0 > p && EINTR == errno ) ) ;
    } else if ( SIGTERM == i ) {
      return sys_halt ( L, 'r' ) ;
      break ;
    } else if ( SIGUSR1 == i ) {
      return sys_halt ( L, 'h' ) ;
      break ;
    } else if ( SIGUSR2 == i ) {
      return sys_halt ( L, 'p' ) ;
      break ;
    } else {
      return sys_halt ( L, 'r' ) ;
      break ;
    }
  } /* end while */

  return sys_halt ( L, 'h' ) ;
}

static int supervise ( const int argc, char ** argv )
{
  pid_t p = 0 ;
  lua_State * L = NULL ;
  char * pname = ( 0 < argc && argv && * argv && ** argv ) ?
    * argv : "pesi" ;
  const uid_t myuid = getuid () ;
  const gid_t mygid = getgid () ;
  /*
  const pid_t mypid = getpid () ;
  */

  /* set up some variables first */
  /* initialize global variables */
  got_sig = 0 ;
#if defined (SIGRTMIN) && defined (SIGRTMAX)
  got_rt_sig = 0 ;
#endif

  /* drop possible orivileges we might have */
  (void) seteuid ( myuid ) ;
  (void) setegid ( mygid ) ;

  if ( 0 < myuid ) {
    (void) fprintf ( stderr, "%s:\tmust be super user\n", pname ) ;
    return 100 ;
  }

  /* secure file creation mask */
  (void) umask ( 00022 ) ;

  /* disable core dumps by setting the corresponding SOFT (!!)
   * limit to zero.
   * it can be raised again later (in child processes) as needed.
   */
  if ( 0 == myuid ) {
    struct rlimit rlim ;
    rlim . rlim_cur = 0 ;
    rlim . rlim_max = RLIM_INFINITY ;
    (void) setrlimit ( RLIMIT_CORE, & rlim ) ;
  } else if ( 0 < myuid ) {
    struct rlimit rlim ;

    if ( 0 == getrlimit ( RLIMIT_CORE, & rlim ) ) {
      rlim . rlim_cur = 0 ;
      (void) setrlimit ( RLIMIT_CORE, & rlim ) ;
    }
  }

#if defined (OSLinux)
  /* set the name of this (calling) thread */
  (void) prctl ( PR_SET_NAME, "pesi", 0, 0, 0 ) ;
#endif
  /* set a preliminary first host name that can be changed later */
  (void) sethostname ( HOSTNAME, strlen ( HOSTNAME ) ) ;
  /* chdir to root dir */
  (void) chdir ( "/" ) ;
  /* change active vt to tty1 */
  (void) chvt ( 1 ) ;
  /* create a new session if possible */
  p = setsid () ;
  (void) setpgid ( 0, 0 ) ;
#if defined (OSbsd)
  if ( 0 == myuid && 0 < p ) { (void) setlogin ( "root" ) ; }
#endif
  /* create new Lua state (interpreter) */
  errno = 0 ;
  L = newLuaState () ;

  if ( L ) {
    int i, j, r = 0 ;
    const char * script =
#ifdef PESI_SCRIPT
      PESI_SCRIPT
#else
      "/etc/rc.lua"
#endif
      ;

    (void) lua_pushstring ( L, pname ) ;
    lua_setglobal ( L, "progname" ) ;
    /* create a new Lua table holding the command line args */
    lua_newtable ( L ) ;
    (void) lua_pushstring ( L, pname ) ;
    lua_rawseti ( L, -2, -1 ) ;
    (void) lua_pushstring ( L, script ) ;
    lua_rawseti ( L, -2, 0 ) ;

    for ( i = j = 1 ; argc > i ; ++ i ) {
      if ( argv [ i ] && argv [ i ] [ 0 ] ) {
        (void) lua_pushstring ( L, argv [ i ] ) ;
        lua_rawseti ( L, -2, j ++ ) ;
      }
    }

    /* set the global name of this Lua table */
    lua_setglobal ( L, "arg" ) ;
    /* push our error message handler function onto the stack */
    lua_pushcfunction ( L, errmsgh ) ;
    /* read and parse the Lua code from the script or stdin.
     * the result is stored as a Lua function on top of the stack
     * when successful.
     */
    r = luaL_loadfile ( L, script ) ;

    if ( LUA_OK == r ) {
      /* the script was successfully read, parsed, and its code
       * has been stored as a function on top of the Lua stack.
       */
      r = lua_pcall ( L, 0, 1, -2 ) ;
    }

    if ( LUA_OK == r ) {
      /* success: parsing and executing the script succeeded.
       * check result.
       */
      if ( lua_isinteger ( L, -1 ) ) {
        r = lua_tointeger ( L, -1 ) ;
      } else if ( lua_isboolean ( L, -1 ) ) {
        r = ! lua_toboolean ( L, -1 ) ;
      }
    } else {
      /* an error ocurred */
      const char * msg = lua_tostring ( L, -1 ) ;

      if ( msg && * msg ) {
        (void) fprintf ( stderr, "%s:\t%s\n", pname, msg ) ;
      }

      (void) fprintf ( stderr, "%s:\tcannot run file \"%s\"\n",
        pname, script ) ;
    }

    (void) clear_stack ( L ) ;
    return evLoopSigWait ( L ) ;
  } else {
    /* deep shit, spawn a rescue shell */
  }

  return 0 ;
}

