/*
 * wrapper functions to environment related functions
 */

/* wrapper function for clearenv that clears the current
 * process' environment
 */
static int Sclearenv ( lua_State * L )
{
/* without the use of clearenv () :
  extern char ** environ ;
  environ = NULL ;
*/
  lua_pushinteger ( L, clearenv () ) ;
  return 1 ;
}

/* wrapper function for unsetenv */
static int Sunsetenv ( lua_State * L )
{
  const char * str = luaL_checkstring ( L, 1 ) ;

  if ( str && * str ) {
    const int i = unsetenv ( str ) ;
    const int e = errno ;

    lua_pushinteger ( L, i ) ;
    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper function for getenv */
static int Sgetenv ( lua_State * L )
{
  const char * var = luaL_checkstring ( L, 1 ) ;

  if ( var && * var ) {
    char * str = getenv ( var ) ;

    if ( str && * str ) {
      (void) lua_pushstring ( L, str ) ;
      return 1 ;
    }
  }

  return 0 ;
}

#if defined (__GLIBC__) && defined (_GNU_SOURCE)
static int Ssecure_getenv ( lua_State * L )
{
  const char * var = luaL_checkstring ( L, 1 ) ;

  if ( var && * var ) {
    char * str = getenv ( var ) ;

    if ( str && * str ) {
      (void) lua_pushstring ( L, str ) ;
      return 1 ;
    }
  }

  return 0 ;
}
#endif

/* wrapper function for setenv */
static int Ssetenv ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    const char * var = luaL_checkstring ( L, 1 ) ;
    const char * str = luaL_checkstring ( L, 2 ) ;

    if ( var && str && * var ) {
      int e = 0, i = 3 ;

      if ( ( 2 < n ) && lua_isboolean ( L, 1 ) ) {
        i = lua_toboolean ( L, 3 ) ;
      }

      i = setenv ( var, str, i ) ;
      e = errno ;
      lua_pushinteger ( L, i ) ;

      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    }
  }

  return 0 ;
}

/* wrapper function for putenv
 * Beware: the given string has to be strdup()ed
 * (since it will be popped of the Lua stack when the function returns)
 * and the copy will not be free()ed by us !! Possible memory leaks !!
 */
static int Sputenv ( lua_State * L )
{
  const char * str = luaL_checkstring ( L, 1 ) ;

  if ( str && * str ) {
    int i ;

    /* scan the string for the (first) occurence of '=' */
    for ( i = 0 ; str [ i ] && ( '=' != str [ i ] ) ; ++ i ) ;

    if ( 0 < i ) {
      char * str2 = strdup ( str ) ;
      int e = errno ;

      if ( str2 ) {
        i = putenv ( str2 ) ;
        e = errno ;
      } else { i = -3 ; }

      lua_pushinteger ( L, i ) ;
      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    }
  }

  return 0 ;
}

/* function that returns a table containing the whole environment */
static int Lget_environ ( lua_State * L )
{
  extern char ** environ ;

  if ( environ && * environ && ** environ ) {
    int i, j ;
    char * s = NULL ;

    /* this table will hold the environment variables as keys */
    lua_newtable ( L ) ;

    for ( i = 0 ; environ [ i ] && environ [ i ] [ 0 ] ; ++ i ) {
      s = environ [ i ] ;
      for ( j = 0 ; '\0' != s [ j ] && '=' != s [ j ] ; ++ j ) ;
      if ( 0 < j && '=' == s [ j ] ) {
        (void) lua_pushlstring ( L, s, j ) ;
        if ( '\0' == s [ 1 + j ] ) {
          (void) lua_pushliteral ( L, "" ) ;
        } else {
          (void) lua_pushstring ( L, 1 + j + s ) ;
        }
        lua_rawset ( L, -3 ) ;
      }
    } /* end for */

    /* return the created new table as result to caller */
    return 1 ;
  }

  return 0 ;
}

/*
static void freeEnv ( char ** envp )
{
  if ( envp && * envp ) {
    int i ;

    for ( i = 0 ; envp [ i ] ; ++ i ) { free ( envp [ i ] ) ; }
    free ( envp ) ;
  }
}
*/

static int mkenv ( const char ce, lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  /* clear the environment first ? */
  if ( ce ) { (void) clearenv () ; }
  if ( 0 < n ) {
    int i, e = 0, j = 0, r = 0 ;
    const char * name = NULL ;
    const char * value = NULL ;

    e = i = j = r = 0 ;
    /* see if the first argument is a Lua table or string */
    if ( lua_isstring ( L, 1 ) ) {
      for ( i = 1 ; 1 + i <= n ; i += 2 ) {
        if ( lua_isstring ( L, i ) ) {
          name = lua_tostring ( L, i ) ;
          value = lua_tostring ( L, 1 + i ) ;
          if ( name && value && * name && * value ) {
            if ( setenv ( name, value, 3 ) ) {
              e = errno ;
              -- r ;
            } else { ++ j ; }
          } else if ( name && value && * name ) {
            if ( setenv ( name, "", 3 ) ) {
              e = errno ;
              -- r ;
            } else { ++ j ; }
          }
        }
      }

      if ( r ) {
        lua_pushinteger ( L, r ) ;
        lua_pushinteger ( L, e ) ;
        return 2 ;
      } else if ( j ) {
        lua_pushinteger ( L, 0 ) ;
        return 1 ;
      }
    } else if ( lua_istable ( L, 1 ) ) {
      /* first (start) "key" for lua_next () */
      lua_pushnil ( L ) ;

      while ( lua_next ( L, 1 ) ) {
        if ( lua_isstring ( L, -2 ) && lua_isstring ( L, -1 ) ) {
          name = lua_tostring ( L, -2 ) ;
          value = lua_tostring ( L, -1 ) ;

          if ( name && value && * name && * value ) {
            if ( setenv ( name, value, 3 ) ) {
              e = errno ;
              -- r ;
            } else { ++ j ; }
          } else if ( name && value && * name ) {
            if ( setenv ( name, "", 3 ) ) {
              e = errno ;
              -- r ;
            } else { ++ j ; }
          }

          /* removes the value.
           * keeps the key on the stack for the next iteration.
           */
          lua_pop ( L, 1 ) ;
        } else {
          return 0 ;
          break ;
        } /* end if */
      } /* end while */

      if ( r ) {
        lua_pushinteger ( L, r ) ;
        lua_pushinteger ( L, e ) ;
        return 2 ;
      } else if ( j ) {
        lua_pushinteger ( L, 0 ) ;
        return 1 ;
      }
    }
  }

  return 0 ;
}

static int Lnewenv ( lua_State * L )
{
  return mkenv ( 3, L ) ;
}

static int Laddenv ( lua_State * L )
{
  return mkenv ( 0, L ) ;
}

