/*
 * Wrapper functions for the reboot(2) syscall.
 * These work on Linux, the BSDs and Solaris (SunOS 5)
 */

#define USED_BY_PESI
#include "reboot.c"
#undef USED_BY_PESI

/* (v)fork(2) and call reboot(2) in the child process */
static pid_t fork_reboot ( const int cmd )
{
  pid_t p = 0 ;

  sync () ;

  while ( 0 > ( p = vfork () ) && ENOSYS != errno ) {
    do_sleep ( 5, 0 ) ;
  }

  if ( 0 == p ) {
    /* child process */
    _exit ( do_reboot ( ( 0 < cmd ) ? cmd : 'r' ) ) ;
  } else if ( 0 < p ) {
    /* parent process */
    pid_t p2 = 0 ;

    do {
      p2 = waitpid ( p, NULL, 0 ) ;
      if ( 0 < p2 && p2 == p ) { break ; }
    } while ( 0 > p2 && EINTR == errno ) ;
  }

  return p ;
}

/* halts the system */
static int Lhalt ( lua_State * L )
{
  sync () ;
  lua_pushinteger ( L, do_reboot ( 'h' ) ) ;
  return 1 ;
}

/* powers down the system */
static int Lpoweroff ( lua_State * L )
{
  sync () ;
  lua_pushinteger ( L, do_reboot ( 'p' ) ) ;
  return 1 ;
}

/* reboots the system */
static int Lreboot ( lua_State * L )
{
  sync () ;
  lua_pushinteger ( L, do_reboot ( 'r' ) ) ;
  return 1 ;
}

/* wrapper function for the reboot() syscall
 * (uses the uadmin() syscall instead on Solaris)
 */
static int Sreboot ( lua_State * L )
{
  int e = 0, i = 'r' ;
  const char * what = luaL_optstring ( L, 1, "rR" ) ;

  if ( what && * what ) { i = * what ; }
  sync () ;
  i = do_reboot ( i ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;
  return 2 ;
}

#if defined (OSsolaris) || defined (OSsunos5)
static int Suadmin ( lua_State * L )
{
  return 0 ;
}
#endif

static int freboot ( lua_State * L, int cmd )
{
  int i = 0 ;

  if ( 0 < geteuid () ) {
    return luaL_error ( L, "must be super user" ) ;
  }

  i = do_reboot ( ( 0 < cmd ) ? cmd : 'r' ) ;

  if ( i ) {
    i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  lua_pushinteger ( L, 0 ) ;
  return 1 ;
}

static int Lfork_halt ( lua_State * L )
{
  return freboot ( L, 'h' ) ;
}

static int Lfork_poweroff ( lua_State * L )
{
  return freboot ( L, 'p' ) ;
}

static int Lfork_reboot ( lua_State * L )
{
  int i = luaL_optinteger ( L, 1, 'r' ) ;

  i = ( 0 < i ) ? i : 'r' ;

  return freboot ( L, i ) ;
}

