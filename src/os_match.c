/*
 * wrappers for pattern matching APIs and syscalls like
 * posix glob, fnmatch and regex
 */

/*
 * glob related functions
 */

/* wrapper function to the glob syscall */
static int Sglob ( lua_State * L )
{
  int r = 0 ;
  glob_t g ;
  const char * pat = luaL_checkstring( L, 1 ) ;

  if ( NULL == pat ) {
    lua_pushnil( L ) ;
    return 1 ;
  }

#ifdef __GLIBC__
  r = glob( pat, GLOB_PERIOD | GLOB_BRACE | GLOB_TILDE, NULL, & g ) ;
#else
  r = glob( pat, 0, NULL, & g ) ;
#endif

  if ( r ) {
    /* no match or error */
    lua_pushnil( L ) ;
  } else {
    /* found matches for pattern "pat" */
    int i ;
    /* push a fresh table onto the stack to hold the results */
    lua_newtable( L ) ;
    /* and fill it with the resulting matches (strings) */
    for ( i = 0 ; ( i < g . gl_pathc ) && g . gl_pathv [ i ] ; ++ i )
    {
      (void) lua_pushstring( L, g . gl_pathv [ i ] ) ;
      lua_rawseti( L, -2, 1 + i ) ;
    }
  }

  globfree( & g ) ;
  if ( r ) {
    lua_pushinteger( L, r ) ;
    return 2 ;
  }

  return 1 ;
}

/*
 * fnmatch related functions
 */

/* wrapper function for the fnmatch syscall */
static int Sfnmatch ( lua_State * L )
{
  int r = 0 ;
  const char * pat = luaL_checkstring( L, 1 ) ;
  const char * str = luaL_checkstring( L, 2 ) ;

  if ( ! ( pat && str ) ) {
    lua_pushboolean( L, 0 ) ;
    return 1 ;
  }

#ifdef __GLIBC__
  r = fnmatch( pat, str, FNM_EXTMATCH ) ;
#else
  r = fnmatch( pat, str, 0 ) ;
#endif

  if ( r ) {
    /* no match or another error occurred */
    lua_pushboolean( L, 0 ) ;
  } else {
    /* matched, ok */
    lua_pushboolean( L, 1 ) ;
  }

  return 1 ;
}

/*
 * wordexp related functions
 */

/* wrapper function for wordexp */
static int Swordexp ( lua_State * L )
{
  int r = 0 ;
  wordexp_t we ;
  const char * pat = luaL_checkstring( L, 1 ) ;

  if ( NULL == pat ) {
    lua_pushnil( L ) ;
    return 1 ;
  }

  r = wordexp( pat, & we, 0 ) ;

  if ( r ) {
    /* no match or error */
    lua_pushnil ( L ) ;
  } else {
    /* found matches for pattern "pat" */
    int i ;
    /* push a fresh table onto the stack to hold the results */
    lua_newtable ( L ) ;
    /* and fill it with the resulting matches (strings) */
    for ( i = 0 ; ( i < we . we_wordc ) && we . we_wordv [ i ] ; ++ i )
    {
      (void) lua_pushstring ( L, we . we_wordv [ i ] ) ;
      lua_rawseti ( L, -2, 1 + i ) ;
    }
  }

  wordfree ( & we ) ;
  if ( r ) {
    lua_pushinteger ( L, r ) ;
    return 2 ;
  }

  return 1 ;
}

/*
 * (POSIX) regex related functions
 */

/* upper limit of the number of saved regex subexpression matches */
#define NSUB	64

/* wrapper function that uses the posix regex API */
static int simple_regmatch ( lua_State * L )
{
  int r = 0 ;
  regex_t preg ;
  const char * re = luaL_checkstring ( L, 1 ) ;
  const char * str = luaL_checkstring ( L, 2 ) ;

  if ( 0 == ( re && str ) ) {
    lua_pushboolean ( L, 0 ) ;
    return 1 ;
  }

  r = regcomp ( & preg, re, REG_NOSUB | REG_EXTENDED ) ;

  if ( r ) {
    /* pattern failed to compile */
    char errbuf [ 128 ] = { 0 } ;
    r = regerror ( r, & preg, errbuf, 127 ) ;
    regfree ( & preg ) ;
    lua_pushboolean ( L, 0 ) ;
    (void) lua_pushstring ( L, errbuf ) ;
    return 2 ;
  }

  r = regexec ( & preg, str, 0, NULL, 0 ) ;
  regfree ( & preg ) ;

  if ( r ) {
    /* no match or error */
    lua_pushboolean ( L, 0 ) ;
  } else {
    /* pattern matched string */
    lua_pushboolean ( L, 1 ) ;
  }

  return 1 ;
}

/* regex wrapper function that returns (substring) matches */
/* TODO: complete it */
static int regmatch ( lua_State * L )
{
  int r = 0 ;
  regex_t preg ;
  const char * re = luaL_checkstring ( L, 1 ) ;
  const char * str = luaL_checkstring ( L, 2 ) ;

  if ( ! ( re && str ) ) {
    lua_pushboolean ( L, 0 ) ;
    return 1 ;
  }

  r = regcomp ( & preg, re, REG_EXTENDED ) ;

  if ( r ) {
    /* pattern failed to compile */
    char errbuf [ 128 ] = { 0 } ;
    r = regerror ( r, & preg, errbuf, 127 ) ;
    regfree ( & preg ) ;
    lua_pushboolean ( L, 0 ) ;
    (void) lua_pushstring ( L, errbuf ) ;
    return 2 ;
  } else {
    regmatch_t pmatch ;
    r = regexec ( & preg, str, 1, & pmatch, 0 ) ;
    regfree ( & preg ) ;

    if ( r ) {
      /* no match or error */
      lua_pushboolean ( L, 0 ) ;
      return 1 ;
    } else if ( 0 <= pmatch . rm_so ) {
      /* pattern matched string */
      luaL_Buffer b ;
      lua_pushboolean ( L, 1 ) ;
      luaL_buffinit ( L, & b ) ;
      luaL_addlstring ( & b, str + pmatch . rm_so,
        pmatch . rm_eo - pmatch . rm_so ) ;
      luaL_addchar ( & b, '\0' ) ;
      luaL_pushresult ( & b ) ;
      return 2 ;
#if 0
      int i, j = 0 ;
      lua_pushboolean ( L, 1 ) ;
      for ( i = 0 ; i < 16 ; ++ i ) {
        if ( 0 <= pmatch [ i ] . rm_so ) {
          ++ j ;
          (void) lua_pushstring ( L, str ) ;
        }
      } /* end for */
      lua_pushinteger ( L, j ) ;
#endif
    } else {
      /* pattern matched string */
      lua_pushboolean ( L, 1 ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* searches a given pattern in a file (like grep -q) */
static int file_regmatch ( lua_State * L )
{
  int r = 0, res = 0 ;
  FILE * fp = NULL ;
  regex_t re ;
  char buf [ 128 ] = { 0 } ;
  const char * file = luaL_checkstring ( L, 1 ) ;
  const char * pat = luaL_checkstring ( L, 2 ) ;

  fp = fopen ( file, "r" ) ;
  if ( NULL == fp ) {
    lua_pushboolean ( L, 0 ) ;
    return 1 ;
  }

  r = regcomp( & re, pat, REG_NOSUB | REG_EXTENDED ) ;
  if ( r ) {
    /* pattern failed to compile */
    /*
    if ( fclose( fp ) ) {
      perror( "fclose failed" ) ;
    }
    */
    r = fclose ( fp ) ;
    r = regerror ( r, & re, buf, 127 ) ;
    regfree ( & re ) ;
    lua_pushboolean ( L, 0 ) ;
    (void) lua_pushstring ( L, buf ) ;
    return 2 ;
  } else {
    size_t s ;

    while ( fgets ( buf, 127, fp ) ) {
      /* some /proc files have \0 separated content so we have to
       * loop through the whole buffer buf */
      s = 0 ;
      do {
        if ( 0 == regexec ( & re, buf + s, 0, NULL, 0 ) ) {
          res = 1 ;
          goto found ;
        }

        s += 1 + str_len ( buf ) ;
        /* len is the size of allocated buffer and we don't
         * want call regexec BUFSIZE times. find next str */
        while ( ( 126 > s ) && '\0' == buf [ s ] ) { ++ s ; }

      } while ( 126 > s ) ;
    }
  }

  res = 0 ;

found :
  r = fclose ( fp ) ;
  regfree ( & re ) ;
  lua_pushboolean ( L, res ) ;

  return 1 ;
}

/* matches a given POSIX regex pattern against a given string */
static int rex_match ( lua_State * L, const char * pat,
  const char * str, int f )
{
  if ( pat && str && * pat && * str ) {
    int i = 0 ;
    regex_t re ;

    /* always use extended POSIX regex matching */
    f |= REG_EXTENDED ;
    /* zero out the regex_t struct first so it can safely regfree()ed
     * later (even if regcomp() failed to compile it)
     */
    (void) memset ( & re, 0, sizeof ( regex_t ) ) ;
    i = regcomp ( & re, pat, f ) ;

    if ( 0 == i ) {
      regmatch_t pmatch [ 1 + NSUB ] ;

      /* zero out the pattern match array first */
      (void) memset ( pmatch, 0, sizeof ( pmatch ) ) ;
      i = regexec ( & re, str, 1 + NSUB, pmatch, 0 ) ;

      if ( 0 == i ) {
        regfree ( & re ) ;
        lua_pushboolean ( L, 1 ) ;
        return 1 ;
      }
    }

    lua_pushnil ( L ) ;
    {
      char buf [ 128 ] = { 0 } ;
      (void) regerror ( i, & re, buf, sizeof ( buf ) - 1 ) ;
      /* is that ok for a non compiling pattern ? */
      regfree ( & re ) ;
      (void) lua_pushstring ( L, buf ) ;
    }
    return 2 ;
  }

  return 0 ;
}

/* case insensitive regex match */
static int Lncgrep ( lua_State * L )
{
  if ( 1 < lua_gettop ( L ) ) {
    const char * pat = luaL_checkstring ( L, 1 ) ;
    const char * str = luaL_checkstring ( L, 2 ) ;

    if ( pat && str && * pat && * str ) {
      return rex_match ( L, pat, str, REG_EXTENDED | REG_ICASE ) ;
    } else {
      return luaL_error ( L, "invalid string args" ) ;
    }
  } else {
    return luaL_error ( L, "not enough args" ) ;
  }

  return 0 ;
}

static int Lgrep ( lua_State * L )
{
  int i = lua_gettop ( L ) ;

  if ( 1 < i ) {
    const char * pat = luaL_checkstring ( L, 1 ) ;
    const char * str = luaL_checkstring ( L, 2 ) ;

    if ( pat && str && * pat && * str ) {
      i = 0 ;
      i = luaL_optinteger ( L, 1, 0 ) ;
      i |= REG_EXTENDED ;
      return rex_match ( L, pat, str, i ) ;
    } else {
      return luaL_error ( L, "invalid string args" ) ;
    }
  } else {
    return luaL_error ( L, "not enough args" ) ;
  }

  return 0 ;
}

