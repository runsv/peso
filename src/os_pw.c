/*
 * functions to access the system passwd and group databases
 */

/* helper functions */
static int pwt ( lua_State * L, struct passwd * p )
{
  if ( NULL == p ) { return 0 ; }

  lua_newtable ( L ) ;

  (void) lua_pushliteral ( L, "uid" ) ;
  lua_pushinteger ( L, p -> pw_uid ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "gid" ) ;
  lua_pushinteger ( L, p -> pw_gid ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "name" ) ;
  (void) lua_pushstring ( L, p -> pw_name ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "passwd" ) ;
  (void) lua_pushstring ( L, p -> pw_passwd ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "gecos" ) ;
  (void) lua_pushstring ( L, p -> pw_gecos ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "dir" ) ;
  (void) lua_pushstring ( L, p -> pw_dir ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "shell" ) ;
  (void) lua_pushstring ( L, p -> pw_shell ) ;
  lua_rawset ( L, -3 ) ;

  return 1 ;
}

static int grt ( lua_State * L, struct group * p )
{
  char ** mp = NULL ;

  if ( NULL == p ) { return 0 ; }

  lua_newtable ( L ) ;

  (void) lua_pushliteral ( L, "gid" ) ;
  lua_pushinteger ( L, p -> gr_gid ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "name" ) ;
  (void) lua_pushstring ( L, p -> gr_name ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "passwd" ) ;
  (void) lua_pushstring ( L, p -> gr_passwd ) ;
  lua_rawset ( L, -3 ) ;

  mp = p -> gr_mem ;

  if ( mp && * mp && ** mp ) {
    int i ;

    (void) lua_pushliteral ( L, "members" ) ;
    lua_newtable ( L ) ;

    for ( i = 0 ; mp [ i ] && mp [ i ] [ 0 ] ; ++ i ) {
      (void) lua_pushstring ( L, mp [ i ] ) ;
      lua_rawseti ( L, -2, 1 + i ) ;
    }

    lua_rawset ( L, -3 ) ;
  }

  return 1 ;
}

/*
 * access the system passwd data base
 */

static int Sgetpwuid ( lua_State * L )
{
  const uid_t id = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;
  struct passwd * pw = getpwuid ( id ) ;

  if ( NULL == pw ) {
    const int e = errno ;
    return luaL_error ( L, "getpwuid( %d ) failed: %s (errno %d)",
      id, strerror ( e ), e ) ;
  }

  return pwt ( L, pw ) ;
}

static int Sgetpwnam ( lua_State * L )
{
  const char * user = luaL_checkstring ( L, 1 ) ;

  if ( user && * user ) {
    struct passwd * pw = getpwnam ( user ) ;

    if ( NULL == pw ) {
      const int e = errno ;
      return luaL_error ( L, "getpwnam( %s ) failed: %s (errno %d)",
        user, strerror ( e ), e ) ;
    }

    return pwt ( L, pw ) ;
  }

  return luaL_argerror ( L, 1, "invalid user name" ) ;
}

static int Lgetuser ( lua_State * L )
{
  const char * user = luaL_checkstring ( L, 1 ) ;

  if ( user && * user ) {
    lua_pushinteger ( L, getuser ( user ) ) ;
    return 1 ;
  }

  return luaL_argerror ( L, 1, "invalid user name" ) ;
}

/* get user info from the passwd db */
static int getpwu ( lua_State * L )
{
  int e = 0 ;
  struct passwd * p = NULL ;

  if ( lua_isinteger( L, 1 ) ) {
    p = getpwuid( (lua_Unsigned) lua_tointeger( L, 1 ) ) ;
  } else if ( lua_isstring( L, 1 ) ) {
    const char * user = lua_tostring( L, 1 ) ;
    if ( NULL == user ) { lua_pushnil( L ) ; return 1 ; }
    p = getpwnam( user ) ;
  } else {
    lua_pushnil( L ) ;
    return 1 ;
  }

  e = errno ;

  if ( p ) {
    lua_newtable( L ) ;
    lua_pushinteger( L, p -> pw_uid ) ;
    lua_pushinteger( L, p -> pw_gid ) ;
    (void) lua_pushstring( L, p -> pw_name ) ;
    lua_setfield( L, -2, "username" ) ;
    (void) lua_pushstring( L, p -> pw_passwd ) ;
    lua_setfield( L, -2, "passwd" ) ;
    (void) lua_pushstring( L, p -> pw_gecos ) ;
    lua_setfield( L, -2, "gecos" ) ;
    (void) lua_pushstring( L, p -> pw_dir ) ;
    lua_setfield( L, -2, "homedir" ) ;
    (void) lua_pushstring( L, p -> pw_shell ) ;
    lua_setfield( L, -2, "shell" ) ;
  } else {
    lua_pushnil( L ) ;
    lua_pushinteger( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/*
 * access the system group data base
 */

static int Sgetgrgid ( lua_State * L )
{
  const gid_t id = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;
  struct group * grp = getgrgid ( id ) ;

  if ( NULL == grp ) {
    const int e = errno ;
    return luaL_error ( L, "getgrgid( %d ) failed: %s (errno %d)",
      id, strerror ( e ), e ) ;
  }

  return grt ( L, grp ) ;
}

static int Sgetgrnam ( lua_State * L )
{
  const char * name = luaL_checkstring ( L, 1 ) ;

  if ( name && * name ) {
    struct group * grp = getgrnam ( name ) ;

    if ( NULL == grp ) {
      const int e = errno ;
      return luaL_error ( L, "getgrnam( %s ) failed: %s (errno %d)",
        name, strerror ( e ), e ) ;
    }

    return grt ( L, grp ) ;
  }

  return luaL_argerror ( L, 1, "invalid group name" ) ;
}

static int Lgetgroup ( lua_State * L )
{
  const char * gname = luaL_checkstring ( L, 1 ) ;

  if ( gname && * gname ) {
    lua_pushinteger ( L, getgroup ( gname ) ) ;
    return 1 ;
  }

  return luaL_argerror ( L, 1, "invalid group name" ) ;
}

/* get group info from the group db */
static int getgre ( lua_State * L )
{
  int e = 0 ;
  struct group * gp = NULL ;

  if ( lua_isinteger( L, 1 ) ) {
    gp = getgrgid( (lua_Unsigned) lua_tointeger( L, 1 ) ) ;
  } else if ( lua_isstring( L, 1 ) ) {
    const char * grp = lua_tostring( L, 1 ) ;
    if ( NULL == grp ) { lua_pushnil( L ) ; return 1 ; }
    gp = getgrnam( grp ) ;
  } else {
    lua_pushnil( L ) ;
    return 1 ;
  }

  e = errno ;

  if ( gp ) {
    lua_newtable( L ) ;
    lua_pushinteger( L, gp -> gr_gid ) ;
    (void) lua_pushstring( L, gp -> gr_name ) ;
    lua_setfield( L, -2, "groupname" ) ;
    (void) lua_pushstring( L, gp -> gr_passwd ) ;
    lua_setfield( L, -2, "passwd" ) ;
    if ( gp -> gr_mem [ 0 ] ) {
      int i ;
      lua_newtable( L ) ;
      for ( i = 0 ; gp -> gr_mem [ i ] ; ++ i ) {
        (void) lua_pushstring( L, gp -> gr_mem [ i ] ) ;
        lua_rawseti( L, -2, 1 + i ) ;
      }
      lua_setfield( L, -2, "members" ) ;
    }
  } else {
    lua_pushnil( L ) ;
    lua_pushinteger( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

