/*
 * wrapper functions to process related syscalls
 */

/* constants */
#define NARG		50

enum {
  EXEC_ARGV0			= 0x0001,
  EXEC_PATH			= 0x0002,
  EXEC_DEFSIG			= 0x0004,
  EXEC_VFORK			= 0x0008,
  EXEC_WAITPID			= 0x0010,
} ;

/* helper function that checks a returned wait(pid)(2) status argument */
static int Lget_exit_status ( lua_State * const L,
  const pid_t pid, const int s )
{
  /* check the wait status now */
  if ( WIFEXITED( s ) ) {
    lua_pushinteger ( L, WEXITSTATUS( s ) ) ;
    lua_pushinteger ( L, 1 ) ;
    lua_pushinteger ( L, pid ) ;
    return 3 ;
  } else if ( WIFSIGNALED( s ) ) {
    lua_pushinteger ( L, WTERMSIG( s ) ) ;
    lua_pushinteger ( L, 2 ) ;
    lua_pushinteger ( L, pid ) ;
#ifdef WCOREDUMP
    if ( WCOREDUMP( s ) ) {
      /* lua_pushinteger ( L, 5 ) ; */
      lua_pushboolean ( L, 1 ) ;
      return 4 ;
    }
#endif
    return 3 ;
  } else if ( WIFSTOPPED( s ) ) {
    lua_pushinteger ( L, WSTOPSIG( s ) ) ;
    lua_pushinteger ( L, 3 ) ;
    lua_pushinteger ( L, pid ) ;
    return 3 ;
  } else if ( WIFCONTINUED( s ) ) {
    lua_pushinteger ( L, SIGCONT ) ;
    lua_pushinteger ( L, 4 ) ;
    lua_pushinteger ( L, pid ) ;
    return 3 ;
  }

  lua_pushinteger ( L, s ) ;
  lua_pushinteger ( L, 0 ) ;
  lua_pushinteger ( L, pid ) ;
  return 3 ;
}

static int Lwait4pid ( lua_State * const L,
  const pid_t pid, const char nohang )
{
  int i = 0 ;
  pid_t p = 0 ;

  if ( 0 > pid ) {
    return luaL_error ( L, "invalid PID %d", pid ) ;
  } else if ( kill ( pid, 0 ) ) {
    i = errno ;
    return luaL_error ( L, "PID %d: %s (errno %d)",
      pid, strerror ( i ), i ) ;
  }

  do {
    i = 0 ;
    p = waitpid ( pid, & i, nohang ? WNOHANG : 0 ) ;
  } while ( 0 > p && EINTR == errno ) ;

  if ( 0 > p ) {
    i = errno ;
    return luaL_error ( L, "waitpid( %d ) failed: %s (errno %d)",
      pid, strerror ( i ), i ) ;
  } else if ( 0 == p ) {
    lua_pushinteger ( L, 0 ) ;
    return 1 ;
  } else if ( 0 < p && pid != p ) {
    return luaL_error ( L, "waitpid( %d ) returned %d != %d",
      pid, p, pid ) ;
  }

  return Lget_exit_status ( L, pid, i ) ;
}

/* helper function for the execv* bindings */
static int Lvfork_exec ( lua_State * const L, const unsigned long int f )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;
    const char * str = NULL ;
    char ** av = NULL ;
    char * argv [ 1 + NARG ] = { (char *) NULL } ;

    /* check if all given args are strings before anything else */
    for ( i = 1 ; n >= i ; ++ i ) { luaL_checktype ( L, i, LUA_TSTRING ) ; }

    if ( 0 < n && NARG >= n ) {
      for ( i = 0 ; n > i ; ++ i ) {
        str = luaL_checkstring ( L, 1 + i ) ;

        if ( str && * str ) {
          argv [ i ] = (char *) str ;
        } else if ( str ) {
          argv [ i ] = "" ;
        } else {
          return luaL_argerror ( L, i, "invalid string" ) ;
        }
      }

      av = argv ;
    } else if ( NARG < n ) {
      av = (char **) lua_newuserdata ( L, (1 + n) * sizeof (char *) ) ;

      if ( NULL == av ) {
        i = errno ;
        luaL_error ( L, "out of memory: %s (errno %d)",
          strerror ( i ), i ) ;
      }

      for ( i = 0 ; n > i ; ++ i ) {
        str = luaL_checkstring ( L, 1 + i ) ;

        if ( str && * str ) {
          av [ i ] = (char *) str ;
        } else if ( str ) {
          av [ i ] = "" ;
        } else {
          return luaL_argerror ( L, i, "invalid string" ) ;
        }
      }

      av [ n ] = (char *) NULL ;
    }

    if ( EXEC_VFORK & f ) {
      pid_t p ;

      (void) fflush ( NULL ) ;
      p = vfork () ;

      if ( 0 > p ) {
        /* vfork(2) failed */
        i = errno ;
        return luaL_error ( L, "vfork() failed: %s (errno %d)",
          strerror ( i ), i ) ;
      } else if ( 0 < p ) {
        /* parent process */
        if ( EXEC_WAITPID & f ) {
          return Lwait4pid ( L, p, 0 ) ;
        }

        lua_pushinteger ( L, p ) ;
        return 1 ;
      }
    }

    /* restore default signal handling behaviour and unblock all signals ? */
    if ( EXEC_DEFSIG & f ) { reset_sigs () ; }

    /* run the requested execv*() syscall now */
    if ( ( EXEC_PATH & f ) && ( EXEC_ARGV0 & f ) ) {
      (void) execvp ( * av, 1 + av ) ;
    } else if ( EXEC_ARGV0 & f ) {
      (void) execv ( * av, 1 + av ) ;
    } else if ( EXEC_PATH & f ) {
      (void) execvp ( * av, av ) ;
    } else {
      (void) execv ( * av, av ) ;
    }

    if ( EXEC_VFORK & f ) {
      /* we are still in the subprocess created by vfork(2),
       * which must be terminated after execve(2) failed.
      perror ( "execve() failed" ) ;
       */
      _exit ( 127 ) ;
    } else {
      i = errno ;
      return luaL_error ( L, "execve() failed: %s (errno %d)",
        strerror ( i ), i ) ;
    }
  }

  return luaL_error ( L, "string args required" ) ;
}

/* wrapper function for getuid */
static int Sgetuid ( lua_State * const L )
{
  lua_pushinteger ( L, getuid () ) ;
  return 1 ;
}

/* wrapper function for geteuid */
static int Sgeteuid ( lua_State * const L )
{
  lua_pushinteger ( L, geteuid () ) ;
  return 1 ;
}

/* wrapper function for getgid */
static int Sgetgid ( lua_State * L )
{
  lua_pushinteger ( L, getgid () ) ;
  return 1 ;
}

/* wrapper function for getegid */
static int Sgetegid ( lua_State * const L )
{
  lua_pushinteger ( L, getegid () ) ;
  return 1 ;
}

/* wrapper function for getpid */
static int Sgetpid ( lua_State * const L )
{
  lua_pushinteger ( L, getpid () ) ;
  return 1 ;
}

/* wrapper function for getppid */
static int Sgetppid ( lua_State * const L )
{
  lua_pushinteger ( L, getppid () ) ;
  return 1 ;
}

/* wrapper function for setuid */
static int Ssetuid ( lua_State * const L )
{
  const uid_t u = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;

  if ( setuid ( u ) ) {
    const int e = errno ;
    return luaL_error ( L, "setuid( %d ) failed: %s (errno %d)",
      u, strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper function for seteuid(2) */
static int Sseteuid ( lua_State * const L )
{
  const uid_t u = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;

  if ( seteuid ( u ) ) {
    const int e = errno ;
    return luaL_error ( L, "seteuid( %d ) failed: %s (errno %d)",
      u, strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper function for setgid(2) */
static int Ssetgid ( lua_State * const L )
{
  const gid_t g = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;

  if ( setgid ( g ) ) {
    const int e = errno ;
    return luaL_error ( L, "setgid( %d ) failed: %s (errno %d)",
      g, strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper function for setegid(2) */
static int Ssetegid ( lua_State * const L )
{
  const gid_t g = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;

  if ( setegid ( g ) ) {
    const int e = errno ;
    return luaL_error ( L, "setegid( %d ) failed: %s (errno %d)",
      g, strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper function for setreuid(2) */
static int Ssetreuid ( lua_State * const L )
{
  const uid_t r = luaL_checkinteger ( L, 1 ) ;
  const uid_t e = luaL_checkinteger ( L, 2 ) ;

  return res_zero ( L, setreuid ( r, e ) ) ;
}

/* wrapper function for setregid(2) */
static int Ssetregid ( lua_State * const L )
{
  const gid_t r = luaL_checkinteger ( L, 1 ) ;
  const gid_t e = luaL_checkinteger ( L, 2 ) ;

  return res_zero ( L, setregid ( r, e ) ) ;
}

/* wrapper function for getresuid(2) */
static int Sgetresuid ( lua_State * const L )
{
#if (defined (OSLinux) && defined (_GNU_SOURCE)) || defined (OSfreebsd) || defined (OShpux)
  uid_t r, e, s ;

  if ( getresuid ( & r, & e, & s ) ) {
    return res_zero ( L, -1 ) ;
  }

  lua_pushinteger ( L, r ) ;
  lua_pushinteger ( L, e ) ;
  lua_pushinteger ( L, s ) ;
  return 3 ;
#else
  lua_pushinteger ( L, -1 ) ;
  lua_pushinteger ( L, ENOSYS ) ;
  return 2 ;
#endif
}

/* wrapper function for getresgid(2) */
static int Sgetresgid ( lua_State * const L )
{
#if (defined (OSLinux) && defined (_GNU_SOURCE)) || defined (OSfreebsd) || defined (OShpux)
  gid_t r, e, s ;

  if ( getresgid ( & r, & e, & s ) ) {
    return res_zero ( L, -1 ) ;
  }

  lua_pushinteger ( L, r ) ;
  lua_pushinteger ( L, e ) ;
  lua_pushinteger ( L, s ) ;
  return 3 ;
#else
  lua_pushinteger ( L, -1 ) ;
  lua_pushinteger ( L, ENOSYS ) ;
  return 2 ;
#endif
}

/* wrapper function for setresuid(2) */
static int Ssetresuid ( lua_State * const L )
{
#if (defined (OSLinux) && defined (_GNU_SOURCE)) || defined (OSfreebsd) || defined (OShpux)
  uid_t r = luaL_checkinteger ( L, 1 ) ;
  uid_t e = luaL_checkinteger ( L, 2 ) ;
  uid_t s = luaL_checkinteger ( L, 3 ) ;

  return res_zero ( L, setresuid ( r, e, s ) ) ;
#else
  lua_pushinteger ( L, -1 ) ;
  lua_pushinteger ( L, ENOSYS ) ;
  return 2 ;
#endif
}

/* wrapper function for setresgid(2) */
static int Ssetresgid ( lua_State * const L )
{
#if (defined (OSLinux) && defined (_GNU_SOURCE)) || defined (OSfreebsd) || defined (OShpux)
  gid_t r = luaL_checkinteger ( L, 1 ) ;
  gid_t e = luaL_checkinteger ( L, 2 ) ;
  gid_t s = luaL_checkinteger ( L, 3 ) ;

  return res_zero ( L, setresgid ( r, e, s ) ) ;
#else
  lua_pushinteger ( L, -1 ) ;
  lua_pushinteger ( L, ENOSYS ) ;
  return 2 ;
#endif
}

#if defined (OSLinux)
/* wrapper function for setfsuid(2) */
static int Ssetfsuid ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  i = setfsuid ( i ) ;
  lua_pushinteger ( L, i ) ;
  return 1 ;
}

/* wrapper function for setfsgid(2) */
static int Ssetfsgid ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  i = setfsgid ( i ) ;
  lua_pushinteger ( L, i ) ;
  return 1 ;
}
#endif

/* wrapper for group_member(3) */
static int Sgroup_member ( lua_State * const L )
{
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
  lua_pushboolean ( L,
    group_member ( (lua_Unsigned) luaL_checkinteger ( L, 1 ) ) ) ;
  return 1 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper for the initgroups syscall */
static int Sinitgroups ( lua_State * const L )
{
  const char * user = luaL_checkstring ( L, 1 ) ;

  if ( user && * user ) {
    const gid_t g = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

    if ( initgroups ( user, g ) ) {
      const int e = errno ;
      return luaL_error ( L, "initgroups( %s, %d ) failed: %s (errno %d)",
        user, g, strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_argerror ( L, 1, "empty user name" ) ;
}

#define NEL	32

/* wrapper for the getgroups syscall */
static int Sgetgroups ( lua_State * const L )
{
  int i ;
  const int n = getgroups ( 0, NULL ) ;

  if ( NEL < n ) {
    gid_t * ap = malloc ( n * sizeof ( gid_t ) ) ;

    if ( NULL == ap ) { return luaL_error ( L, "out of memory" ) ; }

    if ( n == getgroups ( n, ap ) ) {
      lua_newtable ( L ) ;

      for ( i = 0 ; n > i ; ++ i ) {
        lua_pushinteger ( L, ap [ i ] ) ;
        lua_rawseti ( L, -2, 1 + i ) ;
      }

      free ( ap ) ;
      return 1 ;
    } else {
      i = errno ;
      free ( ap ) ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }
  } else if ( 0 < n ) {
    gid_t ga [ NEL ] = { 0 } ;

    if ( n == getgroups ( NEL, ga ) ) {
      lua_newtable ( L ) ;

      for ( i = 0 ; NEL > i && n > i ; ++ i ) {
        lua_pushinteger ( L, ga [ i ] ) ;
        lua_rawseti ( L, -2, 1 + i ) ;
      }

      return 1 ;
    } else {
      i = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }
  }

  return 0 ;
}

/* wrapper for the setgroups syscall */
static int Ssetgroups ( lua_State * const L )
{
  int i, j, k ;
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    /* see if args are valid GIDs */
    for ( i = 1 ; n >= i ; ++ i ) {
      if ( lua_isinteger ( L, i ) ) {
        j = lua_tointegerx ( L, i, & k ) ;
        if ( 0 == k || 0 > j ) {
          return luaL_argerror ( L, i, "invalid GID" ) ;
          break ;
        }
      } else {
        return luaL_argerror ( L, i, "invalid GID" ) ;
        break ;
      }
    }
  } else {
    return luaL_error ( L, "no args" ) ;
  }

  if ( NEL < n ) {
    gid_t * ap = malloc ( n * sizeof ( gid_t ) ) ;

    if ( NULL == ap ) { return luaL_error ( L, "out of memory" ) ; }

    for ( i = 0 ; n > i ; ++ i ) {
      ap [ i ] = lua_tointeger ( L, 1 + i ) ;
    }

    i = getgroups ( n, ap ) ;
    j = errno ;
    free ( ap ) ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, j ) ;
      return 2 ;
    }

    return 1 ;
  } else if ( 0 < n ) {
    gid_t ga [ NEL ] = { 0 } ;

    for ( i = 0 ; NEL > i && n > i ; ++ i ) {
      ga [ i ] = lua_tointeger ( L, 1 + i ) ;
    }

    return res_zero ( L, setgroups ( n, ga ) ) ;
  }

  return 0 ;
}

/* wrapper for the getgrouplist(3) syscall.
 * should not be here but in os_pw.c instead.
 * but we implement it here instead since NEL
 * is defined here and now and are aware that
 * thie is the wrong place for it.
 */
static int Sgetgrouplist ( lua_State * const L )
{
  const char * user = luaL_checkstring ( L, 1 ) ;

  if ( user && * user ) {
    int i, n = NEL ;
    const gid_t g = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

    {
      gid_t ga [ NEL ] = { -1 } ;
      i = getgrouplist ( user, g, ga, & n ) ;

      if ( 0 < i ) {
        lua_newtable ( L ) ;

        for ( i = 0 ; NEL > i ; ++ i ) {
          lua_pushinteger ( L, ga [ i ] ) ;
          lua_rawseti ( L, -2, 1 + i ) ;
        }

        return 1 ;
      }
    }

    if ( 0 > i && 0 < n && NEL < n ) {
      gid_t * ap = malloc ( n * sizeof ( gid_t ) ) ;

      if ( NULL == ap ) { return luaL_error ( L, "out of memory" ) ; }
      i = getgrouplist ( user, g, ap, & n ) ;

      if ( 0 < i ) {
        lua_newtable ( L ) ;

        for ( i = 0 ; n > i ; ++ i ) {
          lua_pushinteger ( L, ap [ i ] ) ;
          lua_rawseti ( L, -2, 1 + i ) ;
        }

        free ( ap ) ;
        return 1 ;
      }

      free ( ap ) ;
    }
  }

  return luaL_argerror ( L, 1, "invalid user name" ) ;
}

#undef NEL

/* wrapper for the kill(2) syscall */
static int Skill ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    const int s = luaL_checkinteger ( L, 1 ) ;

    if ( 0 <= s && NSIG > s ) {
      int i ;

      for ( i = 2 ; n >= i ; ++ i ) {
        pid_t p = luaL_checkinteger ( L, i ) ;

        if ( kill ( p, s ) ) {
          i = errno ;
          return luaL_error ( L, "kill( %d, %d ) failed: %s (errno %d)",
            p, s, strerror ( i ), i ) ;
        }
      }

      return 0 ;
    }

    return luaL_argerror ( L, 1, "invalid signal number" ) ;
  }

  return luaL_error ( L, "valid signal number and PID required" ) ;
}

/* wrapper for the pgkill(2) syscall */
static int Skillpg ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    const int s = luaL_checkinteger ( L, 1 ) ;

    if ( 0 <= s && NSIG > s ) {
      int i ;

      for ( i = 2 ; n >= i ; ++ i ) {
        int p = luaL_checkinteger ( L, i ) ;

        if ( 0 > p ) {
          return luaL_argerror ( L, i, "invalid PGID" ) ;
        } else if ( killpg ( p, s ) ) {
          i = errno ;
          return luaL_error ( L, "killpg( %d, %d ) failed: %s (errno %d)",
            p, s, strerror ( i ), i ) ;
        }
      }

      return 0 ;
    }

    return luaL_argerror ( L, 1, "invalid signal number" ) ;
  }

  return luaL_error ( L, "valid signal number and PID required" ) ;
}

/* wrapper for the sigqueue(2) syscall */
#ifndef OSopenbsd
static int Ssigqueue ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    union sigval v ;
    int e = 0, i = luaL_checkinteger ( L, 1 ) ;

    if ( 1 < n ) {
      e = luaL_checkinteger ( L, 2 ) ;
      e = ( 0 < e && NSIG > e ) ? e : 0 ;
    }

    if ( 2 < n ) {
      v . sival_int = luaL_checkinteger ( L, 3 ) ;
    } else {
      v . sival_int = 0 ;
    }

    i = sigqueue ( i, e, v ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}
#endif

#if defined (OSsolaris) || defined (OSsunos5)
static int Ssigsendset ( lua_State * const L )
{
  return 0 ;
}

static int Lkill_all_sol ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < i && NSIG > i ) {
    i = kill_all_sol ( i ) ;
    lua_pushinteger ( L, i ) ;
    return 1 ;
  }

  return 0 ;
}
#endif

static int Lgen_kill_all ( lua_State * const L )
{
  const int s = luaL_optinteger ( L, 1, SIGTERM ) ;

  if ( 0 < s && NSIG > s ) {
    gen_kill_all ( s ) ;
    return 0 ;
  }

  return luaL_argerror ( L, 1, "invalid signal number" ) ;
}

static int Lkill_all_procs ( lua_State * const L )
{
  int i = lua_gettop ( L ) ;

  if ( ( 0 < i ) && lua_isinteger ( L, 1 ) ) {
    const int sig = luaL_checkinteger ( L, 1 ) ;

    if ( 0 <= sig && NSIG > sig ) {
      char pa, pg, ses ;

      pa = 3 ;
      pg = ses = 0 ;

      if ( ( 1 < i ) && lua_isboolean ( L, 2 ) ) {
        pg = lua_toboolean ( L, 2 ) ? 3 : 0 ;
      }

      if ( ( 2 < i ) && lua_isboolean ( L, 3 ) ) {
        pa = lua_toboolean ( L, 3 ) ? 3 : 0 ;
      }

      if ( ( 3 < i ) && lua_isboolean ( L, 4 ) ) {
        ses = lua_toboolean ( L, 4 ) ? 3 : 0 ;
      }

      i = kill_all_procs ( sig, pg, pa, ses ) ;
      lua_pushinteger ( L, i ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* kills all (remaining) processes (when the system is shut down) */
static int Lkill_procs ( lua_State * const L )
{
  int i = geteuid () ;

  /* this makes only sense for the super user */
  if ( i ) { return 0 ; }
  i = luaL_optinteger ( L, 1, 0 ) ;
  /* time in seconds to wait before sending SIGKILL */
  i = ( 0 < i && 10 > i ) ? i : 0 ;
  /* protect against SIGTERM and SIGHUP
   * (not really necessary on Linux and BSD)
   */
#if defined (OSLinux) || defined (OSbsd)
#else
  (void) signal ( SIGTERM, SIG_IGN ) ;
  (void) signal ( SIGHUP, SIG_IGN ) ;
  (void) signal ( SIGINT, SIG_IGN ) ;
#endif
  sync () ;
#if defined (OSsolaris) || defined (OSsunos5)
  (void) kill_all_sol ( SIGTERM ) ;
  (void) kill_all_sol ( SIGHUP ) ;
  (void) kill_all_sol ( SIGINT ) ;
  (void) kill_all_sol ( SIGCONT ) ;
#else
  (void) kill ( -1, SIGTERM ) ;
  (void) kill ( -1, SIGHUP ) ;
  (void) kill ( -1, SIGINT ) ;
  (void) kill ( -1, SIGCONT ) ;
#endif
  if ( 0 < i && 10 > i ) { do_sleep ( i, 0 ) ; }
  else { do_sleep ( 0, 300 * 1000 ) ; }
#if defined (OSsolaris) || defined (OSsunos5)
  (void) kill_all_sol ( SIGKILL ) ;
#elif defined (OSLinux) || defined (OSbsd)
  (void) kill ( -1, SIGKILL ) ;
#else
  /* TODO: find all remaining processes and kill them one by one */
#endif
  /* reap possible zombie/dead child processes */
  do { i = waitpid ( -1, NULL, WNOHANG ) ; }
  while ( ( 0 < i ) || ( 0 > i && EINTR == errno ) ) ;
  sync () ;
  return 0 ;
}

/* wrapper for the raise syscall */
static int Sraise ( lua_State * const L )
{
  int s = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= s && NSIG > s ) {
    int e = 0 ;
    s = raise ( s ) ;
    e = errno ;
    lua_pushinteger ( L, s ) ;

    if ( s ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper for the pause syscall */
static int Spause ( lua_State * const L )
{
  /* the return value of pause() is always the same and not relevant
  int e = 0, i = -3 ;

  i = pause () ;
  e = errno ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;

  return 2 ;
  */
  (void) pause () ;
  return 0 ;
}

/* wrapper function for the getpgrp syscall
 * (POSIX.1 version without args that always succeeds
 * and returns the PGID of the calling process)
 */
static int Sgetpgrp ( lua_State * const L )
{
  lua_pushinteger ( L, getpgrp () ) ;
  return 1 ;
}

/* wrapper function for the getpgid syscall */
static int Sgetpgid ( lua_State * const L )
{
  int e = 0 ;
  int i = luaL_optinteger ( L, 1, 0 ) ;

  i = 0 > i ? 0 : i ;
  i = getpgid ( i ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( 0 > i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/* wrapper function for the setpgid syscall */
static int Ssetpgid ( lua_State * const L )
{
  pid_t pid = 0, pgid = 0 ;

  pid = luaL_optinteger ( L, 1, 0 ) ;
  pgid = luaL_optinteger ( L, 2, 0 ) ;
  pid = 0 > pid ? 0 : pid ;
  pgid = 0 > pgid ? 0 : pgid ;
  pid = setpgid ( pid, pgid ) ;
  pgid = errno ;
  lua_pushinteger ( L, pid ) ;

  if ( pid ) {
    lua_pushinteger ( L, pgid ) ;
    return 2 ;
  }

  return 1 ;
}

/* wrapper function for the setpgrp syscall
 * (System V version without args)
 */
static int Ssetpgrp ( lua_State * const L )
{
  if ( setpgrp () ) {
    const int e = errno ;
    return luaL_error ( L, "setpgrp() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper function for the getsid syscall */
static int Sgetsid ( lua_State * const L )
{
  const pid_t p = luaL_optinteger ( L, 1, 0 ) ;
  const pid_t s = getsid ( p ) ;

  if ( 0 > s ) {
    const int e = errno ;
    return luaL_error ( L, "getsid( %d ) failed: %s (errno %d)",
      p, strerror ( e ), e ) ;
  }

  lua_pushinteger ( L, s ) ;
  return 1 ;
}

/* wrapper function for the setsid(2) syscall */
static int Ssetsid ( lua_State * const L )
{
  const pid_t p = setsid () ;

  if ( 0 > p ) {
    const int e = errno ;
    return luaL_error ( L, "setsid() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  }

  lua_pushinteger ( L, p ) ;
  return 1 ;
}

/* call setsid() before setpgid() */
static int Ldo_setsid ( lua_State * const L )
{
  int i = setsid () ;

  (void) setpgid ( 0, 0 ) ;
  lua_pushinteger ( L, i ) ;
  return 1 ;
}

/* wrapper function for tcgetpgrp */
static int Stcgetpgrp ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;
    i = tcgetpgrp ( i ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( 0 > i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper function for tcsetpgrp */
static int Stcsetpgrp ( lua_State * const L )
{
  int f = luaL_checkinteger ( L, 1 ) ;
  int i = luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= f && 0 <= i ) {
    i = tcsetpgrp ( f, i ) ;
    f = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, f ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper function for exit
 * (not really necessary since os.exit does the same)
 */
static int Sexit ( lua_State * const L )
{
  exit ( luaL_optinteger ( L, 1, 0 ) ) ;
  /* not reached */
  return 0 ;
}

/* binding for the abort(3) function */
static int Sabort ( lua_State * const L )
{
  abort () ;
  /* not reached */
  return 0 ;
}

/* wrapper function for exit
 * (not really necessary since os.exit does the same)
 */
static int S_exit ( lua_State * const L )
{
  _exit ( luaL_optinteger ( L, 1, 0 ) ) ;
  /* not reached */
  return 0 ;
}

/* wrapper function for the fork syscall */
static int Sfork ( lua_State * const L )
{
  pid_t p = 0 ;

  (void) fflush ( NULL ) ;
  p = fork () ;

  if ( 0 > p ) {
    const int e = errno ;
    return luaL_error ( L, "fork() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  }

  lua_pushinteger ( L, p ) ;
  return 1 ;
}

/* "non failing" version of fork(2), be careful ! */
static int Ldo_fork ( lua_State * const L )
{
  pid_t p = 0 ;

  (void) fflush ( NULL ) ;

  while ( 0 > ( p = fork () ) && ENOSYS != errno ) {
    /* sleep 5 seconds before fork()ing again */
    do_sleep ( 5, 0 ) ;
  }

  lua_pushinteger ( L, p ) ;
  return 1 ;
}

static int Lxfork ( lua_State * const L )
{
  pid_t p = 0 ;

  (void) fflush ( NULL ) ;
  p = xfork () ;

  if ( 0 > p ) {
    const int i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  lua_pushinteger ( L, p ) ;
  return 1 ;
}

/* wrapper function for the umask(2) syscall */
static int Sumask ( lua_State * const L )
{
  lua_pushinteger ( L, umask ( 00077 & (mode_t) luaL_checkinteger ( L, 1 ) ) ) ;
  return 1 ;
}

static int Lsec_umask ( lua_State * L )
{
  lua_pushinteger ( L, umask ( 00022 | ( 00077 & umask ( 00077 ) ) ) ) ;
  return 1 ;
}

/* wrapper function for nice(2) */
static int Snice ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  errno = 0 ;
  i = nice ( i ) ;

  if ( 0 != errno && -1 == i ) {
    i = errno ;
    return luaL_error ( L, "nice() failed: %s (errno %d)",
      strerror ( i ), i ) ;
  }

  lua_pushinteger ( L, i ) ;
  return 1 ;
}

static int Ldelay ( lua_State * const L )
{
  long int s = luaL_checkinteger ( L, 1 ) ;
  long int u = luaL_optinteger ( L, 2, 0 ) ;

  if ( 0 < s || ( 0 < u && ( 1000 * 1000 ) > u ) ) {
    lua_pushinteger ( L, delay ( s, u ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 non negative integer args required" ) ;
}

static int Ldo_sleep ( lua_State * const L )
{
  long int s = luaL_checkinteger ( L, 1 ) ;
  long int m = luaL_optinteger ( L, 2, 0 ) ;

  s = ( 0 > s ) ? 1 : s ;
  m = ( 0 > m ) ? 0 : m ;
  m %= 1000 * 1000 ;
  if ( 0 < s || 0 < m ) { do_sleep ( s, m ) ; }

  return 0 ;
}

static int Lhard_sleep ( lua_State * const L )
{
  long int s = luaL_checkinteger ( L, 1 ) ;
  long int m = luaL_optinteger ( L, 2, 0 ) ;

  s = ( 0 > s ) ? 1 : s ;
  m = ( 0 > m ) ? 0 : m ;
  m %= 1000 * 1000 ;
  if ( 0 < s || 0 < m ) { hard_sleep ( s, m ) ; }

  return 0 ;
}

/* wrapper function for the sleep syscall */
static int Ssleep ( lua_State * const L )
{
  unsigned int s = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;

  if ( 0 < s ) {
    do { s = sleep ( s ) ; }
    while ( 0 < s ) ;
  }

  return 0 ;
}

/* wrapper function for the usleep syscall */
static int Susleep ( lua_State * const L )
{
  unsigned int u = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;
  u %= 1000 * 1000 ;

  if ( 0 < u && 1000 * 1000 > u ) {
    while ( usleep ( u ) && EINTR == errno ) { ; }
  }

  return 0 ;
}

/* wrapper function for the nanosleep syscall */
static int Snanosleep ( lua_State * const L )
{
  int i = -3 ;
  struct timespec ts, rem ;

  ts . tv_sec = luaL_checkinteger ( L , 1 ) ;
  ts . tv_nsec = luaL_checkinteger ( L , 2 ) ;

  while ( 0 > ( i = nanosleep ( & ts, & rem ) ) && EINTR == errno )
  { ts = rem ; }

  return res_zero ( L, i ) ;
}

/* wrapper function for the alarm(2) syscall */
static int Salarm ( lua_State * const L )
{
  lua_pushinteger ( L, (lua_Unsigned) alarm (
    (lua_Unsigned) luaL_checkinteger ( L, 1 )
  ) ) ;

  return 1 ;
}

/* wrapper function for the ualarm(3) syscall */
static int Sualarm ( lua_State * const L )
{
  lua_pushinteger ( L, (lua_Unsigned) ualarm (
    (lua_Unsigned) luaL_checkinteger ( L, 1 ),
    (lua_Unsigned) luaL_optinteger ( L, 2, 0 )
  ) ) ;

  return 1 ;
}

/* wrapper function for the wait(2) syscall */
static int Swait ( lua_State * const L )
{
  int i ;
  pid_t p ;

  do {
    i = 0 ;
    p = wait ( & i ) ;
  } while ( 0 > p && EINTR == errno ) ;

  if ( 0 > p ) {
    i = errno ;
    return luaL_error ( L, "wait() failed: %s (errno %d)",
      strerror ( i ), i ) ;
  }

  return Lget_exit_status ( L, p, i ) ;
}

/* wrapper function for the waitpid(2) syscall */
static int Swaitpid ( lua_State * const L )
{
  return Lwait4pid ( L, luaL_checkinteger ( L, 1 ), 0 ) ;
}

/* wait(pid)s for a child processes without blocking */
static int Lwaitpid_nohang ( lua_State * const L )
{
  return Lwait4pid ( L, luaL_checkinteger ( L, 1 ), 1 ) ;
}

/* wrapper for the waitid(2) syscall */
static int Swaitid ( lua_State * const L )
{
  int e = 0, i = 0, p = 0 ;
  siginfo_t si ;
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) { i = luaL_checkinteger ( L, 1 ) ; }
  if ( 1 < n ) { p = luaL_checkinteger ( L, 2 ) ; }
  if ( 2 < n ) { e = luaL_checkinteger ( L, 3 ) ; }

  i = ( P_PID == i || P_PGID == i ) ? i : P_ALL ;
  p = ( 0 < p ) ? p : 1 ;
  e = e ? e : ( WEXITED | WNOHANG ) ;
  si . si_pid = 0 ;
  i = waitid ( i, p, & si, e ) ;
  e = errno ;

  if ( i ) {
    lua_pushinteger ( L, i ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  } else if ( 0 < si . si_pid ) {
    lua_pushinteger ( L, si . si_pid ) ;
    lua_pushinteger ( L, si . si_status ) ;

    if ( CLD_EXITED == si . si_code ) {
      (void) lua_pushliteral ( L, "exited" ) ;
      return 3 ;
    } else if ( CLD_KILLED == si . si_code ) {
      (void) lua_pushliteral ( L, "signaled" ) ;
      return 3 ;
    } else if ( CLD_DUMPED == si . si_code ) {
      (void) lua_pushliteral ( L, "coredump" ) ;
      return 3 ;
    } else if ( CLD_STOPPED == si . si_code ) {
      (void) lua_pushliteral ( L, "stopped" ) ;
      return 3 ;
    } else if ( CLD_CONTINUED == si . si_code ) {
      (void) lua_pushliteral ( L, "continued" ) ;
      return 3 ;
    } else if ( CLD_TRAPPED == si . si_code ) {
      (void) lua_pushliteral ( L, "trapped" ) ;
      return 3 ;
    }

    return 2 ;
  }

  return 0 ;
}

/* waitid() for all our terminated child processes */
static int Lwaitid_exited_nohang ( lua_State * const L )
{
  int i = -3, e = 0 ;
  siginfo_t si ;

  do {
    si . si_pid = 0 ;
    i = waitid ( P_ALL, 0, & si, WEXITED | WNOHANG ) ;
    e = errno ;
  } while ( i && ( EINTR == e ) ) ;

  if ( i ) {
    lua_pushinteger ( L, i ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  } else if ( 0 < si . si_pid ) {
    lua_pushinteger ( L, si . si_pid ) ;
    lua_pushinteger ( L, si . si_status ) ;

    if ( CLD_EXITED == si . si_code ) {
      (void) lua_pushliteral ( L, "exited" ) ;
      return 3 ;
    } else if ( CLD_KILLED == si . si_code ) {
      (void) lua_pushliteral ( L, "signaled" ) ;
      return 3 ;
    } else if ( CLD_DUMPED == si . si_code ) {
      (void) lua_pushliteral ( L, "coredump" ) ;
      return 3 ;
    } else if ( CLD_STOPPED == si . si_code ) {
      (void) lua_pushliteral ( L, "stopped" ) ;
      return 3 ;
    } else if ( CLD_CONTINUED == si . si_code ) {
      (void) lua_pushliteral ( L, "continued" ) ;
      return 3 ;
    } else if ( CLD_TRAPPED == si . si_code ) {
      (void) lua_pushliteral ( L, "trapped" ) ;
      return 3 ;
    }

    return 2 ;
  }

  return 0 ;
}

/* redirect the responsibility for reaping orphanded sub processes
 * from process #1 to our own.
 */
static int Lset_subreaper ( lua_State * const L )
{
  int e = 0, i = 3 ;
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) { i = lua_toboolean ( L, 1 ) ; }
#if defined (OSLinux) && defined (PR_SET_CHILD_SUBREAPER)
  i = prctl ( PR_SET_CHILD_SUBREAPER, i ? 1 : 0 ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
#elif defined (OSfreebsd) || defined (OSdragonfly)
  if ( i ) {
#  if defined (PROC_REAP_ACQUIRE)
    i = procctl ( P_PID, getpid (), PROC_REAP_ACQUIRE, 0 ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
#  endif
  } else {
#  if defined (PROC_REAP_RELEASE)
    i = procctl ( P_PID, getpid (), PROC_REAP_RELEASE, 0 ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
#  endif
  }

  return 0 ;
#else
  lua_pushinteger ( L, -3 ) ;
  lua_pushinteger ( L, ENOSYS ) ;
  return 2 ;
#endif
}

/* get the current subreaper of this process */
static int Lis_subreaper ( lua_State * const L )
{
#if defined (OSLinux) && defined (PR_GET_CHILD_SUBREAPER)
  int i = 0 ;
  if ( prctl ( PR_GET_CHILD_SUBREAPER, & i, 0, 0, 0 ) )
  { lua_pushboolean ( L, ( 1 == getpid () ) ? 1 : 0 ) ; }
  else { lua_pushboolean ( L, i ) ; }
#else
  lua_pushboolean ( L, ( 1 == getpid () ) ? 1 : 0 ) ;
#endif

  return 1 ;
}

/* wrapper function for getitimer */
static int Sgetitimer ( lua_State * const L )
{
  int i = 0, e = 0 ;
  struct itimerval it ;

  i = luaL_checkinteger ( L, 1 ) ;
  i = getitimer ( i, & it ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;
  lua_pushinteger ( L, it . it_interval . tv_sec ) ;
  lua_pushinteger ( L, it . it_interval . tv_usec ) ;
  lua_pushinteger ( L, it . it_value . tv_sec ) ;
  lua_pushinteger ( L, it . it_value . tv_usec ) ;

  return 6 ;
}

/* wrapper function for setitimer */
static int Ssetitimer ( lua_State * const L )
{
  int i = 0, e = 0, r = 0 ;
  struct itimerval it ;
  const int n = lua_gettop ( L ) ;

  if ( 2 > n ) { return 0 ; }

  i = luaL_checkinteger ( L, 1 ) ;
  r = getitimer ( i, & it ) ;
  e = errno ;
  r = luaL_checkinteger ( L, 2 ) ;
  it . it_value . tv_sec = r ;
  r = setitimer ( i, & it, & it ) ;
  e = errno ;
  lua_pushinteger ( L, r ) ;
  lua_pushinteger ( L, e ) ;
  lua_pushinteger ( L, it . it_interval . tv_sec ) ;
  lua_pushinteger ( L, it . it_interval . tv_usec ) ;
  lua_pushinteger ( L, it . it_value . tv_sec ) ;
  lua_pushinteger ( L, it . it_value . tv_usec ) ;

  return 6 ;
}

/* wrapper function for getcwd */
static int Sgetcwd ( lua_State * const L )
{
  int e = 0 ;
  char * cwd = NULL ;

  {
    char buf [ 512 ] = { 0 } ;
    cwd = getcwd ( buf, sizeof ( buf ) - 1 ) ;
    e = errno ;

    if ( cwd ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }
  }

  if ( NULL == cwd && ERANGE == e ) {
    char buf [ 1024 ] = { 0 } ;
    cwd = getcwd ( buf, sizeof ( buf ) - 1 ) ;
    e = errno ;

    if ( cwd ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }
  }

  if ( NULL == cwd && ERANGE == e ) {
    char buf [ 2048 ] = { 0 } ;
    cwd = getcwd ( buf, sizeof ( buf ) - 1 ) ;
    e = errno ;

    if ( cwd ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }
  }

  if ( NULL == cwd && ERANGE == e ) {
    char buf [ 1 + PATH_MAX ] = { 0 } ;
    cwd = getcwd ( buf, sizeof ( buf ) - 1 ) ;
    e = errno ;

    if ( cwd ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }
  }

  lua_pushnil ( L ) ;
  lua_pushinteger ( L, e ) ;
  return 2 ;
}

/* wrapper function for the chdir(2) syscall */
static int Schdir ( lua_State * const L )
{
  const char * dir = luaL_checkstring ( L, 1 ) ;

  if ( dir && * dir ) {
    if ( chdir ( dir ) ) {
      const int e = errno ;
      return luaL_error ( L, "chdir( %s ) failed: %s (errno %d)",
        dir, strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_argerror ( L, 1, "invalid dir path" ) ;
}

/* wrapper function for fchdir */
static int Sfchdir ( lua_State * const L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;

    i = fchdir ( i ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper function for the chroot syscall */
static int Schroot ( lua_State * const L )
{
  const char * dir = luaL_checkstring ( L , 1 ) ;

  if ( dir && * dir && is_d ( dir ) && ( 0 == chdir ( dir ) ) ) {
    if ( chroot ( dir ) ) {
      const int e = errno ;
      return luaL_error ( L, "chroot( %s ) failed: %s (errno %d)",
        dir, strerror ( e ), e ) ;
    }

    if ( chdir ( "/" ) ) {
      const int e = errno ;
      return luaL_error ( L, "chdir( / ) failed: %s (errno %d)",
        strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_error ( L, "accessible dir path required" ) ;
}

/* wrapper function for getpriority */
static int Sgetpriority ( lua_State * const L )
{
  int e = 0, m = 0, i = 0 ;
  id_t who = 0 ;
  const char * oarr [] = { "proc", "pgrp", "user", NULL } ;
  m = luaL_checkoption ( L, 1, "proc", oarr ) ;
  who = (id_t) luaL_optinteger ( L, 2, 0 ) ;
  errno = 0 ;

  switch ( m ) {
    case 0 :
      i = getpriority ( PRIO_PROCESS, who ) ;
      break ;
    case 1 :
      i = getpriority ( PRIO_PGRP, who ) ;
      break ;
    case 2 :
      i = getpriority ( PRIO_USER, who ) ;
      break ;
    default :
      i = getpriority ( PRIO_PROCESS, who ) ;
      break ;
  }

  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( ( -1 == i ) && e ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/* wrapper function for setpriority */
static int Ssetpriority ( lua_State * const L )
{
  int e = 0, m = 0, i = 0, prio = 0 ;
  id_t who = 0 ;
  const char * oarr [] = { "proc", "pgrp", "user", NULL } ;
  m = luaL_checkoption ( L, 1, "proc", oarr ) ;
  who = (id_t) luaL_optinteger ( L, 2, 0 ) ;
  prio = luaL_optinteger ( L, 3, 0 ) ;

  switch ( m ) {
    case 0 :
      i = setpriority ( PRIO_PROCESS, who, prio ) ;
      break ;
    case 1 :
      i = setpriority ( PRIO_PGRP, who, prio ) ;
      break ;
    case 2 :
      i = setpriority ( PRIO_USER, who, prio ) ;
      break ;
    default :
      i = setpriority ( PRIO_PROCESS, who, prio ) ;
      break ;
  }

  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/*
 * get and set process resource limits
 */

/* helper functions that get/set limits for a given resource */
static int get_reslimit ( lua_State * const L, const int reso )
{
  struct rlimit rlim ;

  if ( getrlimit ( reso, & rlim ) ) {
    /* error */
    const int i = errno ;
    lua_pushboolean ( L, 0 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  /* success */
  lua_pushboolean ( L, 1 ) ;
  lua_pushinteger ( L, rlim . rlim_cur ) ;
  lua_pushinteger ( L, rlim . rlim_max ) ;
  return 3 ;
}

/* set BOTH soft AND hard limits for a given resource */
static int set_reslimit ( lua_State * const L, const int reso )
{
  struct rlimit rlim ;

  rlim . rlim_cur = luaL_checkinteger ( L, 1 ) ;
  rlim . rlim_max = luaL_checkinteger ( L, 2 ) ;

  if ( setrlimit ( reso, & rlim ) ) {
    /* error */
    const int i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  lua_pushinteger ( L, 0 ) ;
  return 1 ;
}

/* set only the SOFT (!!) limit for a given resource */
static int set_soft_limit ( lua_State * const L, const int reso )
{
  struct rlimit rlim ;
  int i = getrlimit ( reso, & rlim ) ;

  if ( i ) {
    /* error */
    i = errno ;
    lua_pushinteger ( L, -3 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  /* success */
  rlim . rlim_cur = luaL_checkinteger ( L, 1 ) ;
  i = setrlimit ( reso, & rlim ) ;

  if ( i ) {
    /* error */
    i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  /* success */
  lua_pushinteger ( L, 0 ) ;
  return 1 ;
}

#ifdef RLIMIT_AS
static int Lgetrlimit_addrspace ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_AS ) ;
}

static int Lsetrlimit_addrspace ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_AS ) ;
}

static int Lsoftlimit_addrspace ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_AS ) ;
}
#endif

#ifdef RLIMIT_CORE
static int Lgetrlimit_core ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_CORE ) ;
}

static int Lsetrlimit_core ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_CORE ) ;
}

static int Lsoftlimit_core ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_CORE ) ;
}
#endif

#ifdef RLIMIT_CPU
static int Lgetrlimit_cpu ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_CPU ) ;
}

static int Lsetrlimit_cpu ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_CPU ) ;
}

static int Lsoftlimit_cpu ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_CPU ) ;
}
#endif

#ifdef RLIMIT_DATA
static int Lgetrlimit_data ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_DATA ) ;
}

static int Lsetrlimit_data ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_DATA ) ;
}

static int Lsoftlimit_data ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_DATA ) ;
}
#endif

#ifdef RLIMIT_FSIZE
static int Lgetrlimit_fsize ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_FSIZE ) ;
}

static int Lsetrlimit_fsize ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_FSIZE ) ;
}

static int Lsoftlimit_fsize ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_FSIZE ) ;
}
#endif

#ifdef RLIMIT_LOCKS
static int Lgetrlimit_locks ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_LOCKS ) ;
}

static int Lsetrlimit_locks ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_LOCKS ) ;
}

static int Lsoftlimit_locks ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_LOCKS ) ;
}
#endif

#ifdef RLIMIT_MEMLOCK
static int Lgetrlimit_memlock ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_MEMLOCK ) ;
}

static int Lsetrlimit_memlock ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_MEMLOCK ) ;
}

static int Lsoftlimit_memlock ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_MEMLOCK ) ;
}
#endif

#ifdef RLIMIT_MSGQUEUE
static int Lgetrlimit_msgqueue ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_MSGQUEUE ) ;
}

static int Lsetrlimit_msgqueue ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_MSGQUEUE ) ;
}

static int Lsoftlimit_msgqueue ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_MSGQUEUE ) ;
}
#endif

#ifdef RLIMIT_NICE
static int Lgetrlimit_nice ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_NICE ) ;
}

static int Lsetrlimit_nice ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_NICE ) ;
}

static int Lsoftlimit_nice ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_NICE ) ;
}
#endif

#ifdef RLIMIT_NOFILE
static int Lgetrlimit_nofile ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_NOFILE ) ;
}

static int Lsetrlimit_nofile ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_NOFILE ) ;
}

static int Lsoftlimit_nofile ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_NOFILE ) ;
}
#endif

#ifdef RLIMIT_NPROC
static int Lgetrlimit_nproc ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_NPROC ) ;
}

static int Lsetrlimit_nproc ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_NPROC ) ;
}

static int Lsoftlimit_nproc ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_NPROC ) ;
}
#endif

#ifdef RLIMIT_RSS
static int Lgetrlimit_rss ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_RSS ) ;
}

static int Lsetrlimit_rss ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_RSS ) ;
}

static int Lsoftlimit_rss ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_RSS ) ;
}
#endif

#ifdef RLIMIT_RTPRIO
static int Lgetrlimit_rt_prio ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_RTPRIO ) ;
}

static int Lsetrlimit_rt_prio ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_RTPRIO ) ;
}

static int Lsoftlimit_rt_prio ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_RTPRIO ) ;
}
#endif

#ifdef RLIMIT_RTTIME
static int Lgetrlimit_rt_time ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_RTTIME ) ;
}

static int Lsetrlimit_rt_time ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_RTTIME ) ;
}

static int Lsoftlimit_rt_time ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_RTTIME ) ;
}
#endif

#ifdef RLIMIT_SIGPENDING
static int Lgetrlimit_sigpending ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_SIGPENDING ) ;
}

static int Lsetrlimit_sigpending ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_SIGPENDING ) ;
}

static int Lsoftlimit_sigpending ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_SIGPENDING ) ;
}
#endif

#ifdef RLIMIT_STACK
static int Lgetrlimit_stack ( lua_State * const L )
{
  return get_reslimit ( L, RLIMIT_STACK ) ;
}

static int Lsetrlimit_stack ( lua_State * const L )
{
  return set_reslimit ( L, RLIMIT_STACK ) ;
}

static int Lsoftlimit_stack ( lua_State * const L )
{
  return set_soft_limit ( L, RLIMIT_STACK ) ;
}
#endif

/* wrapper function for getrlimit */
static int Sgetrlimit ( lua_State * const L )
{
  int m = 0, reso = 0 ;
  struct rlimit rlim ;
  const char * oarr [ 1 + RLIMIT_NLIMITS ] = { NULL } ;

  /*
  for ( i = 0 ; RLIMIT_NLIMITS >= i ; ++ i ) {
    oarr [ i ] = "undef" ;
  }
  */

  oarr [ RLIMIT_CPU ] = "cpu" ;
  oarr [ RLIMIT_FSIZE ] = "fsize" ;
  oarr [ RLIMIT_DATA ] = "data" ;
  oarr [ RLIMIT_STACK ] = "stack" ;
  oarr [ RLIMIT_CORE ] = "core" ;
  oarr [ RLIMIT_RSS ] = "rss" ;
  oarr [ RLIMIT_NOFILE ] = "nofile" ;
  oarr [ RLIMIT_AS ] = "as" ;
  oarr [ RLIMIT_NPROC ] = "nproc" ;
  oarr [ RLIMIT_MEMLOCK ] = "memlock" ;
  oarr [ RLIMIT_LOCKS ] = "locks" ;
  oarr [ RLIMIT_SIGPENDING ] = "sigpending" ;
  oarr [ RLIMIT_MSGQUEUE ] = "msgqueue" ;
  oarr [ RLIMIT_NICE ] = "nice" ;
  oarr [ RLIMIT_RTPRIO ] = "rtprio" ;
  oarr [ RLIMIT_RTTIME ] = "rttime" ;
  oarr [ RLIMIT_NLIMITS ] = NULL ;

  /* the first arg is a string describing the requested resource */
  m = luaL_checkoption ( L, 1, "core", oarr ) ;

  if ( ( 0 <= m ) && ( RLIMIT_NLIMITS >= m ) ) {
    reso = m ;
  } else {
    reso = RLIMIT_CORE ;
  }

  if ( getrlimit ( reso, & rlim ) ) {
    const int e = errno ;
    lua_pushnil ( L ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  /* check the returned hard limit for the given resource */
  if ( RLIM_INFINITY == rlim.rlim_max ) {
    (void) lua_pushliteral ( L, "unlimited" ) ;
  } else {
    lua_pushinteger ( L, rlim.rlim_max ) ;
  }

  /* check the returned soft limit for the given resource */
  if ( RLIM_INFINITY == rlim.rlim_cur ) {
    (void) lua_pushliteral ( L, "unlimited" ) ;
  } else {
    lua_pushinteger ( L, rlim.rlim_cur ) ;
  }

  /* return 2 values: the first is the hard limit,
   * the second is the soft limit for the requested resource */
  return 2 ;
}

/* wrapper function for setrlimit */
static int Ssetrlimit ( lua_State * const L )
{
  int i = 0, e = 0, m = 0, reso = 0 ;
  struct rlimit rlim ;
  const int n = lua_gettop ( L ) ;
  const char * oarr [ 1 + RLIMIT_NLIMITS ] = { NULL } ;

  /* got enough args ? */
  if ( 3 > n ) {
    lua_pushboolean ( L, 0 ) ;
    return 1 ;
  }

  /*
  for ( i = 0 ; RLIMIT_NLIMITS >= i ; ++ i ) {
    oarr [ i ] = "undef";
  }
  */

  oarr [ RLIMIT_CPU ] = "cpu" ;
  oarr [ RLIMIT_FSIZE ] = "fsize" ;
  oarr [ RLIMIT_DATA ] = "data" ;
  oarr [ RLIMIT_STACK ] = "stack";
  oarr [ RLIMIT_CORE ] = "core" ;
  oarr [ RLIMIT_RSS ] = "rss" ;
  oarr [ RLIMIT_NOFILE ] = "nofile" ;
  oarr [ RLIMIT_AS ] = "as" ;
  oarr [ RLIMIT_NPROC ] = "nproc" ;
  oarr [ RLIMIT_MEMLOCK ] = "memlock" ;
  oarr [ RLIMIT_LOCKS ] = "locks" ;
  oarr [ RLIMIT_SIGPENDING ] = "sigpending" ;
  oarr [ RLIMIT_MSGQUEUE ] = "msgqueue" ;
  oarr [ RLIMIT_NICE ] = "nice" ;
  oarr [ RLIMIT_RTPRIO ] = "rtprio" ;
  oarr [ RLIMIT_RTTIME ] = "rttime" ;
  oarr [ RLIMIT_NLIMITS ] = NULL ;

  /* the first arg is a string describing the requested resource */
  m = luaL_checkoption ( L, 1, "core", oarr ) ;

  if ( ( 0 <= m ) && ( RLIMIT_NLIMITS >= m ) ) {
    reso = m ;
  } else {
    reso = RLIMIT_CORE ;
  }

  /* second arg is the hard limit for the given resource (first arg) */
  if ( lua_isinteger ( L, 2 ) ) {
    rlim . rlim_max = luaL_checkinteger ( L, 2 ) ;
  } else {
    rlim . rlim_max = RLIM_INFINITY ;
  }

  /* third arg is the soft limit for the given resource (first arg) */
  if ( lua_isinteger ( L, 3 ) ) {
    rlim . rlim_cur = luaL_checkinteger ( L, 3 ) ;
  } else {
    rlim . rlim_cur = RLIM_INFINITY ;
  }

  i = setrlimit ( reso, & rlim ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/* wrapper function for prlimit ? (Linux only) */

/* wrapper function for daemon(3) */
static int Sdaemon ( lua_State * const L )
{
  int e = 0, i = lua_gettop ( L ) ;

  e = ( 0 < i ) ? luaL_optinteger ( L, 1, 0 ) : 0 ;
  i = ( 1 < i ) ? luaL_optinteger ( L, 2, 0 ) : 0 ;
  i = daemon ( e, i ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/*
 * helper functions for the exec* bindings
 */

#if 0
/* parse a given command string and call execv* */
static int exec_str ( char * com )
{
  if ( com && * com ) {
    if ( strpbrk ( com, SHELL_METACHARS ) ) {
      /* the given command string contains shell meta chars,
       * pass it to the shell
       */
      return execl ( "/bin/sh", "sh", "-c", com, (char *) NULL ) ;
    } else if ( strpbrk ( com, " \t\n" ) ) {
      /* the given command string contains whitespace,
       * split it into tokens
       */
    } else if ( '/' == * com ) {
      return execl ( com, com, (char *) NULL ) ;
    } else {
      return execlp ( com, com, (char *) NULL ) ;
    }
  }

  return -11 ;
}
#endif

/*
 * wrapper functions for the exec* syscalls
 */

/* wrapper function for the execv syscall */
static int Lexecl ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, 0 ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

/* wrapper function for the execvp syscall */
static int Lexeclp ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_PATH ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

/* wrapper function for the execv syscall */
static int Ldefsig_execl ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_DEFSIG ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

/* wrapper function for the execv syscall */
static int Ldefsig_execlp ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_PATH | EXEC_DEFSIG ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

/* vfork(2) and execve(2) into a given executable */
static int Lvfork_exec_nowait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_VFORK ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_exec_wait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_VFORK | EXEC_WAITPID ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_execp_nowait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_PATH | EXEC_VFORK ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_execp_wait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_PATH | EXEC_VFORK | EXEC_WAITPID ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_exec0_nowait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_ARGV0 | EXEC_VFORK ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_exec0_wait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_ARGV0 | EXEC_VFORK | EXEC_WAITPID ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_execp0_nowait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_ARGV0 | EXEC_PATH | EXEC_VFORK ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

static int Lvfork_execp0_wait ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_isstring ( L, 1 ) ) {
    return Lvfork_exec ( L, EXEC_ARGV0 | EXEC_PATH | EXEC_VFORK | EXEC_WAITPID ) ;
  }

  return luaL_error ( L, "string args required" ) ;
}

/*
 * end of execv wrapper functions
 */

/* our own daemon(3) like function */
static int Ldaemonize ( lua_State * const L )
{
  char two = 3, nu = 3 ;
  const char * pidf = NULL ;
  int i = lua_gettop ( L ) ;

  if ( ( 0 < i ) && lua_isboolean ( L, 1 ) ) {
    nu = lua_toboolean ( L, 1 ) ? 1 : 0 ;
  }

  if ( ( 1 < i ) && lua_isboolean ( L, 2 ) ) {
    two = lua_toboolean ( L, 2 ) ? 1 : 0 ;
  }

  if ( ( 2 < i ) && lua_isstring ( L, 3 ) ) {
    pidf = lua_tostring ( L, 3 ) ;
    pidf = ( pidf && * pidf && ( '/' == * pidf ) ) ? pidf : NULL ;
  }

  (void) fflush ( NULL ) ;
  i = fork () ;

  if ( two ) {
    /* double fork */
    if ( 0 == i ) {
      /* child process, fork() again */
      (void) setsid () ;
      (void) setpgid ( 0, 0 ) ;
      (void) chdir ( "/" ) ;
      i = fork () ;
    } else if ( 0 < i ) {
      /* quit the (calling) parent process */
      _exit ( 0 ) ;
      return 0 ;
    }
  }

  if ( 0 == i ) {
    /* child process */
    i = getpid () ;
    /* since forked above and run in the child process now
     * we got a fresh PID so that the following should succeed.
     */
    (void) setsid () ;
    (void) setpgid ( 0, 0 ) ;
    (void) chdir ( "/" ) ;
    reset_sigs () ;

    /* write pid file */
    if ( pidf && * pidf && ( '/' == * pidf ) ) {
      /* lock the pid file after writing ? */
      if ( 0 == unlink ( pidf ) || ENOENT == errno ) {
        FILE * fp = fopen ( pidf, "w" ) ;
        if ( fp ) {
          (void) fprintf ( fp, "%d\n", i ) ;
          (void) fclose ( fp ) ;
        }
      }
    }

#if defined (__GLIBC__) && defined (_GNU_SOURCE)
    (void) fcloseall () ;
#else
    (void) fclose ( stdin ) ;
    (void) fclose ( stdout ) ;
    (void) fclose ( stderr ) ;
#endif
    close_all ( -3 ) ;

    if ( nu ) {
      const int fd = open ( "/dev/null", O_RDWR | O_NONBLOCK | O_NOCTTY ) ;
      if ( 0 <= fd ) {
        (void) dup2 ( fd, STDIN_FILENO ) ;
        (void) dup2 ( fd, STDOUT_FILENO ) ;
        (void) dup2 ( fd, STDERR_FILENO ) ;
        if ( STDERR_FILENO < fd ) { close_fd ( fd ) ; }
      }
    }

    lua_pushinteger ( L, i ) ;
    return 1 ;
  } else if ( 0 < i ) {
    /* quit the parent process */
    _exit ( 0 ) ;
  } else if ( 0 > i ) {
    /* fork failed */
    i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  return 0 ;
}

/*
 * functions dealing with command pipelining
 */

/* waitpid(2) for all given PIDs */
#if 0
static int Lwait4pid ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( ( 0 < n ) && lua_isinteger ( L, 1 ) ) {
    int i, s, r = 0 ;
    pid_t p, pid = 0 ;

    /* reset SIGCHLD disposition to SIG_DFL before proceeding any further ? */
    for ( i = 1 ; n >= i ; ++ i ) {
      pid = luaL_checkinteger ( L, i ) ;

      if ( 1 < pid ) {
        do {
          s = 0 ;
          p = waitpid ( pid, & s, 0 ) ;

          if ( 0 < p && p == pid ) {
            if ( WIFEXITED( s ) ) {
              r = WEXITSTATUS( s ) ;
            } else if ( WIFSIGNALED( s ) ) {
              r = 128 + WTERMSIG( s ) ;
            } else if ( WIFSTOPPED( s ) ) {
              r = 128 + WSTOPSIG( s ) ;
            }

            break ;
          }
        } while ( 0 > p && EINTR == errno ) ;
      } /* end if */
    } /* end for */

    lua_pushinteger ( L, r ) ;
    return 1 ;
  }

  return luaL_error ( L, "non negative integer args expected" ) ;
}

static int Lwait4pids ( lua_State * const L )
{
  if ( ( 0 < lua_gettop ( L ) ) && lua_istable ( L, 1 ) ) {
    int s, r = 0 ;
    pid_t p, pid = 0 ;
    size_t i ;
    const size_t len = lua_rawlen ( L, 2 ) ;

    for ( i = 1 ; len >= i ; ++ i ) {
      if ( LUA_TNUMBER == lua_rawgeti ( L, 1, i ) ) {
        pid = lua_isinteger ( L, -1 ) ? lua_tointeger ( L, -1 ) : 0 ;

        if ( 1 < pid ) {
          do {
            s = 0 ;
            p = waitpid ( pid, & s, 0 ) ;

            if ( 0 < p && p == pid ) {
              if ( WIFEXITED( s ) ) {
                r = WEXITSTATUS( s ) ;
              } else if ( WIFSIGNALED( s ) ) {
                r = 128 + WTERMSIG( s ) ;
              } else if ( WIFSTOPPED( s ) ) {
                r = 128 + WSTOPSIG( s ) ;
              }

              break ;
            }
          } while ( 0 > p && EINTR == errno ) ;
        }
      } /* end if */

      /* pop the value pushed by rawgeti() off the stack */
      lua_pop ( L, 1 ) ;
    } /* end for */

    lua_pushinteger ( L, r ) ;
    return 1 ;
  }

  return luaL_error ( L, "non empty table expected" ) ;
}
#endif

