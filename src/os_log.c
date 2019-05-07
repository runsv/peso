/*
 * logging related wrapper functions
 * (for things like syslog and klog)
 *
 * public domain code
 */

static int Lgetlogbit ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  //lua_pushinteger ( L, LOG_UPTO( i ) ) ;
  lua_pushinteger ( L, LOG_MASK( i ) ) ;
  return 1 ;
}

static int Lgetlogmask ( lua_State * L )
{
  lua_pushinteger ( L, setlogmask ( 0 ) ) ;
  return 1 ;
}

static int Lsetlogmask ( lua_State * L )
{
  int i = luaL_optinteger ( L, 1, 0 ) ;

  i = setlogmask ( i ) ;
  lua_pushinteger ( L, i ) ;
  return 1 ;
}

static int Lopenlog ( lua_State * L )
{
  return 0 ;
}

static int Lcloselog ( lua_State * L )
{
  closelog () ;
  return 0 ;
}

static int Lsyslog ( lua_State * L )
{
  return 0 ;
}

static int Llog2sys ( lua_State * L )
{
  /* Re-establish connection with syslogd every time.
   * Block signals while talking to syslog.
   */
  sigset_t nmask, omask ;
  (void) sigfillset ( & nmask ) ;
  NOINTR( sigprocmask ( SIG_BLOCK, & nmask, & omask ) )
  openlog ( "init", 0, LOG_DAEMON ) ;
  syslog ( LOG_INFO, "%s", buf ) ;
  closelog () ;
  NOINTR( sigprocmask ( SIG_SETMASK, & omask, NULL ) )

  return 0 ;
}

/* TODO: SysV: write to log STREAMS "log" and "conslog" */

static int Lwrite2cons ( lua_State * L )
{
  const char * msg = luaL_checkstring ( L, 1 ) ;

  if ( msg && * msg ) {
    int i = open ( CONSOLE, O_WRONLY | O_CLOEXEC | O_NOCTTY | O_NONBLOCK ) ;

    if ( 0 <= i ) {
      (void) write ( i, msg, str_len ( msg ) ) ;
      close_fd ( i ) ;
    }
  }

  return 0 ;
}

/*
static void Log ( const char co, const int err, const char * msg )
{
}

static void setup_io ( void )
{
}
*/

