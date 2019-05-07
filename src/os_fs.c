/*
 * Wrapper and helper functions for (u)mount(2), swapo(n,ff)(2),
 * and file system related syscalls.
 */

/* helper functions that pass the result of successful stat(v)fs() calls
 * back to Lua
 */
static int res_statvfs ( lua_State * const L, struct statvfs * const stp )
{
  lua_newtable ( L ) ;

  (void) lua_pushliteral ( L, "fsid" ) ;
  lua_pushinteger ( L, stp -> f_fsid ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "flag" ) ;
  lua_pushinteger ( L, stp -> f_flag ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "namemax" ) ;
  lua_pushinteger ( L, stp -> f_namemax ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "bsize" ) ;
  lua_pushinteger ( L, stp -> f_bsize ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "frsize" ) ;
  lua_pushinteger ( L, stp -> f_frsize ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "blocks" ) ;
  lua_pushinteger ( L, stp -> f_blocks ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "bfree" ) ;
  lua_pushinteger ( L, stp -> f_bfree ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "bavail" ) ;
  lua_pushinteger ( L, stp -> f_bavail ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "files" ) ;
  lua_pushinteger ( L, stp -> f_files ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "ffree" ) ;
  lua_pushinteger ( L, stp -> f_ffree ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "favail" ) ;
  lua_pushinteger ( L, stp -> f_favail ) ;
  lua_rawset ( L, -3 ) ;

  return 1 ;
}

static int res_statfs ( lua_State * const L, struct statfs * const stp )
{
  lua_newtable ( L ) ;

  return 1 ;
}

/* bindings for the (f)stat(v)fs() syscalls */
static int Sstatvfs ( lua_State * const L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    struct statvfs st ;

    if ( statvfs ( path, & st ) ) {
      const int e = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
      return res_statvfs ( L, & st ) ;
    }
  }

  return luaL_argerror ( L, 1, "invalid path" ) ;
}

static int Sfstatvfs ( lua_State * const L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    struct statvfs st ;

    if ( fstatvfs ( fd, & st ) ) {
      const int e = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
      return res_statvfs ( L, & st ) ;
    }
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

static int Sstatfs ( lua_State * const L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    struct statfs st ;

    if ( statfs ( path, & st ) ) {
      const int e = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
      return res_statfs ( L, & st ) ;
    }
  }

  return luaL_argerror ( L, 1, "invalid path" ) ;
}

static int Sfstatfs ( lua_State * const L )
{
  const int fd = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= fd ) {
    struct statfs st ;

    if ( fstatfs ( fd, & st ) ) {
      const int e = errno ;
      lua_pushnil ( L ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
      return res_statfs ( L, & st ) ;
    }
  }

  return luaL_argerror ( L, 1, "invalid fd" ) ;
}

/* helper function for the umount(2) wrapper functions */
static int Lumountit ( lua_State * const L, const int f )
{
#if defined (OSLinux)
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;
    const char * path = NULL ;

    for ( i = 1 ; n >= i ; ++ i ) {
      path = luaL_checkstring ( L, i ) ;

      if ( path && * path ) {
        if ( f ? umount2 ( path, f ) : umount ( path ) ) {
          i = errno ;
          return luaL_error ( L, "umount( %s ) failed: %s (errno %d)",
            path, strerror ( i ), i ) ;
        }
      } else {
        return luaL_argerror ( L, i, "invalid mount point" ) ;
      }
    }

    return 0 ;
  }

  return luaL_error ( L, "mount point required" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper function for the umount(2) syscall */
static int Sumount ( lua_State * const L )
{
#if defined (OSLinux)
  return Lumountit ( L, 0 ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper function for the mount(2) syscall */
static int Smount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * source = luaL_checkstring ( L, 1 ) ;
  const char * target = luaL_checkstring ( L, 2 ) ;
  const char * fstype = luaL_checkstring ( L, 3 ) ;
  unsigned long f = (lua_Unsigned) luaL_checkinteger ( L, 4 ) ;
  const char * data = luaL_checkstring ( L, 5 ) ;

  if ( source && target && fstype && data
    && * source && * target && * fstype && * data )
  {
    return res_zero ( L, mount ( source, target, fstype, f, data ) ) ;
  }
#endif

  return 0 ;
}

static int Lremount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * target = luaL_checkstring ( L, 1 ) ;
  unsigned long f = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;
  const char * data = luaL_optstring ( L, 3, NULL ) ;

  if ( target && * target ) {
    if ( mount ( NULL, target, NULL, MS_REMOUNT | f,
        ( data && * data ) ? data : NULL ) )
    {
      const int e = errno ;
      return luaL_error ( L, "remount( %s ) failed: %s (errno %d)",
        target, strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_argerror ( L, 1, "invalid path" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lremount_ro ( lua_State * const L )
{
#if defined (OSLinux)
  const char * target = luaL_checkstring ( L, 1 ) ;
  unsigned long f = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;
  const char * data = luaL_optstring ( L, 3, NULL ) ;

  if ( target && * target ) {
    if ( mount ( NULL, target, NULL, MS_REMOUNT | MS_RDONLY | f,
        ( data && * data ) ? data : NULL ) )
    {
      const int e = errno ;
      return luaL_error ( L, "remount( %s ) read-only failed: %s (errno %d)",
        target, strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_argerror ( L, 1, "invalid path" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lmove_mount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * source = luaL_checkstring ( L, 1 ) ;
  const char * target = luaL_checkstring ( L, 2 ) ;

  if ( source && target && * source && * target ) {
    return res_zero ( L, mount ( source, target, NULL, MS_MOVE, NULL ) ) ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lbind_mount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * source = luaL_checkstring ( L, 1 ) ;
  const char * target = luaL_checkstring ( L, 2 ) ;

  if ( source && target && * source && * target ) {
    return res_zero ( L, mount ( source, target, NULL, MS_BIND, NULL ) ) ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lrec_bind_mount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * source = luaL_checkstring ( L, 1 ) ;
  const char * target = luaL_checkstring ( L, 2 ) ;

  if ( source && target && * source && * target ) {
    return res_zero ( L,
      mount ( source, target, NULL, MS_BIND | MS_REC, NULL ) ) ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lchange_mount ( lua_State * const L )
{
#if defined (OSLinux)
  const char * target = luaL_checkstring ( L, 1 ) ;
  unsigned long f = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;

  if ( target && * target ) {
    return res_zero ( L, mount ( NULL, target, NULL, f, NULL ) ) ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper function for the swapoff syscall */
static int Sswapoff ( lua_State * const L )
{
#if defined (OSLinux)
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;
    const char * path = NULL ;

    for ( i = 1 ; n >= i ; ++ i ) {
      path = luaL_checkstring ( L, i ) ;

      if ( path && * path ) {
        if ( swapoff ( path ) ) {
          i = errno ;
          return luaL_error ( L, "swapoff( %s ) failed: %s (errno %d)",
            path, strerror ( i ), i ) ;
        }
      } else {
        return luaL_argerror ( L, i, "invalid file path" ) ;
      }
    }

    return 0 ;
  }

  return luaL_error ( L, "path argument required" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper function for the swapon(2) syscall */
static int Sswapon ( lua_State * const L )
{
#if defined (OSLinux)
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    int i = luaL_optinteger ( L, 2, 0 ) ;
    return res_zero ( L, swapon ( path, i ) ) ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* see if a given directory is a mountpoint */
static int Lmountpoint ( lua_State * const L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    lua_pushboolean ( L, is_mount_point ( path ) ) ;
  } else {
    lua_pushboolean ( L, 0 ) ;
  }

  return 1 ;
}

/* see if a given directory is a mountpoint using the mtab file */
static int Lmtab_mount_point ( lua_State * const L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;
  const char * mtab =
#if defined (OSLinux)
    luaL_optstring ( L, 2, "/proc/self/mounts" )
#else
    luaL_checkstring ( L, 2 )
#endif
    ;

  if ( path && mtab && * path && * mtab ) {
    lua_pushboolean ( L, mtab_mount_point ( path, mtab ) ) ;
  } else {
    lua_pushboolean ( L, 0 ) ;
  }

  return 1 ;
}

/* returns all pseudo fs currently known to the (Linux) kernel */
static int Lget_pseudofs ( lua_State * const L )
{
#if defined (OSLinux)
  FILE * fp = NULL ;
  const char * path = luaL_optstring ( L, 1, NULL ) ;

  path = ( path && * path ) ? path : "/proc/filesystems" ;
  fp = fopen ( path, "r" ) ; 

  if ( fp ) {
    char * s = NULL ;
    char * s2 = NULL ;
    char buf [ 64 ] = { 0 } ;

    /* create a new Lua table for the results */
    lua_newtable ( L ) ;

    while ( NULL != fgets ( buf, sizeof ( buf ) - 1, fp ) ) {
      s = strtok_r ( buf, " \t\n", & s2 ) ;

      if ( s && * s && 0 == strncmp ( "nodev", s, 5 ) ) {
        s = strtok_r ( NULL, " \t\n", & s2 ) ;

        if ( s && * s ) {
          (void) lua_pushstring ( L, s ) ;
          lua_pushboolean ( L, 1 ) ;
          lua_rawset ( L, -3 ) ;
        }
      }
    }

    (void) fclose ( fp ) ;
    /* returns the result table at the top of the stack */
    return 1 ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Lcgroup_level ( lua_State * const L )
{
#if defined (OSLinux)
  const char * path = luaL_optstring ( L, 1, "/proc/filesystems" ) ;

  if ( path && * path ) {
    int i = 0 ;
    FILE * fp = fopen ( path, "r" ) ;

    if ( fp ) {
      char * s = NULL ;
      char * s2 = NULL ;
      char buf [ 101 ] = { 0 } ;

      while ( fgets ( buf, sizeof ( buf ) - 1, fp ) ) {
        s = strtok_r ( buf, " \t\n", & s2 ) ;
        if ( s && 'n' == * s ) {
          s = strtok_r ( NULL, " \t\n", & s2 ) ;
          if ( s && 'c' == * s ) {
            if ( 0 == strcmp ( s, "cgroup2" ) ) { i = 2 ; }
            else if ( 2 > i && 0 == strcmp ( s, "cgroup" ) ) { i = 1 ; }
          }
        }
      }

      (void) fclose ( fp ) ;
    }

    lua_pushinteger ( L, i ) ;
    return 1 ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

