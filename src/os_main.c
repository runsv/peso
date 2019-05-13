/*
 * some posix bindings for Lua (as a Lua module)
 * wrappers to system calls for Lua 5.3+
 */

/*
include "common.h"
include "pesi.h"
*/

/* constant with an unique address to use as key in the Lua registry */
static const char Key = 'K' ;

/* helper function that parses a given integer literal string */
static lua_Integer str2i ( const int b, const char * const s )
{
  if ( ( 1 < b ) && ( 37 > b ) && s && * s ) {
    lua_Integer r = 0 ;
    int i = 1, j = str_len ( s ) ;

    while ( 0 < j && '\0' != s [ -- j ] ) {
      int c = s [ j ] ;

      if ( LUA_MAXINTEGER <= r ) {
        return r ;
      } else if ( ( 0 < c ) && isdigit ( c ) ) {
        c -= '0' ;
      } else if ( ( 0 < c ) && isalpha ( c ) && isascii ( c ) ) {
        c = 10 + tolower ( c ) - 'a' ;
      } else {
        continue ;
      }

      if ( 0 > c ) {
        return -1 ;
      } else if ( 0 == c ) {
        i *= b ;
      } else if ( 0 < c && b > c ) {
        c *= i ;

        if ( LUA_MAXINTEGER <= c + r ) { return r ; }

        r += c ;
        i *= b ;
      } else if ( b <= c ) {
        return -1 ;
      }
    }

    if ( 1 < i ) { return r ; }
  }

  return -1 ;
}

/*
 *          *@endptr = @str
 * Not a complete replacement for strtol yet, Does not process base prefixes,
 * nor +/- sign yet.
 *
 * - take the largest sequence of character that is in range of 0-@maxval
 * - will consume the minimum of @maxlen or @base digits in @maxval
 * - if there is not n valid characters for the base only the n-1 will be taken
 *   eg. for the sequence string 4z with base 16 only 4 will be taken as the
 *   hex number
static long strntol(const char *str, const char **endptr, int base, long maxval,
	     size_t n)
{
	long c, val = 0;

	if (base > 1 && base < 37) {
		for (; n && (c = chrtoi(*str, base)) != -1; str++, n--) {
			long tmp = (val * base) + c;
			if (tmp > maxval)
				break;
			val = tmp;
		}
	}

	if (endptr)
		*endptr = str;

	return val;
}
 */

/* helper functions for all other Lua wrapper functions */
/*
static int res0 ( lua_State * const L, const int r )
{
  if ( r ) {
    return luaL_error ( L, strerror ( errno ) ) ;
  }

  return 0 ;
}
*/

static int res_zero ( lua_State * const L, const int res )
{
  if ( res ) {
    const int e = errno ;
    lua_pushinteger ( L, res ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  lua_pushinteger ( L, 0 ) ;
  return 1 ;
}

static int res_lt ( lua_State * const L, const int min, const int res )
{
  if ( res < min ) {
    const int e = errno ;
    lua_pushinteger ( L, res ) ;
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  lua_pushinteger ( L, res ) ;
  return 1 ;
}

/*
static int res_neq0 ( lua_State * const L, const int res )
{
  return 0 ;
}

static int res_eq ( lua_State * const L, const int v, const int res )
{
  return 0 ;
}

static int res_neq ( lua_State * const L, const int v, const int res )
{
  return 0 ;
}
*/

static int arg_is_fd ( lua_State * const L, const int index )
{
  const int i = luaL_checkinteger ( L, index ) ;

  if ( 0 > i ) {
    return luaL_argerror ( L, index, "invalid fd" ) ;
  }

  return i ;
}

/* include the Lua wrapper functions */
#include "os_proc.c"
#include "os_time.c"
#include "os_svipc.c"
#include "os_file.c"
#include "os_io.c"
#include "os_env.c"
#include "os_match.c"
#include "os_pw.c"
#include "os_fs.c"
#include "os_reboot.c"
#if defined (OSsolaris) || defined (OSsunos5)
#  include "os_streams.c"
#endif
#include "os_net.c"
#include "os_utmp.c"
#include "os_utils.c"
#include "os_sig.c"
#ifdef SUPERVISORD
# include "os_super.c"
#endif

/* OS specific functions */
#if defined (OSLinux)
  /* Linux specific functions */
#  include "os_Linux.c"
#elif defined (OSfreebsd)
#endif

/*
 * integer bit ops
 */

/* bitwise (unary) negation */
static int Lbitneg ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;

    for ( i = 1 ; n >= i ; ++ i ) {
      lua_pushinteger ( L, ~ luaL_checkinteger ( L, i ) ) ;
      lua_replace ( L, i ) ;
    }

    return n ;
  }

  return luaL_error ( L, "integer arg required" ) ;
}

/* bitwise and for multiple integer args */
static int Lbitand ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    int i ;
    lua_Integer j = luaL_checkinteger ( L, 1 ) ;

    for ( i = 2 ; n >= i ; ++ i ) {
      j &= luaL_checkinteger ( L, i ) ;
    }

    lua_pushinteger ( L, j ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 integer args required" ) ;
}

static int Lbitor ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    int i ;
    lua_Integer j = luaL_checkinteger ( L, 1 ) ;

    for ( i = 2 ; n >= i ; ++ i ) {
      j |= luaL_checkinteger ( L, i ) ;
    }

    lua_pushinteger ( L, j ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 integer args required" ) ;
}

static int Lbitxor ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 1 < n ) {
    int i ;
    lua_Integer j = luaL_checkinteger ( L, 1 ) ;

    for ( i = 2 ; n >= i ; ++ i ) {
      j ^= luaL_checkinteger ( L, i ) ;
    }

    lua_pushinteger ( L, j ) ;
    return 1 ;
  }

  return luaL_error ( L, "2 integer args required" ) ;
}

static int Lbitlshift ( lua_State * const L )
{
  lua_pushinteger ( L,
    luaL_checkinteger ( L, 1 ) << luaL_checkinteger ( L, 2 ) ) ;
  return 1 ;
}

static int Lbitrshift ( lua_State * const L )
{
  lua_pushinteger ( L,
    luaL_checkinteger ( L, 1 ) >> luaL_checkinteger ( L, 2 ) ) ;
  return 1 ;
}

/* function that calculates sum and average of its args */
static int add ( lua_State * const L )
{
  int i, n = lua_gettop ( L ) ; /* number of arguments */
  lua_Number sum = 0.0 ;

  for ( i = 1 ; i <= n ; ++ i ) {
    if ( ! lua_isnumber ( L, i ) ) {
      lua_pushliteral ( L, "incorrect argument" ) ;
      lua_error ( L ) ;
    }
    sum += lua_tonumber ( L, i ) ;
  }

  lua_pushnumber ( L, sum / n ) ; /* first result */
  lua_pushnumber ( L, sum ) ; /* second result */
  return 2 ; /* number of results */
}

/* function that tries to get the specified subarray of a table
 * similar to string . sub(). it does the same that function
 * does for strings. so indexing starts at 1, not at 0.
 * the result is always a (possibly empty) table */
static int subarr ( lua_State * const L )
{
  int i = 1, j = 0, k = 1 ;
  size_t s = 0 ;

  /* was the required table passed as first arg ? */
  luaL_checktype ( L, 1, LUA_TTABLE ) ;
  /* get it's length without invoking metamethods */
  s = lua_rawlen ( L, 1 ) ;
  i = (int) luaL_checkinteger ( L, 2 ) ;
  j = (int) luaL_optinteger ( L, 3, s ) ;
  /* push a new empty result table onto the stack */
  lua_newtable ( L ) ;

  /* return that fresh empty table if the given table
   * is empty or no sequence */
  if ( 0 >= s ) {
    return 1 ;
  }
  /* translate given negative index args */
  if ( 0 > i ) {
    i += 1 + s ;
  }
  if ( 0 > j ) {
    j += 1 + s ;
  }
  /* ensure the first given index i (possibly after
   * translation) is at least 1 */
  if ( 1 > i ) {
    i = 1 ;
  }
  /* esnsure j is not greater than the table (raw) length s */
  if ( j > s ) {
    j = s ;
  }
  /* if the (corrected) i is greater than the (corrected) j
   * return the newly created empty table on top of the
   * stack as result */
  if ( i > j ) {
    return 1 ;
  }

  for ( k = i ; k <= j ; ++ k ) {
    /* the first argument (which is at stack index 1) was the given table */
    (void) lua_rawgeti ( L, 1, k ) ;
    /* now the element arg1 [ k ] is on the stack and we set index k
    ** of the new table above the old given one to it's value */
    lua_rawseti ( L, -2, k ) ;
  }

  return 1 ;
}

/* try to parse the given octal integer literal strings as integers */
static int Lstr2i ( lua_State * const L, const int b )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;

    for ( i = 1 ; n >= i ; ++ i ) {
      const char * const s = luaL_checkstring ( L, i ) ;

      if ( s && * s ) {
        const lua_Integer u = str2i ( b, s ) ;

        if ( 0 > u ) {
          return luaL_argerror ( L, i, "string is no integer literal for given base" ) ;
        }

        lua_pushinteger ( L, u ) ;
        lua_replace ( L, i ) ;
      } else {
        return luaL_argerror ( L, i, "illegal integer literal string" ) ;
      }
    }

    return n ;
  }

  return luaL_error ( L, "base %d integer literal string required", b ) ;
}

static int Lstr2int ( lua_State * const L )
{
  if ( 1 < lua_gettop ( L ) ) {
    const int b = (int) luaL_checkinteger ( L, 1 ) ;
    lua_pop ( L, 1 ) ;

    if ( 1 < b && 37 > b ) {
      return Lstr2i ( L, b ) ;
    }

    return luaL_error ( L, "illegal representation base %d", b ) ;
  }

  return luaL_error ( L, "representation base and integer literal string args required" ) ;
}

static int Lstr2bi ( lua_State * const L )
{
  return Lstr2i ( L, 2 ) ;
}

static int Lstr2oi ( lua_State * const L )
{
  return Lstr2i ( L, 8 ) ;
}

static int Lstr2xi ( lua_State * const L )
{
  return Lstr2i ( L, 16 ) ;
}

/* convert a string consisting of on octal integer literal
 * to an int. useful for handling unix modes since Lua lacks
 * octal integer literals.
 */
static int Loctintstr ( lua_State * const L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    int i ;
    unsigned int u ;
    const char * str = NULL ;

    for ( i = 1 ; n >= i ; ++ i ) {
      str = luaL_checkstring ( L, i ) ;

      if ( str && * str && ( 1 == sscanf ( str, "%o", & u ) ) ) {
        lua_pushinteger ( L, (lua_Unsigned) u ) ;
        lua_replace ( L, i ) ;
      } else {
        return luaL_argerror ( L, i, "invalid octal integer string" ) ;
      }
    }

    return n ;
  }

  return luaL_error ( L, "octal integer string required" ) ;
}

/* remove \n and anything after */
static int Lchomp ( lua_State * const L )
{
  const char * str = luaL_checkstring ( L, 1 ) ;

  if ( str && * str ) {
    char * s = (char *) str ;
    (void) chomp ( s ) ;

    if ( s && * s ) {
      (void) lua_pushstring ( L, s ) ;
      return 1 ;
    }
  }

  return 0 ;
}

/* returns error msg belonging to current errno or given int arg */
static int get_errno ( lua_State * const L )
{
  const int e = errno ;

  lua_pushinteger ( L, e ) ;
  (void) lua_pushstring ( L, strerror ( e ) ) ;

  return 2 ;
}

/* sets the current errno to given int arg or 0 */
static int Sstrerror ( lua_State * const L )
{
  const int e = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < e ) {
    char * str = strerror ( e ) ;

    if ( str && * str ) {
      (void) lua_pushstring ( L, str ) ;
    } else {
      (void) lua_pushfstring ( L, "unknown error code %d", e ) ;
    }

    return 1 ;
  }

  return luaL_argerror ( L, 1, "positive error code required" ) ;
}

static int Sstrerror_r ( lua_State * const L )
{
  const int e = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < e ) {
    int i ;
    char buf [ 200 ] = { 0 } ;
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
    char * const str = strerror_r ( e, buf, sizeof ( buf ) - 1 ) ;

    if ( str && * str ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }

    i = errno ;
#else
    i = strerror_r ( e, buf, sizeof ( buf ) - 1 ) ;

    if ( 0 == i ) {
      (void) lua_pushstring ( L, buf ) ;
      return 1 ;
    }
#endif

    return luaL_error ( L, "strerror_r( %d ) failed: %s (errno %d)",
      e, strerror ( i ), i ) ;
  }

  return luaL_argerror ( L, 1, "positive error code required" ) ;
}

static int Lgetprogname ( lua_State * const L )
{
#if defined (__GLIBC__) || defined (OSopenbsd) || defined (OSsolaris) || defined (OSnetbsd) || defined (OSfreebsd) || defined (OSdragonfly)
  const char * const pname =
# if defined (__GLIBC__) || defined (OSopenbsd)
    __progname
# else
    getprogname ()
# endif
    ;

  if ( pname && * pname ) {
    (void) lua_pushstring ( L, pname ) ;
    return 1 ;
  }

  return luaL_error ( L, "program name has not been set" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

#if defined (OSLinux)
/* changes the root filesystem */
static int Spivot_root ( lua_State * const L )
{
#  if defined (SYS_pivot_root)
  const char * new = luaL_checkstring ( L, 1 ) ;
  const char * old = luaL_checkstring ( L, 2 ) ;

  if ( new && old && * new && * old ) {
    long int i = syscall ( SYS_pivot_root, new, old ) ;

    if ( i ) {
      i = errno ;
      lua_pushinteger ( L, -1 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    } else {
      lua_pushinteger ( L, 0 ) ;
      return 1 ;
    }
  }
#  endif

  return 0 ;
}

/* get an (unsigned short) int from /dev/urandom */
static int Lgetrandom_int ( lua_State * const L )
{
#  if defined (SYS_getrandom)
  unsigned short int u = 0 ;
  long int i = syscall ( SYS_getrandom, & u, sizeof ( u ), GRND_NONBLOCK ) ;

  if ( sizeof ( u ) == i ) {
    lua_pushinteger ( L, u ) ;
  } else if ( 0 > i ) {
    i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  } else {
    lua_pushinteger ( L, -3 ) ;
  }

  return 1 ;
#  else
  return 0 ;
#  endif
}
#endif

/* wrapper to the uname syscall */
static int Suname ( lua_State * const L )
{
  int i = -3, e = 0 ;
  struct utsname utsn ;

  i = uname ( & utsn ) ;
  e = errno ;

  if ( i ) {
    lua_pushnil ( L ) ;
  } else {
    lua_createtable ( L, 0, 5 ) ;
    (void) lua_pushliteral ( L, "os" ) ;
    (void) lua_pushstring ( L, utsn . sysname ) ;
    lua_rawset ( L, -3 ) ;
    (void) lua_pushliteral ( L, "host" ) ;
    (void) lua_pushstring ( L, utsn . nodename ) ;
    lua_rawset ( L, -3 ) ;
    (void) lua_pushliteral ( L, "arch" ) ;
    (void) lua_pushstring ( L, utsn . machine ) ;
    lua_rawset ( L, -3 ) ;
    (void) lua_pushliteral ( L, "os_version" ) ;
    (void) lua_pushstring ( L, utsn . version ) ;
    lua_rawset ( L, -3 ) ;
    (void) lua_pushliteral ( L, "os_release" ) ;
    (void) lua_pushstring ( L, utsn . release ) ;
    lua_rawset ( L, -3 ) ;
  }

  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, e ) ;

  return 3 ;
}

/* wrapper to acct (process accounting) */
static int Sacct ( lua_State * const L )
{
  const char * fname = luaL_optstring ( L, 1, NULL ) ;

  if ( acct ( fname ) ) {
    const int e = errno ;
    return luaL_error ( L, "acct() failed: %s (errno %d)", strerror ( e ), e ) ;
  }

  return 0 ;
}

/* wrapper to quotactl */
/*
static int Squotactl ( lua_State * const L )
{
#if defined (OSLinux)
#elif defined (OSsolaris)
#else
#endif
  return 0 ;
}
*/

/* wrapper to sethostname(2) to set/change the systen's host name */
static int Ssethostname ( lua_State * const L )
{
  const char * name = luaL_checkstring ( L, 1 ) ;

  if ( name && * name ) {
    size_t s = str_len ( name ) ;
    s = ( HOST_NAME_MAX < s ) ? HOST_NAME_MAX : s ;

    if ( 0 < s ) {
      if ( sethostname ( name, s ) ) {
        const int e = errno ;
        return luaL_error ( L, "sethostname() failed: %s (errno %d)",
          strerror ( e ), e ) ;
      }

      return 0 ;
    }
  }

  return luaL_error ( L, "non empty host name required" ) ;
}

/* wrapper to gethostname(2) */
static int Sgethostname ( lua_State * const L )
{
  char buf [ 2 + HOST_NAME_MAX ] = { 0 } ;

  if ( gethostname ( buf, 1 + HOST_NAME_MAX ) ) {
    const int e = errno ;
    return luaL_error ( L, "gethostname() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  } else if ( buf [ 0 ] ) {
    (void) lua_pushstring ( L, buf ) ;
    return 1 ;
  }

  return 0 ;
}

/* wrapper to setdomainname(2) */
static int Ssetdomainname ( lua_State * const L )
{
#ifdef OSLinux
  const char * name = luaL_checkstring ( L, 1 ) ;

  if ( name && * name ) {
    if ( setdomainname ( name, str_len ( name ) ) ) {
      const int e = errno ;
      return luaL_error ( L, "setdomainname() failed: %s (errno %d)",
        strerror ( e ), e ) ;
    }

    return 0 ;
  }

  return luaL_error ( L, "non empty domain name required" ) ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

/* wrapper to getdomainname(2) */
static int Sgetdomainname ( lua_State * const L )
{
#ifdef OSLinux
  char buf [ 66 ] = { 0 } ;

  if ( getdomainname ( buf, 65 ) ) {
    const int e = errno ;
    return luaL_error ( L, "getdomainname() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  } else if ( buf [ 0 ] ) {
    (void) lua_pushstring ( L, buf ) ;
    return 1 ;
  }

  return 0 ;
#else
  return luaL_error ( L, "platform not supported" ) ;
#endif
}

static int Sgethostid ( lua_State * const L )
{
  lua_pushinteger ( L, gethostid () ) ;
  return 1 ;
}

static int Ssethostid ( lua_State * const L )
{
  long int i = luaL_checkinteger ( L, 1 ) ;

  if ( sethostid ( i ) ) {
    const int e = errno ;
    return luaL_error ( L, "sethostid() failed: %s (errno %d)",
      strerror ( e ), e ) ;
  }

  return 0 ;
}

/* this procedure exports important posix constants to Lua */
static void add_const ( lua_State * const L )
{
  /* create a new (sub)table holding the constants */
  lua_newtable ( L ) ;

  /* add the constants now to that (sub)table */
#if defined (_POSIX_VERSION)
  L_ADD_CONST( L, _POSIX_VERSION )
#endif
#if defined (_POSIX_C_SOURCE)
  L_ADD_CONST( L, _POSIX_C_SOURCE )
#endif
#if defined (_XOPEN_SOURCE)
  L_ADD_CONST( L, _XOPEN_SOURCE )
#endif
  L_ADD_CONST( L, EOF )
  L_ADD_CONST( L, PATH_MAX )
  L_ADD_CONST( L, CLOCKS_PER_SEC )
  L_ADD_CONST( L, FD_SETSIZE )

  /* constants used by wait(p)id */
  L_ADD_CONST( L, WNOHANG )
  L_ADD_CONST( L, WNOWAIT )
  L_ADD_CONST( L, WEXITED )
  L_ADD_CONST( L, WSTOPPED )
  L_ADD_CONST( L, WCONTINUED )
  L_ADD_CONST( L, WUNTRACED )
  L_ADD_CONST( L, P_ALL )
  L_ADD_CONST( L, P_PID )
  L_ADD_CONST( L, P_PGID )

  /* constants used by clock_(g,s)ettime(2) et al */
#if defined (_POSIX_TIMERS) && (0 < _POSIX_TIMERS)
  L_ADD_CONST( L, CLOCK_REALTIME )
#  ifdef OSLinux
  L_ADD_CONST( L, CLOCK_REALTIME_COARSE )
#  endif
#  ifdef _POSIX_MONOTONIC_CLOCK
  L_ADD_CONST( L, CLOCK_MONOTONIC )
#    ifdef OSLinux
  L_ADD_CONST( L, CLOCK_MONOTONIC_COARSE )
  L_ADD_CONST( L, CLOCK_MONOTONIC_RAW )
  L_ADD_CONST( L, CLOCK_BOOTTIME )
#    endif
#  endif
#  ifdef _POSIX_CPUTIME
  L_ADD_CONST( L, CLOCK_PROCESS_CPUTIME_ID )
#  endif
#  ifdef _POSIX_THREAD_CPUTIME
  L_ADD_CONST( L, CLOCK_THREAD_CPUTIME_ID )
#  endif
#endif

  /* constants used by SysV ipc */
  L_ADD_CONST( L, IPC_CREAT )
  L_ADD_CONST( L, IPC_EXCL )
  L_ADD_CONST( L, IPC_NOWAIT )
  L_ADD_CONST( L, IPC_PRIVATE )
  L_ADD_CONST( L, IPC_SET )
  L_ADD_CONST( L, IPC_RMID )
  L_ADD_CONST( L, IPC_STAT )
  L_ADD_CONST( L, MSG_NOERROR )
#ifdef OSLinux
  L_ADD_CONST( L, MSG_INFO )
  L_ADD_CONST( L, MSG_STAT )
  L_ADD_CONST( L, SEM_INFO )
  L_ADD_CONST( L, SEM_STAT )
/*
  L_ADD_CONST( L, MSGMNI )
  L_ADD_CONST( L, SHMMNI )
  L_ADD_CONST( L, MSGMNB )
  L_ADD_CONST( L, MSGMAX )
*/
#  ifdef _GNU_SOURCE
  L_ADD_CONST( L, IPC_INFO )
  L_ADD_CONST( L, MSG_COPY )
  L_ADD_CONST( L, MSG_EXCEPT )
#  endif
#endif

  /* constants used by fcntl */
  L_ADD_CONST( L, F_GETFD )
  L_ADD_CONST( L, F_SETFD )
  L_ADD_CONST( L, F_GETFL )
  L_ADD_CONST( L, F_SETFL )
  L_ADD_CONST( L, F_GETLK )
  L_ADD_CONST( L, F_SETLK )
  L_ADD_CONST( L, F_SETLKW )
  L_ADD_CONST( L, F_DUPFD )
  L_ADD_CONST( L, F_DUPFD_CLOEXEC )
  L_ADD_CONST( L, F_GETOWN )
  L_ADD_CONST( L, F_SETOWN )
#ifdef OSLinux
  L_ADD_CONST( L, F_RDLCK )
  L_ADD_CONST( L, F_WRLCK )
  L_ADD_CONST( L, F_UNLCK )
/*
  L_ADD_CONST( L, F_ADD_SEALS )
  L_ADD_CONST( L, F_GET_SEALS )
  L_ADD_CONST( L, F_SEAL_SEAL )
  L_ADD_CONST( L, F_SEAL_SHRINK )
  L_ADD_CONST( L, F_SEAL_GROW )
  L_ADD_CONST( L, F_SEAL_WRITE )
*/
# ifdef _GNU_SOURCE
  L_ADD_CONST( L, F_GETOWN_EX )
  L_ADD_CONST( L, F_SETOWN_EX )
  L_ADD_CONST( L, F_GETPIPE_SZ )
  L_ADD_CONST( L, F_SETPIPE_SZ )
  L_ADD_CONST( L, F_GETLEASE )
  L_ADD_CONST( L, F_SETLEASE )
  L_ADD_CONST( L, F_GETSIG )
  L_ADD_CONST( L, F_SETSIG )
  L_ADD_CONST( L, F_OFD_GETLK )
  L_ADD_CONST( L, F_OFD_SETLK )
  L_ADD_CONST( L, F_OFD_SETLKW )
  L_ADD_CONST( L, F_NOTIFY )
  L_ADD_CONST( L, DN_ACCESS )
  L_ADD_CONST( L, DN_ATTRIB )
  L_ADD_CONST( L, DN_CREATE )
  L_ADD_CONST( L, DN_DELETE )
  L_ADD_CONST( L, DN_MODIFY )
  L_ADD_CONST( L, DN_RENAME )
  L_ADD_CONST( L, DN_MULTISHOT )
  L_ADD_CONST( L, SEEK_DATA )
  L_ADD_CONST( L, SEEK_HOLE )
# endif
  /* constants used by the Linux memfd_create() syscall */
  L_ADD_CONST( L, MFD_CLOEXEC )
  L_ADD_CONST( L, MFD_ALLOW_SEALING )
#endif

  /* constants used by lseek etc */
  L_ADD_CONST( L, SEEK_SET )
  L_ADD_CONST( L, SEEK_CUR )
  L_ADD_CONST( L, SEEK_END )

#ifdef OSLinux
  /* constants used by ioctl */
  L_ADD_CONST( L, TIOCSCTTY )

  /* constants for inotify (Linux only !) */
  L_ADD_CONST( L, IN_ACCESS )
  L_ADD_CONST( L, IN_ATTRIB )
  L_ADD_CONST( L, IN_CLOSE_WRITE )
  L_ADD_CONST( L, IN_CLOSE_NOWRITE )
  L_ADD_CONST( L, IN_CREATE )
  L_ADD_CONST( L, IN_DELETE )
  L_ADD_CONST( L, IN_DELETE_SELF )
  L_ADD_CONST( L, IN_MODIFY )
  L_ADD_CONST( L, IN_MOVE_SELF )
  L_ADD_CONST( L, IN_MOVED_FROM )
  L_ADD_CONST( L, IN_MOVED_TO )
  L_ADD_CONST( L, IN_OPEN )
  L_ADD_CONST( L, IN_ALL_EVENTS )
  L_ADD_CONST( L, IN_CLOSE )
  L_ADD_CONST( L, IN_MOVE )
  L_ADD_CONST( L, IN_DONT_FOLLOW )
  L_ADD_CONST( L, IN_EXCL_UNLINK )
  L_ADD_CONST( L, IN_MASK_ADD )
  L_ADD_CONST( L, IN_ONESHOT )
  L_ADD_CONST( L, IN_ONLYDIR )
  L_ADD_CONST( L, IN_IGNORED )
  L_ADD_CONST( L, IN_ISDIR )
  L_ADD_CONST( L, IN_Q_OVERFLOW )
  L_ADD_CONST( L, IN_UNMOUNT )

  /* constants for fanotify (Linux only !) */
  L_ADD_CONST( L, FAN_CLASS_PRE_CONTENT )
  L_ADD_CONST( L, FAN_CLASS_CONTENT )
  L_ADD_CONST( L, FAN_CLASS_NOTIF )
  L_ADD_CONST( L, FAN_CLOEXEC )
  L_ADD_CONST( L, FAN_NONBLOCK )
  L_ADD_CONST( L, FAN_UNLIMITED_QUEUE )
  L_ADD_CONST( L, FAN_UNLIMITED_MARKS )
#endif

  /* constants used by glob */
  L_ADD_CONST( L, GLOB_ERR )
  L_ADD_CONST( L, GLOB_MARK )
  L_ADD_CONST( L, GLOB_NOESCAPE )
  L_ADD_CONST( L, GLOB_NOSORT )
  L_ADD_CONST( L, GLOB_DOOFFS )
  L_ADD_CONST( L, GLOB_NOCHECK )
  L_ADD_CONST( L, GLOB_APPEND )
#if defined(OSLinux) && defined(__GLIBC__)
  L_ADD_CONST( L, GLOB_PERIOD )
  L_ADD_CONST( L, GLOB_ALTDIRFUNC )
  L_ADD_CONST( L, GLOB_BRACE )
  L_ADD_CONST( L, GLOB_NOMAGIC )
  L_ADD_CONST( L, GLOB_TILDE )
  L_ADD_CONST( L, GLOB_TILDE_CHECK )
  L_ADD_CONST( L, GLOB_ONLYDIR )
#endif
  L_ADD_CONST( L, GLOB_ABORTED )
  L_ADD_CONST( L, GLOB_NOMATCH )
  L_ADD_CONST( L, GLOB_NOSPACE )

  /* constants used by fnmatch */
  L_ADD_CONST( L, FNM_NOESCAPE )
  L_ADD_CONST( L, FNM_PATHNAME )
  L_ADD_CONST( L, FNM_PERIOD )

  /* constants for wordexp(3) */
  L_ADD_CONST( L, WRDE_APPEND )
  L_ADD_CONST( L, WRDE_DOOFFS )
  L_ADD_CONST( L, WRDE_NOCMD )
  L_ADD_CONST( L, WRDE_REUSE )
  L_ADD_CONST( L, WRDE_SHOWERR )
  L_ADD_CONST( L, WRDE_UNDEF )
  L_ADD_CONST( L, WRDE_BADCHAR )
  L_ADD_CONST( L, WRDE_BADVAL )
  L_ADD_CONST( L, WRDE_CMDSUB )
  L_ADD_CONST( L, WRDE_NOSPACE )
  L_ADD_CONST( L, WRDE_SYNTAX )

  /* constants for the POSIX regex API */
  L_ADD_CONST( L, REG_EXTENDED )
  L_ADD_CONST( L, REG_ICASE )
  L_ADD_CONST( L, REG_NOSUB )
  L_ADD_CONST( L, REG_NEWLINE )
  L_ADD_CONST( L, REG_NOTBOL )
  L_ADD_CONST( L, REG_NOTEOL )
  L_ADD_CONST( L, REG_NOMATCH )

  /* constants used by open(2) */
  L_ADD_CONST( L, O_RDONLY )
  L_ADD_CONST( L, O_WRONLY )
  L_ADD_CONST( L, O_RDWR )
  L_ADD_CONST( L, O_CLOEXEC )
  L_ADD_CONST( L, O_NONBLOCK )
  L_ADD_CONST( L, O_NDELAY )
  L_ADD_CONST( L, O_CREAT )
  L_ADD_CONST( L, O_TRUNC )
  L_ADD_CONST( L, O_APPEND )
  L_ADD_CONST( L, O_EXCL )
  L_ADD_CONST( L, O_NOCTTY )
  L_ADD_CONST( L, O_DIRECTORY )
  L_ADD_CONST( L, O_SYNC )
  L_ADD_CONST( L, O_ASYNC )
  L_ADD_CONST( L, O_DIRECT )
  L_ADD_CONST( L, O_DSYNC )
  L_ADD_CONST( L, O_NOATIME )
  L_ADD_CONST( L, O_NOFOLLOW )
  L_ADD_CONST( L, O_PATH )
  L_ADD_CONST( L, O_TMPFILE )

  /* constants for stat(2) */
  L_ADD_CONST( L, S_IFMT )
  L_ADD_CONST( L, S_IFREG )
  L_ADD_CONST( L, S_IFDIR )
  L_ADD_CONST( L, S_IFLNK )
  L_ADD_CONST( L, S_IFIFO )
  L_ADD_CONST( L, S_IFSOCK )
  L_ADD_CONST( L, S_IFBLK )
  L_ADD_CONST( L, S_IFCHR )
  L_ADD_CONST( L, S_ISUID )
  L_ADD_CONST( L, S_ISGID )
  L_ADD_CONST( L, S_ISVTX )
  L_ADD_CONST( L, S_IRWXU )
  L_ADD_CONST( L, S_IRWXG )
  L_ADD_CONST( L, S_IRWXO )
  L_ADD_CONST( L, S_IRUSR )
  L_ADD_CONST( L, S_IRGRP )
  L_ADD_CONST( L, S_IROTH )
  L_ADD_CONST( L, S_IWUSR )
  L_ADD_CONST( L, S_IWGRP )
  L_ADD_CONST( L, S_IWOTH )
  L_ADD_CONST( L, S_IXUSR )
  L_ADD_CONST( L, S_IXGRP )
  L_ADD_CONST( L, S_IXOTH )

  /* constants for interval timers */
  L_ADD_CONST( L, ITIMER_REAL )
  L_ADD_CONST( L, ITIMER_VIRTUAL )
  L_ADD_CONST( L, ITIMER_PROF )

  /* signal number constants */
  L_ADD_CONST( L, NSIG )
  L_ADD_CONST( L, SIGHUP )
  L_ADD_CONST( L, SIGINT )
  L_ADD_CONST( L, SIGTERM )
  L_ADD_CONST( L, SIGALRM )
  L_ADD_CONST( L, SIGUSR1 )
  L_ADD_CONST( L, SIGUSR2 )
  L_ADD_CONST( L, SIGCONT )
  L_ADD_CONST( L, SIGSTOP )
  L_ADD_CONST( L, SIGKILL )
  L_ADD_CONST( L, SIGCHLD )
  L_ADD_CONST( L, SIGTRAP )
  L_ADD_CONST( L, SIGQUIT )
  L_ADD_CONST( L, SIGABRT )
  L_ADD_CONST( L, SIGPIPE )
  L_ADD_CONST( L, SIGTSTP )
  L_ADD_CONST( L, SIGTTIN )
  L_ADD_CONST( L, SIGTTOU )
  L_ADD_CONST( L, SIGSEGV )
  L_ADD_CONST( L, SIGFPE )
  L_ADD_CONST( L, SIGILL )
  L_ADD_CONST( L, SIGBUS )
  L_ADD_CONST( L, SIGPOLL )
  L_ADD_CONST( L, SIGIO )
  L_ADD_CONST( L, SIGSYS )
  L_ADD_CONST( L, SIGPROF )
  L_ADD_CONST( L, SIGURG )
  L_ADD_CONST( L, SIGVTALRM )
  L_ADD_CONST( L, SIGXCPU )
  L_ADD_CONST( L, SIGXFSZ )
  L_ADD_CONST( L, SIGIOT )
#ifdef SIGEMT
  L_ADD_CONST( L, SIGEMT )
#endif
#ifdef SIGWINCH
  L_ADD_CONST( L, SIGWINCH )
#endif
#ifdef SIGLOST
  L_ADD_CONST( L, SIGLOST )
#endif
#ifdef SIGCLD
  L_ADD_CONST( L, SIGCLD )
#endif
#ifdef SIGPWR
  L_ADD_CONST( L, SIGPWR )
#endif
#ifdef SIGINFO
  L_ADD_CONST( L, SIGINFO )
#endif
#ifdef SIGUNUSED
  L_ADD_CONST( L, SIGUNUSED )
#endif
  /* add posix real time signal constants where possible */
#ifdef SIGRTMIN
  L_ADD_CONST( L, SIGRTMIN )
#endif
#ifdef SIGRTMAX
  L_ADD_CONST( L, SIGRTMAX )
#endif

  /* socket constants */
  L_ADD_CONST( L, AF_UNIX )
  L_ADD_CONST( L, AF_LOCAL )
  L_ADD_CONST( L, AF_INET )
  L_ADD_CONST( L, AF_INET6 )
  L_ADD_CONST( L, AF_IPX )
  L_ADD_CONST( L, AF_NETLINK )
  L_ADD_CONST( L, AF_X25 )
  L_ADD_CONST( L, AF_AX25 )
  L_ADD_CONST( L, AF_ATMPVC )
  L_ADD_CONST( L, AF_PACKET )
  L_ADD_CONST( L, AF_ALG )
  L_ADD_CONST( L, AF_APPLETALK )
  L_ADD_CONST( L, SOCK_STREAM )
  L_ADD_CONST( L, SOCK_DGRAM )
  L_ADD_CONST( L, SOCK_SEQPACKET )
  L_ADD_CONST( L, SOCK_RAW )
  L_ADD_CONST( L, SOCK_RDM )
  L_ADD_CONST( L, SOCK_PACKET )
  L_ADD_CONST( L, SOCK_NONBLOCK )
  L_ADD_CONST( L, SOCK_CLOEXEC )
  L_ADD_CONST( L, SO_KEEPALIVE )

  /* constants for swapon/off */
#ifdef OSLinux
  L_ADD_CONST( L, SWAP_FLAG_DISCARD )
  L_ADD_CONST( L, SWAP_FLAG_PREFER )
  L_ADD_CONST( L, SWAP_FLAG_PRIO_MASK )
  L_ADD_CONST( L, SWAP_FLAG_PRIO_SHIFT )
#endif

  /* constants used by syslog() */
  /* options */
  L_ADD_CONST( L, LOG_CONS )
  L_ADD_CONST( L, LOG_NDELAY )
  L_ADD_CONST( L, LOG_ODELAY )
  L_ADD_CONST( L, LOG_NOWAIT )
  L_ADD_CONST( L, LOG_PID )
  L_ADD_CONST( L, LOG_PERROR )
  /* facilities */
  L_ADD_CONST( L, LOG_AUTH )
  L_ADD_CONST( L, LOG_AUTHPRIV )
  L_ADD_CONST( L, LOG_CRON )
  L_ADD_CONST( L, LOG_DAEMON )
  L_ADD_CONST( L, LOG_FTP )
  L_ADD_CONST( L, LOG_KERN )
  L_ADD_CONST( L, LOG_LPR )
  L_ADD_CONST( L, LOG_MAIL )
  L_ADD_CONST( L, LOG_NEWS )
  L_ADD_CONST( L, LOG_SYSLOG )
  L_ADD_CONST( L, LOG_USER )
  L_ADD_CONST( L, LOG_UUCP )
  L_ADD_CONST( L, LOG_LOCAL0 )
  L_ADD_CONST( L, LOG_LOCAL1 )
  L_ADD_CONST( L, LOG_LOCAL2 )
  L_ADD_CONST( L, LOG_LOCAL3 )
  L_ADD_CONST( L, LOG_LOCAL4 )
  L_ADD_CONST( L, LOG_LOCAL5 )
  L_ADD_CONST( L, LOG_LOCAL6 )
  L_ADD_CONST( L, LOG_LOCAL7 )
  /* levels */
  L_ADD_CONST( L, LOG_EMERG )
  L_ADD_CONST( L, LOG_ALERT )
  L_ADD_CONST( L, LOG_CRIT )
  L_ADD_CONST( L, LOG_ERR )
  L_ADD_CONST( L, LOG_WARNING )
  L_ADD_CONST( L, LOG_NOTICE )
  L_ADD_CONST( L, LOG_INFO )
  L_ADD_CONST( L, LOG_DEBUG )

  /* constants used by (u)mount(2) */
#if defined (OSLinux)
  /* constants used by mount */
  L_ADD_CONST( L, MS_REMOUNT )
  L_ADD_CONST( L, MS_MOVE )
  L_ADD_CONST( L, MS_BIND )
  L_ADD_CONST( L, MS_SHARED )
  L_ADD_CONST( L, MS_PRIVATE )
  L_ADD_CONST( L, MS_SLAVE )
  L_ADD_CONST( L, MS_UNBINDABLE )
  L_ADD_CONST( L, MS_DIRSYNC )
  L_ADD_CONST( L, MS_LAZYTIME )
  L_ADD_CONST( L, MS_MANDLOCK )
  L_ADD_CONST( L, MS_NOATIME )
  L_ADD_CONST( L, MS_NODIRATIME )
  L_ADD_CONST( L, MS_NODEV )
  L_ADD_CONST( L, MS_NOEXEC )
  L_ADD_CONST( L, MS_NOSUID )
  L_ADD_CONST( L, MS_RDONLY )
  L_ADD_CONST( L, MS_REC )
  L_ADD_CONST( L, MS_RELATIME )
  L_ADD_CONST( L, MS_SILENT )
  L_ADD_CONST( L, MS_STRICTATIME )
  L_ADD_CONST( L, MS_SYNCHRONOUS )
  /* constants used by umount */
  L_ADD_CONST( L, MNT_DETACH )
  L_ADD_CONST( L, MNT_EXPIRE )
  L_ADD_CONST( L, MNT_FORCE )
  L_ADD_CONST( L, UMOUNT_NOFOLLOW )
#endif

  /* constants for utmp */
  L_ADD_CONST( L, EMPTY )
  L_ADD_CONST( L, RUN_LVL )
  L_ADD_CONST( L, BOOT_TIME )
  L_ADD_CONST( L, NEW_TIME )
  L_ADD_CONST( L, OLD_TIME )
  L_ADD_CONST( L, INIT_PROCESS )
  L_ADD_CONST( L, LOGIN_PROCESS )
  L_ADD_CONST( L, USER_PROCESS )
  L_ADD_CONST( L, DEAD_PROCESS )

  /* constants for quotactl */
#if defined (OSLinux)
  L_ADD_CONST( L, Q_QUOTAON )
  L_ADD_CONST( L, Q_QUOTAOFF )
  L_ADD_CONST( L, Q_GETQUOTA )
  L_ADD_CONST( L, Q_SETQUOTA )
  L_ADD_CONST( L, Q_GETINFO )
  L_ADD_CONST( L, Q_SETINFO )
//L_ADD_CONST( L, Q_GETNEXTQUOTA )
  L_ADD_CONST( L, Q_GETFMT )
//L_ADD_CONST( L, Q_GETSTATS )
  L_ADD_CONST( L, Q_SYNC )
  L_ADD_CONST( L, Q_XQUOTAON )
  L_ADD_CONST( L, Q_XQUOTAOFF )
  L_ADD_CONST( L, Q_XQUOTARM )
  L_ADD_CONST( L, Q_XGETQUOTA )
//L_ADD_CONST( L, Q_XGETNEXTQUOTA )
  L_ADD_CONST( L, Q_XSETQLIM )
  L_ADD_CONST( L, Q_XGETQSTAT )
#endif

  /* constants for Linux syscalls */
#if defined (OSLinux)
  /* constants for the clone/unshare(2) Linux syscalls */
  L_ADD_CONST( L, CLONE_FILES )
  L_ADD_CONST( L, CLONE_FS )
  L_ADD_CONST( L, CLONE_NEWIPC )
  L_ADD_CONST( L, CLONE_NEWNET )
  L_ADD_CONST( L, CLONE_NEWPID )
  L_ADD_CONST( L, CLONE_NEWUSER )
  L_ADD_CONST( L, CLONE_NEWUTS )
  /*
  L_ADD_CONST( L, CLONE_NEWCGROUP )
  L_ADD_CONST( L, CLONE_NS )
  */
  L_ADD_CONST( L, CLONE_SIGHAND )
  L_ADD_CONST( L, CLONE_SYSVSEM )
  L_ADD_CONST( L, CLONE_THREAD )
  L_ADD_CONST( L, CLONE_VM )

 /* Linux capabilities */
  L_ADD_CONST( L, CAP_AUDIT_CONTROL )
  L_ADD_CONST( L, CAP_AUDIT_READ )
  L_ADD_CONST( L, CAP_AUDIT_WRITE )
  L_ADD_CONST( L, CAP_BLOCK_SUSPEND )
  L_ADD_CONST( L, CAP_CHOWN )
  L_ADD_CONST( L, CAP_DAC_OVERRIDE )
  L_ADD_CONST( L, CAP_DAC_READ_SEARCH )
  L_ADD_CONST( L, CAP_FOWNER )
  L_ADD_CONST( L, CAP_FSETID )
  L_ADD_CONST( L, CAP_IPC_LOCK )
  L_ADD_CONST( L, CAP_IPC_OWNER )
  L_ADD_CONST( L, CAP_KILL )
  L_ADD_CONST( L, CAP_LEASE )
  L_ADD_CONST( L, CAP_LINUX_IMMUTABLE )
  L_ADD_CONST( L, CAP_MAC_ADMIN )
  L_ADD_CONST( L, CAP_MAC_OVERRIDE )
  L_ADD_CONST( L, CAP_MKNOD )
  L_ADD_CONST( L, CAP_NET_ADMIN )
  L_ADD_CONST( L, CAP_NET_BIND_SERVICE )
  L_ADD_CONST( L, CAP_NET_BROADCAST )
  L_ADD_CONST( L, CAP_NET_RAW )
  L_ADD_CONST( L, CAP_SETUID )
  L_ADD_CONST( L, CAP_SETGID )
  L_ADD_CONST( L, CAP_SETFCAP )
  L_ADD_CONST( L, CAP_SETPCAP )
  L_ADD_CONST( L, CAP_SYS_ADMIN )
  L_ADD_CONST( L, CAP_SYS_BOOT )
  L_ADD_CONST( L, CAP_SYS_CHROOT )
  L_ADD_CONST( L, CAP_SYS_MODULE )
  L_ADD_CONST( L, CAP_SYS_NICE )
  L_ADD_CONST( L, CAP_SYS_PACCT )
  L_ADD_CONST( L, CAP_SYS_PTRACE )
  L_ADD_CONST( L, CAP_SYS_RAWIO )
  L_ADD_CONST( L, CAP_SYS_RESOURCE )
  L_ADD_CONST( L, CAP_SYS_TIME )
  L_ADD_CONST( L, CAP_SYS_TTY_CONFIG )
  L_ADD_CONST( L, CAP_SYSLOG )
  L_ADD_CONST( L, CAP_WAKE_ALARM )
#endif

  /* add the subtable to the main module table.
   * the subtable is now on top of the Lua stack,
   * while the main module table should be at the stack
   * position directly below it (i. e. at index -2).
   */
  lua_setfield ( L, -2, "SysConstants" ) ;

  /* and of function */
}

/* this array contains the needed Lua wrapper functions for posix syscalls */
static const luaL_Reg sys_func [ ] =
{
  /* begin of struct array for exported Lua C functions */

  /* functions imported from "os_proc.c" : */
  { "getuid",			Sgetuid		},
  { "geteuid",			Sgeteuid	},
  { "getgid",			Sgetgid		},
  { "getegid",			Sgetegid	},
  { "getpid",			Sgetpid		},
  { "getppid",			Sgetppid	},
  { "setuid",			Ssetuid		},
  { "setgid",			Ssetgid		},
  { "seteuid",			Sseteuid	},
  { "setegid",			Ssetegid	},
  { "setreuid",			Ssetreuid	},
  { "setregid",			Ssetregid	},
  { "getresuid",		Sgetresuid	},
  { "getresgid",		Sgetresgid	},
  { "setresuid",		Ssetresuid	},
  { "setresgid",		Ssetresgid	},
#if defined (OSLinux)
  { "setfsuid",			Ssetfsuid	},
  { "setfsgid",			Ssetfsgid	},
#endif
  { "getpgid",			Sgetpgid	},
  { "setpgid",			Ssetpgid	},
  { "getpgrp",			Sgetpgrp	},
  { "setpgrp",			Ssetpgrp	},
  { "getsid",			Sgetsid		},
  { "setsid",			Ssetsid		},
  { "do_setsid",		Ldo_setsid	},
  { "tcgetpgrp",		Stcgetpgrp	},
  { "tcsetpgrp",		Stcsetpgrp	},
  { "getpriority",		Sgetpriority	},
  { "setpriority",		Ssetpriority	},
  { "nice",			Snice		},
  { "group_member",		Sgroup_member	},
  { "initgroups",		Sinitgroups	},
  { "getgroups",		Sgetgroups	},
  { "setgroups",		Ssetgroups	},
  { "getgrouplist",		Sgetgrouplist	},
  { "kill",			Skill		},
  { "killpg",			Skillpg		},
  { "gen_kill_all",		Lgen_kill_all	},
  { "kill_all_procs",		Lkill_all_procs		},
  { "kill_procs",		Lkill_procs	},
  { "raise",			Sraise		},
#ifdef RLIMIT_AS
  { "getrlimit_addrspace",	Lgetrlimit_addrspace	},
  { "setrlimit_addrspace",	Lsetrlimit_addrspace	},
  { "softlimit_addrspace",	Lsoftlimit_addrspace	},
#endif
#ifdef RLIMIT_CORE
  { "getrlimit_core",		Lgetrlimit_core		},
  { "setrlimit_core",		Lsetrlimit_core		},
  { "softlimit_core",		Lsoftlimit_core		},
#endif
#ifdef RLIMIT_CPU
  { "getrlimit_cpu",		Lgetrlimit_cpu		},
  { "setrlimit_cpu",		Lsetrlimit_cpu		},
  { "softlimit_cpu",		Lsoftlimit_cpu		},
#endif
#ifdef RLIMIT_DATA
  { "getrlimit_data",		Lgetrlimit_data		},
  { "setrlimit_data",		Lsetrlimit_data		},
  { "softlimit_data",		Lsoftlimit_data		},
#endif
#ifdef RLIMIT_FSIZE
  { "getrlimit_fsize",		Lgetrlimit_fsize	},
  { "setrlimit_fsize",		Lsetrlimit_fsize	},
  { "softlimit_fsize",		Lsoftlimit_fsize	},
#endif
#ifdef RLIMIT_LOCKS
  { "getrlimit_locks",		Lgetrlimit_locks	},
  { "setrlimit_locks",		Lsetrlimit_locks	},
  { "softlimit_locks",		Lsoftlimit_locks	},
#endif
#ifdef RLIMIT_MEMLOCK
  { "getrlimit_memlock",	Lgetrlimit_memlock	},
  { "setrlimit_memlock",	Lsetrlimit_memlock	},
  { "softlimit_memlock",	Lsoftlimit_memlock	},
#endif
#ifdef RLIMIT_MSGQUEUE
  { "getrlimit_msgqueue",	Lgetrlimit_msgqueue	},
  { "setrlimit_msgqueue",	Lsetrlimit_msgqueue	},
  { "softlimit_msgqueue",	Lsoftlimit_msgqueue	},
#endif
#ifdef RLIMIT_NICE
  { "getrlimit_nice",		Lgetrlimit_nice		},
  { "setrlimit_nice",		Lsetrlimit_nice		},
  { "softlimit_nice",		Lsoftlimit_nice		},
#endif
#ifdef RLIMIT_NOFILE
  { "getrlimit_nofile",		Lgetrlimit_nofile	},
  { "setrlimit_nofile",		Lsetrlimit_nofile	},
  { "softlimit_nofile",		Lsoftlimit_nofile	},
#endif
#ifdef RLIMIT_NPROC
  { "getrlimit_nproc",		Lgetrlimit_nproc	},
  { "setrlimit_nproc",		Lsetrlimit_nproc	},
  { "softlimit_nproc",		Lsoftlimit_nproc	},
#endif
#ifdef RLIMIT_RSS
  { "getrlimit_rss",		Lgetrlimit_rss	},
  { "setrlimit_rss",		Lsetrlimit_rss	},
  { "softlimit_rss",		Lsoftlimit_rss	},
#endif
#ifdef RLIMIT_RTPRIO
  { "getrlimit_rt_prio",	Lgetrlimit_rt_prio	},
  { "setrlimit_rt_prio",	Lsetrlimit_rt_prio	},
  { "softlimit_rt_prio",	Lsoftlimit_rt_prio	},
#endif
#ifdef RLIMIT_RTTIME
  { "getrlimit_rt_time",	Lgetrlimit_rt_time	},
  { "setrlimit_rt_time",	Lsetrlimit_rt_time	},
  { "softlimit_rt_time",	Lsoftlimit_rt_time	},
#endif
#ifdef RLIMIT_SIGPENDING
  { "getrlimit_sigpending",	Lgetrlimit_sigpending	},
  { "setrlimit_sigpending",	Lsetrlimit_sigpending	},
  { "softlimit_sigpending",	Lsoftlimit_sigpending	},
#endif
#ifdef RLIMIT_STACK
  { "getrlimit_stack",		Lgetrlimit_stack	},
  { "setrlimit_stack",		Lsetrlimit_stack	},
  { "softlimit_stack",		Lsoftlimit_stack	},
#endif
  { "getrlimit",		Sgetrlimit	},
  { "setrlimit",		Ssetrlimit	},
  { "umask",			Sumask		},
  { "sec_umask",		Lsec_umask	},
  { "delay",			Ldelay		},
  { "do_sleep",			Ldo_sleep	},
  { "hard_sleep",		Lhard_sleep	},
  { "sleep",			Ssleep		},
  { "usleep",			Susleep		},
  { "nanosleep",		Snanosleep	},
  { "alarm",			Salarm		},
  { "ualarm",			Sualarm		},
  { "wait",			Swait		},
  { "waitpid",			Swaitpid	},
  { "waitid",			Swaitid		},
  { "waitpid_nohang",		Lwaitpid_nohang	},
  { "wait_nohang",		Lwaitpid_nohang	},
  { "waitid_exited_nohang",	Lwaitid_exited_nohang	},
  { "set_subreaper",		Lset_subreaper	},
  { "is_subreaper",		Lis_subreaper	},
  { "getitimer",		Sgetitimer	},
  { "setitimer",		Ssetitimer	},
  { "exit",			Sexit		},
  { "abort",			Sabort		},
  { "_exit",			S_exit		},
  { "pause",			Spause		},
  { "fork",			Sfork		},
  { "do_fork",			Ldo_fork	},
  { "xfork",			Lxfork		},
  { "chroot",			Schroot		},
  { "chdir",			Schdir		},
  { "getcwd",			Sgetcwd		},
  { "fchdir",			Sfchdir		},
  { "daemon",			Sdaemon		},
  { "execl",			Lexecl		},
  { "execlp",			Lexeclp		},
  { "defsig_execl",		Ldefsig_execl	},
  { "defsig_execlp",		Ldefsig_execlp	},
  { "vfork_exec_nowait",	Lvfork_exec_nowait	},
  { "vfork_exec_wait",		Lvfork_exec_wait	},
  { "vrun",			Lvfork_exec_wait	},
  { "vfork_execp_nowait",	Lvfork_execp_nowait	},
  { "vfork_execp_wait",		Lvfork_execp_wait	},
  { "vrunp",			Lvfork_execp_wait	},
  { "vfork_exec0_nowait",	Lvfork_exec0_nowait	},
  { "vfork_exec0_wait",		Lvfork_exec0_wait	},
  { "vrun0",			Lvfork_exec0_wait	},
  { "vfork_execp0_nowait",	Lvfork_execp0_nowait	},
  { "vfork_execp0_wait",	Lvfork_execp0_wait	},
  { "vrunp0",			Lvfork_execp0_wait	},
  { "daemonize",		Ldaemonize	},
#ifndef OSopenbsd
  { "sigqueue",			Ssigqueue	},
#endif
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
#endif
#if defined (OSsolaris) || defined (OSsunos5)
  { "sigsendset",		Ssigsendset	},
  { "kill_all_sol",		Lkill_all_sol	},
#endif
  /* end of imported functions from "os_proc.c" */

  /* functions imported from "os_time.c" : */
  { "tzset",			Stzset		},
  { "tzget",			Ltzget		},
  { "time",			Stime		},
  { "stime",			Sstime		},
  { "ftime",			Sftime		},
  { "gettimeofday",		Sgettimeofday	},
  { "settimeofday",		Ssettimeofday	},
  { "ctime",			Sctime		},
  { "clock",			Sclock		},
  { "times",			Stimes		},
  { "getrusage_proc",		Lgetrusage_proc		},
  { "getrusage_children",	Lgetrusage_children	},
  { "getrusage_child",		Lgetrusage_children	},
#ifdef RUSAGE_THREAD
  { "getrusage_thread",		Lgetrusage_thread	},
#endif
#if defined (_POSIX_TIMERS) && (0 < _POSIX_TIMERS)
  { "clock_getcpuclockid",	Sclock_getcpuclockid	},
  { "clock_getres",		Sclock_getres		},
  { "clock_gettime",		Sclock_gettime		},
  { "clock_settime",		Sclock_settime		},
#endif
#if defined (OSLinux)
  { "hwclock_runs_in_utc",	Lhwclock_runs_in_utc	},
#elif defined (OSfreebsd)
#elif defined (OSnetbsd)
#elif defined (OSopenbsd)
#endif
  /* end of imported functions from "os_time.c" */

  /* functions imported from "os_svipc.c" : */
  { "ftok",			Sftok },
  { "msgget",			Smsgget },
  { "msgctl",			Smsgctl },
  { "msgsnd",			Smsgsnd },
  { "msgrcv",			Smsgrcv },
  { "mqv_set",			Lmqv_set },
  { "mqv_stat",			Lmqv_stat },
  { "mqv_remove",		Lmqv_remove },
  { "mqv_rm",			Lmqv_rm },
  { "mqv_empty",		Lmqv_empty },
  { "mqv_close",		Lmqv_close },
  { "mqv_open",			Lmqv_open },
  /* end of imported functions from "os_svipc.c" */

  /* functions imported from "os_streams.c" : */
#if defined (OSsolaris) || defined (OSsunos5)
  { "isastream",		Lisastream },
  { "fattach",			Lfattach },
  { "fdetach",			Ldetach },
  { "fputmsg",			Lputmsg },
  { "fputpmsg",			Lputpmsg },
  { "fgetmsg",			Lgetmsg },
  { "fgetpmsg",			Lgetpmsg },
  { "serv_stream",		Lserv_stream },
  { "check_stream",		Lcheck_stream },
  { "req_stream_fd",		Lreq_stream_fd },
#endif
  /* end of imported functions from "os_streams.c" */

  /* functions imported from "os_sig.c" : */
  { "strsignal",		Sstrsignal },
  { "pause_forever",		Lpause_forever },
  { "reset_sigs",		Lreset_sigs },
  { "block_all_sigs",		Lblock_all_sigs },
  { "unblock_all_sigs",		Lunblock_all_sigs },
  { "block_sig",		Lblock_sig },
  { "unblock_sig",		Lunblock_sig },
  { "sig_wait_all",		Lsig_wait_all },
  { "sig_timed_wait_all",	Lsig_timed_wait_all },
  { "sig_reset",		Lsig_reset },
  { "killall5",			Lkillall5 },
  { "kill_all",			Lkill_all },
  { "nuke",			Lnuke		},
  { "got_sig",			Lgot_sig		},
  { "trap_sig",			Ltrap_sig		},
#if defined (OSLinux)
  { "signalfd",			Lsignalfd		},
#endif
  /* end of imported functions from "os_sig.c" */

  /* functions imported from "os_env.c" : */
  { "clearenv",			Sclearenv	},
  { "unsetenv",			Sunsetenv	},
  { "getenv",			Sgetenv		},
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
  { "secure_getenv",		Ssecure_getenv	},
#endif
  { "setenv",			Ssetenv		},
  { "putenv",			Sputenv		},
  { "get_environ",		Lget_environ	},
  { "addenv",			Laddenv		},
  { "newenv",			Lnewenv		},
  /* end of imported functions from "os_env.c" */

  /* functions imported from "os_file.c" : */
  { "sync",			Ssync		},
  { "fsync",			Lfsync		},
  { "fdatasync",		Lfdatasync	},
  { "dirname",			Sdirname	},
  { "basename",			Sbasename	},
  { "base_name",		Lbase_name	},
  { "dir_name",		        Ldir_name	},
  { "ends_with_slash",		Lends_with_slash	},
  { "realpath",			Srealpath	},
  { "truncate",			Struncate	},
  { "remove",			Sremove		},
  { "unlink",			Sunlink		},
  { "rename",			Srename		},
  { "chmod",			Schmod		},
  { "chown",			Schown		},
  { "lchown",			Slchown		},
  { "mknodes",			Lmknode		},
  { "mkfifos",			Lmkfifo		},
  { "mkdir",			Smkdir		},
  { "mkpath",			Lmkpath		},
  { "rmdir",			Srmdir		},
  { "link",			Slink		},
  { "symlink",			Ssymlink	},
  { "mkfifo",			Smkfifo		},
  { "mknod",			Smknod		},
  { "touch",			Ltouch		},
  { "octouch",			Loctouch	},
  { "create",			Lcreate		},
  { "empty_file",		Lempty_file	},
  { "touch_file",		Ltouch_file	},
  { "empty_files",		Lempty_files	},
  { "munlink",			Lmunlink	},
  { "mremove",			Lmremove	},
  { "stat",			Sstat		},
  { "lstat",			Slstat		},
  { "fstat",			Sfstat		},
  { "readlink",			Sreadlink	},
  { "fdcompare",		Lfdcompare	},
  { "is_tmpfs",			Lis_tmpfs	},
  { "test_path",		Ltest_path	},
  { "is_nt",			Lis_nt		},
  { "is_newer",			Lis_nt		},
  { "isNewer",			Lis_nt		},
  { "is_ot",			Lis_ot		},
  { "is_older",			Lis_ot		},
  { "isOlder",			Lis_ot		},
  { "is_eq",			Lis_eq		},
  { "is_ef",			Lis_eq		},
  { "is_eqf",			Lis_eq		},
  { "is_eqFile",		Lis_eq		},
  { "is_hard_link",		Lis_eq		},
  { "isHardLink",		Lis_eq		},
  { "is_same_file",		Lis_eq		},
  { "isSameFile",		Lis_eq		},
  { "copy_file",		Lcopy_file	},
  { "move_file",		Lmove_file	},
  { "read_file",		Lread_file	},
  { "ftruncate",		Sftruncate	},
  { "fchmod",			Sfchmod		},
  { "scandir",			Sscandir	},
  { "list_dir",			get_dirent	},
  { "list_dir",			list_dir	},
  { "dir",			dir_iter_factory	},
  { "rm",			Lrm		},
  { "rmr",			Lrmr		},
/*{ "fchown",			Sfchown		},	*/
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
#endif
  /* end of imported functions from "os_file.c" */

  /* functions imported from "os_io.c" : */
  { "open",			Sopen	},
  { "creat",			Screat	},
  { "dup",			Sdup	},
  { "xdup",			Lxdup		},
  { "dup_fd",			Ldup_fd		},
  { "dup2",			Sdup2	},
  { "xdup2",			Lxdup2		},
  { "dup_to",			Ldup_to		},
  { "dup3",			Sdup3	},
  { "close",			Sclose	},
  { "close_noint",		Lclose_fd	},
  { "close_fd",			Lclose_fd	},
  { "fd_close",			Lclose_fd	},
  { "close_all",		Lclose_all	},
  { "lseek",			Slseek	},
  { "read",			Sread	},
  { "buf_read",			Lbuf_read	},
  { "buf_read_close",		Lbuf_read_close		},
  { "write",			Swrite	},
  { "pipe",			Spipe	},
  { "open_pipe",		Lpipe	},
  { "stream_pipe",		Lstream_pipe	},
  { "socketpair",		Ssocketpair	},
  { "fd_has_data",		Lfd_has_data	},
  { "fds_have_data",		Lfds_have_data	},
  { "wait_for_fd_data",		Lwait_for_fd_data	},
  { "fd_has_input",		Lfd_has_input	},
  { "poll_input_fd",		Lpoll_input_fd	},
  { "redirio",			Lredirio	},
  { "isatty",			Sisatty		},
  { "ttyname",			Sttyname	},
  { "ctermid",			Sctermid	},
  { "ctty",			Sctty	},
  { "flushall",			Lflushallout	},
  { "flushallout",		Lflushallout	},
  { "puts",			Sputs	},
  { "fileno",			Lfileno	},
  { "fdopen",			Lfdopen	},
  { "chvt",			Lchvt	},
#if defined (OSLinux)
  { "memfd_create",		Smemfd_create	},
#endif
  /* end of imported functions from "os_io.c" */

  /* functions imported from "os_pw.c" : */
  { "getpwuid",			Sgetpwuid	},
  { "getpwnam",			Sgetpwnam	},
  { "getgrgid",			Sgetgrgid	},
  { "getgrnam",			Sgetgrnam	},
  { "getuser",			Lgetuser	},
  { "getpw",			getpwu		},
  { "getgroup",			Lgetgroup	},
  { "getgr",			getgre		},
  /* end of imported functions from "os_pw.c" */

  /* functions imported from "os_match.c" : */
  { "glob",			Sglob		},
  { "wordexp",			Swordexp	},
  { "fnmatch",			Sfnmatch	},
  { "sregmatch",		simple_regmatch	},
  { "regmatch",			regmatch	},
  { "file_regmatch",		file_regmatch	},
  /* end of imported functions from "os_match.c" */

  /* functions imported from "os_fs.c" : */
  { "statvfs",			Sstatvfs	},
  { "fstatvfs",			Sfstatvfs	},
  { "statfs",			Sstatfs		},
  { "fstatfs",			Sfstatfs	},
  { "umount",			Sumount		},
  { "mount",			Smount		},
  { "remount",			Lremount	},
  { "remount_ro",		Lremount_ro	},
  { "move_mount",		Lmove_mount	},
  { "mvmount",			Lmove_mount	},
  { "bind_mount",		Lbind_mount	},
  { "rec_bind_mount",		Lrec_bind_mount		},
  { "change_mount",		Lchange_mount		},
  { "chmount",			Lchange_mount		},
  { "swapon",			Sswapon		},
  { "swapoff",			Sswapoff	},
  { "mountpoint",		Lmountpoint	},
  { "is_mount_point",		Lmountpoint	},
  { "mtab_mount_point",		Lmtab_mount_point	},
  { "is_mtab_mount_point",	Lmtab_mount_point	},
  { "get_pseudofs",		Lget_pseudofs	},
  { "cgroup_level",		Lcgroup_level	},
  /* end of imported functions from "os_fs.c" */

  /* functions imported from "os_reboot.c" : */
  { "halt",			Lhalt		},
  { "poweroff",			Lpoweroff	},
  { "reboot",			Lreboot		},
  { "sys_reboot",		Sreboot		},
  { "fork_halt",		Lfork_halt	},
  { "fork_poweroff",		Lfork_poweroff	},
  { "fork_reboot",		Lfork_reboot	},
  /* end of imported functions from "os_reboot.c" */

  /* functions imported from "os_utmp.c" : */
  { "write_utmp",		Lwrite_utmp		},
  { "utmp_boot_time",		Lutmp_boot_time		},
  { "utmp_halt_time",		Lutmp_halt_time		},
  { "utmp_shutdown_time",	Lutmp_halt_time		},
  { "utmp_write_runlevel",	Lutmp_write_runlevel	},
  { "utmp_change_runlevel",	Lutmp_write_runlevel	},
  { "utmp_new_runlevel",	Lutmp_write_runlevel	},
  { "utmp_dead_proc",		Lutmp_dead_proc		},
  { "utmp_init_proc",		Lutmp_init_proc		},
  { "utmp_login_proc",		Lutmp_login_proc	},
  { "utmp_dead_process",	Lutmp_dead_proc		},
  { "utmp_init_process",	Lutmp_init_proc		},
  { "utmp_login_process",	Lutmp_login_proc	},
  { "utmp_create",		Lutmp_create		},
  { "utmp_set_boot",		Lutmp_set_boot		},
  { "utmp_set_halt",		Lutmp_set_halt		},
  { "utmp_set_runlevel",	Lutmp_set_runlevel	},
  { "utmp_set_dead",		Lutmp_set_dead		},
  { "utmp_set_init",		Lutmp_set_init		},
  { "utmp_set_login",		Lutmp_set_login		},
  { "utmp_dump",		Lutmp_dump		},
  /* end of imported functions from "os_utmp.c" */

  /* functions imported from "os_net.c" : */
  { "if_up",			Lif_up		},
  { "if_down",			Lif_down	},
  { "route_add_netmask",	Lroute_add_netmask	},
  { "route_add_defgw",		Lroute_add_defgw	},
  /* end of imported functions from "os_net.c" */

  /* functions imported from "os_utils.c" : */
  { "init_urandom",		Linit_urandom		},
  { "get_urandom_int",		Lget_urandom_int	},
  { "check_pidfile",		Lcheck_pidfile		},
#if defined (OSLinux)
#endif
  /* end of imported functions from "os_utils.c" */

  /* functions imported from "os_Linux.c" : */
#if defined (OSLinux)
  /* Linux specific functions */
  { "gettid",			Sgettid		},
  { "unshare",			Sunshare	},
  { "setns",			Ssetns		},
  { "set_ns",			Lsetns		},
  { "set_name_space",		Lsetns		},
  { "capget",			Scapget		},
  { "capset",			Scapset		},
  { "sysinfo",			Ssysinfo	},
  { "load_module",		Lload_module	},
  { "setup_iface_lo",		Lsetup_iface_lo		},
#endif
  /* end of imported functions from "os_Linux.c" */

  /* skalibs wrapper functions */
#ifdef SKALIBS_VERSION
#endif
  /* end of skalibs wrapper functions */

  /* local to this file */
  { "bitneg",			Lbitneg		},
  { "bitand",			Lbitand		},
  { "bitor",			Lbitor		},
  { "bitxor",			Lbitxor		},
  { "bitlshift",		Lbitlshift	},
  { "bitrshift",		Lbitrshift	},
  { "add",			add		},
  { "subarr",			subarr		},
  { "str2int",			Lstr2int	},
  { "str2bi",			Lstr2bi		},
  { "str2oi",			Lstr2oi		},
  { "str2xi",			Lstr2xi		},
  { "octintstr",		Loctintstr	},
  { "chomp",			Lchomp		},
  { "get_errno",		get_errno	},
  { "strerror",			Sstrerror	},
  { "strerror_r",		Sstrerror_r	},
  { "getprogname",		Lgetprogname	},
  { "uname",			Suname		},
  { "acct",			Sacct		},
  { "sethostname",		Ssethostname		},
  { "gethostname",		Sgethostname		},
  { "setdomainname",		Ssetdomainname		},
  { "getdomainname",		Sgetdomainname		},
  { "gethostid",		Sgethostid		},
  { "sethostid",		Ssethostid		},
#if defined (OSLinux)
  { "pivot_root",		Spivot_root		},
  { "getrandom_int",		Lgetrandom_int		},
#elif defined (OSsolaris) || defined (OSsunos5)
  { "uadmin",			Suadmin		},
#endif
  /* end of local functions */

  /* last sentinel entry to mark the end of this array */
  { NULL,			NULL }

  /* end of struct array sys_func [] */
} ;

/* open function for Lua to open this very module/library */
static int openMod ( lua_State * const L )
{
  /*
  struct utsname uts ;
  extern char ** environ ;
  */

  /* create a metatable for directory iterators */
  (void) dir_create_meta ( L ) ;
  /* add posix wrapper functions to module table */
  luaL_newlib ( L, sys_func ) ;
  /* add posix constants to module */
  add_const ( L ) ;

  /* set version information */
  (void) lua_pushliteral ( L, "_Version" ) ;
  (void) lua_pushliteral ( L,
    "pesi version " LUARC_VERSION " for " LUA_VERSION " ("
#if defined (OS)
    OS ")"
#else
    "Unix)"
#endif
    " compiled " __DATE__ " " __TIME__
    ) ;
  lua_rawset ( L, -3 ) ;

  /* save the current environment into a subtable */
  /* better not, let the user call the coresponding function
   * when needed instead. this saves some memory.
  if ( environ && * environ && ** environ ) {
    int i, j ;
    char * s = NULL ;

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
    }

    lua_setfield ( L, -2, "ENV" ) ;
  }
  */

  /* set some useful vars as we are at it */
  (void) lua_pushliteral ( L, "OSNAME" ) ;
  (void) lua_pushliteral ( L,
#if defined (OS)
    OS
#else
    "Unix"
#endif
    ) ;
  lua_rawset ( L, -3 ) ;

  return 1 ;
}

static void regMod ( lua_State * const L, const char * const name )
{
  (void) openMod ( L ) ;
  lua_setglobal ( L, ( name && * name ) ? name : "sys" ) ;
}

static const char * progname = NULL ;

static void cannot ( const char * pname, const char * msg, const int e )
{
  if ( pname && * pname ) {
    ; /* ok */
  } else {
    pname = "runlua" ;
  }

  (void) fprintf ( stderr, "%s:\tcannot %s:\n", pname, msg ) ;

  if ( 0 < e ) {
    (void) fprintf ( stderr, "\t%s\n", strerror ( e ) ) ;
  }

  (void) fflush ( NULL ) ;
  exit ( 111 ) ;
}

static int clear_stack ( lua_State * const L )
{
  const int i = lua_gettop ( L ) ;

  if ( 0 < i ) { lua_pop ( L, i ) ; }

  return i ;
}

/* message handler function used by lua_pcall() */
static int errmsgh ( lua_State * const L )
{
  const char * msg = lua_tostring ( L, 1 ) ;

  /* error object is no string or number ? */
  if ( NULL == msg ) {
    /* does it have a metamethod that produces a string ? */
    if ( luaL_callmeta ( L, 1, "__tostring" ) &&
      LUA_TSTRING == lua_type ( L, -1 ) )
    {
      /* this is the error message */
      return 1 ;
    } else {
      msg = lua_pushfstring ( L,
        "(error object is a %s value)",
        luaL_typename ( L, 1 ) ) ;
    }
  }

  /* append a standard traceback */
  luaL_traceback ( L, L, msg, 1 ) ;
  return 1 ;
}

/* create and set up a new Lua state (i. e. interpreter) */
static lua_State * new_vm ( void )
{
  lua_State * L = NULL ;

  while ( NULL == ( L = luaL_newstate () ) ) {
    do_sleep ( 5, 0 ) ;
  }

  if ( L ) {
    /* open all standard Lua libs */
    luaL_openlibs ( L ) ;

    /* register the above module */
    regMod ( L, "ux" ) ;

    /* useful global info variables */
    lua_pushboolean ( L, 1 ) ;
    lua_setglobal ( L, "_PESI_STANDALONE" ) ;

#if defined (OS)
    (void) lua_pushliteral ( L, OS ) ;
#else
    (void) lua_pushliteral ( L, "Unix" ) ;
#endif
    lua_setglobal ( L, "OSNAME" ) ;

    /* set version information */
    (void) lua_pushliteral ( L,
      "pesi runlua version " LUARC_VERSION " for " LUA_VERSION " ("
#if defined (OS)
      OS ")"
#else
      "Unix)"
#endif
      " compiled " __DATE__ " " __TIME__
      ) ;
    lua_setglobal ( L, "_RUNLUA" ) ;

#ifdef SUPERVISORD
    (void) lua_pushliteral ( L,
      "pesi supervise version " LUARC_VERSION " for " LUA_VERSION " ("
# if defined (OS)
      OS ")"
# else
      "Unix)"
# endif
      " compiled " __DATE__ " " __TIME__
      ) ;
    lua_setglobal ( L, "_SUPERVISE" ) ;
#endif
  }

  return L ;
}

static int report ( lua_State * const L, const char * const pname, const int r,
  const char * script )
{
  if ( LUA_OK != r ) {
    /* some error(s) ocurred while compiling or running Lua code */
    const char * msg = NULL ;

    if ( script && * script ) {
      (void) fprintf ( stderr,
        "%s:\tError in script \"%s\":\n"
        , pname, script ) ;
    } else {
      (void) fprintf ( stderr, "%s:\tERROR:\n", pname ) ;
    }

    switch ( r ) {
      case LUA_ERRSYNTAX :
        (void) fprintf ( stderr, "%s:\tsyntax error\n", pname ) ;
        break ;
      case LUA_ERRMEM :
        (void) fprintf ( stderr, "%s:\tout of memory\n", pname ) ;
        break ;
      case LUA_ERRGCMM :
        (void) fprintf ( stderr,
          "%s:\terror while running a __gc metamethod\n", pname ) ;
        break ;
      case LUA_ERRFILE :
        (void) fprintf ( stderr, "%s:\tfile access error:\n", pname ) ;

        if ( script && * script ) {
          (void) fprintf ( stderr,
            "%s:\tcannot open and/or read script file \"%s\"\n"
            , pname, script ) ;
        } else {
          (void) fprintf ( stderr,
            "%s:\tcannot open and/or read script file\n", pname ) ;
        }
        break ;
      default :
        (void) fprintf ( stderr, "%s:\tunknown error\n", pname ) ;
        break ;
    }

    msg = lua_tostring ( L, -1 ) ;

    if ( msg && * msg ) {
      (void) fprintf ( stderr,
        "%s:\terror message:\n\t%s\n\n", pname, msg ) ;
    }

    lua_pop ( L, 1 ) ;
    (void) fflush ( stderr ) ;
  }

  return r ;
}

static int push_args ( lua_State * const L, const int argc, char ** argv )
{
  int i, n = lua_gettop ( L ) ;

  for ( i = 0 ; argc > i ; ++ i ) {
    if ( argv [ i ] && argv [ i ] [ 0 ] ) {
      (void) lua_pushstring ( L, argv [ i ] ) ;
    }
  }

  n = n - lua_gettop ( L ) ;
  return n ;
}

static void show_version ( const char * const pname )
{
  (void) fputs ( pname, stdout ) ;
  (void) puts (
    ":\tPesi runlua v" PESI_VERSION " for " LUA_VERSION " ("
#if defined (OS)
    OS ")"
#else
    "Unix)"
#endif
    " compiled " __DATE__ " " __TIME__
    ) ;
  (void) puts ( LUA_COPYRIGHT ) ;
  (void) fflush ( stdout ) ;
}

static void show_usage ( const char * const pname )
{
  (void) fprintf ( stderr,
    "\nusage:\t%s -blah\n\n"
    , pname ) ;
  (void) fflush ( stderr ) ;
}

static int check4script ( const char * const path, const char * const pname )
{
  int i = 0 ;
  errno = 0 ;

  if ( is_fnr ( path ) ) {
    return 0 ;
  }

  i = ( 0 < errno ) ? errno : 0 ;
  (void) fprintf ( stderr,
    "%s\tcannot read script \"%s\"\n"
    , pname, path ) ;
  (void) fflush ( stderr ) ;
  cannot ( pname, "open script for reading", errno ) ;

  return -1 ;
}

/* set process resource (upper) limits */
static int set_rlimits ( const uid_t u )
{
  struct rlimit rlim ;

  (void) getrlimit ( RLIMIT_CORE, & rlim ) ;
  rlim . rlim_cur = 0 ;

  if ( 0 == u ) { rlim . rlim_max = RLIM_INFINITY ; }

  /* set the SOFT (!!) default upper limit of coredump sizes to zero
   * (i. e. disable coredumps). can raised again later in child/sub
   * processes as needed.
   */
  return setrlimit ( RLIMIT_CORE, & rlim ) ;
}

static int writer ( lua_State * const L, const void * p, const size_t s, void * u )
{
  (void) L ;

  if ( p && u && ( 0 < s ) ) {
    return 1 > fwrite ( p, s, 1, (FILE *) u ) ;
  }

  return -1 ;
}

/* command line flags */
enum {
  FLAG_STMT		= 0x01,
  FLAG_INTER		= 0x02,
  FLAG_SCR		= 0x01,
  FLAG_PARSE		= 0x02,
  FLAG_SAVEBC		= 0x04,
  FLAG_STRIP		= 0x08,
} ;

/* load and compile a given Lua script to bytecode */
static int compile_script ( lua_State * const L,
  const char * const pname, const unsigned long int f,
  const char * const script, const char * output, const int narg )
{
  int i = luaL_loadfile ( L, script ) ;

  if ( LUA_OK != i ) {
    (void) fprintf ( stderr,
      "%s:\tcannot load and compile script \"%s\"\n"
      , pname, script ) ;
    return report ( L, pname, i, script ) ;
  } else if ( FLAG_PARSE & f ) {
    return i ;
  } else if ( LUA_OK == i ) {
    FILE * fp = NULL ;

    output = ( output && * output ) ? output : "out.lc" ;
    fp = fopen ( output, "wb" ) ;
    /* fp = fp ? fp : stdout ;	*/

    if ( NULL == fp ) {
      (void) fprintf ( stderr,
        "%s:\tcannot open output file \"%s\" for writing:\n\t%s\n"
        , pname, output, strerror ( errno ) ) ;
      return -1 ;
    }

    i = lua_dump ( L, writer, fp, ( FLAG_STRIP & f ) ? 1 : 0 ) ;

    if ( i ) {
      (void) fprintf ( stderr,
        "%s:\terror while writing bytecode to output file \"%s\"\n"
        , pname, output ) ;
    }

    if ( ferror ( fp ) ) {
    }

    if ( fclose ( fp ) ) {
      (void) fprintf ( stderr,
        "%s:\tcannot close script file \"%s\":\n\t%s\n"
        , pname, script, strerror ( errno ) ) ;
    }
  }

  (void) clear_stack ( L ) ;
  return i ;
}

/* load, compile and run a given chunk of Lua code */
static int run_chunk ( lua_State * const L,
  const char * const pname, const unsigned long int f,
  const char * const s, const int narg, int * rp )
{
  int i = 0 ;

  if ( ( FLAG_SCR & f ) && ( NULL == s ) ) {
    (void) fprintf ( stderr,
      "%s:\tno script given, will read Lua code from stdin\n"
      , pname ) ;
  }

  /* read and parse the Lua code from the given string or script (or stdin).
   * the result is stored as a Lua function on top of the stack
   * when successful.
   */
  i = ( FLAG_SCR & f ) ?
    luaL_loadfile ( L, s ) : luaL_loadstring ( L, s ) ;

  /* call the chunk if loading and compiling it succeeded */
  if ( LUA_OK == i ) {
    /* the chunk was successfully read, parsed, and its code
     * has been stored as a function on top of the Lua stack.
     */
    const int n = lua_gettop ( L ) - narg ;
    /* push our error message handler function onto the stack */
    lua_pushcfunction ( L, errmsgh ) ;
    lua_insert ( L, n ) ;
    i = lua_pcall ( L, narg, 1, n ) ;
    /* remove our message handler function from the stack */
    lua_remove ( L, n ) ;
  }

  if ( LUA_OK == i ) {
    /* success: parsing and executing the script succeeded.
     * check for an integer result
     */
    * rp = lua_isinteger ( L, -1 ) ? lua_tointeger ( L, -1 ) : 0 ;
  } else {
    if ( s && * s ) {
      (void) fprintf ( stderr,
        "%s:\tFailad to run script \"%s\":\n"
        , pname, s ) ;
    }

    (void) report ( L, pname, i, s ) ;
  }

  (void) clear_stack ( L ) ;
  return i ;
}

static void defsigs ( void )
{
  int i ;
  struct sigaction sa ;

  (void) memset ( & sa, 0, sizeof ( struct sigaction ) ) ;
  sa . sa_flags = SA_RESTART ;
  (void) sigemptyset ( & sa . sa_mask ) ;
  sa . sa_handler = SIG_DFL ;

  /* restore default swignal handling behaviour */
  for ( i = 1 ; NSIG > i ; ++ i ) {
    if ( SIGKILL != i && SIGSTOP != i ) {
      (void) sigaction ( i, & sa, NULL ) ;
    }
  }

  /* catch SIGCHLD */
  sa . sa_handler = pseudo_sighand ;
  (void) sigaction ( SIGCHLD, & sa, NULL ) ;
  /* unblock all signals */
  (void) sigprocmask ( SIG_SETMASK, & sa . sa_mask, NULL ) ;
}

static int imain ( const int argc, char ** argv )
{
  int i, j, k, n, r = 0 ;
  unsigned long int f = 0 ;
  const char * s = NULL ;
  const char * output = NULL ;
  const char * pname = NULL ;
  lua_State * L = NULL ;
  const uid_t myuid = getuid () ;
  extern int opterr, optind, optopt ;
  extern char * optarg ;

  /* set up some variables first */
  opterr = 1 ;
  pname = ( ( 0 < argc ) && argv && * argv && ** argv ) ? * argv : "runlua" ;
  progname = ( ( 0 < argc ) && argv && * argv && ** argv ) ? * argv : "runlua" ;

  /* drop possible orivileges we might have */
  (void) setegid ( getgid () ) ;
  (void) seteuid ( myuid ) ;

  /* secure file creation mask */
  (void) umask ( 00022 | ( 00077 & umask ( 00077 ) ) ) ;

  /* disable core dumps by setting the corresponding SOFT (!!)
   * limit to zero.
   * it can be raised again later (in child processes) as needed.
   */
  (void) set_rlimits ( myuid ) ;

  /* restore default dispostions for all signals */
  defsigs () ;

  /* create a new Lua VM (state) */
  errno = 0 ;
  L = new_vm () ;

  if ( NULL == L ) {
    if ( 0 < errno ) {
      perror ( "cannot create Lua VM" ) ;
    } else {
      (void) fputs ( pname, stderr ) ;
      (void) fputs ( ":\tcannot create Lua VM:\n\tNot enough memory\n"
        , stderr ) ;
    }

    (void) fflush ( NULL ) ;
    return 111 ;
  }

  luaL_checkversion ( L ) ;
  /* create some global helper variables in this VM */
  (void) lua_pushstring ( L, pname ) ;
  lua_setglobal ( L, "RUNLUA" ) ;

  /* create a new Lua table holding ALL command line args */
  lua_newtable ( L ) ;
  (void) lua_pushstring ( L, pname ) ;
  lua_rawseti ( L, -2, -1 ) ;

  for ( i = 0, j = 0 ; argc > i ; ++ i ) {
    if ( argv [ i ] && argv [ i ] [ 0 ] ) {
      (void) lua_pushstring ( L, argv [ i ] ) ;
      lua_rawseti ( L, -2, j ++ ) ;
    }
  }

  /* set the global name of this Lua table */
  lua_setglobal ( L, "ARGV" ) ;

  (void) clear_stack ( L ) ;
  /* parse the command line args to figure out what to do */
  while ( 0 < ( i = getopt ( argc, argv, ":cC:e:g:hHio:pqR:s:u:vVw:" ) ) ) {
    switch ( i ) {
      case 'c' :
        f |= FLAG_SAVEBC ;
        break ;
      case 'd' :
        f |= FLAG_STRIP ;
        break ;
      case 'i' :
        f |= FLAG_INTER ;
        break ;
      case 'p' :
        f |= FLAG_PARSE ;
        break ;
      case 'o' :
        if ( optarg && * optarg ) {
          output = optarg ;
        }
        break ;
      case 'C' :
        if ( optarg && * optarg && chdir ( optarg ) ) {
          perror ( "chdir failed" ) ;
          r = 111 ;
          goto fin ;
        }
        break ;
      case 'w' :
        if ( optarg && * optarg ) {
          char * str = strchr ( optarg, '.' ) ;
          j = k = 0 ;
          j = atoi ( optarg ) ;
          if ( str && * str ) { k = atoi ( str ) ; }
          j = ( 0 < j ) ? j : 0 ;
          k = ( 0 < k ) ? k : 0 ;
          if ( 0 < j || 0 < k ) { (void) delay ( j, k ) ; }
        }
        break ;
      case 'g' :
        if ( myuid ) { continue ; }
        else if ( optarg && * optarg ) {
          j = atoi ( optarg ) ;
          if ( 0 <= j && 0 != setegid ( j ) ) {
            perror ( "setegid failed" ) ;
            r = 111 ;
            goto fin ;
          }
        }
        break ;
      case 'u' :
        if ( myuid ) { continue ; }
        else if ( optarg && * optarg ) {
          j = atoi ( optarg ) ;
          if ( 0 <= j && 0 != seteuid ( j ) ) {
            perror ( "seteuid failed" ) ;
            r = 111 ;
            goto fin ;
          }
        }
        break ;
      case 'R' :
        if ( myuid ) { continue ; }
        else if ( optarg && * optarg ) {
          if ( chdir ( optarg ) || chroot ( optarg ) || chdir ( "/" ) ) {
            perror ( "chroot failed" ) ;
            r = 111 ;
            goto fin ;
          }
        }
        break ;
      case 'q' :
        break ;
      case 'e' :
        /* (Lua code) string to execute */
        if ( optarg && * optarg ) {
          f |= FLAG_STMT ;
          n = push_args ( L, argc, argv ) ;
          j = run_chunk ( L, pname, 0, optarg, ( 0 < n ) ? n : 0, & k ) ;
          if ( LUA_OK == j ) { r += k ; }
          else { r = j ? j : 1 ; goto fin ; }
        }
        break ;
      case 's' :
        /* script to execute */
        if ( optarg && * optarg ) {
          if ( check4script ( optarg, pname ) ) {
            r = 111 ;
            goto fin ;
          } else {
            s = optarg ;
          }
        }
        break ;
      case 'h' :
      case 'H' :
        show_version ( pname ) ;
        show_usage ( pname ) ;
        r = 0 ;
        goto fin ;
        break ;
      case 'v' :
      case 'V' :
        show_version ( pname ) ;
        r = 0 ;
        goto fin ;
        break ;
      case ':' :
        show_version ( pname ) ;
        (void) fprintf ( stderr,
          "\n%s:\tmissing argument for option \"-%c\"\n\n"
          , pname, optopt ) ;
        show_usage ( pname ) ;
        r = 100 ;
        goto fin ;
        break ;
      case '?' :
        show_version ( pname ) ;
        (void) fprintf ( stderr,
          "\n%s:\tunrecognized option \"-%c\"\n\n"
          , pname, optopt ) ;
        show_usage ( pname ) ;
        r = 100 ;
        goto fin ;
        break ;
      default :
        break ;
    }
  }

  n = k = 0 ;
  i = j = optind ;

  if ( 0 < i && argc > i ) {
    /* script to execute */
    if ( ( NULL == s ) && argv [ i ] && argv [ i ] [ 0 ] ) {
      s = argv [ i ] ;

      if ( check4script ( s, pname ) ) {
        r = 111 ;
        goto fin ;
      }
    }
  }

  if ( s && * s ) {
    (void) lua_pushstring ( L, s ) ;
    lua_setglobal ( L, "SCRIPT" ) ;
  }

  if ( ( FLAG_SAVEBC & f ) || ( FLAG_PARSE & f ) ) {
    if ( s && * s ) {
      n = push_args ( L, ( 0 < i && argc > i ) ? i : argc, argv ) ;
      r = compile_script ( L, pname, f, s, output, ( 0 < n ) ? n : 0 ) ;
    } else {
      r = 100 ;
      (void) fprintf ( stderr,
        "%s:\tno script to parse/compile\n", pname ) ;
      //goto fin ;
    }
  } else {
    n = push_args ( L, ( 0 < i && argc > i ) ? i : argc, argv ) ;
    i = run_chunk ( L, pname, FLAG_SCR & f, s, ( 0 < n ) ? n : 0, & j ) ;
    r = ( LUA_OK == i ) ? j : 1 ;
  }

  if ( ( FLAG_INTER & f ) && s && * s ) {
    if ( LUA_OK != i ) {
      (void) fprintf ( stderr,
        "%s:\tcould not run Lua script \"%s\"\n"
        , pname, s ) ;
    }

    (void) printf (
      "%s:\tentering interactive mode after running Lua script \"%s\"\n\n"
      , pname, s ) ;
    (void) fflush ( NULL ) ;
    (void) clear_stack ( L ) ;
    n = push_args ( L, ( 0 < i && argc > i ) ? i : argc, argv ) ;
    i = run_chunk ( L, pname, FLAG_SCR, NULL, ( 0 < n ) ? n : 0, & j ) ;
    r = ( LUA_OK == i ) ? j : 1 ;
  }

fin :
  (void) clear_stack ( L ) ;
  lua_close ( L ) ;
  (void) fflush ( NULL ) ;

  return r ;
}

#if defined (STANDALONE)

int main ( const int argc, char ** argv )
{
  return imain ( argc, argv ) ;
}

#else

/* used as as Lua module (shared object), so luaopen_* functions
 * are needed, define them here.
 */
int luaopen_sys ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_posix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_ox ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_pox ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_psx ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_px ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_unix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_unx ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_ux ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_xinu ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_xo ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_xu ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_aix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_bsd ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_hpux ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_irix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_linux ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_osf ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_sun ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_sunos ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_osunix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_os_unix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_pesi ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_peso ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_os_posix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_sys_posix ( lua_State * const L )
{
  return openMod ( L ) ;
}

int luaopen_sys_unix ( lua_State * const L )
{
  return openMod ( L ) ;
}

#endif

