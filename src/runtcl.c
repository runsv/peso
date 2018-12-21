/*
 * runtcl.c
 *
 * interpreter for TCL scripts.

OCaml stdlib: look for relevant file + unix functions

Handling of (POSIX) errors in failing sys/library calls:
 Tcl_SetErrno ( errno ) ;
 Tcl_AddErrorInfo ( T, "fu() failed: " ) ;
 Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
 return TCL_ERROR ;

strcmds:
  vfork_and_exec
  syslog()
  kexec_load
  pivot_root, swapo(n,ff)
  write (uw)tmp
  net: ifup, ifconfig, etc

objcmds:
  ensembles:
  id user, uid, convert, group, gid, effective, host, process
  signal default, ignore, error, trap, (g,s)et, (un)block

  normal:
  alarm, execl, chroot, fork, (pg)kill, (sys)link, nice, readdir, sleep,
  system, sync, times, umask, wait(pid)

  file:
  ch(grp,mod,own), dup, fcntl, f(un)lock, (f)stat, ftruncate, pipe, select,
  (read,write)_file (read, write)

  unistd.h :
  mass mknod: mksock, mkfile, mkfifos, mkdev, ...
  exec*, (v)fork, nice
  access, chdir, alarm
  (l)ch(mod,own), get(u,g,p(p,g),s)id, getpgrp, (g,s)ethost(id,name),
  (g,s)etlogin,
  link, pipe, readlink
  setsid, setpgid, setr(e)s(gu)id, symlink, truncate, ttyname

  resources.h :
  (g,s)et(rlimit,priority,rusage)

  signal.h :
  kill(pg), raise

  time.h :
  nanosleep, time, (g,s)ettimeofday

  wait.h :
  wait((p)id)

  sysv ipc:
  msg queues

  net/if.h:
  if_nametoindex(3), if_nameindex

  ifaddrs.h:
  getifaddrs(3)

  misc:
  utime
  creat, basename,

*********************************************************************/

#include "common.h"
#include <tcl.h>

#ifdef WANT_TK
# include <tk.h>
#endif

#define OBJSTREQU(obj1, str1) \
	( 0 == strcmp ( Tcl_GetStringFromObj ( obj1, NULL ), str1 ) )

#define OBJSTRNEQU(obj1, str1, cnt) \
	( 0 == strncmp ( Tcl_GetStringFromObj ( obj1, NULL), str1, cnt ) )

/* flag constants */
enum {
  FTEST_NOFOLLOW	= 0x01,
  FTEST_ZERO		= 0x02,
  FTEST_NONZERO		= 0x04,
} ;

extern int do_reboot ( const int how ) ;

/* set process resource (upper) limits */
static int set_rlimits ( void )
{
  struct rlimit rlim ;

  (void) getrlimit ( RLIMIT_CORE, & rlim ) ;
  rlim . rlim_cur = 0 ;

  if ( 0 == geteuid () ) { rlim . rlim_max = RLIM_INFINITY ; }

  /* set the SOFT (!!) default upper limit of coredump sizes to zero
   * (i. e. disable coredumps). can raised again later in child/sub
   * processes as needed.
   */
  return setrlimit ( RLIMIT_CORE, & rlim ) ;
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

static int fs_perm ( Tcl_StatBuf * const stp, mode_t m )
{
  mode_t i = 0 ;
  m &= 00007 ;

  if ( geteuid () == stp -> st_uid ) {
    i = ( m << 6 ) & ( stp -> st_mode ) ;
  } else if ( getegid () == stp -> st_gid ) {
    i = ( m << 3 ) & ( stp -> st_mode ) ;
  } else {
    i = m & ( stp -> st_mode ) ;
  }

  return i ? 1 : 0 ;
}

static int fs_dostat ( Tcl_Obj * const optr,
  const unsigned long int f, const mode_t mode )
{
  if ( NULL != optr ) {
    const mode_t perm = 007777 & mode ;
    Tcl_StatBuf st ;
    int i = 1 ;

    if ( ( FTEST_NOFOLLOW & f ) ?
      Tcl_FSLstat ( optr, & st ) : Tcl_FSStat ( optr, & st ) )
    { return 0 ; }
    else if ( 1 > st . st_nlink ) { return 0 ; }

    if ( 07000 & perm ) {
      i = ( 07000 & perm & st . st_mode ) ? 1 : 0 ;
    }

    if ( i && ( S_IFMT & mode ) ) {
      i = ( S_IFMT & mode & st . st_mode ) ? 1 : 0 ;
    }

    if ( 0 == i ) {
      return 0 ;
    } else if ( FTEST_NONZERO & f ) {
      i = ( 0 < st . st_size ) ? 1 : 0 ;
    } else if ( FTEST_ZERO & f ) {
      i = ( 0 == st . st_size ) ? 1 : 0 ;
    }

    if ( i && ( 00007 & perm ) ) {
      i = fs_perm ( & st, 00007 & perm ) ;
    }

    return i ;
  }

  return 0 ;
}

static int fs_test ( Tcl_Interp * T, const int objc, Tcl_Obj * const * objv,
  const unsigned long int f, const mode_t mode )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 1 ; objc > i ; ++ i ) {
      if ( NULL == objv [ i ] ) {
        return TCL_ERROR ;
      } else if ( 0 == fs_dostat ( objv [ i ], f, mode ) ) {
        Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 0 ) ;
        return TCL_OK ;
      }
    }

    Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 1 ) ;
    return TCL_OK ;
  }

  Tcl_AddErrorInfo ( T, "path arguments required" ) ;
  return TCL_ERROR ;
}

static int fs_acc ( Tcl_Interp * T, const int objc, Tcl_Obj * const * objv,
  const int mode )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 1 ; objc > i ; ++ i ) {
      if ( NULL == objv [ i ] ) {
        return TCL_ERROR ;
      } else if ( Tcl_FSAccess ( objv [ i ], mode ) ) {
        Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 0 ) ;
        return TCL_OK ;
      }
    }

    Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 1 ) ;
    return TCL_OK ;
  }

  Tcl_AddErrorInfo ( T, "path arguments required" ) ;
  return TCL_ERROR ;
}

#if defined (OSLinux)
/* bindings to some Linux syscalls */

#elif defined (OSsolaris)
/* bindings to some Solaris/SunOS 5 syscalls */

#elif defined (OSfreebsd)

#elif defined (OSnetbsd)

#elif defined (OSopenbsd)
/* bindings to some OpenBSD syscalls */

/* binding for the pledge(2) syscall */
static int objcmd_pledge ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    const char * prom = Tcl_GetStringFromObj ( objv [ 1 ], NULL ) ;
    const char * eprom = Tcl_GetStringFromObj ( objv [ 2 ], NULL ) ;

    if ( prom && * eprom ) {
      if ( pledge ( prom, eprom ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AddErrorInfo ( T, "pledge() failed: " ) ;
        Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
        return TCL_ERROR ;
      }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "string args required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "prom exprom" ) ;
  return TCL_ERROR ;
}

static int objcmd_unveil ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  const char * path = ( 1 < objc ) ? Tcl_GetStringFromObj ( objv [ 1 ], NULL ) : NULL ;
  const char * perm = ( 2 < objc ) ? Tcl_GetStringFromObj ( objv [ 2 ], NULL ) : NULL ;

  if ( unveil ( path, perm ) ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "pledge() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  return TCL_OK ;
}

static int objcmd_last_unveil ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( unveil ( NULL, NULL ) ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "pledge() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  return TCL_OK ;
}

#endif

/*
 * helper functions
 */

/* flags modifying certain behaviour */
#define EXEC_ARGV0		0x01
#define EXEC_PATH		0x02
#define EXEC_VFORK		0x04
#define EXEC_WAIT		0x08

/* wait for a given (by its PID) process to terminate */
static int wait4pid ( Tcl_Interp * T, const char nohang, const pid_t pid )
{
  if ( 0 < pid ) {
    int i, r = 0 ;
    pid_t p ;

    do {
      i = 0 ;
      p = waitpid ( pid, & i, nohang ? WNOHANG : 0 ) ;
      if ( 0 < p && pid == p ) { break ; }
    } while ( 0 > p && EINTR == errno ) ;

    if ( 0 > p ) {
      return TCL_ERROR ;
    } else if ( WIFEXITED( i ) ) {
      r = WEXITSTATUS( i ) ;
    } else if ( WIFSIGNALED( i ) ) {
      r = WTERMSIG( i ) ;
    } else if ( WIFSTOPPED( i ) ) {
      r = WSTOPSIG( i ) ;
    } else if ( WIFCONTINUED( i ) ) {
    }

    return TCL_OK ;
  }

  return TCL_ERROR ;
}

/* exec into executable */
static int execit ( Tcl_Interp * T, const int f, char ** av )
{
  if ( av && av [ 1 ] && av [ 1 ] [ 0 ] ) {
    if ( ( EXEC_PATH & f ) && ( EXEC_ARGV0 & f ) ) {
      (void) execvp ( * av, 1 + av ) ;
    } else if ( EXEC_PATH & f ) {
      (void) execvp ( * av, av ) ;
    } else if ( EXEC_ARGV0 & f ) {
      (void) execv ( * av, 1 + av ) ;
    } else {
      (void) execv ( * av, av ) ;
    }

    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "execve() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_AddErrorInfo ( T, "missing args" ) ;
  return TCL_ERROR ;
}

/* spawn a subprocesses */
static int spawn ( Tcl_Interp * T, const int f, const int argc, char ** argv )
{
  if ( ( 1 < argc ) && argv && argv [ 1 ] && argv [ 1 ][ 0 ] ) {
  const pid_t p = ( EXEC_VFORK & f ) ? vfork () : fork () ;

  if ( 0 == p ) {
    /* child process */
  } else if ( 0 < p ) {
    /* calling (parent) process */
  } else if ( 0 > p ) {
    /* (v)fork(2) has failed */
    return TCL_ERROR ;
  }

  return TCL_OK ;
  }

  return TCL_ERROR ;
}

static int strcmd_system ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv && argv [ 1 ] && argv [ 1 ][ 0 ] ) {
    if ( strpbrk ( argv [ 1 ], SHELL_META_CHARS ) ) {
      const char * av [ 5 ] = { NULL } ;

      av [ 0 ] = "/bin/sh" ;
      av [ 1 ] = "sh" ;
      av [ 2 ] = "-c" ;
      av [ 3 ] = argv [ 1 ] ;

      //return spawn () ;
    }

    return TCL_OK ;
  }

  return TCL_ERROR ;
}

/*
 * signal handling
 */

static volatile sig_atomic_t got_sig = 0 ;

/* pseudo signal handler */
static void sighand ( const int sig )
{
  got_sig = sig ;
}

static int objcmd_pause ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  (void) pause () ;
  return TCL_OK ;
}

static int objcmd_pause_forever ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  while ( 1 ) {
    (void) pause () ;
  }

  return TCL_OK ;
}

/*
 * functions using TCLs FS API functions
 */

/*
static int objcmd_fs_chdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && 0 == Tcl_FSChdir ( objv [ 1 ] ) ) {
    return TCL_OK ;
  }

  return TCL_ERROR ;
}

static int objcmd_fs_getcwd ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_Obj * optr = Tcl_FSGetCwd ( T ) ;

  if ( optr ) {
    Tcl_SetObjResult ( T, optr ) ;
    return TCL_OK ;
  }

  return TCL_ERROR ;
}
*/

static int objcmd_fs_acc_ex ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, F_OK ) ;
}

static int objcmd_fs_acc_r ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, R_OK ) ;
}

static int objcmd_fs_acc_w ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, W_OK ) ;
}

static int objcmd_fs_acc_x ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, X_OK ) ;
}

static int objcmd_fs_acc_rw ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, R_OK | W_OK ) ;
}

static int objcmd_fs_acc_rx ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, R_OK | X_OK ) ;
}

static int objcmd_fs_acc_wx ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, W_OK | X_OK ) ;
}

static int objcmd_fs_acc_rwx ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_acc ( T, objc, objv, R_OK | W_OK | X_OK ) ;
}

static int objcmd_fs_is_f ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFREG ) ;
}

static int objcmd_fs_is_d ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFDIR ) ;
}

static int objcmd_fs_is_p ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFIFO ) ;
}

static int objcmd_fs_is_s ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFSOCK ) ;
}

static int objcmd_fs_is_c ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFCHR ) ;
}

static int objcmd_fs_is_b ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IFBLK ) ;
}

static int objcmd_fs_is_u ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_ISUID ) ;
}

static int objcmd_fs_is_g ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_ISGID ) ;
}

static int objcmd_fs_is_t ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_ISVTX ) ;
}

static int objcmd_fs_is_r ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IROTH ) ;
}

static int objcmd_fs_is_w ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IWOTH ) ;
}

static int objcmd_fs_is_x ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, 0, S_IXOTH ) ;
}

static int objcmd_fs_is_L ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, FTEST_NOFOLLOW, S_IFLNK ) ;
}

/* see if a given config file exists, is not empty,
 * and has the right access rights
 */
static int objcmd_fs_is_fnr ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, FTEST_NONZERO, S_IFREG | S_IROTH ) ;
}

static int objcmd_fs_is_fnrx ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return fs_test ( T, objc, objv, FTEST_NONZERO, S_IFREG | S_IROTH | S_IXOTH ) ;
}

/* flush (the buffers of) all open stdio (output) streams */
static int objcmd_fflush_all ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  (void) fflush ( NULL ) ;
  return TCL_OK ;
}

/*
 * wrappers for POSIX syscalls
 */

static int objcmd_sync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  sync () ;
  return TCL_OK ;
}

static int objcmd_getuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getuid () ) ;
  return TCL_OK ;
}

static int objcmd_geteuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), geteuid () ) ;
  return TCL_OK ;
}

static int objcmd_getgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getgid () ) ;
  return TCL_OK ;
}

static int objcmd_getegid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getegid () ) ;
  return TCL_OK ;
}

static int objcmd_getpid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getpid () ) ;
  return TCL_OK ;
}

static int objcmd_getppid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getppid () ) ;
  return TCL_OK ;
}

static int objcmd_getpgrp ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), getpgrp () ) ;
  return TCL_OK ;
}

static int objcmd_setsid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  const pid_t p = setsid () ;

  if ( 0 > p ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "setsid() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) p ) ;
  return TCL_OK ;
}

static int objcmd_fork ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  pid_t p = 0 ;

  (void) fflush ( NULL ) ;
  p = fork () ;

  if ( 0 > p ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "fork() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) p ) ;
  return TCL_OK ;
}

static int objcmd_time ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetLongObj ( Tcl_GetObjResult ( T ), (long int) time ( NULL ) ) ;
  return TCL_OK ;
}

static int objcmd_umask ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  mode_t m = 00077 ;

  if ( 1 < objc ) {
    int i = 0 ;

    if ( TCL_OK != Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      Tcl_AddErrorInfo ( T, "integer arg >= 0 required" ) ;
      return TCL_ERROR ;
    }

    m = 007777 & (mode_t) i ;
    m = umask ( m ) ;
  } else {
    m = umask ( 00077 ) ;
    (void) umask ( m ) ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) m ) ;
  return TCL_OK ;
}

static int objcmd_halt ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  sync () ;
  (void) do_reboot ( 'h' ) ;
  Tcl_SetErrno ( errno ) ;
  Tcl_AddErrorInfo ( T, "reboot() failed: " ) ;
  Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
  return TCL_ERROR ;
}

static int objcmd_poweroff ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  sync () ;
  (void) do_reboot ( 'p' ) ;
  (void) do_reboot ( 'h' ) ;
  Tcl_SetErrno ( errno ) ;
  Tcl_AddErrorInfo ( T, "reboot() failed: " ) ;
  Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
  return TCL_ERROR ;
}

static int objcmd_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  char how = 'r' ;

  sync () ;

  if ( 1 < objc ) {
    const char * what = Tcl_GetStringFromObj ( objv [ 1 ], NULL ) ;

    if ( what && * what ) {
      switch ( * what ) {
        case 'h' :
        case 'H' :
          how = 'h' ;
          break ;
        case 'p' :
        case 'P' :
          how = 'p' ;
          break ;
        case 'r' :
        case 'R' :
        default :
          how = 'r' ;
          break ;
      }
    }
  }

  if ( do_reboot ( how ) ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "reboot() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  return TCL_OK ;
}

static int objcmd_kill ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int s = 0 ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & s ) ) {
      if ( 0 <= s && NSIG > s ) {
        int i, p = 0 ;

        for ( i = 2 ; objc > i ; ++ i ) {
          if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & p ) && 0 < p ) {
            if ( kill ( p, s ) ) {
              Tcl_SetErrno ( errno ) ;
              Tcl_AddErrorInfo ( T, "kill() failed: " ) ;
              Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
              return TCL_ERROR ;
              break ;
            }
          } else {
            Tcl_AddErrorInfo ( T, "positive integer arg required" ) ;
            return TCL_ERROR ;
            break ;
          }
        }

        return TCL_OK ;
      }
    }

    Tcl_AddErrorInfo ( T, "invalid signal number" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "sig pid [pid ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_nice ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = 0 ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      errno = 0 ;
      i = nice ( i ) ;

      if ( 0 != errno && -1 == i ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AddErrorInfo ( T, "nice() failed: " ) ;
        Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
        return TCL_ERROR ;
      }

      Tcl_SetIntObj ( Tcl_GetObjResult ( T ), i ) ;
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "integer arg required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "inc" ) ;
  return TCL_ERROR ;
}

static int objcmd_sleep ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = 0 ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) && 0 < i ) {
      unsigned int s = (unsigned int) i ;

      while ( 0 < s ) { s = sleep ( s ) ; }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "positive integer arg required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "secs" ) ;
  return TCL_ERROR ;
}

static int objcmd_delay ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    long int i, u ;

    if ( TCL_OK == Tcl_GetLongFromObj ( T, objv [ 1 ], & i ) &&
      TCL_OK == Tcl_GetLongFromObj ( T, objv [ 2 ], & u ) &&
      0 <= i && 0 <= u && ( 0 < i || 0 < u ) )
    {
#if defined (OSLinux)
      struct timeval tv ;

      tv . tv_sec = i ;
      tv . tv_usec = ( 0 < u ) ? u % ( 1000 * 1000 ) : 0 ;

    /* This only works correctly because the Linux select(2) updates
     * the elapsed time in the struct timeval passed to select !
     * (this also holds for the BSDs)
     */
      while ( 0 > select ( 0, NULL, NULL, NULL, & tv ) && EINTR == errno ) { ; }
#else
    /* use nanosleep(2) */
    struct timespec ts, rem ;

    ts . tv_sec = i ;
    ts . tv_nsec = 1000 * u ;

    while ( 0 > nanosleep ( & ts, & rem ) && EINTR == errno )
    { ts = rem ; }
#endif

      return TCL_OK ;
    }

    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "secs usecs" ) ;
  return TCL_ERROR ;
}

/* sleep with nanosleep(2) */
static int objcmd_nanosleep ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    long int i, n ;

    if ( TCL_OK == Tcl_GetLongFromObj ( T, objv [ 1 ], & i ) &&
      TCL_OK == Tcl_GetLongFromObj ( T, objv [ 2 ], & n ) &&
      0 <= i && 0 <= n && ( 0 < i || 0 < n ) )
    {
      struct timespec ts, rem ;

      ts . tv_sec = i ;
      ts . tv_nsec = ( 0 < n ) ? n % ( 1000 * 1000 * 1000 ) : 0 ;

      while ( 0 > nanosleep ( & ts, & rem ) && EINTR == errno )
      { ts = rem ; }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "2 positive integer args required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "secs nsecs" ) ;
  return TCL_ERROR ;
}

static int objcmd_isatty ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i, f ;

    for ( i = 1 ; objc > i ; ++ i ) {
      if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ i ], & f ) ) {
        if ( 0 > f || 0 == isatty ( f ) ) {
          Tcl_SetIntObj ( Tcl_GetObjResult ( T ), 0 ) ;
          return TCL_OK ;
          break ;
        }
      } else {
        return TCL_ERROR ;
        break ;
      }
    }

    Tcl_SetIntObj ( Tcl_GetObjResult ( T ), 1 ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "fd [fd ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_chroot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    const char * path = Tcl_GetStringFromObj ( objv [ 1 ], NULL ) ;

    if ( path && * path ) {
      if ( chroot ( path ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendStringsToObj ( Tcl_GetObjResult ( T ),
          "chroot to \"", path, "\" failed: ", Tcl_PosixError ( T ),
          (char *) NULL ) ;
        return TCL_ERROR ;
      }

      return TCL_OK ;
    }

    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "secs nsecs" ) ;
  return TCL_ERROR ;
}

static int objcmd_hostname ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_Obj * rp = Tcl_GetObjResult ( T ) ;

  if ( 1 < objc ) {
    /* set hostname */
    int i = 0 ;
    const char * name = Tcl_GetStringFromObj ( objv [ 1 ], & i ) ;

    if ( ( 0 < i ) && name && * name ) {
      if ( sethostname ( name, i ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendStringsToObj ( rp,
          "sethostname failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
        return TCL_ERROR ;
      }

      return TCL_OK ;
    }
  } else {
    /* get hostname */
    char buf [ 1 + HOST_NAME_MAX ] = { 0 } ;

    if ( gethostname ( buf, sizeof ( buf ) - 1 ) ) {
      Tcl_SetErrno ( errno ) ;
      Tcl_AppendStringsToObj ( rp,
        "gethostname failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
      return TCL_ERROR ;
    }

    Tcl_SetStringObj ( rp, buf, str_len ( buf ) ) ;
    return TCL_OK ;
  }

  return TCL_ERROR ;
}

static int objcmd_setrlimit ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;
    const char * rsc = Tcl_GetStringFromObj ( objv [ 1 ], NULL ) ;

    if ( rsc && * rsc ) {
      switch ( * rsc ) {
        case 'a' :
        case 'A' :
          break ;
        case 'C' :
          break ;
        case 'd' :
        case 'D' :
          break ;
        case 'f' :
        case 'F' :
          i = RLIMIT_FSIZE ;
          break ;
        case 'l' :
        case 'L' :
          i = RLIMIT_LOCKS ;
          break ;
        case 'm' :
        case 'M' :
          i = RLIMIT_MEMLOCK ;
          break ;
        case 'n' :
        case 'N' :
          break ;
        case 'r' :
        case 'R' :
          break ;
        case 's' :
        case 'S' :
          break ;
        case 'c' :
        default :
          i = RLIMIT_CORE ;
          break ;
      }
    }
  }

  Tcl_WrongNumArgs ( T, 1, objv, "resource softlimit [hardlimit]" ) ;
  return TCL_ERROR ;
}

static int objcmd_chmod ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      const mode_t m = 007777 & (mode_t) i ;

      for ( i = 2 ; objc > i ; ++ i ) {
        const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

        if ( path && * path ) {
          if ( chmod ( path, m ) ) {
            Tcl_SetErrno ( errno ) ;
            Tcl_AppendResult ( T, "chmod \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
            return TCL_ERROR ;
            break ;
          }
        } else {
          return TCL_ERROR ;
          break ;
        }
      }

      return TCL_OK ;
    }

    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "mode path [path ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_mkdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      const mode_t m = 000700 | ( 007777 & (mode_t) i ) ;

      for ( i = 2 ; objc > i ; ++ i ) {
        const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

        if ( path && * path ) {
          if ( mkdir ( path, m ) ) {
            Tcl_SetErrno ( errno ) ;
            Tcl_AppendResult ( T, "mkdir \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
            return TCL_ERROR ;
            break ;
          }
        } else {
          Tcl_AddErrorInfo ( T, "empty path" ) ;
          return TCL_ERROR ;
          break ;
        }
      }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "integer mode arg required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "mode dir [dir ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_mkfifo ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      const mode_t m = 000600 | ( 007777 & (mode_t) i ) ;

      for ( i = 2 ; objc > i ; ++ i ) {
        const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

        if ( path && * path ) {
          if ( mkfifo ( path, m ) ) {
            Tcl_SetErrno ( errno ) ;
            Tcl_AppendResult ( T, "mkfifo \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
            return TCL_ERROR ;
            break ;
          }
        } else {
          Tcl_AddErrorInfo ( T, "empty path" ) ;
          return TCL_ERROR ;
          break ;
        }
      }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "integer mode arg required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "mode path [path ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_unlink ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 2 ; objc > i ; ++ i ) {
      const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

      if ( path && * path ) {
        if ( unlink ( path ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "unlink \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path" ) ;
        return TCL_ERROR ;
        break ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "file [file ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_rmdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 2 ; objc > i ; ++ i ) {
      const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

      if ( path && * path ) {
        if ( rmdir ( path ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "rmdir \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path" ) ;
        return TCL_ERROR ;
        break ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir [dir ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_remove ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 2 ; objc > i ; ++ i ) {
      const char * path = Tcl_GetStringFromObj ( objv [ i ], NULL ) ;

      if ( path && * path ) {
        if ( remove ( path ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "remove \"", path, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path" ) ;
        return TCL_ERROR ;
        break ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "path [path ...]" ) ;
  return TCL_ERROR ;
}

static int strcmd_execl ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv && argv [ 1 ] && argv [ 1 ] [ 0 ] ) {
    /* this does not return when successful */
    (void) execv ( argv [ 1 ], 1 + (char **) argv ) ;
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "execv() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_AddErrorInfo ( T, "missing args" ) ;
  return TCL_ERROR ;
}

static int strcmd_execl0 ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 2 < argc ) && argv && argv [ 1 ] && argv [ 2 ] &&
    argv [ 1 ] [ 0 ] && argv [ 2 ] [ 0 ] )
  {
    /* this does not return when successful */
    (void) execv ( argv [ 1 ], 2 + (char **) argv ) ;
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "execv() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_AddErrorInfo ( T, "missing args" ) ;
  return TCL_ERROR ;
}

static int strcmd_execlp ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv && argv [ 1 ] && argv [ 1 ] [ 0 ] ) {
    /* this does not return when successful */
    (void) execvp ( argv [ 1 ], 1 + (char **) argv ) ;
    Tcl_SetErrno ( errno ) ;
    Tcl_AddErrorInfo ( T, "execvp() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    return TCL_ERROR ;
  }

  Tcl_AddErrorInfo ( T, "missing args" ) ;
  return TCL_ERROR ;
}

static int strcmd_execlp0 ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 2 < argc ) && argv && argv [ 1 ] && argv [ 2 ] &&
    argv [ 1 ] [ 0 ] && argv [ 2 ] [ 0 ] )
  {
    /* this does not return when successful */
    (void) execvp ( argv [ 1 ], 2 + (char **) argv ) ;
    Tcl_AddErrorInfo ( T, "execvp() failed: " ) ;
    Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
    Tcl_SetErrno ( errno ) ;
    return TCL_ERROR ;
  }

  Tcl_AddErrorInfo ( T, "missing args" ) ;
  return TCL_ERROR ;
}

static int strcmd_vfork_and_execl_nowait ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv [ 1 ] && * argv [ 1 ] ) {
    pid_t p = vfork () ;

    if ( 0 == p ) {
      (void) execvp ( argv [ 1 ], 1 + (char **) argv ) ;
      _exit ( 127 ) ;
    } else if ( 0 < p ) {
    } else if ( 0 > p ) {
      Tcl_SetErrno ( errno ) ;
      Tcl_AppendResult ( T,
        "vfork failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
    }
  }

  return TCL_ERROR ;
}

static int strcmd_vfork_and_execl ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv [ 1 ] && * argv [ 1 ] ) {
    pid_t p = vfork () ;

    if ( 0 == p ) {
      (void) execvp ( argv [ 1 ], 1 + (char **) argv ) ;
      _exit ( 127 ) ;
    } else if ( 0 < p ) {
      int s = 0 ;
      pid_t p2 ;

      do {
        p2 = waitpid ( p, & s, 0 ) ;
        if ( 0 < p2 && p2 == p ) {
        }
      } while ( 0 > p && EINTR == errno ) ;
    } else if ( 0 > p ) {
      Tcl_SetErrno ( errno ) ;
      Tcl_AppendResult ( T,
        "vfork failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
    }
  }

  return TCL_ERROR ;
}

/*
static int strcmd_chdir ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv [ 1 ] && argv [ 1 ][ 0 ] ) {
    if ( chdir ( argv [ 1 ] ) ) {
      Tcl_SetErrno ( errno ) ;
      Tcl_AppendResult ( T,
        "chdir ", argv [ 1 ], " failed: ",
        Tcl_PosixError ( T ), (char *) NULL ) ;
    } else { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_chroot ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( ( 1 < argc ) && argv [ 1 ] && argv [ 1 ][ 0 ] ) {
    if ( chroot ( argv [ 1 ] ) ) {
      Tcl_SetErrno ( errno ) ;
      Tcl_AppendResult ( T, argv [ 0 ],
        ": chroot ", argv [ 1 ], " failed: ",
        Tcl_PosixError ( T ), (char *) NULL ) ;
    } else { return TCL_OK ; }
  }

  return TCL_ERROR ;
}
*/

static int strcmd_pivot_root ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
#if defined (OSLinux)
  if ( 2 < argc ) {
    const char * pn = argv [ 1 ] ;
    const char * po = argv [ 2 ] ;

    if ( pn && po && * pn && * po ) {
      if (
#  if defined (__GLIBC__) && defined (_GNU_SOURCE)
        syscall ( SYS_pivot_root, pn, po )
#  else
        0
#  endif
      )
      {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T, argv [ 0 ],
          ": pivot_root ", pn, " ", po, " failed: ",
          Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  }

  return TCL_ERROR ;
#else
  return TCL_OK ;
#endif
}

static int strcmd_swapoff ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 1 < argc ) {
    int i ;
    const char * str = NULL ;

    for ( i = 1 ; argc > i ; ++ i ) {
      str = argv [ i ] ;

      if ( str && * str && swapoff ( str ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T, argv [ 0 ],
          ": swapoff ", str, " failed: ",
        Tcl_PosixError ( T ), (char *) NULL ) ;
        return TCL_ERROR ;
        break ;
      }
    }

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_umount ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 1 < argc ) {
    int i ;
    const char * str = NULL ;

    for ( i = 1 ; argc > i ; ++ i ) {
      str = argv [ i ] ;

      if ( str && * str && umount ( str ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T, argv [ 0 ],
          ": umount ", str, " failed: ",
        Tcl_PosixError ( T ), (char *) NULL ) ;
        return TCL_ERROR ;
        break ;
      }
    }

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_umount2 ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 1 < argc ) {
    const char * path = argv [ 1 ] ;

    if ( ! ( path && * path ) ) {
      Tcl_AddErrorInfo ( T, "empty path arg" ) ;
      return TCL_ERROR ;
    } else if ( 2 == argc ) {
      if ( umount ( path ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T,
          "chroot ", argv [ 1 ], " failed: ",
          Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    } else if ( 2 < argc ) {
      char what = 0 ;
      int i, f = 0 ;
      const char * str = NULL ;

      for ( i = 2 ; argc > i ; ++ i ) {
        str = argv [ i ] ;

        if ( str && * str ) {
          what = * str ;

	  if ( '-' == what ) { what = str [ 1 ] ; }

          switch ( what ) {
            case 'd' :
            case 'D' :
              f |= MNT_DETACH ;
              break ;
            case 'e' :
            case 'E' :
            case 'x' :
            case 'X' :
              f |= MNT_EXPIRE ;
              break ;
            case 'f' :
            case 'F' :
              f |= MNT_FORCE ;
              break ;
            case 'n' :
            case 'N' :
              f |= UMOUNT_NOFOLLOW ;
              break ;
          }
        }
      } /* end for */

      if ( umount2 ( path, f ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T, "umount2 ", path, " failed: ",
        Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  }

  return TCL_ERROR ;
}

static int strcmd_rename ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 2 < argc ) {
    const char * src = argv [ 1 ] ;
    const char * dst = argv [ 2 ] ;

    if ( src && dst && * src && * dst ) {
      if ( rename ( src, dst ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T,
          "rename failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  } else {
    Tcl_AddErrorInfo ( T, "2 arguments expected" ) ;
  }

  return TCL_ERROR ;
}

static int strcmd_link ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 2 < argc ) {
    const char * src = argv [ 1 ] ;
    const char * dst = argv [ 2 ] ;

    if ( src && dst && * src && * dst ) {
      if ( link ( src, dst ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T,
          "link failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  } else {
    Tcl_AddErrorInfo ( T, "2 arguments expected" ) ;
  }

  return TCL_ERROR ;
}

static int strcmd_symlink ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 2 < argc ) {
    const char * src = argv [ 1 ] ;
    const char * dst = argv [ 2 ] ;

    if ( src && dst && * src && * dst ) {
      if ( symlink ( src, dst ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T,
          "symlink failed: ", Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  } else {
    Tcl_AddErrorInfo ( T, "2 arguments expected" ) ;
  }

  return TCL_ERROR ;
}

static int strcmd_make_files ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 1 < argc ) {
    int i = 1 ;
    mode_t m = 00600 | S_IFREG ;
    const char * str = NULL ;

    if ( 3 < argc && '-' == argv [ 1 ][ 0 ] && 'm' == argv [ 1 ][ 1 ] ) {
      str = argv [ 2 ] ;

      if ( str && * str && isdigit ( * str ) ) {
        if ( ( TCL_OK == Tcl_GetInt ( T, str, & i ) ) && ( 007777 & i ) ) {
          m = 00600 | S_IFREG | ( 007777 & i ) ;
          i = 3 ;
        } else {
          Tcl_AddErrorInfo ( T, "integer string expected" ) ;
          return TCL_ERROR ;
        }
      } else if ( str && * str ) { i = 2 ; }
      else {
        Tcl_AddErrorInfo ( T, "empty arg" ) ;
        return TCL_ERROR ;
      }

      str = NULL ;
    }

    for ( ; argc > i ; ++ i ) {
      str = argv [ i ] ;

      if ( str && * str ) {
        if ( mknod ( str, 00600 | S_IFREG | m, 0 ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "mknod \"", str, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      }
    }

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_make_sockets ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 1 < argc ) {
    int i = 1 ;
    mode_t m = 00600 | S_IFSOCK ;
    const char * str = NULL ;

    if ( 3 < argc && '-' == argv [ 1 ][ 0 ] && 'm' == argv [ 1 ][ 1 ] ) {
      str = argv [ 2 ] ;

      if ( str && * str && isdigit ( * str ) ) {
        if ( ( TCL_OK == Tcl_GetInt ( T, str, & i ) ) && ( 000777 & i ) ) {
          m = 00600 | S_IFSOCK | ( 000777 & i ) ;
          i = 3 ;
        } else {
          Tcl_AddErrorInfo ( T, "integer string expected" ) ;
          return TCL_ERROR ;
        }
      } else if ( str && * str ) { i = 2 ; }
      else {
        Tcl_AddErrorInfo ( T, "empty arg" ) ;
        return TCL_ERROR ;
      }

      str = NULL ;
    }

    for ( ; argc > i ; ++ i ) {
      str = argv [ i ] ;

      if ( str && * str ) {
        if ( mknod ( str, 00600 | S_IFSOCK | m, 0 ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "mknod \"", str, "\" failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      }
    }

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_uname ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  char what = 0 ;
  struct utsname uts ;

  /* zero out result struct before use */
  memset ( & uts, 0, sizeof ( struct utsname ) ) ;

  if ( uname ( & uts ) ) {
    Tcl_SetErrno ( errno ) ;
    Tcl_AppendResult ( T, "uname failed: ",
      Tcl_PosixError ( T ), (char *) NULL ) ;
    return TCL_ERROR ;
  }

  if ( 1 < argc ) {
    what = argv [ 1 ][ 0 ] ;
    if ( '-' == what ) { what = argv [ 1 ][ 1 ] ; }
  }

  switch ( what ) {
    case 'n' :
    case 'N' :
      /*Tcl_SetResult ( T, uts . nodename, TCL_VOLATILE ) ;	*/
      Tcl_AppendResult ( T, uts . nodename, (char *) NULL ) ;
      break ;
    case 'm' :
    case 'M' :
      /*Tcl_SetResult ( T, uts . machine, TCL_VOLATILE ) ;	*/
      Tcl_AppendResult ( T, uts . machine, (char *) NULL ) ;
      break ;
    case 'r' :
    case 'R' :
      /*Tcl_SetResult ( T, uts . release, TCL_VOLATILE ) ;	*/
      Tcl_AppendResult ( T, uts . release, (char *) NULL ) ;
      break ;
    case 's' :
    case 'S' :
      /*Tcl_SetResult ( T, uts . sysname, TCL_VOLATILE ) ;	*/
      Tcl_AppendResult ( T, uts . sysname, (char *) NULL ) ;
      break ;
    case 'v' :
    case 'V' :
      /*Tcl_SetResult ( T, uts . version, TCL_VOLATILE ) ;	*/
      Tcl_AppendResult ( T, uts . version, (char *) NULL ) ;
      break ;
    case 'a' :
    case 'A' :
    default :
      Tcl_AppendElement ( T, uts . nodename ) ;
      Tcl_AppendElement ( T, uts . machine ) ;
      Tcl_AppendElement ( T, uts . sysname ) ;
      Tcl_AppendElement ( T, uts . release ) ;
      break ;
  }

  return TCL_OK ;
}

static int objcmd_mkfile ( ClientData cd, Tcl_Interp * T,
  int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i = 0, j = 0 ;
    mode_t m = 00600 ;
    const char * str = NULL ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      i &= 007777 ;
      m = i ? i : 00600 ;
      m |= 00600 ;
    } else {
      Tcl_AddErrorInfo ( T, "first arg must be integer" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i ; ++ i ) {
      j = -3 ;
      str = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && str && * str ) {
        j = open ( str, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 00600 | m ) ;

        if ( 0 > j ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "creat ", str, " failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        } else {
          while ( close ( j ) && ( EINTR == errno ) ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path arg" ) ;
        return TCL_ERROR ;
        break ;
      }
    } /* end for */

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int objcmd_mknfile ( ClientData cd, Tcl_Interp * T,
  int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i = 0, j = 0 ;
    mode_t m = 00600 | S_IFREG ;
    const char * str = NULL ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      i &= 000777 ;
      m = i ? i : 00600 ;
      m |= 000600 | S_IFREG ;
    } else {
      Tcl_AddErrorInfo ( T, "first arg must be integer" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i ; ++ i ) {
      j = -3 ;
      str = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && str && * str ) {
        if ( mknod ( str, 00600 | S_IFREG | m, 0 ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "mknod ", str, " failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path arg" ) ;
        return TCL_ERROR ;
        break ;
      }
    } /* end for */

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int objcmd_mknfifo ( ClientData cd, Tcl_Interp * T,
  int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i = 0, j = 0 ;
    mode_t m = 00600 | S_IFIFO ;
    const char * str = NULL ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      i &= 000777 ;
      m = i ? i : 00600 ;
      m |= 000600 | S_IFIFO ;
    } else {
      Tcl_AddErrorInfo ( T, "first arg must be integer" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i ; ++ i ) {
      j = -3 ;
      str = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && str && * str ) {
        if ( mknod ( str, 00600 | S_IFIFO | m, 0 ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "mknod ", str, " failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path arg" ) ;
        return TCL_ERROR ;
        break ;
      }
    } /* end for */

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int objcmd_mksock ( ClientData cd, Tcl_Interp * T,
  int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i = 0, j = 0 ;
    mode_t m = 00600 | S_IFSOCK ;
    const char * str = NULL ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      i &= 000777 ;
      m = i ? i : 00600 ;
      m |= 000600 | S_IFSOCK ;
    } else {
      Tcl_AddErrorInfo ( T, "first arg must be integer" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i ; ++ i ) {
      j = -3 ;
      str = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && str && * str ) {
        if ( mknod ( str, 00600 | S_IFSOCK | m, 0 ) ) {
          Tcl_SetErrno ( errno ) ;
          Tcl_AppendResult ( T, "mknod ", str, " failed: ",
            Tcl_PosixError ( T ), (char *) NULL ) ;
          return TCL_ERROR ;
          break ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "empty path arg" ) ;
        return TCL_ERROR ;
        break ;
      }
    } /* end for */

    if ( str && * str ) { return TCL_OK ; }
  }

  return TCL_ERROR ;
}

static int strcmd_mount ( ClientData cd, Tcl_Interp * T,
  const int argc, const char ** argv )
{
  if ( 4 < argc ) {
    long int f = 0 ;
    const char * src = NULL ;
    const char * dst = NULL ;
    const char * fs = NULL ;
    const char * data = NULL ;

    /*
    if ( 3 < objc ) {
      Tcl_ArgvInfo aiv [ 2 ] ;
      Tcl_Obj ** rem = NULL ;

      aiv [ 0 ] . type = TCL_ARGV_INT ;
      aiv [ 0 ] . keyStr = "-m" ;
      aiv [ 0 ] . srcPtr = NULL ;
      aiv [ 0 ] . dstPtr = & i ;
      aiv [ 1 ] . type = TCL_ARGV_END ;

      if ( TCL_OK == Tcl_ParseArgsObjv ( T, aiv, & objc, objv, & rem ) ) {
      }
    }
    */

    src = argv [ 1 ] ;
    dst = argv [ 2 ] ;
    fs = argv [ 3 ] ;
    data = argv [ 4 ] ;

    if ( 5 < argc ) {
      /* parse given command flags */
      int i ;
      const char * str = NULL ;

      for ( i = 5 ; argc > i ; ++ i ) {
        str = argv [ i ] ;

        if ( str && * str ) {
          if ( strstr ( str, "noatime" ) ) { f |= MS_NOATIME ; }
          else if ( strstr ( str, "nodev" ) ) { f |= MS_NODEV ; }
          else if ( strstr ( str, "nodiratime" ) ) { f |= MS_NODIRATIME ; }
          else if ( strstr ( str, "noexec" ) ) { f |= MS_NOEXEC ; }
          else if ( strstr ( str, "nosuid" ) ) { f |= MS_NOSUID ; }
          else if ( strstr ( str, "unbindab" ) ) { f |= MS_UNBINDABLE ; }
          else if ( strstr ( str, "remount" ) ) { f |= MS_REMOUNT ; }
          else if ( strstr ( str, "bind" ) ) { f |= MS_BIND ; }
          else if ( strstr ( str, "dirsync" ) ) { f |= MS_DIRSYNC ; }
          else if ( strstr ( str, "lazytime" ) ) { f |= MS_LAZYTIME ; }
          else if ( strstr ( str, "mandlock" ) ) { f |= MS_MANDLOCK ; }
          else if ( strstr ( str, "move" ) ) { f |= MS_MOVE ; }
          else if ( strstr ( str, "privat" ) ) { f |= MS_PRIVATE ; }
          else if ( strstr ( str, "readon" ) ) { f |= MS_RDONLY ; }
          else if ( strstr ( str, "relatime" ) ) { f |= MS_RELATIME ; }
          else if ( strstr ( str, "rec" ) ) { f |= MS_REC ; }
          else if ( strstr ( str, "share" ) ) { f |= MS_SHARED ; }
          else if ( strstr ( str, "slave" ) ) { f |= MS_SLAVE ; }
          else if ( strstr ( str, "silent" ) ) { f |= MS_SILENT ; }
          else if ( strstr ( str, "strictatime" ) ) { f |= MS_STRICTATIME ; }
          else if ( strstr ( str, "sync" ) ) { f |= MS_SYNCHRONOUS ; }
        }
      } /* end for */
    }

    if ( src && dst && fs && data && * src && * dst && * fs && * data ) {
      if ( mount ( src, dst, fs, f, data ) ) {
        Tcl_SetErrno ( errno ) ;
        Tcl_AppendResult ( T,
          "mount failed: ",
          Tcl_PosixError ( T ), (char *) NULL ) ;
      } else { return TCL_OK ; }
    }
  }

  return TCL_ERROR ;
}

/*
 * signal handling functions
 */

/* reset given or all signals to defaults */

/* tclsh application init function */
int Tcl_AppInit ( Tcl_Interp * T )
{
  /* Tcl_Command tcmd ;		*/
  Tcl_Namespace * nsp = NULL ;

  if ( NULL == T ) { return TCL_ERROR ; }

  /* create namespace for new commands */
  nsp = Tcl_CreateNamespace ( T, "::fs", NULL, NULL ) ;
  /* add new object commands to it */
  /* access(2) related */
  (void) Tcl_CreateObjCommand ( T, "::fs::ex", objcmd_fs_acc_ex, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::exists", objcmd_fs_acc_ex, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_ex", objcmd_fs_acc_ex, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_r", objcmd_fs_acc_r, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_w", objcmd_fs_acc_w, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_x", objcmd_fs_acc_x, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_rw", objcmd_fs_acc_rw, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_rx", objcmd_fs_acc_rx, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_wx", objcmd_fs_acc_wx, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::acc_rwx", objcmd_fs_acc_rwx, NULL, NULL ) ;
  /* (l)stat(2) related */
  (void) Tcl_CreateObjCommand ( T, "::fs::is_f", objcmd_fs_is_f, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_d", objcmd_fs_is_d, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_p", objcmd_fs_is_p, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_s", objcmd_fs_is_s, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_c", objcmd_fs_is_c, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_b", objcmd_fs_is_b, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_u", objcmd_fs_is_u, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_g", objcmd_fs_is_g, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_t", objcmd_fs_is_t, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_k", objcmd_fs_is_t, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_r", objcmd_fs_is_r, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_w", objcmd_fs_is_w, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_x", objcmd_fs_is_x, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_L", objcmd_fs_is_L, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_l", objcmd_fs_is_L, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_h", objcmd_fs_is_L, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_fnr", objcmd_fs_is_fnr, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::is_fnrx", objcmd_fs_is_fnrx, NULL, NULL ) ;

  /* create namespace for new commands */
  nsp = Tcl_CreateNamespace ( T, "::ux", NULL, NULL ) ;

  /* add new string commands */
  (void) Tcl_CreateCommand ( T, "::ux::system", strcmd_system, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execl", strcmd_execl, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execl0", strcmd_execl0, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execlp", strcmd_execlp, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execlp0", strcmd_execlp0, NULL, NULL ) ;
  /*
  (void) Tcl_CreateCommand ( T, "::ux::chdir", strcmd_chdir, NULL, NULL ) ;
  */
  (void) Tcl_CreateCommand ( T, "::ux::pivot_root", strcmd_pivot_root, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::swapoff", strcmd_swapoff, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::umount", strcmd_umount, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::umount2", strcmd_umount2, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::rename", strcmd_rename, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::link", strcmd_link, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::symlink", strcmd_symlink, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::make_files", strcmd_make_files, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::make_sockets", strcmd_make_sockets, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::uname", strcmd_uname, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::mount", strcmd_mount, NULL, NULL ) ;

  /* add new object commands */
  (void) Tcl_CreateObjCommand ( T, "::ux::fflush_all", objcmd_fflush_all, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sync", objcmd_sync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pause", objcmd_pause, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pause_forever", objcmd_pause_forever, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getuid", objcmd_getuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::geteuid", objcmd_geteuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getgid", objcmd_getgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getegid", objcmd_getegid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getpid", objcmd_getpid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getppid", objcmd_getppid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getpgrp", objcmd_getpgrp, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setsid", objcmd_setsid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::fork", objcmd_fork, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::time", objcmd_time, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::umask", objcmd_umask, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::kill", objcmd_kill, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nice", objcmd_nice, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::halt", objcmd_halt, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::poweroff", objcmd_poweroff, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::reboot", objcmd_reboot, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sleep", objcmd_sleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nanosleep", objcmd_nanosleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nsleep", objcmd_nanosleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::delay", objcmd_delay, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::isatty", objcmd_isatty, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::hostname", objcmd_hostname, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::chroot", objcmd_chroot, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::chmod", objcmd_chmod, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::unlink", objcmd_unlink, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::rmdir", objcmd_rmdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::remove", objcmd_remove, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mkfile", objcmd_mkfile, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mkdir", objcmd_mkdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mkfifo", objcmd_mkfifo, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mknfile", objcmd_mknfile, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mknfifo", objcmd_mknfifo, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::mksock", objcmd_mksock, NULL, NULL ) ;

#if defined (OSLinux)
#elif defined (OSopenbsd)
  (void) Tcl_CreateObjCommand ( T, "::ux::pledge", objcmd_pledge, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::unveil", objcmd_unveil, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::last_unveil", objcmd_last_unveil, NULL, NULL ) ;
#endif

  /* pattern of exported command names */
  (void) Tcl_Export ( T, nsp, "[a-z]?*", 0 ) ;
  /* create command ensembles containing the exported commands */
  (void) Tcl_CreateEnsemble ( T, "::ux", nsp, TCL_ENSEMBLE_PREFIX ) ;
  /* group (process) id related commands together into an ensemble command */
  (void) Tcl_CreateEnsemble ( T, "::ux::id", nsp, TCL_ENSEMBLE_PREFIX ) ;
  /* group file test related commands together into an ensemble command */
  (void) Tcl_CreateEnsemble ( T, "::ux::test", nsp, TCL_ENSEMBLE_PREFIX ) ;
  (void) Tcl_CreateEnsemble ( T, "::unix", nsp, TCL_ENSEMBLE_PREFIX ) ;
  (void) Tcl_CreateEnsemble ( T, "::os", nsp, TCL_ENSEMBLE_PREFIX ) ;
  (void) Tcl_CreateEnsemble ( T, "::sys", nsp, TCL_ENSEMBLE_PREFIX ) ;

  (void) Tcl_SetVar ( T, "tcl_rcFileName", "~/.tclrc", TCL_GLOBAL_ONLY ) ;
  /*
  (void) Tcl_ObjSetVar2 ( T,
    Tcl_NewStringObj ( "tcl_rcFileName", -1 ), NULL,
    Tcl_NewStringObj ( "~/.tclshrc", -1 ), TCL_GLOBAL_ONLY ) ;
  */

  /* read /path/to/tcl/lib/tcl8.6/init.tcl */
  (void) Tcl_Init ( T ) ;
  /*
  if ( TCL_ERROR == Tcl_Init ( T ) ) { return TCL_ERROR ; }
  */

  /* source above tcl_rcFile ~/.tclrc when running interactively */
  /*
  if ( isatty ( STDIN_FILENO ) && isatty ( STDOUT_FILENO )
    && isatty ( STDERR_FILENO ) )
  {
    Tcl_SourceRCFile ( T ) ;
  }
  */

#ifdef WANT_TK
  return Tk_Init ( T ) ;
#else
  return TCL_OK ;
#endif
}

/* Module/Library init function */
int ux_Init ( Tcl_Interp * T )
{
  if ( TCL_OK == Tcl_AppInit ( T ) ) {
    Tcl_PkgProvide ( T, "ux", "0.01" ) ;
    return TCL_OK ;
  }

  return TCL_ERROR ;
}

int unix_Init ( Tcl_Interp * T )
{
  return ux_Init ( T ) ;
}

/* the main function */
int main ( const int argc, char ** argv )
{
  //int i = 0 ;
  const uid_t uid = getuid () ;
  const gid_t gid = getgid () ;

  /* drop privileges: reset euid/egid to real uid/gid */
  (void) setgid ( gid ) ;
  (void) setuid ( uid ) ;

  /* secure file creation mask */
  (void) umask ( 00022 | ( 00077 & umask ( 00077 ) ) ) ;

  /* set the SOFT (!!) limit for core dumps to zero */
  (void) set_rlimits () ;

  /* T(cl,k)_Main create the new intereter for us */
#ifdef WANT_TK
  Tk_Main ( argc, argv, Tcl_AppInit ) ;
#else
  Tcl_Main ( argc, argv, Tcl_AppInit ) ;
#endif

  return 0 ;
}
