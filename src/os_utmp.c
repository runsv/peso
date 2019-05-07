/*
 * funtions to access the system U/WTMP databases
 */

/* wrappers for functions that interact with the u/wtmp records */

/* writes a record to both utmp and wtmp */
/* set an utmp entry directly */
static int Lwrite_utmp ( lua_State * L )
{
  int type = -3 ;
  pid_t pid = 0 ;
  const char * id = NULL ;
  const char * user = NULL ;
  const char * line = NULL ;
  const char * host = NULL ;
  struct utmp ut ;
  const int n = lua_gettop ( L ) ;

  if ( 2 > n ) { return 0 ; }
  type = luaL_checkinteger ( L, 1 ) ;

  switch ( type ) {
    case EMPTY :
    case RUN_LVL :
    case BOOT_TIME :
    case NEW_TIME :
    case OLD_TIME :
    case INIT_PROCESS :
    case LOGIN_PROCESS :
    case USER_PROCESS :
    case DEAD_PROCESS :
      /* ok, valid utmp type */
      break ;
    default :
      return luaL_argerror ( L, 1, "invalid utmp type" ) ;
      break ;
  }

  pid = luaL_checkinteger ( L, 2 ) ;
  pid = ( 0 > pid ) ? pid : getpid () ;
  id = ( 2 < n ) ? luaL_checkstring ( L, 3 ) : NULL ;
  line = ( 3 < n ) ? luaL_checkstring ( L, 4 ) : NULL ;
  user = ( 4 < n ) ? luaL_checkstring ( L, 5 ) : NULL ;
  host = ( 5 < n ) ? luaL_checkstring ( L, 6 ) : NULL ;
  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = type ;
  ut . ut_pid  = pid ;

  if ( id && * id ) {
    strncopy ( ut . ut_id, id, sizeof ( ut . ut_id ) ) ;
  }

  if ( user && * user ) {
    strncopy ( ut . ut_user, user, sizeof ( ut . ut_user ) ) ;
  }

  if ( line && * line ) {
    strncopy ( ut . ut_line, line, sizeof ( ut . ut_line ) ) ;
  }

  if ( host && * host ) {
    strncopy ( ut . ut_host, host, sizeof ( ut . ut_host ) ) ;
  }

  lua_pushinteger ( L, write_utmp ( & ut ) ) ;
  return 1 ;
}

/* add an utmp boot time entry */
static int Lutmp_boot_time ( lua_State * L )
{
  struct utmp ut ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = BOOT_TIME ;
  ut . ut_pid = 0 ;
  strncopy ( ut . ut_user, "reboot", sizeof ( ut . ut_user ) ) ;
  lua_pushinteger ( L, write_utmp ( & ut ) ) ;

  return 1 ;
}

/* add an utmp halt/shutdown time entry */
static int Lutmp_halt_time ( lua_State * L )
{
  struct utmp ut ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = RUN_LVL ;
  ut . ut_pid = 0 ;
  strncopy ( ut . ut_user, "shutdown", sizeof ( ut . ut_user ) ) ;
  lua_pushinteger ( L, write_utmp ( & ut ) ) ;

  return 1 ;
}

/* add/change an utmp runlevel entry */
static int Lutmp_write_runlevel ( lua_State * L )
{
  char now = 'S' ;
  char prev = 'S' ;
  struct utmp ut ;
  int n = luaL_checkinteger ( L, 1 ) ;
  int p = luaL_optinteger ( L, 2, -1 ) ;

  now = ( 0 <= n && 9 >= n ) ? '0' + n : '3' ;
  prev = ( 0 <= p && 9 >= p ) ? '0' + p : 0 ;
  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = RUN_LVL ;
  ut . ut_pid = now + 256 * prev ;
  strncopy ( ut . ut_user, "runlevel", sizeof ( ut . ut_user ) ) ;
  lua_pushinteger ( L, write_utmp ( & ut ) ) ;

  return 1 ;
}

/* mark a process' utmp entry as dead */
static int Lutmp_dead_proc ( lua_State * L )
{
  struct utmp ut ;
  pid_t p = luaL_optinteger ( L, 1, 0 ) ;

  p = ( 1 < p ) ? p : getpid () ;
  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = DEAD_PROCESS ;
  ut . ut_pid = p ;
  lua_pushinteger ( L, write_utmp ( & ut ) ) ;

  return 1 ;
}

static int Lutmp_init_proc ( lua_State * L )
{
  const char * id = luaL_checkstring ( L, 1 ) ;
  const char * line = luaL_checkstring ( L, 2 ) ;

  if ( id && line && * id && * line ) {
    struct utmp ut ;
    pid_t p = luaL_optinteger ( L, 3, 0 ) ;

    p = ( 1 < p ) ? p : getpid () ;
    /* zero out the utmp entry struct before use */
    (void) memset ( & ut, 0, sizeof ( ut ) ) ;
    ut . ut_type = INIT_PROCESS ;
    ut . ut_pid = p ;
    strncopy ( ut . ut_id, id, sizeof ( ut . ut_id ) ) ;
    strncopy ( ut . ut_line, line, sizeof ( ut . ut_line ) ) ;
    lua_pushinteger ( L, write_utmp ( & ut ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 non empty string args required" ) ;
}

static int Lutmp_login_proc ( lua_State * L )
{
  const char * id = luaL_checkstring ( L, 1 ) ;
  const char * line = luaL_checkstring ( L, 2 ) ;

  if ( id && line && * id && * line ) {
    struct utmp ut ;
    pid_t p = luaL_optinteger ( L, 3, 0 ) ;

    p = ( 1 < p ) ? p : getpid () ;
    /* zero out the utmp entry struct before use */
    (void) memset ( & ut, 0, sizeof ( ut ) ) ;
    ut . ut_type = LOGIN_PROCESS ;
    ut . ut_pid = p ;
    strncopy ( ut . ut_id, id, sizeof ( ut . ut_id ) ) ;
    strncopy ( ut . ut_line, line, sizeof ( ut . ut_line ) ) ;
    strncopy ( ut . ut_user, "LOGIN", sizeof ( ut . ut_user ) ) ;
    lua_pushinteger ( L, write_utmp ( & ut ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 non empty string args required" ) ;
}

static int Lutmp_create ( lua_State * L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;
  const gid_t g = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

  if ( path && * path ) {
    lua_pushboolean ( L, utmp_create ( path, g ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "non empty file path and integer GID >= 0 required" ) ;
}

static int Lutmp_set_boot ( lua_State * L )
{
  lua_pushinteger ( L, utmp_set_boot () ) ;
  return 1 ;
}

static int Lutmp_set_halt ( lua_State * L )
{
  lua_pushinteger ( L, utmp_set_halt () ) ;
  return 1 ;
}

static int Lutmp_set_runlevel ( lua_State * L )
{
  int n = luaL_checkinteger ( L, 1 ) ;
  int p = luaL_checkinteger ( L, 2 ) ;

  n = ( 0 <= n && 9 >= n ) ? n : 3 ;
  p = ( 0 <= p && 9 >= p ) ? p : 0 ;
  lua_pushinteger ( L, utmp_set_runlevel ( n, p ) ) ;

  return 1 ;
}

static int Lutmp_set_dead ( lua_State * L )
{
  pid_t p = luaL_optinteger ( L, 1, 0 ) ;

  p = ( 1 < p ) ? p : getpid () ;
  lua_pushinteger ( L, utmp_set_dead ( p ) ) ;

  return 1 ;
}

static int Lutmp_set_init ( lua_State * L )
{
  const char * id = luaL_checkstring ( L, 1 ) ;
  const char * line = luaL_checkstring ( L, 2 ) ;

  if ( id && line && * id && * line ) {
    lua_pushinteger ( L, utmp_set_init ( id, line ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 non empty string args required" ) ;
}

static int Lutmp_set_login ( lua_State * L )
{
  const char * id = luaL_checkstring ( L, 1 ) ;
  const char * line = luaL_checkstring ( L, 2 ) ;

  if ( id && line && * id && * line ) {
    lua_pushinteger ( L, utmp_set_login ( id, line ) ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 non empty string args required" ) ;
}

static int Lutmp_dump ( lua_State * L )
{
  int i = -3 ;
  const int n = lua_gettop ( L ) ;
  const char * path = NULL ;
  FILE * fp = NULL ;

  if ( 0 < n ) {
    path = luaL_checkstring ( L, 1 ) ;
  } else {
    path = UTMP_FILE ;
  }

  if ( 1 < n ) {
    const char * dest = luaL_checkstring ( L, 2 ) ;

    if ( dest && * dest ) {
      i = open ( dest, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, 00644 ) ;

      if ( 0 > i ) {
        return luaL_error ( L, "open \"%s\" failed: %s",
          dest, strerror ( errno ) ) ;
      } else {
        fp = fdopen ( i, "w" ) ;
      }

      if ( NULL == fp ) {
        return luaL_error ( L, "fdopen \"%s\" failed: %s",
          dest, strerror ( errno ) ) ;
      }
    } else {
      return luaL_argerror ( L, 2, "invalid string" ) ;
    }
  }

  if ( path && * path && ( 0 == utmpname ( path ) ) ) {
    pid_t pid = 0 ;
    time_t sec = 0 ;
    struct utmp * utp = NULL ;
    struct tm * sectm = NULL ;
    char id [ 1 + sizeof ( utp -> ut_id ) ] = { 0 } ;
    char user [ 1 + sizeof ( utp -> ut_user ) ] = { 0 } ;
    char when [ 81 ] = { 0 } ;
    char addr [ 65 ] = { 0 } ;

    (void) fprintf ( fp ? fp : stdout,
      "%s =============================================================================================\n"
      , path ) ;
    setutent () ;

    while ( NULL != ( utp = getutent () ) ) {
      pid = utp -> ut_pid ;
      sec = utp -> ut_tv . tv_sec ;
      sectm = localtime ( & sec ) ;
      /*
      (void) memset ( id, 0, sizeof ( id ) ) ;
      (void) memset ( user, 0, sizeof ( user ) ) ;
      */
      (void) memset ( addr, 0, sizeof ( user ) ) ;
      (void) memset ( when, 0, sizeof ( user ) ) ;
      strncopy ( id, utp -> ut_id, sizeof ( id ) - 1 ) ;
      strncopy ( user, utp -> ut_user, sizeof ( user ) - 1 ) ;
      (void) strftime ( when, sizeof ( when ) - 1, "%F %T", sectm ) ;

      if ( utp -> ut_addr_v6 [ 1 ]
        || utp -> ut_addr_v6 [ 2 ]
        || utp -> ut_addr_v6 [ 3 ] )
      {
        (void) inet_ntop ( AF_INET6, utp -> ut_addr_v6, addr, sizeof ( addr ) - 1 ) ;
      } else {
        (void) inet_ntop ( AF_INET, & utp -> ut_addr, addr, sizeof ( addr ) - 1 ) ;
      }

      (void) fprintf ( fp ? fp : stdout,
        "[%d] [%05d] [%-4.4s] [%-8.8s] [%-12.12s] [%-20.20s] [%-15.15s] [%-19.19s]\n",
        utp -> ut_type, pid, id, user, utp -> ut_line, utp -> ut_host,
        addr, when ) ;
    }

    endutent () ;
  }

  if ( 0 <= i ) { while ( close ( i ) && ( EINTR == errno ) ) ; }
  if ( fp ) { (void) fclose ( fp ) ; }

  return 0 ;
}

