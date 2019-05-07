/*
 * wrappers to io related syscalls (using file descriptors)
 */

/*
(void) printf ( "\033[1;32;44m  %s  \n\033[0m", msg ) ;
*/

/* wrapper function for the open syscall */
static int Sopen ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    const char * path = luaL_checkstring ( L, 1 ) ;

    if ( path && * path ) {
      int i = luaL_checkinteger ( L, 2 ) ;
      i = i ? i : O_RDONLY ;

      if ( ( O_CREAT & i ) || ( O_TMPFILE & i ) ) {
        mode_t m = 00600 ;
        if ( 2 < n ) { m = luaL_checkinteger ( L, 3 ) ; }
        m = ( 007777 & m ) ? m : 00600 ;
        return res_lt ( L, 0, open ( path, i, m ) ) ;
      } else {
        return res_lt ( L, 0, open ( path, i ) ) ;
      }
    } else {
      return luaL_argerror ( L, 1, "empty path" ) ;
    }
  }

  return 0 ;
}

/* wrapper function for the close syscall */
static int Screat ( lua_State * L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    mode_t m = luaL_optinteger ( L, 2, 00600 ) ;
    m = ( 007777 & m ) ? m : 00600 ;
    return res_lt ( L, 0, creat ( path, m ) ) ;
  } else {
    return luaL_argerror ( L, 1, "empty path" ) ;
  }

  return 0 ;
}

/* wrapper function for the dup(2) syscall */
static int Sdup ( lua_State * L )
{
  const int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    return res_lt ( L, 0, dup ( i ) ) ;
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

static int Lxdup ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    int i ;

    do { i = dup ( fd ) ; }
    while ( 0 > i && EINTR == errno ) ;

    return res_lt ( L, 0, i ) ;
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

static int Ldup_fd ( lua_State * L )
{
  const int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 > i ) {
    return luaL_argerror ( L, 1, "invalid fd" ) ;
  }

  return res_lt ( L, 0, dup_fd ( i ) ) ;
}

/* wrapper function for the dup2(2) syscall */
static int Sdup2 ( lua_State * L )
{
  const int o = luaL_checkinteger ( L, 1 ) ;
  const int n = luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= o && 0 <= n ) {
    return res_lt ( L, 0, dup2 ( o, n ) ) ;
  }

  return luaL_error ( L, "2 vaild fds required" ) ;
}

static int Lxdup2 ( lua_State * L )
{
  const int o = luaL_checkinteger ( L, 1 ) ;
  const int n = luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= o && 0 <= n ) {
    int i ;

    do { i = dup2 ( o, n ) ; }
    while ( 0 > i && EINTR == errno ) ;

    return res_lt ( L, 0, i ) ;
  }

  return luaL_error ( L, "2 vaild fds required" ) ;
}

static int Ldup_to ( lua_State * L )
{
  const int o = luaL_checkinteger ( L, 1 ) ;
  const int n = luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= o && 0 <= n ) {
    return res_lt ( L, 0, dup_to ( o, n ) ) ;
  }

  return luaL_error ( L, "2 vaild fds required" ) ;
}

/* wrapper function for the (Linux) dup3 syscall */
static int Sdup3 ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  int e = luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= i && 0 <= e ) {
#if defined (OSLinux)
    int f = luaL_optinteger ( L, 3, 0 ) ;
    f = ( 0 < f ) ? f : O_CLOEXEC ;
    i = dup3 ( i, e, f ) ;
#else
    i = dup2 ( i, e ) ;
#endif

    if ( 0 > i ) {
      e = errno ;
      lua_pushinteger ( L, -3 ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
#if defined (OSLinux)
#else
      (void) fcntl ( i, F_SETFD, FD_CLOEXEC ) ;
#endif
      lua_pushinteger ( L, i ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* wrapper function for the close(2) syscall */
static int Sclose ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    return res_zero ( L, close ( fd ) ) ;
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

/* wrapper function for the close(2) syscall */
static int Lclose_fd ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    return res_zero ( L, close_fd ( fd ) ) ;
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

static int Lclose_all ( lua_State * L )
{
  const int i = luaL_optinteger ( L, 1, -3 ) ;
  close_all ( i ) ;
  return 0 ;
}

/* wrapper function for the lseek syscall */
static int Slseek ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;
  off_t o = luaL_checkinteger ( L, 2 ) ;
  int w = luaL_checkinteger ( L, 3 ) ;

  if ( 0 <= fd ) {
    o = lseek ( fd, o, w ) ;
    w = errno ;
    lua_pushinteger ( L, o ) ;

    if ( 0 > o ) {
      lua_pushinteger ( L, w ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper function for the write syscall */
static int Swrite ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  const char * str = luaL_checkstring ( L, 2 ) ;
  //size_t s = luaL_optinteger ( L, 3, 0 ) ;

  if ( ( 0 <= i ) && str && * str ) {
    int e = 0 ;

    i = write ( i, str, str_len ( str ) ) ;
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

#define BUF_LEN		801

/* wrapper function for the read syscall */
static int Sread ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  size_t s = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

  if ( 0 <= i && 0 < s ) {
    if ( 0 < s && BUF_LEN > s ) {
      char buf [ BUF_LEN ] = { 0 } ;

      i = read ( i, buf, s ) ;
      if ( 0 > i ) {
        i = errno ;
        lua_pushinteger ( L, -3 ) ;
        lua_pushinteger ( L, i ) ;
        return 2 ;
      } else if ( 0 < i ) {
        lua_pushinteger ( L, i ) ;
        lua_pushstring ( L, buf ) ;
        return 2 ;
      } else if ( 0 == i ) {
        lua_pushinteger ( L, 0 ) ;
        return 1 ;
      }
    } else if ( 0 < s && BUF_LEN <= s ) {
      /* allocate a Lua buffer of the requested length */
      luaL_Buffer b ;
      char * p = luaL_buffinitsize ( L, & b, s ) ;
      i = read ( i, p, s ) ;
      luaL_pushresultsize ( & b, s ) ;

      if ( 0 > i ) {
        i = errno ;
        lua_pushinteger ( L, -3 ) ;
        lua_pushinteger ( L, i ) ;
        return 2 ;
      } else if ( 0 < i ) {
        lua_pushinteger ( L, i ) ;
        return 2 ;
      } else if ( 0 == i ) {
        lua_pushinteger ( L, 0 ) ;
        return 1 ;
      }
    }
  }

  return 0 ;
}

static int Lbuf_read ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    ssize_t s = 0 ;
    char buf [ 1024 ] = { 0 } ;

    do { s = read ( fd, buf, sizeof ( buf ) - 1 ) ; }
    while ( 0 > s && EINTR == errno ) ;

    if ( 0 == s ) {
      /* reached EOF */
      lua_pushnil ( L ) ;
      return 1 ;
    } else if ( 0 > s ) {
      s = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, s ) ;
      return 2 ;
    } else if ( 0 < s ) {
      lua_pushlstring ( L, buf, s ) ;
      lua_pushinteger ( L, s ) ;
      return 2 ;
    }
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

static int Lbuf_read_close ( lua_State * L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    ssize_t s = 0 ;
    char buf [ 1024 ] = { 0 } ;

    do { s = read ( fd, buf, sizeof ( buf ) - 1 ) ; }
    while ( 0 > s && EINTR == errno ) ;

    if ( 0 == s ) {
      /* reached EOF */
      while ( close ( fd ) && ( EINTR == errno ) ) { ; }

      lua_pushnil ( L ) ;
      return 1 ;
    } else if ( 0 > s ) {
      s = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, s ) ;
      return 2 ;
    } else if ( 0 < s ) {
      lua_pushlstring ( L, buf, s ) ;
      lua_pushinteger ( L, s ) ;
      return 2 ;
    }
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

/* wrapper function for the pipe syscall */
static int Spipe ( lua_State * L )
{
  int p [ 2 ] = { -1 } ;

  if ( pipe ( p ) ) {
    const int i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
  } else {
    lua_pushinteger ( L, p [ 0 ] ) ;
    lua_pushinteger ( L, p [ 1 ] ) ;
  }

  return 2 ;
}

/* another wrapper for the pipe syscall */
static int Lpipe ( lua_State * L )
{
  int p [ 2 ] ;

  if ( create_pipe ( 3, 3, p ) ) {
    lua_pushinteger ( L, -2 ) ;
    lua_pushinteger ( L, -2 ) ;
  } else {
    lua_pushinteger ( L, p [ 0 ] ) ;
    lua_pushinteger ( L, p [ 1 ] ) ;
  }

  return 2 ;
}

static int Ssocketpair ( lua_State * L )
{
  int i, p [ 2 ] ;

#if defined (OSLinux)
  i = socketpair ( AF_UNIX,
    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, p ) ;
#else
  i = socketpair ( AF_UNIX, SOCK_STREAM, 0, p ) ;
#endif

  if ( i ) {
    i = errno ;
    lua_pushinteger ( L, -3 ) ;
    lua_pushinteger ( L, i ) ;
  } else {
    (void) fcntl ( p [ 0 ], F_SETFD, FD_CLOEXEC ) ;
    (void) fcntl ( p [ 1 ], F_SETFD, FD_CLOEXEC ) ;
    (void) fcntl ( p [ 0 ], F_SETFL, O_NONBLOCK ) ;
    (void) fcntl ( p [ 1 ], F_SETFL, O_NONBLOCK ) ;
    lua_pushinteger ( L, p [ 0 ] ) ;
    lua_pushinteger ( L, p [ 1 ] ) ;
  }

  return 2 ;
}

static int Lstream_pipe ( lua_State * L )
{
  int i = 0, p [ 2 ] ;

#if defined (OSsolaris)
  i = pipe ( p ) ;
#elif defined (OSLinux)
  i = socketpair ( AF_UNIX,
    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, p ) ;
#else
  i = socketpair ( AF_UNIX, SOCK_STREAM, 0, p ) ;
#endif

  if ( i ) {
    i = errno ;
    lua_pushinteger ( L, -3 ) ;
    lua_pushinteger ( L, i ) ;
  } else {
    (void) fcntl ( p [ 0 ], F_SETFD, FD_CLOEXEC ) ;
    (void) fcntl ( p [ 1 ], F_SETFD, FD_CLOEXEC ) ;
    (void) fcntl ( p [ 0 ], F_SETFL, O_NONBLOCK ) ;
    (void) fcntl ( p [ 1 ], F_SETFL, O_NONBLOCK ) ;
    lua_pushinteger ( L, p [ 0 ] ) ;
    lua_pushinteger ( L, p [ 1 ] ) ;
  }

  return 2 ;
}

#if 0
/* wrapper function for the fcntl syscall */
static int Sfcntl ( lua_State * L )
{
  int e = 0, r = 0 ;
  int fd = luaL_checkinteger ( L, 1 ) ;
  int cmd = luaL_checkinteger ( L, 2 ) ;

  return 0 ;
}

/* wrapper function for the ioctl syscall */
static int Sioctl ( lua_State * L )
{
  int fd = luaL_checkinteger ( L, 1 ) ;
  unsigned int cmd = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

  return 0 ;
}
#endif

/* checks if a given file descriptor has data pending */
static int Lfd_has_data ( lua_State * L )
{
  int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 > fd ) {
    lua_pushboolean ( L, 0 ) ;
  } else {
    lua_pushboolean ( L, fd_has_data ( fd ) ) ;
  }

  return 1 ;
}

/* check if a number of given fds is ready for reading using select(2) */
static int Lfds_have_data ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i = luaL_checkinteger ( L, 1 ) ;

    if ( 0 <= i ) {
      int j, m = i ;
      struct timeval tv ;
      fd_set fds ;

      tv . tv_sec = 0 ;
      tv . tv_usec = 0 ;
      FD_ZERO( & fds ) ;
      FD_SET( i, & fds ) ;

      for ( j = 2 ; n >= j ; ++ j ) {
        i = luaL_checkinteger ( L, j ) ;
        if ( 0 <= i ) {
          m = ( m < i ) ? i : m ;
          FD_SET( i, & fds ) ;
        }
      }

      j = select ( 1 + m, & fds, NULL, NULL, & tv ) ;
      if ( 0 > j ) {
        j = errno ;
        lua_pushinteger ( L, -1 ) ;
        lua_pushinteger ( L, j ) ;
        return 2 ;
      } else {
        lua_pushinteger ( L, j ) ;
        return 1 ;
      }
    }

    lua_pushinteger ( L, -3 ) ;
    return 1 ;
  }

  return 0 ;
}

/* wait for some time until a number of given fds become
 * ready for reading using select(2)
 */
static int Lwait_for_fd_data ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 2 < n ) {
    int i = luaL_checkinteger ( L, 3 ) ;

    if ( 0 <= i ) {
      int j, m = i ;
      struct timeval tv ;
      fd_set fds ;

      j = luaL_checkinteger ( L, 1 ) ;
      tv . tv_sec = ( 0 < j ) ? j : 0 ;
      j = luaL_checkinteger ( L, 2 ) ;
      tv . tv_usec = ( 0 < j && 999999 >= j ) ? j : 0 ;
      FD_ZERO( & fds ) ;
      FD_SET( i, & fds ) ;

      for ( j = 2 ; n >= j ; ++ j ) {
        i = luaL_checkinteger ( L, j ) ;
        if ( 0 <= i ) {
          m = ( m < i ) ? i : m ;
          FD_SET( i, & fds ) ;
        }
      }

      j = select ( 1 + m, & fds, NULL, NULL, & tv ) ;
      if ( 0 > j ) {
        j = errno ;
        lua_pushinteger ( L, -1 ) ;
        lua_pushinteger ( L, j ) ;
        return 2 ;
      } else {
        lua_pushinteger ( L, j ) ;
        return 1 ;
      }
    }

    lua_pushinteger ( L, -3 ) ;
    return 1 ;
  }

  return 0 ;
}

static int Lfd_has_input ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    long int s, u ;
    struct timeval tv ;
    fd_set fds ;
    int i = luaL_checkinteger ( L, 1 ) ;

    s = u = 0 ;
    if ( 1 < n ) { s = luaL_checkinteger ( L, 2 ) ; }
    if ( 2 < n ) { u = luaL_checkinteger ( L, 3 ) ; }
    s = ( 0 < s ) ? s : 0 ;
    u = ( 0 < u ) ? u : 0 ;
    tv . tv_sec = s ;
    tv . tv_usec = u ;
    FD_ZERO( & fds ) ;
    FD_SET( i, & fds ) ;
    i = select ( 1 + i, & fds, NULL, NULL, & tv ) ;

    if ( 0 > i ) {
      i = errno ;
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    } else {
      lua_pushinteger ( L, i ) ;
      return 1 ;
    }
  }

  return 0 ;
}

static int Lpoll_input_fd ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;
    struct pollfd pfd ;

    pfd . events = POLLIN ;
    pfd . fd = i ;
    e = luaL_optinteger ( L, 2, 0 ) ;
    i = poll ( & pfd, 1, e ) ;

    if ( 0 > i ) {
      i = errno ;
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    } else if ( 0 < i ) {
      lua_pushinteger ( L, 1 ) ;
      /* check the pfd . revents field */
      return 1 ;
    } else if ( 0 == i ) {
      lua_pushinteger ( L, 0 ) ;
      return 1 ;
    }
  }

  return 0 ;
}

static int Lredirio ( lua_State * L )
{
  int i = 0 ;
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) { i = redirio ( path ) ; }
  lua_pushboolean ( L, i ) ;

  return 1 ;
}

/* wrapper function for the isatty syscall */
static int Sisatty ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i, j ;

    for ( i = 1 ; n >= i ; ++ i ) {
      j = luaL_checkinteger ( L, i ) ;

      if ( 0 > j ) {
        lua_pushboolean ( L, 0 ) ;
        return 1 ;
        break ;
      } else if ( 0 == isatty ( j ) ) {
        j = errno ;
        lua_pushboolean ( L, 0 ) ;

        if ( 0 < j ) {
          lua_pushinteger ( L, j ) ;
          return 2 ;
        }

        lua_pushboolean ( L, 0 ) ;
        return 1 ;
        break ;
      }
    }

    lua_pushboolean ( L, 1 ) ;
    return 1 ;
  }

  return luaL_error ( L, "expected non negative integer args" ) ;
}

/* wrapper function for the ttyname syscall */
static int Sttyname ( lua_State * L )
{
  int e = 0, i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 > i ) { return 0 ; }
  else if ( isatty ( i ) ) {
    const char * name = ttyname ( i ) ;
    if ( name && * name ) {
      (void) lua_pushstring ( L, name ) ;
      return 1 ;
    }
  }

  e = errno ;
  lua_pushnil ( L ) ;
  lua_pushinteger ( L, e ) ;
  return 2 ;
}

/* get the name of the controlling tty of the calling process */
static int Sctermid ( lua_State * L )
{
  char cterm [ 1 + L_ctermid ] = { 0 } ;

  (void) ctermid ( cterm ) ;
  (void) lua_pushstring ( L, cterm ) ;

  return 1 ;
}

/* set the controlling tty of the calling process */
static int Sctty ( lua_State * L )
{
  int e = 0, r = 0, fd = -1, steal = 0 ;
  const char * path = NULL ;
  const int n = lua_gettop ( L ) ;

  if ( ( 1 > n ) || NULL == ( path = luaL_checkstring ( L, 1 ) ) )
  {
    lua_pushinteger ( L, -1 ) ;
    return 1 ;
  }

  if ( 1 < n ) {
    if ( lua_isboolean ( L, 2 ) ) {
      steal = lua_toboolean ( L, 2 ) ;
    } else if ( lua_isinteger ( L, 2 ) ) {
      steal = lua_tointeger ( L, 2 ) ;
    }
  }

  fd = open ( path, O_RDONLY ) ;
  e = errno ;
  if ( 0 > fd ) {
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

#ifdef OSLinux
  r = ioctl ( fd, TIOCSCTTY, steal ) ;
  e = errno ;
  lua_pushinteger ( L, r ) ;
  lua_pushinteger ( L, e ) ;
  return 2 ;
#else
  lua_pushinteger ( L, -1 ) ;
  return 1 ;
#endif
}

/*
 * bindings for some stdio functions
 */

/* fflush(3) ALL open output (FILE) streams */
static int Lflushallout ( lua_State * L )
{
  (void) fflush ( NULL ) ;
  return 0 ;
}

/* just calls puts(3) */
static int Sputs ( lua_State * L )
{
  const char * msg = luaL_checkstring ( L, 1 ) ;

  if ( msg && * msg ) { (void) puts ( msg ) ; }
  return 0 ;
}

/* get the fd corresponding to a Lua file "object" */
static int Lfileno ( lua_State * L )
{
  /* FILE * fp = * (FILE **) luaL_checkudata ( L, 1, LUA_FILEHANDLE ) ; */
  luaL_Stream * p = (luaL_Stream *) luaL_checkudata ( L, 1, LUA_FILEHANDLE ) ;

  if ( p && ( p -> f ) ) {
    int i = fileno ( p -> f ) ;

    if ( 0 > i ) {
      i = errno ;
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    } else {
      lua_pushinteger ( L, i ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* helper function that closes a Lua file "object".
 * this function is adapted from Lua 5.3's liolib.c
 */
static int Lstdio_fclose ( lua_State * L )
{
  luaL_Stream * p = (luaL_Stream *) luaL_checkudata ( L, 1, LUA_FILEHANDLE ) ;

  if ( p && ( p -> f ) ) {
    int i = fclose ( p -> f ) ;

    if ( i ) {
      i = errno ;
      lua_pushnil ( L ) ;
      (void) lua_pushstring ( L, strerror ( i ) ) ;
      return 2 ;
    } else {
      lua_pushboolean ( L, true ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* create a Lua file "object" from a given file descriptor */
static int Lfdopen ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  const char * mode = luaL_checkstring ( L, 2 ) ;

  luaL_argcheck ( L, 0 <= i, 1, "fd argument must not be negative" ) ;
  if ( ( 0 <= i ) && mode && * mode ) {
    FILE * fp = fdopen ( i, mode ) ;

    if ( fp ) {
      luaL_Stream * p = (luaL_Stream *)
        lua_newuserdata ( L, sizeof ( luaL_Stream ) ) ;

      if ( p ) {
        (void) luaL_getmetatable ( L, LUA_FILEHANDLE ) ;
        lua_setmetatable ( L, -2 ) ;
        p -> closef = Lstdio_fclose ;
        p -> f = fp ;
        return 1 ;
      }
    } else {
      i = errno ;
      return luaL_error ( L, "fdopen failed: %s", strerror ( i ) ) ;
    }
  }

  return 0 ;
}

#if defined (OSLinux)
static int Smemfd_create ( lua_State * L )
{
#  if defined (SYS_memfd_create)
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    unsigned int f = luaL_optinteger ( L, 2, 0 ) ;
    long int i = syscall ( SYS_memfd_create, path, f ) ;

    if ( 0 > i ) {
      i = errno ;
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    } else {
      lua_pushinteger ( L, i ) ;
      return 1 ;
    }
  }
#  endif

  return 0 ;
}
#endif

static int Lreset_term ( lua_State * L )
{
  const int fd = luaL_optinteger ( L, 1, 0 ) ;

  if ( 0 <= fd ) {
    const int i = reset_term ( fd ) ;

    if ( 0 > i ) {
      const int e = errno ;
      lua_pushinteger ( L, i ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    lua_pushinteger ( L, i ) ;
    return 1 ;
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

/* change the active virtual terminal */
static int Lchvt ( lua_State * L )
{
#if defined (OSLinux)
  int e = 0, i = luaL_optinteger ( L, 1, 1 ) ;

  i = ( 0 < i && 13 > i ) ? i : 1 ;
  i = chvt ( i ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( 0 > i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
#else
  return 0 ;
#endif
}

