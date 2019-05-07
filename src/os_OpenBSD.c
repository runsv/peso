/*
 * OpenBSD specific functions
 *
 * of interest to us:
 * setlogin(), getttynam(), getpwnam_shadow(). readpassphrase().
 * crypt_checkpass(), end(pw,tty)ent(), logwtmp(), login_fbtab(),
 * acct(), login_getclass(), setusercontext(), login_close()
 */

/*
static void warning ( char * message, ... )
{
  va_list ap ;

  va_start ( ap, message ) ;
  vsyslog ( LOG_ALERT, message, ap ) ;
  va_end ( ap ) ;
  closelog () ;
}

static void emergency ( char * message, ... )
{
  struct syslog_data sdata = SYSLOG_DATA_INIT ;
  va_list ap ;

  va_start ( ap, message ) ;
  vsyslog_r ( LOG_EMERG, & sdata, message, ap ) ;
  va_end ( ap ) ;
}
*/

/* get the security level of the kernel */
static int getseclevel ( void )
{
#ifdef KERN_SECURELVL
  int i = 0, name [ 2 ] = { 0 } ;
  size_t len = sizeof ( int ) ;

  name [ 0 ] = CTL_KERN ;
  name [ 1 ] = KERN_SECURELVL ;

  if ( 0 > sysctl ( name, 2, & i, & len, NULL, 0 ) ) {
    emergency ( "cannot get kernel security level: %s",
      strerror ( errno ) ) ;
    return -1 ;
  }

  return i ;
#else
  errno = ENOSYS ;

  return -1 ;
#endif
}

/* set the security level of the kernel */
static void setseclevel ( const int n )
{
#ifdef KERN_SECURELVL
  const int c = getseclevel () ;

  if ( n != c ) {
    int name [ 2 ] = { 0 } ;
    name [ 0 ] = CTL_KERN ;
    name [ 1 ] = KERN_SECURELVL ;

    if ( 0 > sysctl ( name, 2, NULL, NULL, & n, sizeof ( int ) ) ) {
      emergency (
        "cannot change kernel security level from %d to %d: %s",
        c, n, strerror ( errno ) ) ;
      return ;
    }

/*
# ifdef SECURE
  warning ( "kernel security level changed from %d to %d", cur, new ) ;
# endif
*/
  }
#endif
}

/* close out the accounting files for a login session */
static void clear_session_logs ( session_t * sp )
{
  char * line = sp -> se_device + sizeof ( _PATH_DEV ) - 1 ;

  if ( logout ( line ) ) {
    logwtmp ( line, "", "" ) ;
  }
}

/* start a session and allocate a controlling terminal.
 * only called by children of init after forking.
 */
static void setctty ( char * name )
{
  int i ;

  (void) revoke ( name ) ;
  sleep ( 2 ) ; /* leave DTR low */
  i = open ( name, O_RDWR ) ;

  if ( 0 > i ) {
    //stall ( "can't open %s: %m", name ) ;
    _exit ( 1 ) ;
  } else if ( 0 > login_tty ( fd ) ) {
    //stall ( "can't get %s for controlling terminal: %m", name ) ;
    _exit ( 1 ) ;
  }
}

static void setprocresources ( char * name )
{
#ifdef LOGIN_CAP
  login_cap_t * cp = login_getclass ( name ) ;

  if ( cp ) {
    setusercontext ( cp, NULL, 0,
      LOGIN_SETPRIORITY | LOGIN_SETRESOURCES | LOGIN_SETUMASK ) ;
    login_close ( cp ) ;
  }
#endif
}

/*
#ifdef CPU_LIDSUSPEND
	int lidsuspend_mib [ ] = { CTL_MACHDEP, CPU_LIDSUSPEND } ;
	int dontsuspend = 0 ;

	if ( ( death_howto & RB_POWERDOWN ) &&
	    ( sysctl ( lidsuspend_mib, 2, NULL, NULL, & dontsuspend,
		    sizeof ( dontsuspend ) ) == -1 ) && ( errno != EOPNOTSUPP ) )
			warning ( "cannot disable lid suspend" ) ;
#endif
*/

/* wrapper to the pledge(2) syscall */
static int Spledge ( lua_State * L )
{
  const char * prom = luaL_checkstring ( L, 1 ) ;
  const char * exprom = luaL_checkstring ( L, 2 ) ;

  if ( prom && exprom ) {
    return res_zero ( L, pledge ( prom, exprom ) ) ;
  }

  return luaL_error ( L, "2 string args required" ) ;
}

/* wrapper to the unveil(2) syscall */
static int Sunveil ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;
  const char * path = ( 0 < n ) ? luaL_checkstring ( L, 1 ) : NULL ;
  const char * perm = ( 1 < n ) ? luaL_checkstring ( L, 2 ) : NULL ;

  return res_zero ( L, unveil ( path, perm ) ) ;
}

/* disable further unveil(2) calls */
static int Llast_unveil ( lua_State * L )
{
  return res_zero ( L, unveil ( NULL, NULL ) ) ;
}

