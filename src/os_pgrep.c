/*
 * find/match running processes by pid, executable, owner etc
 */

#if defined(__linux__) || defined(__GNU__) || \
  (defined (__FreeBSD_kernel__) && defined(__GLIBC__))


/* checks if a given pid belongs to an executable given by it's
 * absolute (i. e. starting with a '/' !) path.
 */
static int pid_is_exec ( const int pid, char * exe )
{
  if ( ( 0 < pid ) && exe && ( '/' == * exe ) ) {
    int i = 0 ;
    struct stat st ;
    /*
    int c = 0, i = 0 ;
    FILE * fp = NULL ;
    char buf [ 33 ] = { 0 } ;

    exe = basename_c ( exe ) ;
    (void) snprintf( buf, sizeof ( buf ), "/proc/%d/stat", pid ) ;
    fp = fopen ( buf, "r" ) ;

    if ( fp ) {
      while ( EOF != ( c = getc ( fp ) ) && '(' != c ) ;

      if ( '(' == c ) {
        while ( ( c = getc ( fp ) ) != EOF && c == * exe )
        { ++ exe ; }

        if ( ')' == c && * exe == '\0' ) { i = 1 ; }
      }

      (void) fclose ( fp ) ;
    }
    */
    i = stat ( exe, & st ) ;
    if ( ( 0 == i ) && S_ISREG( st . st_mode ) && ( 0 < st . st_nlink )
      && ( 0 < st . st_size ) )
    {
      //readlink () ;
    }
  }

  return 0 ;
}

static int pid_is_argv ( const int pid, const char ** argv )
{
  if ( 0 < pid && argv && * argv && ** argv ) {
    int fd ;
    ssize_t bytes ;
    char * p ;
    char cmd [ 33 ] = { 0 } ;
    char buf [ 1 + PATH_MAX ] = { 0 } ;

    (void) snprintf ( cmd, sizeof ( cmd ) - 1,
      "/proc/%d/cmdline", pid ) ;
    fd = open ( cmd, O_RDONLY ) ;

    if ( 0 > fd ) { return 0 ; }
    else {
      bytes = read ( fd, buf, sizeof ( buf ) - 1 ) ;
      (void) close_fd ( fd ) ;
    }

    if ( 0 > bytes ) { return 0 ; }
    buf [ bytes ] = '\0' ;
    p = buf ;

    while ( argv && * argv ) {
      if ( 0 != strcmp ( * argv, p ) ) { return 0 ; }

      ++ argv ;
      p += 1 + str_len ( p ) ;
      if ( (unsigned) (p - buffer) > sizeof ( buffer ) )
      { return 0 ; }
    }

    return 1 ;
  }

  return 0 ;
}

static int Lfind_proc ( lua_State * L )
{
  /* args: const char * exec, char ** argv, uid_t uid, pid_t pid */
  char container_pid = 0 ;
  char openvz_host = 0 ;
  char * line = NULL ;
  size_t len = 0 ;
  pid_t p ;
  char * pp = NULL ;
  struct dirent * entry ;
  FILE * fp ;
  struct stat sb ;
  char buffer [ PATH_MAX ] ;
  DIR * procdir = opendir ( "/proc" ) ;

  if ( NULL == procdir ) { return 0 ; }

  /* If /proc/self/status contains EnvID: 0, then we are an OpenVZ host,
   * and we will need to filter out processes that are inside containers
   * from our list of pids.
   */
  if ( fexists ( "/proc/self/status" ) ) {
    fp = fopen ( "/proc/self/status", "r" ) ;

    if ( fp ) {
      while ( 0 == feof ( fp ) ) {
        rc_getline ( & line, & len, fp ) ;
        if ( 0 == strncmp ( line, "envID:\t0", 8 ) ) {
          openvz_host = 1 ; break ;
        }
      }

      (void) fclose( fp ) ;
    }
  }

  while ( NULL != ( entry = readdir ( procdir ) ) )
  {
		if ( sscanf( entry -> d_name, "%d", & p ) != 1 )
			continue ;
		if ( openrc_pid != 0 && openrc_pid == p )
			continue ;
		if ( pid != 0 && pid != p )
			continue ;
		if ( uid ) {
			snprintf( buffer, sizeof( buffer ), "/proc/%d", p ) ;
			if ( stat( buffer, & sb ) != 0 || sb . st_uid != uid )
				continue ;
		}
		if ( exec && 0 == pid_is_exec ( p, exec ) )
			continue ;
		if ( argv &&
		    0 == pid_is_argv ( p, (char **) argv ) )
			continue ;
		/* If this is an OpenVZ host, filter out container processes */
		if ( openvz_host ) {
			snprintf( buffer, sizeof( buffer ), "/proc/%d/status", p ) ;
			if ( fexists( buffer ) ) {
				fp = fopen( buffer, "r" ) ;
				if ( ! fp )
					continue ;
				while ( ! feof( fp ) ) {
					rc_getline( & line, & len, fp ) ;
					if ( strncmp( line, "envID:", 6 ) == 0 ) {
						container_pid = ! ( strncmp( line, "envID:\t0", 8 ) == 0 ) ;
						break ;
					}
				}
				fclose( fp ) ;
			}
		}
		if ( container_pid )
			continue ;
		if ( ! pids ) {
			pids = xmalloc( sizeof( * pids ) ) ;
			LIST_INIT( pids ) ;
		}
		pi = xmalloc( sizeof( *pi ) ) ;
		pi -> pid = p ;
		LIST_INSERT_HEAD( pids, pi, entries ) ;
  }

  if ( line ) { free ( line ) ; }
  if ( procdir ) { (void) closedir ( procdir ) ; }

  return pids ;
}


#elif defined (OSbsd)


# if defined (OSnetbsd) || defined (OSopenbsd)
#  define _KVM_GETPROC2
#  define _KINFO_PROC kinfo_proc2
#  define _KVM_GETARGV kvm_getargv2
#  define _GET_KINFO_UID(kp) (kp.p_ruid)
#  define _GET_KINFO_COMM(kp) (kp.p_comm)
#  define _GET_KINFO_PID(kp) (kp.p_pid)
#  define _KVM_PATH NULL
#  define _KVM_FLAGS KVM_NO_FILES
# elif defined (OSfreebsd) || defined (OSdragonfly)
#  ifndef KERN_PROC_PROC
#    define KERN_PROC_PROC KERN_PROC_ALL
#  endif
#  define _KINFO_PROC kinfo_proc
#  define _KVM_GETARGV kvm_getargv
#  if defined (OSdragonfly)
#    define _GET_KINFO_UID(kp) (kp.kp_ruid)
#    define _GET_KINFO_COMM(kp) (kp.kp_comm)
#    define _GET_KINFO_PID(kp) (kp.kp_pid)
#  elif defined (OSfreebsd)
#    define _GET_KINFO_UID(kp) (kp.ki_ruid)
#    define _GET_KINFO_COMM(kp) (kp.ki_comm)
#    define _GET_KINFO_PID(kp) (kp.ki_pid)
#  endif
#  define _KVM_PATH _PATH_DEVNULL
#  define _KVM_FLAGS O_RDONLY
# endif

static int Lfind_pids ( lua_State * L )
{
  /* args: char * exec, char ** argv, uid_t uid, pid_t pid */
  int i = 0 ;
  int processes = 0 ;
  int match = 0 ;
  int pargc = 0 ;
  char ** pargv ;
  pid_t p = 0 ;
  char ** arg ;
  static kvm_t * kd = NULL;
  struct _KINFO_PROC * kp = NULL ;
  char errbuf [ 1 + _POSIX2_LINE_MAX ] = { 0 } ;

  if ( ( kd = kvm_openfiles ( _KVM_PATH, _KVM_PATH,
    NULL, _KVM_FLAGS, errbuf ) ) == NULL )
  {
    (void) fprintf ( stderr, "kvm_open: %s\n", errbuf ) ;
    return 0 ;
  }

#ifdef _KVM_GETPROC2
  kp = kvm_getproc2 ( kd, KERN_PROC_ALL, 0, sizeof ( * kp ), & processes ) ;
#else
  kp = kvm_getprocs ( kd, KERN_PROC_PROC, 0, & processes ) ;
#endif
  if ( ( kp == NULL && processes > 0 ) || (kp != NULL && processes < 0 ) )
  {
    (void) fprintf ( stderr, "kvm_getprocs: %s\n", kvm_geterr ( kd ) ) ;
    kvm_close ( kd ) ;
    return 0 ;
  }

  if ( exec ) { exec = basename_c ( exec ) ; }

  for ( i = 0 ; i < processes ; ++ i )
  {
    p = _GET_KINFO_PID( kp [ i ] ) ;
    if ( 0 < pid && pid != p ) continue ;

    if ( uid != 0 && uid != _GET_KINFO_UID(kp[i]))
      continue ;

    if ( exec ) {
      if ( ! _GET_KINFO_COMM( kp [ i ] ) ||
        0 != strcmp ( exec, _GET_KINFO_COMM( kp [ i ] ) ) )
      { continue ; }
    }

    if ( argv && * argv ) {
      pargv = _KVM_GETARGV( kd, & kp [ i ], pargc ) ;
      if ( ! pargv || ! * pargv ) continue ;
      arg = argv ;
      match = 1 ;

      while ( * arg && * pargv )
				if ( strcmp( * arg ++, * pargv ++ ) != 0) {
					match = 0 ;
					break ;
				}
      if ( 0 == match ) continue ;
    }

    /*
    if ( NULL == pids ) {
      pids = xmalloc ( sizeof ( * pids ) ) ;
      LIST_INIT( pids ) ;
    }

    pi = xmalloc ( sizeof ( * pi ) ) ;
    pi -> pid = p ;
    LIST_INSERT_HEAD( pids, pi, entries ) ;
    */
  }

  kvm_close ( kd ) ;

  return 0 ;
}


#elif defined (OSsolaris) || defined (OSsunos5)

#else
# warning "Platform not (yet ?) supported !"

static int Lfind_proc ( lua_State * L )
{
  return 0 ;
}

#endif

