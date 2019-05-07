/*
 * Lua wrapper functions for interaction with System V STREAMS
 *
 * public domain code
 */

#if defined (OSsolaris) || defined (OSsunos5)
static int Lisastream ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 > i ) {
    lua_pushboolean ( L, 0 ) ;
  } else {
    i = isastream ( i ) ;
    lua_pushboolean ( L, i ? 1 : 0 ) ;
  }

  return 1 ;
}

static int Lfattach ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  const char * path = luaL_checkstring ( L, 2 ) ;

  if ( ( 0 <= i ) && path && * path ) {
    int e = 0 ;

    i = fattach ( i, path ) ;
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

static int Lfdetach ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;

    i = fdetach ( i ) ;
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

static int Lputmsg ( lua_State * L )
{
  return 0 ;
}

static int Lputpmsg ( lua_State * L )
{
  return 0 ;
}

static int Lgetmsg ( lua_State * L )
{
  return 0 ;
}

static int Lgetpmsg ( lua_State * L )
{
  return 0 ;
}

/* ipc using SysV STREANS for Solaris */
static int serv_stream ( const char * path )
{
  if ( path && * path ) {
    int i, p [ 2 ] ;

    /* unlink and (re)create the given file first to ensure the given
     * file path is ok
     */
    i = unlink ( path ) ;
    if ( i && ( ENOENT != errno ) ) { return -1 ; }
    i = creat ( path, 00600 ) ;
    if ( 0 > i ) { return -1 ; }
    i = close ( i ) ;
    if ( i ) { return -3 ; }
    /* create a new (STREAM) pipe */
    i = pipe ( p ) ;
    if ( i ) { return -11 ; }
    /* add the "connld" STREAN module to its client side fd */
    i = ioctl ( p [ 1 ], I_PUSH, "connld" ) ;
    if ( 0 > i ) { return -13 ; }
    /* attach the client side fd to the given file */
    i = fattach ( p [ 1 ], path ) ;
    if ( i ) { return -15 ; }
    return p [ 1 ] ;
  }

  return -111 ;
}

static int serv_stream_acc ( const int fd, int * uidp, int * gidp )
{
  if ( 0 <= fd ) {
    struct strrecvfd st ;

    if ( 0 > ioctl ( fd, i_recvfd, & st ) ) { return -5 ; }
    if ( uidp ) { * uidp = st . uid ; }
    if ( gidp ) { * gidp = st . gid ; }

    return st . fd ;
  }

  return -3 ;
}

static int req_stream_fd ( const char * path )
{
  if ( path && * path ) {
    int i = open ( path, O_RDWR | O_NONBLOCK ) ;

    if ( 0 > i ) { return -3 ; }
    else if ( isastream ( i ) ) { return i ; }
    else { close_fd ( i ) ; return -5 ; }
  }

  return -11 ;
}

static int Lreq_stream_fd ( lua_State * L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    lua_pushinteger ( L, req_stream_fd ( path ) ) ;
    return 1 ;
  }

  return 0 ;
}

static int Lserv_stream ( lua_State * L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    lua_pushinteger ( L, serv_stream ( path ) ) ;
    return 1 ;
  }

  return 0 ;
}

static int Lcheck_stream ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    int i, e = 0 ;
    struct pollfd pfd ;

    pfd . events = POLLIN ;
    pfd . fd = fd ;
    i = luaL_optinteger ( L, 2, 0 ) ;
    i = ( 0 < i ) ? i : 2 ;
    i = poll ( & pfd, 1, i * 1000 ) ;
    e = errno ;

    if ( 0 == i ) {
      lua_pushinteger ( L, -3 ) ;
      return 1 ;
    } else if ( 0 > i ) {
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else if ( 0 < i ) {
      if ( POLLIN & pfd . revents ) {
        int g ;
        g = e = -3 ;
        i = serv_stream_acc ( fd, & e, & g ) ;
        lua_pushinteger ( L, i ) ;
        lua_pushinteger ( L, e ) ;
        lua_pushinteger ( L, g ) ;
        return 3 ;
      } else if ( POLLNVAL & pfd . revents ) {
        /* we have closed the server end of the STREAM pipe (accidently ?),
         * reopen it ? better let the caller decide what to do
         */
        lua_pushinteger ( L, -5 ) ;
        return 1 ;
      } else if ( POLLHUP & pfd . revents ) {
        /* similar to above situation */
        lua_pushinteger ( L, -9 ) ;
        return 1 ;
      }

      lua_pushinteger ( L, -11 ) ;
      return 1 ;
    }
  }

  return 0 ;
}
#endif /* Solaris */

