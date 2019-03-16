/*
 * helper functions
 */

/* constants */
enum {
  FCOMP_NOFOLLOW		= 0x0001,
  FCOMP_DEV			= 0x0002,
  FCOMP_INO			= 0x0004,
  FCOMP_UID			= 0x0008,
  FCOMP_GID			= 0x0010,
  FCOMP_DEVID			= 0x0020,
  FCOMP_SIZE			= 0x0040,
  FCOMP_MTIME			= 0x0080,
  FCOMP_MTIME_NEWER		= 0x0100,
  FCOMP_MTIME_OLDER		= 0x0200,
  FCOMP_SIZE_LESS		= 0x0400,
  FCOMP_SIZE_MORE		= 0x0800,
  FTEST_NOFOLLOW		= 0x0001,
  FTEST_EFF			= 0x0002,
  FTEST_UID			= 0x0004,
  FTEST_GID			= 0x0008,
  FTEST_SZZ			= 0x0010,
  FTEST_SZNZ			= 0x0020,
} ;

/* pseudo signal handler function (mainly used for SIGCHLD) */
static void pseudo_sighand ( const int s )
{
  (void) s ;
}

static size_t str_len ( const char * const str )
{
  if ( str && * str ) {
    register size_t i = 0 ;

    while ( '\0' != str [ i ] ) { ++ i ; }

    return i ;
  }

  return 0 ;
}

static size_t str_nlen ( const char * const str, const size_t n )
{
  if ( str && * str ) {
    register size_t i = 0 ;

    while ( n > i && '\0' != str [ i ] ) { ++ i ; }

    return i ;
  }

  return 0 ;
}

static int dupfd2 ( const int o, const int n )
{
  if ( 0 <= o && 0 <= n ) {
    if ( o != n ) {
      int i ;

      do { i = dup2 ( o, n ) ; }
      while ( 0 > i && EINTR == errno ) ;

      return i ;
    }

    return n ;
  }

  return -3 ;
}

/* reap terminated child/sub processes with waitpid(2) */
static pid_t reap_proc ( const char nohang, const pid_t pid )
{
  pid_t p = 0 ;

  do {
    p = waitpid ( ( 1 < pid ) ? pid : -1, NULL, nohang ? WNOHANG : 0 ) ;

    if ( 1 < pid && 0 < p && pid == p ) {
      return p ;
      break ;
    }
  } while ( 0 < p || ( 0 > p && EINTR == errno ) ) ;

  return p ;
}

static int chkperm ( mode_t m, struct stat * const stp )
{
  int i = 0 ;
  m &= 007777 ;

  if ( geteuid () == stp -> st_uid ) {
    m <<= 6 ;
    i = ( m == ( m & ( stp -> st_mode ) ) ) ;
  } else if ( getegid () == stp -> st_gid ) {
    m <<= 3 ;
    i = ( m == ( m & ( stp -> st_mode ) ) ) ;
  } else {
    i = ( m == ( m & ( stp -> st_mode ) ) ) ;
  }

  return i ? 1 : 0 ;
}

static int ftype ( const unsigned long int f, const mode_t m,
  const char * const path )
{
  if ( path && * path ) {
    struct stat st ;

    if ( ( FTEST_NOFOLLOW & f ) ? lstat ( path, & st ) : stat ( path, & st ) )
    { return 0 ; }
    else if ( 1 > st . st_nlink ) { return 0 ; }

    if ( S_IFMT & m ) {
      if ( S_IFMT & m & st . st_mode ) { ; /* ok */ }
      else { return 0 ; }
    }

    if ( 007000 & m ) {
      if ( 007000 & m & st . st_mode ) { ; /* ok */ }
      else { return 0 ; }
    }

    if ( ( FTEST_SZNZ & f ) && 1 > st . st_size ) { return 0 ; }
    else if ( ( FTEST_SZZ & f ) && 0 != st . st_size ) { return 0 ; }

    if ( ( 00007 & m ) && ( 0 == chkperm ( 00007 & m, & st ) ) ) {
      return 0 ;
    }

    if ( FTEST_EFF & f ) {
      if ( ( FTEST_UID & f ) && ( geteuid () != st . st_uid ) ) {
        return 0 ;
      } else if ( ( FTEST_GID & f ) && ( getegid () != st . st_gid ) ) {
        return 0 ;
      }
    } else if ( ( FTEST_UID & f ) && ( getuid () != st . st_uid ) ) {
      return 0 ;
    } else if ( ( FTEST_GID & f ) && ( getgid () != st . st_gid ) ) {
      return 0 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* helper function that compares 2 given files according to first arg */
static int fcompare ( const unsigned int f,
  const char * const fn, const char * const fn2 )
{
  if ( fn && fn2 && * fn && * fn2 ) {
    struct stat st, st2 ;

    if ( ( FCOMP_NOFOLLOW & f ) ? lstat ( fn, & st ) : stat ( fn, & st ) )
    { return 0 ; }

    if ( ( FCOMP_NOFOLLOW & f ) ? lstat ( fn2, & st2 ) : stat ( fn2, & st2 ) )
    { return 0 ; }

    if ( FCOMP_DEV & f ) {
      /* files are on the same device (file system) */
      return ( st . st_dev == st2 . st_dev ) ;
    } else if ( FCOMP_INO & f ) {
      /* files are equal */
      return ( st . st_dev == st2 . st_dev ) && ( st . st_ino == st2 . st_ino ) ;
    } else if ( FCOMP_UID & f ) {
      /* files have the same owner */
      return ( st . st_uid == st2 . st_uid ) ;
    } else if ( FCOMP_GID & f ) {
      /* files belong to the same group */
      return ( st . st_gid == st2 . st_gid ) ;
    } else if ( FCOMP_DEVID & f ) {
      return ( st . st_rdev == st2 . st_rdev ) ;
    } else if ( FCOMP_SIZE & f ) {
      /* files have the same size (in bytes) */
      return ( st . st_size == st2 . st_size ) ;
    } else if ( FCOMP_MTIME & f ) {
      /* files have the same modification time(stamp) */
      return ( st . st_mtime == st2 . st_mtime ) ;
    } else if ( FCOMP_MTIME_OLDER & f ) {
      /* fn is older than fn2 */
      return ( st . st_mtime < st2 . st_mtime ) ;
    } else if ( FCOMP_MTIME_NEWER & f ) {
      /* fn is newer than fn2 */
      return ( st . st_mtime > st2 . st_mtime ) ;
    } else if ( FCOMP_SIZE_LESS & f ) {
      /* fn is smaller than fn2 */
      return ( st . st_size < st2 . st_size ) ;
    } else if ( FCOMP_SIZE_MORE & f ) {
      /* fn is bigger than fn2 */
      return ( st . st_size > st2 . st_size ) ;
    }
  }

  return 0 ;
}

static int is_f ( const char * const path )
{
  return ftype ( 0, S_IFREG, path ) ;
}

static int is_d ( const char * const path )
{
  return ftype ( 0, S_IFDIR, path ) ;
}

static int is_c ( const char * const path )
{
  return ftype ( 0, S_IFCHR, path ) ;
}

static int is_fn ( const char * const path )
{
  return ftype ( FTEST_SZNZ, S_IFREG, path ) ;
}

static int is_fz ( const char * const path )
{
  return ftype ( FTEST_SZZ, S_IFREG, path ) ;
}

static int is_fnr ( const char * const path )
{
  return ftype ( FTEST_SZNZ, 00004 | S_IFREG, path ) ;
}

static int is_fnrx ( const char * const path )
{
  return ftype ( FTEST_SZNZ, 00005 | S_IFREG, path ) ;
}

/*
#ifndef timespecsub
#define	timespecsub(tsp, usp, vsp)					      \
	do {								      \
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		      \
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	      \
		if ((vsp)->tv_nsec < 0) {				      \
			(vsp)->tv_sec--;				      \
			(vsp)->tv_nsec += 1000000000L;			      \
		}							      \
	} while ( 0 )
#endif
*/

/* Sleep a number of seconds without being interrupted.
 * The sleep() syscall may be implemented using SIGALRM.
 * Thus mixing calls to alarm(2) and sleep() is a bad idea.
 */
static int delay ( const long int s, const long int u )
{
  if ( 0 < s || ( 0 < u && ( 1000 * 1000 ) > u ) ) {
    int i = 0 ;
    struct timeval tv ;

    tv . tv_sec = ( 0 < s ) ? s : 0 ;
    tv . tv_usec = ( 0 < u ) ? u : 0 ;

    while ( 0 > select ( 0, NULL, NULL, NULL, & tv )
      && EINTR == errno )
    { ++ i ; }

    return i ;
  }

  return -1 ;
}

static int msleep ( const int m )
{
  if ( 0 < m ) {
    struct pollfd pfd ;

    return poll ( & pfd, 0, m ) ;
  }

  errno = EINVAL ;
  return -1 ;
}

static void do_sleep ( long int s, long int m )
{
  if ( 0 < s || 0 < m ) {
#ifdef OSLinux
    struct timeval tv ;

    s = ( 0 < s ) ? s : 0 ;
    m = ( 0 < m ) ? m % ( 1000 * 1000 ) : 0 ;
    tv . tv_sec = s ;
    tv . tv_usec = m ;

    /* This only works correctly because the linux select updates
     * the elapsed time in the struct timeval passed to select !
     */
    while ( 0 > select ( 0, NULL, NULL, NULL, & tv )
      && EINTR == errno )
    ;
#else
    struct timespec ts, rem ;

    s = ( 0 < s ) ? s : 0 ;
    m = ( 0 < m ) ? 1000 * ( m % ( 1000 * 1000 ) ) : 0 ;
    ts . tv_sec = s ;
    ts . tv_nsec = m ;

    while ( 0 > nanosleep ( & ts, & rem ) && EINTR == errno )
    { ts = rem ; }
#endif
  }
}

/* sleep a number of seconds without being interrupted */
static void hard_sleep ( long int s, long int m )
{
  if ( 0 < s || 0 < m ) {
    struct timespec ts, rem ;

    s = ( 0 < s ) ? s : 0 ;
    m = ( 0 < m ) ? 1000 * ( m % ( 1000 * 1000 ) ) : 0 ;
    ts . tv_sec = s ;
    ts . tv_nsec = m ;

    while ( 0 > nanosleep ( & ts, & rem ) && EINTR == errno )
    { ts = rem ; }
  }
}

#if 0
/* non-failing allocation routines (init must not fail on errors) */
static void * imalloc ( const size_t size )
{
  void * m = malloc ( size ) ;

  while ( NULL == m ) {
    do_sleep ( 5, 0 ) ;
    m = malloc ( size ) ;
  }

  (void) memset ( m, 0, size ) ;
  return m ;
}

static void * irealloc ( void * const ptr, const size_t size )
{
  if ( ptr ) {
    if ( 0 < size ) {
      void * m = realloc ( ptr, size ) ;

      while ( NULL == m ) {
        do_sleep ( 5, 0 ) ;
        m = realloc ( ptr, size ) ;
      }

      //(void) memset ( m, 0, size ) ;
      return m ;
    } else {
      free ( ptr ) ;
    }
  } else if ( 0 <= size ) {
    return imalloc ( size ) ;
  }

  return NULL ;
}

static char * istrdup ( const char * const s )
{
  char * m ;
  const size_t len = 1 + str_len ( s ) ;

  m = imalloc ( len ) ;
  (void) memcpy ( m, s, len ) ;

  return m ;
}

static void * xmalloc ( const size_t size )
{
  void * res = malloc ( size ) ;

  if ( res ) { return res ; }

  (void) fputs ( "out of memory\n", stderr ) ;
  exit ( -1 ) ;

  return NULL ;
}

static void * xrealloc ( void * const ptr, const size_t size )
{
  void * res = realloc ( ptr, size ) ;

  if ( res ) { return res ; }

  (void) fputs ( "out of memory\n", stderr ) ;
  exit ( -1 ) ;

  return NULL ;
}

static char * xstrdup ( const char * const str )
{
  char * res ;

  if ( ! str ) { return NULL ; }

  res = strdup ( str ) ;

  if ( res ) { return res ; }

  (void) fputs ( "out of memory\n", stderr ) ;
  exit ( -1 ) ;

  return NULL ;
}
#endif

static void strncopy ( char * const dest, const char * const src, const size_t n )
{
  size_t i ;

  for ( i = 0 ; ( n > i ) && src [ i ] ; ++ i ) {
    dest [ i ] = src [ i ] ;
  }

  for ( ; n > i ; ++ i ) { dest [ i ] = '\0' ; }
}

static size_t strcopy ( char * const dest, const char * const src, const size_t n )
{
  if ( ( 0 < n ) && dest && src && * src ) {
    size_t i ;

    for ( i = 0 ; ( i + 1 < n ) && src [ i ] ; ++ i )
    { dest [ i ] = src [ i ] ; }

    dest [ i ] = '\0' ;
    return i ;
  }

  return 0 ;
}

/* our version of strlcat */
/*
static size_t str_cat ( char * dst, const char * src, const size_t size )
{
  char * d = dst ;
  const char *s = src ;
  size_t src_n = size ;
  size_t dst_n ;

  while ( 0 < src_n -- && * d ) { ++ d ; }
  dst_n = d - dst ;
  src_n = size - dst_n ;

  if ( 0 == src_n ) { return dst_n + str_len ( src ) ; }

  while ( * s ) {
    if ( 1 != src_n ) {
      * d ++ = * s ;
      -- src_n ;
    }
    ++ s ;
  }

  * d = '\0' ;

  return dst_n + ( s - src ) ;
}
*/

/* returns the last component (the base name) of a given path
 * without modifying the argument. if the path ends with a trailing
 * slash then an empty string is returned. */
/*
static const char * basename_c ( const char * const path )
{
  const char * slash = strrchr ( path, '/' ) ;

  if ( slash ) { return ++ slash ; }

  return path ;
}
*/

/* remove \n and anything after */
static char * chomp ( char * const str )
{
  if ( str && * str ) {
    register char c = 0 ;
    register int i ;

    for ( i = 0 ; str [ i ] ; ++ i ) {
      if ( '\n' == str [ i ] ) {
        str [ i ] = 0 ; c = 3 ;
      } else if ( c ) {
        str [ i ] = 0 ;
      }
    }
  }

  return str ;
}

/* "failsafe" version of the fork(2) syscall */
static pid_t xfork ( void )
{
  pid_t p = 0 ;

  while ( 0 > ( p = fork () ) && ENOSYS != errno ) {
    /* sleep a few seconds and try again */
    do_sleep ( 5, 0 ) ;
  }

  return p ;
}

static int wait4pid ( const pid_t pid )
{
  if ( 0 < pid && 0 == kill ( pid, 0 ) ) {
    int i ;
    pid_t p ;

    do {
      i = 0 ;
      p = waitpid ( pid, & i, 0 ) ;

      if ( 0 < p && pid == p ) {
        if ( WIFEXITED( i ) ) {
          return WEXITSTATUS( i ) ;
        } else if ( WIFSIGNALED( i ) ) {
          return WTERMSIG( i ) ;
        } else if ( WIFSTOPPED( i ) ) {
          return WSTOPSIG( i ) ;
        }

        return 0 ;
        break ;
      }
    } while ( 0 > p && EINTR == errno ) ;
  }

  return -1 ;
}

/* reap any zombie processes that might exist */
static pid_t reap ( const pid_t pid, const char nohang, int * const sp )
{
  pid_t p = 0 ;

  do {
    p = waitpid ( ( 1 < pid ) ? pid : -1, sp, nohang ? WNOHANG : 0 ) ;

    if ( 1 < pid && 0 < p && pid == p ) {
      return p ;
      break ;
    }
  } while ( 0 < p || ( 0 > p && EINTR == errno ) ) ;

  return p ;
}

/* tries to close a given fd */
static int close_fd ( const int fd )
{
  if ( 0 <= fd ) {
    int i ;

    do { i = close ( fd ) ; }
    while ( i && ( EINTR == errno ) ) ;

    return i ;
  }

  return -3 ;
}

/* close all possibly open files (descriptors) */
static void close_all ( const int upper )
{
  int i = 0 ;

  if ( 0 > upper ) {
    /* get the limit of the number of open files and close all file descriptors
     * from zero upto that limit (regardless if they are open).
     */
    i = (int) sysconf ( _SC_OPEN_MAX ) - 1 ;
    i = ( STDERR_FILENO <= i && 100001 > i ) ? i : 1023 ;
  } else {
    i = upper ;
  }

  while ( 0 <= i ) {
    while ( close ( i ) && ( EINTR == errno ) ) ;
    -- i ;
  }
}

static int dup_fd ( const int fd )
{
  if ( 0 <= fd ) {
    int i ;

    do { i = dup ( fd ) ; }
    while ( 0 > i && EINTR == errno ) ;

    return i ;
  }

  return -3 ;
}

static int dup_to ( const int o, const int n )
{
  if ( 0 <= o && 0 <= n ) {
    if ( o != n ) {
      int i ;

      do { i = dup2 ( o, n ) ; }
      while ( 0 > i && EINTR == errno ) ;

      return i ;
    }

    return n ;
  }

  return -3 ;
}

static int create_pipe ( const char nb, const char coe, int p [ 2 ] )
{
  int r = pipe ( p ) ;

  if ( r ) { return r ; }

  if ( nb ) {
    r += fcntl ( p [ 0 ], F_SETFL, O_NONBLOCK ) ;
    r += fcntl ( p [ 1 ], F_SETFL, O_NONBLOCK ) ;
  }

  if ( coe ) {
    r += fcntl ( p [ 0 ], F_SETFD, FD_CLOEXEC ) ;
    r += fcntl ( p [ 1 ], F_SETFD, FD_CLOEXEC ) ;
  }

  return r ;
}

/* checks if a given file descriptor has data pending */
static int fd_has_data ( const int fd )
{
  struct timeval tv ;
  fd_set fds ;

  tv . tv_sec = 0 ;
  tv . tv_usec = 0 ;
  FD_ZERO( & fds ) ;
  FD_SET( fd, & fds ) ;

  return 1 == select ( 1 + fd, & fds, NULL, NULL, & tv ) ;
}

static void octouch ( const char * const path )
{
  if ( path && * path ) {
    int i = open ( path, O_CREAT | O_RDWR, 00644 ) ;
    if ( 0 <= i ) { close_fd ( i ) ; }
  }
}

static int touch ( const char * const path )
{
  if ( path && * path ) {
    if ( mknod ( path, S_IFREG | 00644, 0 ) && EEXIST != errno )
    { return -1 ; }

    return 0 ;
  }

  return -3 ;
}

static int create ( const char * const path, mode_t mode,
  const uid_t uid, const gid_t gid )
{
  if ( path && * path ) {
    /*
    int i = touch ( path ) || chmod ( path, mode )
      || chown ( path, uid, gid ) ;

    if ( i ) { (void) printf ( "Failed creating %s properly\n", path ) ; }
    */
    mode = mode ? mode : 00644 ;
    mode &= 007777 ;

    return touch ( path ) || chmod ( path, mode )
      || chown ( path, uid, gid ) ;
  }

  return -3 ;
}

static int make_dir ( const char * const path, const mode_t mode )
{
  if ( path && * path ) {
    if ( mkdir ( path, mode ) && EEXIST != errno )
    { return -1 ; }

    return 0 ;
  }

  return -3 ;
}

static int make_fifo ( const char * const path, const mode_t mode )
{
  if ( path && * path ) {
    if ( mkfifo ( path, mode ) && EEXIST != errno )
    { return -1 ; }

    return 0 ;
  }

  return -3 ;
}

/* does a given path end with a slash ? */
static int ends_with_slash ( const char * const path )
{
  if ( path && * path ) {
    const size_t s = str_len ( path ) ;

    if ( 0 < s ) { return '/' == path [ s - 1 ] ; }
  }

  return 0 ;
}

/* e(uid)access(2) like test function */
static int xstest ( const char eff, const char * const path, int mode )
{
  if ( path && * path ) {
    struct stat st ;

    if ( 0 == stat ( path, & st ) && 0 < st . st_nlink ) {
      int i = 0 ;
      const uid_t u = eff ? geteuid () : getuid () ;
      const gid_t g = eff ? getegid () : getgid () ;
      mode &= F_OK | R_OK | W_OK | X_OK ;

      if ( 1 > mode || F_OK == mode ) { return 0 ; }
      else if ( u == st . st_uid ) {
        i = 0 ;

        if ( R_OK & mode ) { i |= S_IRUSR ; }
        if ( W_OK & mode ) { i |= S_IWUSR ; }
        if ( X_OK & mode ) { i |= S_IXUSR ; }

        i &= st . st_mode ;
        return i ? 0 : -1 ;
      } else if ( g == st . st_gid ) {
        i = 0 ;

        if ( R_OK & mode ) { i |= S_IRGRP ; }
        if ( W_OK & mode ) { i |= S_IWGRP ; }
        if ( X_OK & mode ) { i |= S_IXGRP ; }

        i &= st . st_mode ;
        return i ? 0 : -1 ;
      } else {
        i = 0 ;

        if ( R_OK & mode ) { i |= S_IROTH ; }
        if ( W_OK & mode ) { i |= S_IWOTH ; }
        if ( X_OK & mode ) { i |= S_IXOTH ; }

        i &= st . st_mode ;
        return i ? 0 : -1 ;
      }
    }
  }

  return -1 ;
}

/* check to see if file descriptors apply to the same file.
 * returns true if fd1 & fd2 refer to the same device and inode.
 */
static int fdcompare ( const int fd1, const int fd2 )
{
  if ( 0 <= fd1 && 0 <= fd2 ) {
    struct stat st1, st2 ;

    if ( 0 == fstat ( fd1, & st1 ) && 0 == fstat ( fd2, & st2 ) )
    {
      return st1 . st_dev == st2 . st_dev
        && st1 . st_ino == st2 . st_ino ;
    }
  }

  return 0 ;
}

#if 0
/* similar to above function but for fds using fstat.
 * checks if the file belonging to a fd still exists */
static int test_fd ( const int fd, const mode_t ftype, const mode_t perm )
{
  struct stat sb ;

  errno = 0 ;

  if ( 0 > fstat ( fd, & sb ) ) {
    switch ( errno ) {
      case ELOOP :
      case EACCES :
      case ENOTDIR :
      case ENOENT :
        return 0 ;
        break ;
      default :
        return 0 ;
        break ;
    }
  }

  if ( 0 == sb . st_nlink ) {
    return 0 ;
  }

  if ( S_IFMT & sb . st_mode & ftype ) {
    if ( perm ) {
      return perm & sb . st_mode ;
    }
  }

  return 0 ;
}

static int openreadclose ( const char * fn, char ** buf, unsigned long * len )
{
  int fd = open ( fn, O_RDONLY ) ;

  if ( 0 > fd ) {
    return fd ;
  }

  if ( NULL == * buf ) {
    * len = lseek ( fd, 0, SEEK_END ) ;
    lseek ( fd, 0, SEEK_SET ) ;
    * buf = (char *) malloc ( 1 + * len ) ;
    if ( NULL == * buf ) {
      (void) close ( fd ) ;
      return -2 ;
    }
  }

  * len = read( fd, * buf, * len ) ;
  if ( * len != (unsigned long) -1 ) {
    (* buf) [ * len ] = 0 ;
  }

  return close ( fd ) ;
}
#endif

/* secure replacement for the tmpfile function on Linux */
static FILE * tempfile ( void )
{
#ifdef O_TMPFILE
  /* Linux only */
  int fd = -2 ;
  mode_t m = umask ( 00077 ) ;

  fd = open ( _PATH_TMP, O_TMPFILE | O_RDWR | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR ) ;
  m = umask ( m ) ;
  if ( 0 > fd ) { return NULL ; }

  return fdopen ( fd, "rw" ) ;
#else
  /* Fallback on older GLIBC/Linux and actual UNIX systems */
  return tmpfile () ;
#endif
}

/* copies data between two given file descriptors */
static ssize_t fd_copy ( const int ifd, const int ofd, const size_t len )
{
#if defined (OSLinux)
  off_t off = 0 ;

  return sendfile ( ofd, ifd, & off, len ) ;
/*
  loff_t offi, offo ;
  offi = offo = 0 ;
  return copy_file_range ( ifd, & offi, ofd, & offo, len, 0 ) ;
*/
#else
  int s ;
  char buf [ 501 ] = { 0 } ;

  while ( 0 < ( s = read ( ifd, buf, sizeof ( buf ) - 1 ) ) ) {
    (void) write ( ofd, buf, s ) ;
  }

  return s ;
#endif
}

/* copy the contents of file src to file dst */
static int copy_file ( const char * const src, const char * const dst )
{
  const int ifd = open ( src, O_RDONLY | O_CLOEXEC ) ;
  const int ofd = open ( src, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 00644 ) ;

  if ( 0 <= ifd && 0 <= ofd ) {
    int i = -3 ;
    struct stat st ;

    if ( ( 0 == fstat ( ofd, & st ) ) && S_ISREG( st . st_mode )
      && ( 0 == fstat ( ifd, & st ) ) && S_ISREG( st . st_mode ) )
    {
      off_t o = lseek ( ifd, 0, SEEK_END ) ;
      o = ( o < st . st_size ) ? st . st_size : o ;
      (void) lseek ( ifd, 0, SEEK_SET ) ;
      i = ( 0 < o ) ? fd_copy ( ifd, ofd, o ) : -5 ;
    }

    close_fd ( ifd ) ;
    close_fd ( ofd ) ;
    return i ;
  }

  return -1 ;
}

/* rename/move a given file from src to dst */
static int move_file ( const char * const src, const char * const dst )
{
  int i = rename ( src, dst ) ;

  if ( i && ( EXDEV == errno ) ) {
    i = copy_file ( src, dst ) ;
    i = ( 0 > i ) ? i : unlink ( src ) ;
  }

  return i ;
}

/* tries to creates a given dir with all its parent dirs
 * (i. e. the whole path leading to that dir, hence mkpath)
 */
static int mkpath ( mode_t m, char * const p, const size_t s )
{
  if ( ( 0 < s ) && p && * p ) {
    char c = 0 ;
    size_t i = s - 1 ;

    /* ensure some basic mode sanity */
    m &= 007777 ;
    m |= 000700 ;

    /* remove any trailing slashes first */
    while ( 0 < i && '/' == p [ i ] ) {
      p [ i -- ] = '\0' ;
    }

    for ( i = 0 ; ( s > i ) && p [ i ] ; ) {
      i += strspn ( i + p, "/" ) ;
      i += strcspn ( i + p, "/" ) ;
      c = p [ i ] ;
      p [ i ] = '\0' ;

      if ( mkdir ( p, m ) ) {
        struct stat st ;
        const int e = errno ;

	if ( stat ( p, & st ) ) {
          errno = e ;
          return -1 ;
        } else if ( 0 == S_ISDIR( st . st_mode ) ) {
          errno = ENOTDIR ;
          return -1 ;
        }
      }

      if ( chmod ( p, m ) ) {
        return -1 ;
      }

      p [ i ] = c ;
    }

    return 0 ;
  }

  errno = EINVAL ;
  return -1 ;
}

/* recusrivly remove a given dir and its contents */
static int rmr ( const int dirfd, const char * const path )
{
  struct stat st ;

  if ( 0 == ( path && * path ) ) {
    errno = EINVAL ;
    return -2 ;
  } else if ( '.' == path [ 0 ] && '\0' == path [ 1 ] ) {
    errno = EINVAL ;
    return -3 ;
  } else if ( '.' == path [ 0 ] && '.' == path [ 1 ] && '\0' == path [ 2 ] ) {
    errno = ENOTEMPTY ;
    return -4 ;
  } else if ( fstatat ( dirfd, path, & st, AT_SYMLINK_NOFOLLOW ) ) {
    return -5 ;
  } else if ( S_ISDIR( st . st_mode ) ) {
    int i = unlinkat ( dirfd, path, AT_REMOVEDIR ) ;

    if ( i && ( EEXIST == errno || ENOTEMPTY == errno ) ) {
      const int fd = openat ( dirfd, path,
        O_RDONLY | O_PATH | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW ) ;

      if ( 0 > fd ) { return -6 ; }
      else {
        DIR * dp = fdopendir ( fd ) ;

        if ( dp ) {
          const char * str = NULL ;
          struct dirent * dep = NULL ;

          while ( NULL != ( dep = readdir ( dp ) ) ) {
            str = dep -> d_name ;

            if ( str && * str ) {
              if ( '.' == str [ 0 ] && '\0' == str [ 1 ] ) {
                continue ;
              } else if ( '.' == str [ 0 ] && '.' == str [ 1 ] && '\0' == str [ 2 ] ) {
                continue ;
              }

              i = rmr ( fd, str ) ? -7 : 0 ;
            }
          }

          (void) closedir ( dp ) ;
        }

        (void) close_fd ( fd ) ;
      }

      return unlinkat ( dirfd, path, AT_REMOVEDIR ) ? -8 : i ;
    }

    return i ;
  } else {
    return unlinkat ( dirfd, path, 0 ) ;
  }

  return -9 ;
}

/* remove a given file/path, calls rmr() for non emtpy dirs */
static int rm ( const char * const path )
{
  if ( path && * path ) {
    struct stat st ;

    if ( '.' == path [ 0 ] && '\0' == path [ 1 ] ) {
      errno = EINVAL ;
      return -3 ;
    } else if ( '.' == path [ 0 ] && '.' == path [ 1 ] && '\0' == path [ 2 ] ) {
      errno = ENOTEMPTY ;
      return -4 ;
    } else if ( lstat ( path, & st ) ) {
      return -5 ;
    } else if ( S_ISDIR( st . st_mode ) ) {
      const int r = rmdir ( path ) ;

      if ( r && ( EEXIST == errno || ENOTEMPTY == errno ) ) {
        return rmr ( AT_FDCWD, path ) ;
      }

      return r ;
    } else {
      return unlink ( path ) ;
    }
  } else {
    errno = EINVAL ;
  }

  return -2 ;
}

/* lookup a given username's UID in the passwd database */
static int getuser ( const char * const username )
{
  struct passwd * pwent ;

  if ( NULL == username ) { return -2 ; }
  pwent = getpwnam ( username ) ;
  if ( pwent ) { return pwent -> pw_uid ; }

  return -1 ;
}

/* lookup a given groupname's GID in the group database */
static int getgroup ( const char * const grpname )
{
  struct group * grpent ;

  if ( NULL == grpname ) { return -2 ; }
  grpent = getgrnam ( grpname ) ;
  if ( grpent ) { return grpent -> gr_gid ; }

  return -1 ;
}

/* creates a new SysV message queue */
static int mqv_open ( key_t key, const char create )
{
  if ( create ) {
    int e = 0, mqid = -3 ;
    struct msqid_ds mqs ;

    key = ( 1 > key ) ? 0x01 : key ;

    while ( 0 > ( mqid = msgget ( key, 00600 | IPC_CREAT | IPC_EXCL ) ) )
    {
      e = errno ;
      if ( EEXIST == e ) { (void) msgctl ( mqid, IPC_RMID, NULL ) ; }
      else if ( EACCES == e || ENOSPC == e ) { return -3 ; break ; }
      else { e = 0 ; (void) do_sleep ( 5, 0 ) ; }
    }

    if ( 0 <= mqid && 0 == msgctl ( mqid, IPC_STAT, & mqs ) ) {
      mqs . msg_perm . mode = 00600 ;
      mqs . msg_perm . uid = getuid () ;
      mqs . msg_perm . gid = getgid () ;
      (void) msgctl ( mqid, IPC_SET, & mqs ) ;
    }

    if ( e && ( 1 > errno ) ) { errno = e ; }
    return mqid ;
  }

  return msgget ( key, 0 ) ;
}

/* wrapper around sigwait(2) */
static int sig_wait ( const sigset_t * const ssp )
{
  int s = 0 ;
  const int i = sigwait ( ssp, & s ) ;

  if ( 0 < i ) { errno = i ; }

  return i ? -1 : s ;
}

/* restore default dispostions for all signals (except SIGCHLD) */
static void reset_sigs ( void )
{
  int i ;
  struct sigaction sa ;

  /* zero out the struct before use */
  (void) memset ( & sa, 0, sizeof ( struct sigaction ) ) ;
  sa . sa_flags = SA_RESTART ;
  sa . sa_handler = SIG_DFL ;
  (void) sigemptyset ( & sa . sa_mask ) ;

  for ( i = 1 ; NSIG > i ; ++ i ) {
    if ( SIGKILL != i && SIGSTOP != i && SIGCHLD != i ) {
      (void) sigaction ( i, & sa, NULL ) ;
    }
  }

  sa . sa_handler = pseudo_sighand ;
  (void) sigaction ( SIGCHLD, & sa, NULL ) ;

  /* unblock all signals */
  (void) sigprocmask ( SIG_SETMASK, & sa . sa_mask, NULL ) ;
}

static int redirio ( const char * const path )
{
  if ( path && * path && '/' == * path ) {
    const int fd = open ( path, O_RDWR | O_NONBLOCK | O_NOCTTY ) ;

    if ( 0 <= fd ) {
      (void) dup_to ( fd, 0 ) ;
      (void) dup_to ( fd, 1 ) ;
      (void) dup_to ( fd, 2 ) ;

      if ( 2 < fd ) { close_fd ( fd ) ; }
    }

    return 0 <= fd ;
  }

  return 0 ;
}

static pid_t vspawn ( char ** av, char ** env )
{
  pid_t p, q ;

  (void) fflush ( NULL ) ;

  /* vfork(2) instead of fork(2) */
  while ( 0 > ( p = vfork () ) && ENOSYS != errno ) {
    (void) delay ( 5, 0 ) ;
  }

  if ( 0 == p ) {
    /* child process */
    (void) setsid () ;
    (void) setpgid ( 0, 0 ) ;
    (void) execve ( * av, av, env ) ;
#if defined (__GLIBC__) && defined (_GNU_SOURCE)
    (void) execvpe ( * av, av, env ) ;
#endif
    (void) execvp ( * av, av ) ;
    _exit ( 127 ) ;
  } else if ( 0 < p ) {
    /* parent process */
    while ( 1 ) {
      q = waitpid ( p, NULL, 0 ) ;
      if ( 0 > q && EINTR != errno ) { break ; }
      else if ( 0 < q && q == p ) { break ; }
    }
  }

  return p ;
}

#if 0
static int spawn ( const uid_t uid, const gid_t gid,
  const char w, const char s, const char e,
  char ** argv, char ** envp )
{
  int i = -3 ;

  (void) fflush ( stdin ) ;
  (void) fflush ( stdout ) ;
  (void) fflush ( stderr ) ;
  /*
  while ( 0 > ( i = fork () ) && ENOMEM == errno )
  { (void) usleep ( 250 * 1000 ) ; }
  */
  i = fork () ;

  if ( 0 == i ) {
    /* child process */
    if ( s ) {
      (void) setsid () ;
      (void) setpgid ( 0, 0 ) ;
    }

    if ( 0 < uid ) {
      if ( setuid ( uid ) ) { perror ( "setuid failed" ) ; }
    }

    if ( 0 < gid ) {
      if ( setgid ( gid ) ) { perror ( "setgid failed" ) ; }
    }

    reset_sigs () ;

    if ( e ) {
      //envp = NULL ;
      envp = (char **) { NULL } ;
      environ = envp ;
      (void) execve ( * argv, argv, envp ) ;
    } else if ( envp ) {
      environ = envp ;
      (void) execve ( * argv, argv, envp ) ;
    } else {
      (void) execv ( * argv, argv ) ;
    }

    /* execv(e) failed, terminate process and return error code */
    _exit ( 111 ) ;
    return -11 ;
  } else if ( w && 0 < i ) {
    /* parent process */
    if ( 0 > waitpid ( i, & i, 0 ) ) {
      return -2 ;
    } else if ( WIFEXITED( i ) ) {
      return WEXITSTATUS( i ) ;
    } else if ( WIFSIGNALED( i ) ) {
      return 128 + WTERMSIG( i ) ;
    } else if ( WIFSTOPPED( i ) ) {
      return 128 + WSTOPSIG( i ) ;
    }
  } else if ( 0 > i ) {
    /* fork failed */
    /* perror ( "could not fork" ) ; */
    return -3 ;
  }

  return i ;
}
#endif

#if defined (OSsolaris) || defined (OSsunos5)
/* use sigsendset on Solaris to avoid signaling our own process */
static int kill_all_sol ( const int sig )
{
  if ( 0 <= sig && NSIG > sig ) {
    procset_t pset ;

    pset . p_op = POP_DIFF ;
    pset . p_lidtype = P_ALL ;
    pset . p_ridtype = P_PID ;
    pset . p_rid = P_MYID ;

    return sigsendset ( & pset, sig ) ;
  }

  return -3 ;
}
#endif

/* generic killall5 like function that kill(2)s/signals all running processes.
 * it does an exhaustive search for processes and kills ALL of them, EXCEPT;
 *	- itself
 *	- its parent process
 */
static void gen_kill_all ( const int s )
{
  if ( 0 < s && NSIG > s ) {
    pid_t p ;
    const pid_t mypid = getpid () ;
    const pid_t ppid = getppid () ;
    const pid_t m = sysconf ( _SC_CHILD_MAX ) ;

    for ( p = 3 ; m >= p ; ++ p ) {
      if ( mypid != p && ppid != p && 0 == kill ( p, 0 ) ) {
        (void) kill ( p, s ) ;
      }
    }
  }
}

/* kills all processes we are allowed to (except our own (P)PID and
 * possibly SID and kernel processes/threads)
 * regardless of /proc being mounted (Linux) or access to kvm
 * (BSD, SunOS, AIX)
 */
static int kill_all_procs ( const int sig, const char pg,
  const char pa, const char ses )
{
  if ( 0 <= sig && NSIG > sig ) {
    int i, j, m, r ;
    const int mypid = getpid () ;
    const int myppid = getppid () ;
    const int mysid = getsid ( 0 ) ;
    const int mypgid = getpgrp () ;

    r = 0 ;
    m = sysconf ( _SC_CHILD_MAX ) ;
    m = ( 1 < m ) ? m : 1 << 14 ;
#if defined (OSLinux)
    i = 3 ;
#else
    i = 2 ;
#endif

    while ( i <= m && 0 == kill ( i, 0 ) ) {
      if ( 1 > sig ) { ++ r ; goto inc ; }
      else if ( 2 > i || mypid == i ) { goto inc ; }
      else if ( pa && ( myppid == i ) ) { goto inc ; }
      else if ( ses ) {
        j = getsid ( i ) ;
        if ( 1 > j || mysid == j ) { goto inc ; }
      }

      /* do not signal kernel processes/threads ( (P)PID == 2) on Linux */
      j = getpgid ( i ) ;
      if ( 0 < j && 1 < i && mypid != i ) {
        if ( 0 == kill ( i, sig ) ) { ++ r ; }
        if ( pg && ( 1 < j ) && ( mypgid != j )
          && ( 0 == kill ( - i, sig ) ) )
        { ++ r ; }
      }

inc :
      ++ i ;
    }

    return r ;
  }

  return -1 ;
}

/* killall5 like function that kills all running processes
 * execpt our own, our parent process and all members of session 1.
 * send a signal to all processes except process #1 and the processes in
 * our own session.
 * this function uses a very simple approach. it does an
 * exhaustive search for processes and kills ALL of them, EXCEPT;
 *	- itself
 *	- its parent process (usually a shell)
 *	- processes w/ session id of 1 or lower (init, kernel threads)
 *
 * the benefits of doing an exhaustive search for processes is that
 * there is no need for the /proc fs (or similar) to be mounted.
 */
static void killall5 ( const int sig )
{
  register int i, s ;
#if defined (OSLinux)
  /* needed to open /proc and read its contents */
  DIR * dirp ;
  struct dirent * ent ;
#endif
  const pid_t mypid = getpid () ;
  const pid_t ppid = getppid () ;
  const pid_t mysid = getsid ( 0 ) ;

  if ( 1 > sig || NSIG <= sig ) { return ; }

  /* kill ( -1, SIGSTOP ) ; */
#if defined (OSLinux)
  dirp = opendir ( "/proc" ) ;

  if ( dirp ) {
    register char sig_sent = 0 ;

    while ( ( ent = readdir ( dirp ) ) )
    {
      i = atoi ( ent -> d_name ) ;
      if ( 1 < i ) {
        /* be careful about getsid when using PID arguments not
         * belonging to our own session
         */
        s = getsid ( i ) ;
        if ( 0 < s && mysid != s && mypid != i && ppid != i )
        {
          sig_sent = ( 0 == kill ( i, sig ) ) ? 3 : sig_sent ;
        }
      }
    }

    i = closedir ( dirp ) ;
    if ( sig_sent ) { return ; }
  }

  /* this is the Linux solution without relying on /proc being mounted.
   * is 65536 high enough ? pid_t is int on Linux.
   */
  for ( i = 2 ; 65536 > i ; ++ i )
  {
    /* again: be careful about getsid */
    s = getsid ( i ) ;
    if ( 1 < s && mysid != s && mypid != i && ppid != i ) {
      (void) kill ( i, sig ) ;
    }
  }

  /* use kvm on the BSDs and Solaris */
#elif defined (OSbsd)
#elif defined (OSsolaris)
#endif
  /* kill ( -1, SIGCONT ) ; */
}

static int setnoctty ( const int fd )
{
  if ( ( 0 <= fd ) && isatty ( fd ) ) {
    return ioctl ( fd, TIOCNOTTY, 0 ) ;
  } else { errno = ENOTTY ; }

  return -3 ;
}

/* set the controlling tty of the calling process */
/*
static void set_ctty ( const char * path, const int steal )
{
  int fd = -2 ;

  if ( path ) {
    fd = open ( path, O_RDONLY ) ;
    if ( 0 > fd ) {
      perror ( "open failed" ) ;
      return ;
    }
  }

#ifdef OSLinux
  if ( 0 > ioctl ( fd, TIOCSCTTY, steal ) ) {
    perror ( "could not set controlling terminal" ) ;
  }
#else
#endif
}
*/

/* searches a given pattern in a file (like grep -q) */
static int file_regex ( const char * const file, const char * const pat )
{
  int r = 0, res = 0 ;
  FILE * fp = NULL ;
  regex_t re ;
  char buf [ 128 ] = { 0 } ;

  if ( 0 == ( file && pat && * file && * pat ) ) {
    return 0 ;
  }

  fp = fopen ( file, "r" ) ;
  if ( NULL == fp ) {
    return 0 ;
  }

  r = regcomp ( & re, pat, REG_NOSUB | REG_EXTENDED ) ;
  if ( r ) {
    /* pattern failed to compile */
    (void) fclose ( fp ) ;
    (void) regerror ( r, & re, buf, 127 ) ;
    regfree ( & re ) ;
    (void) fprintf ( stderr, "error compiling regex pattern \"%s\" : %s\n",
      pat, buf ) ;
    return 0 ;
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
  (void) fclose ( fp ) ;
  regfree ( & re ) ;

  return res ;
}

/* see if a given directory is a mountpoint */
static int is_mount_point ( const char * const path )
{
  if ( path && * path ) {
    struct stat st ;

    if ( ( 0 == lstat ( path, & st ) ) && S_ISDIR( st . st_mode ) ) {
      if (
#if defined (OSLinux)
        0 < st . st_ino && 4 > st . st_ino
#else
        /* this is traditional, and what FreeBSD/PC-BSD does.
         * on-disc volumes on Linux mostly do this, too.
         */
        2 == st . st_ino
#endif
      ) { return 1 ; }
      else {
        unsigned int i ;
        struct stat st2 ;
        char buf [ 256 ] = { 0 } ;
        const size_t s = sizeof ( buf ) - 5 ;

        for ( i = 0 ; s > i && '\0' != path [ i ] ; ++ i ) {
          buf [ i ] = path [ i ] ;
        }

        buf [ i ++ ] = '/' ;
        buf [ i ++ ] = '.' ;
        buf [ i ++ ] = '.' ;
        buf [ i ] = '\0' ;

        if ( ( 0 == lstat ( buf, & st2 ) ) && S_ISDIR( st2 . st_mode ) ) {
          if ( st2 . st_dev != st . st_dev ) { return 1 ; }
          /* this is true for the root dir "/" */
          else if ( st2 . st_ino == st . st_ino ) { return 1 ; }
        }
      }
    }
  }

  return 0 ;
}

/* requires /proc to be mounted */
static int mtab_mount_point ( const char * const path, const char * const mtab )
{
#if defined (OSLinux)
  if ( path && * path ) {
    char r = 0 ;
    FILE * fp = NULL ;
    struct mntent * mep ;

    if ( mtab && * mtab ) { fp = setmntent ( mtab, "r" ) ; }
    if ( NULL == fp ) { fp = setmntent ( "/proc/self/mounts", "r" ) ; }
    if ( NULL == fp ) { return 0 ; }

    while ( NULL != ( mep = getmntent ( fp ) ) ) {
      if ( path [ 0 ] == mep -> mnt_dir [ 0 ]
        && 0 == strcmp ( mep -> mnt_dir, path ) )
      {
        r = 1 ;
        break ;
      }
    }

    endmntent ( fp ) ;
    return r ? 1 : 0 ;
  }
#endif

  return 0 ;
}

/* requires /proc to be mounted */
static int is_tmpfs ( const char * const path, const char * const mtab )
{
#if defined (OSLinux)
  if ( path && * path ) {
    char r = 0 ;
    FILE * fp = NULL ;
    struct mntent * mep = NULL ;

    if ( mtab && * mtab ) { fp = setmntent ( mtab, "r" ) ; }
    if ( NULL == fp ) { fp = setmntent ( "/proc/self/mounts", "r" ) ; }
    if ( NULL == fp ) { return 0 ; }

    while ( NULL != ( mep = getmntent ( fp ) ) ) {
      if ( 0 == strcmp ( path, mep -> mnt_dir ) ) { break ; }
    }

    if ( NULL != mep && ( 't' == mep -> mnt_type [ 0 ] )
      && 0 == strcmp ( "tmpfs", mep -> mnt_type ) )
    { r = 1 ; }

    endmntent ( fp ) ;
    return r ? 1 : 0 ;
  }
#endif

  return 0 ;
}

/* set terminal settings to reasonable defaults */
static int reset_term ( const int fd )
{
  int i = 1 ;

  if ( 0 <= fd && 0 != isatty ( fd ) ) {
    struct termios tio ;

    i = tcgetattr ( fd, & tio ) ;
    if ( i ) { return i ; }

    /* set control chars */
#if 0
    tio . c_cc [ VINTR ]	= 3	; /* C-c */
    tio . c_cc [ VQUIT ]	= 28	; /* C-\ */
    tio . c_cc [ VERASE ]	= 127	; /* C-? */
    tio . c_cc [ VKILL ]	= 21	; /* C-u */
    tio . c_cc [ VEOF ]		= 4	; /* C-d */
    tio . c_cc [ VSTART ]	= 17	; /* C-q */
    tio . c_cc [ VSTOP ]	= 19	; /* C-s */
    tio . c_cc [ VSUSP ]	= 26	; /* C-z */
#endif
    tio . c_cc [ VINTR ]	= CINTR ;
    tio . c_cc [ VQUIT ]	= CQUIT ;
    tio . c_cc [ VERASE ]	= CERASE ; /* ASCII DEL (0177) */
    tio . c_cc [ VKILL ]	= CKILL ;
    tio . c_cc [ VEOF ]		= CEOF ;
    tio . c_cc [ VTIME ]	= 0 ;
    tio . c_cc [ VMIN ]		= 1 ;
    tio . c_cc [ VSWTC ]	= _POSIX_VDISABLE ;
    tio . c_cc [ VSTART ]	= CSTART ;
    tio . c_cc [ VSTOP ]	= CSTOP ;
    tio . c_cc [ VSUSP ]	= CSUSP ;
    tio . c_cc [ VEOL ]		= _POSIX_VDISABLE ;
    tio . c_cc [ VREPRINT ]	= CREPRINT ;
    tio . c_cc [ VDISCARD ]	= CDISCARD ;
    tio . c_cc [ VWERASE ]	= CWERASE ;
    tio . c_cc [ VLNEXT ]	= CLNEXT ;
    tio . c_cc [ VEOL2 ]	= _POSIX_VDISABLE ;

#ifdef OSLinux
    /* use line discipline 0 */
    tio . c_line = 0 ;
#endif
#ifndef CRTSCTS
#  define CRTSCTS	0
#endif
    /* make it be sane */
    tio . c_cflag &= CBAUD | CBAUDEX | CSIZE | CSTOPB | PARENB
      | PARODD | CRTSCTS ;
    tio . c_cflag |= CREAD | HUPCL | CLOCAL ;
    tio . c_cflag &= CRTSCTS | PARODD | PARENB | CSTOPB | CSIZE | CBAUDEX | CBAUD ;
    tio . c_cflag |= CLOCAL | HUPCL | CREAD ;
    /* input modes */
    /* enable start/stop input and output control + map CR to NL on input */
    /* IGNPAR | IXANY */
    tio . c_iflag = ICRNL | IXON | IXOFF ;
#ifdef IUTF8
    tio . c_iflag |= IUTF8 ;
#endif
    /* output modes */
    /* Map NL to CR-NL on output */
    tio . c_oflag = ONLCR | OPOST ;
    /* local modes */
    /* ECHOPRT */
    tio . c_lflag = ECHO | ECHOCTL | ECHOE | ECHOK | ECHOKE |
      ICANON | IEXTEN | ISIG ;

    i += tcsetattr ( fd, TCSANOW, & tio ) ;
    /* don't care about non-transmitted output data and non-read input data */
    i += tcflush ( fd, TCIOFLUSH ) ;
  }

  return i ;
}

/* change the active virtual console */
static int chvt ( int vt )
{
#if defined (OSLinux)
  int fd ;

  vt = ( 0 < vt && 12 >= vt ) ? vt : 1 ;
  fd = open ( CONSOLE, O_RDONLY ) ;

  if ( 0 > fd ) { return -1 ; }
  else {
    char bytes [ 2 ] = { 0 } ;
    bytes [ 0 ] = 11 ;
    bytes [ 1 ] = (char) vt ;
    (void) ioctl ( fd, TIOCLINUX, bytes ) ;
    (void) close_fd ( fd ) ;
  }

  fd = open ( VT_MASTER, O_RDONLY ) ;
  if ( 0 <= fd ) {
    int i = ioctl ( fd, VT_ACTIVATE, vt ) ;

    if ( 0 == i ) { i += ioctl ( fd, VT_WAITACTIVE, vt ) ; }
    (void) close_fd ( fd ) ;
    return i ;
  }
#endif

  return -1 ;
}

/*
 * access the system u/wtmp databases
 * routines to read/write/update the system u/wtmp records
 */

/* writes a single record to both utmp and wtmp */
static int write_utmp ( struct utmp * const utp )
{
  /* ensure the utmp file is writable */
  int i = open ( UTMP_FILE, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 00644 ) ;

  if ( 0 > i ) {
    return 1 ;
  } else {
    int r = 0 ;

    while ( close ( i ) && ( EINTR == errno ) ) ;

    (void) utmpname ( UTMP_FILE ) ;

    if ( ( RUN_LVL == utp -> ut_type ) || ( BOOT_TIME == utp -> ut_type ) )
    {
      struct utsname uts ;

      strncopy ( utp -> ut_id, "~~", sizeof ( utp -> ut_id ) ) ;
      strncopy ( utp -> ut_line, "~", sizeof ( utp -> ut_line ) ) ;

      if ( 0 == uname ( & uts ) ) {
        strncopy ( utp -> ut_host, uts . release,
          sizeof ( utp -> ut_host ) ) ;
      }
    } else if ( DEAD_PROCESS == utp -> ut_type ) {
      struct utmp * utp2 = NULL ;

      setutent () ;
      /* better search the right utmp entry by PID only instead */
      /* utp2 = getutid ( & ut ) ; */

      while ( NULL != ( utp2 = getutent () ) ) {
        if ( NULL != utp2 && utp2 -> ut_pid == utp -> ut_pid ) {
          strncopy ( utp -> ut_line, utp2 -> ut_line,
            sizeof ( utp -> ut_line ) ) ;
          /* zero out certain struct fields again ? */
          (void) memset ( utp -> ut_user, 0, sizeof ( utp -> ut_user ) ) ;
          (void) memset ( utp -> ut_host, 0, sizeof ( utp -> ut_host ) ) ;
          (void) memset ( & utp -> ut_tv, 0, sizeof ( utp -> ut_tv ) ) ;
          break ;
        }
      }
    }

    if ( DEAD_PROCESS != utp -> ut_type ) {
      /* add current time in seconds + micro seconds */
      /*
      struct timeval tv ;

      if ( gettimeofday ( & tv, NULL ) ) {
        utp -> ut_tv . tv_usec = 0 ;
        utp -> ut_tv . tv_sec = time ( NULL ) ;
      } else {
        utp -> ut_tv . tv_usec = tv . tv_usec ;
        utp -> ut_tv . tv_sec = tv . tv_sec ;
      }
      */

      /* add current time in seconds (without micro seconds) */
      utp -> ut_tv . tv_usec = 0 ;
      utp -> ut_tv . tv_sec = time ( NULL ) ;
    }

    /* update the utmp database now */
    setutent () ;
    r = pututline ( utp ) ? 0 : 3 ;
    endutent () ;
    /* ensure the wtmp file is writable */
    i = open ( WTMP_FILE, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 00644 ) ;

    if ( 0 > i ) {
      return 2 + r ;
    } else {
      while ( close ( i ) && ( EINTR == errno ) ) ;
      /* update the wtmp database now */
      updwtmp ( WTMP_FILE, utp ) ;
    }

    return r ;
  } /* end if */

  return -1 ;
}

static int utmp_create ( const char * const name, const gid_t ugid )
{
  if ( name && * name ) {
    struct stat st ;
    int i = 0 ;

    if ( 0 == lstat ( name, & st ) ) {
      if ( ( S_IFREG & st . st_mode ) && 0 < st . st_nlink ) {
        (void) chmod ( name, 00644 ) ;
      } else if ( 0 == unlink ( name ) || 0 == rmdir ( name ) ) {
        goto trywrite ;
      } else {
        return -3 ;
      }
    } else if ( ENOENT == errno ) {
trywrite :
      /*
      i = mknod ( name, 00644 | S_IFREG, 0 ) ;
      */
      i = open ( name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 00644 ) ;
      if ( 0 > i ) { return -1 ; }
      else { while ( close ( i ) && EINTR == errno ) ; }
    }

    if ( 0 < ugid ) {
      (void) chmod ( name, 00664 ) ;
      (void) chown ( name, 0, ugid ) ;
    } else {
      (void) chmod ( name, 00644 ) ;
      (void) chown ( name, 0, 0 ) ;
    }

    /*
    return access ( name, W_OK ) ;
    */
    return 0 ;
  }

  return 1 ;
}

static int utmp_set_boot ( void )
{
  struct utmp ut ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = BOOT_TIME ;
  ut . ut_pid = 0 ;
  strncopy ( ut . ut_user, "reboot", sizeof ( ut . ut_user ) ) ;
  return write_utmp ( & ut ) ;
}

static int utmp_set_halt ( void )
{
  struct utmp ut ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = RUN_LVL ;
  ut . ut_pid = 0 ;
  strncopy ( ut . ut_user, "shutdown", sizeof ( ut . ut_user ) ) ;
  return write_utmp ( & ut ) ;
}

static int utmp_set_runlevel ( const int now, const int prev )
{
  struct utmp ut ;
  char n = ( 0 <= now && 9 >= now ) ? '0' + now : '3' ;
  char p = ( 0 <= prev && 9 >= prev ) ? '0' + prev : 0 ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = RUN_LVL ;
  ut . ut_pid = n + 256 * p ;
  strncopy ( ut . ut_user, "runlevel", sizeof ( ut . ut_user ) ) ;
  return write_utmp ( & ut ) ;
}

static int utmp_set_dead ( const pid_t pid )
{
  struct utmp ut ;

  /* zero out the utmp entry struct before use */
  (void) memset ( & ut, 0, sizeof ( ut ) ) ;
  ut . ut_type = DEAD_PROCESS ;
  ut . ut_pid = ( 0 < pid ) ? pid : getpid () ;
  return write_utmp ( & ut ) ;
}

static int utmp_set_init ( const char * const id, const char * const line )
{
  if ( id && line && * id && * line ) {
    struct utmp ut ;

    /* zero out the utmp entry struct before use */
    (void) memset ( & ut, 0, sizeof ( ut ) ) ;
    ut . ut_type = INIT_PROCESS ;
    ut . ut_pid = getpid () ;
    strncopy ( ut . ut_id, id, sizeof ( ut . ut_id ) ) ;
    strncopy ( ut . ut_line, line, sizeof ( ut . ut_line ) ) ;
    return write_utmp ( & ut ) ;
  }

  return -3 ;
}

static int utmp_set_login ( const char * const id, const char * const line )
{
  if ( id && line && * id && * line ) {
    struct utmp ut ;

    /* zero out the utmp entry struct before use */
    (void) memset ( & ut, 0, sizeof ( ut ) ) ;
    ut . ut_type = LOGIN_PROCESS ;
    ut . ut_pid = getpid () ;
    strncopy ( ut . ut_id, id, sizeof ( ut . ut_id ) ) ;
    strncopy ( ut . ut_line, line, sizeof ( ut . ut_line ) ) ;
    strncopy ( ut . ut_user, "LOGIN", sizeof ( ut . ut_user ) ) ;
    return write_utmp ( & ut ) ;
  }

  return -3 ;
}

