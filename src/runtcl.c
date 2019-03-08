/*
 * runtcl.c
 *
 * interpreter for Tcl scripts

Look for relevant file + unix functions in the OCaml stdlib.

  FS: copy/delete files/dirs
  SysV IPC: msgqueue functions
  setgroups() mit array aus 64 elementen
  Linux: mount_{procfs,sysfs,devfs,run,...} + seed_{dev,run,...},
    zusammenfassen in mount_pseudofs()
  Ensembles fs + x

strcmds:
  vfork_and_exec
  syslog()
  kexec_load
  pivot_root, swapo(n,ff)
  write (uw)tmp
  net: ifup, ifconfig, etc

objcmds:
  normal:
  alarm, execl, pgkill, (sym)link, readdir, sleep,
  system, times, wait(pid)

  file:
  ch(grp,mod,own), dup, fcntl, f(un)lock, (f)stat, (f)truncate, pipe, select,
  (read,write)_file (read, write)

  unistd.h :
  mass mknod: mksock, mkfile, mkfifos, mkdev, ...
  exec*, access, alarm
  (l)ch(mod,own), get(u,g,p(p,g),s)id, (g,s)ethost(id,name),
  (g,s)etlogin, setr(e)s(gu)id, (sym)link, ttyname
  pipe, readlink

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

/* flag bitmasks that modify the behaviour of certain functions */
enum {
  EXEC_ARGV0			= 0x01,
  EXEC_PATH			= 0x02,
  EXEC_VFORK			= 0x04,
  EXEC_VFORK_WAITPID		= 0x08,
  FTEST_NOFOLLOW		= 0x01,
  FTEST_ZERO			= 0x02,
  FTEST_NONZERO			= 0x04,
  FSYNC_FSYNC			= 0x01,
  FSYNC_FDATASYNC		= 0x02,
  FSYNC_SYNCFS			= 0x04,
} ;

/* global vars */
static const char * progname = NULL ;

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

/* close a given file descriptor without interuption by signals */
static int close_fd ( const int fd )
{
  if ( 0 <= fd ) {
    int i = -1 ;

    do { i = close ( fd ) ; }
    while ( 0 != i && EINTR == errno ) ;

    return i ;
  }

  return -1 ;
}

/* helper function to propagate a posix error back to the calling Tcl code */
static int psx_err ( Tcl_Interp * T, const int en, const char * const msg )
{
  Tcl_SetErrno ( en ) ;
  Tcl_AddErrorInfo ( T, msg ) ;
  Tcl_AddErrorInfo ( T, "() failed: " ) ;
  Tcl_AddErrorInfo ( T, Tcl_PosixError ( T ) ) ;
  return TCL_ERROR ;
}

/* varg psx_verr() helper function */

static int res_zero ( Tcl_Interp * T, const char * const msg, const int res )
{
  if ( res ) {
    const int e = errno ;
    return psx_err ( T, e, msg ) ;
  }

  return TCL_OK ;
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

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
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

static int sync_file ( Tcl_Interp * T, const char * const path,
  const unsigned int what )
{
  int i = 0 ;
  const char * s = NULL ;
  /* TODO: RDONLY on Linux, WRONLY elsewhere */
  const int fd = open ( path,
    /* what about the BSDs ? is read-only sufficient there ? */
#if defined (OSLinux)
    O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY
#else
    O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY
#endif
    ) ;

  if ( 0 > fd ) {
    i = errno ;
    return psx_err ( T, i, "open" ) ;
  }

  switch ( what ) {
    case FSYNC_FDATASYNC :
#if defined (_POSIX_SYNCHRONIZED_IO) && (0 < _POSIX_SYNCHRONIZED_IO)
      if ( fdatasync ( fd ) ) { i = errno ; s = "fdatasync" ; }
#endif
      break ;
    case FSYNC_SYNCFS :
#if defined (OSLinux)
      if ( syncfs ( fd ) ) { i = errno ; s = "syncfs" ; }
#endif
      break ;
    case FSYNC_FSYNC :
    default :
      if ( fsync ( fd ) ) { i = errno ; s = "fsync" ; }
      break ;
  }

  if ( close_fd ( fd ) ) {
    return psx_err ( T, errno, "close" ) ;
  }

  if ( i ) {
    return psx_err ( T, i, ( s && * s ) ? s : "fsync" ) ;
  }

  return TCL_OK ;
}

static int mpsync ( Tcl_Interp * T, const int objc, Tcl_Obj * const * objv,
  const unsigned int what )
{
  if ( 1 < objc ) {
    int i, j ;
    const char * path = NULL ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;
      path = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && path && * path ) {
        if ( sync_file ( T, path, what ) != TCL_OK ) {
          return TCL_ERROR ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "invalid file name" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "file [file ...]" ) ;
  return TCL_ERROR ;
}

static int do_reboot ( Tcl_Interp * T, const int what )
{
  sync () ;

  return res_zero ( T, "reboot", reboot ( what
#if defined (OSsolaris) || defined (OSsunos5) || defined (OSnetbsd)
      , NULL
#endif
    ) ) ;
}

#if defined (OSLinux)

/*
 * bindings for Linux specific syscalls
 */

static int objcmd_setfsuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      Tcl_SetIntObj ( Tcl_GetObjResult ( T ), setfsuid ( (gid_t) i ) ) ;
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "illegal uid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "uid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setfsgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      Tcl_SetIntObj ( Tcl_GetObjResult ( T ), setfsgid ( (gid_t) i ) ) ;
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "illegal gid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "gid" ) ;
  return TCL_ERROR ;
}

/* reboot the system with the reboot(2) syscall */
static int objcmd_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_AUTOBOOT ) ;
}

/* halt the system with the reboot(2) syscall */
static int objcmd_halt ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_HALT_SYSTEM ) ;
}

/* power down the system with the reboot(2) syscall */
static int objcmd_poweroff ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_POWER_OFF ) ;
}

/* hibernate (suspend to disk) the system with the reboot(2) syscall */
static int objcmd_hibernate ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_SW_SUSPEND ) ;
}

/* execute new loaded kernel with the reboot(2) syscall */
static int objcmd_kexec ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_KEXEC ) ;
}

/* enable the secure attention key sequence with the reboot(2) syscall */
static int objcmd_cad_on ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_ENABLE_CAD ) ;
}

/* disable the secure attention key sequence with the reboot(2) syscall */
static int objcmd_cad_off ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_ENABLE_CAD ) ;
}

#elif defined (OSdragonfly)

/*
 * bindings for DrgaonFlyBSD specific syscalls
 */

#elif defined (OSfreebsd)

/*
 * bindings for FreeBSD specific syscalls
 */

static int objcmd_powercycle ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_HALT | RB_POWERCYCLE ) ;
}

static int objcmd_reroot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_REROOT ) ;
}

/* helper function that does the real work for the unmount(2) bindings */
static int do_unmount ( Tcl_interp * T, const int objc,
  Tcl_Obj * const * objv, const int f )
{
  if ( 1 < objc ) {
    int i, j ;
    const char * path = NULL ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;
      path = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && path && * path ) {
        if ( unmount ( path, f ) ) {
          return psx_err ( T, errno, "unmount" ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "invalid mountpoint path" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir [dir ...]" ) ;
  return TCL_ERROR ;
}

/* bindings for the unmount(2) syscall */
static int objcmd_unmount ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_unmount ( T, objc, objv, 0 ) ;
}

static int objcmd_force_unmount ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_unmount ( T, objc, objv, MNT_FORCE ) ;
}

/* binding for the swapon(2) syscall */
static int objcmd_swapon ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i, j ;
    const char * path = NULL ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;
      path = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && path && * path ) {
        if ( swapon ( path ) ) {
          return psx_err ( T, errno, "swapon" ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "invalid swap device path" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dev [dev ...]" ) ;
  return TCL_ERROR ;
}

#elif defined (OSnetbsd)

/* swapctl(2) */

#elif defined (OSopenbsd)

/*
 * bindings for OpenBSD specific syscalls
 */

/* binding for arc4random(3) */
static int objcmd_pledge ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  /* arc4random returns an uint32 value */
  Tcl_SetLongObj ( Tcl_GetObjResult ( T ), arc4random () ) ; 
  return TCL_OK ;
}

/* binding for the pledge(2) syscall */
static int objcmd_pledge ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    const char * prom = Tcl_GetStringFromObj ( objv [ 1 ], NULL ) ;
    const char * eprom = Tcl_GetStringFromObj ( objv [ 2 ], NULL ) ;

    if ( prom && * eprom ) {
      if ( pledge ( prom, eprom ) ) {
        return psx_err ( T, errno, "pledge" ) ;
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
    return psx_err ( T, errno, "unveil" ) ;
  }

  return TCL_OK ;
}

static int objcmd_last_unveil ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( unveil ( NULL, NULL ) ) {
    return psx_err ( T, errno, "unveil" ) ;
  }

  return TCL_OK ;
}

/* swapctl(2) */
/* sendsyslog(2) */

#elif defined (OSsolaris) || defined (OSsunos5)

/*
 * bindings for Solaris/SunOS 5 specific syscalls
 */

static int run_uadmin ( Tcl_Interp * T, const int cmd, const int fcn )
{
  sync () ;

  if ( uadmin ( cmd, fcn, NULL ) ) {
    return psx_err ( T, errno, "uadmin" ) ;
  }

  return TCL_OK ;
}

/* halt the system with the uadmin(2) syscall */
static int objcmd_halt ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return run_uadmin ( T, A_SHUTDOWN, AD_HALT ) ;
}

/* power down the system with the uadmin(2) syscall */
static int objcmd_poweroff ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return run_uadmin ( T, A_SHUTDOWN, AD_POWEROFF ) ;
}

/* reboot the system with the uadmin(2) syscall */
static int objcmd_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return run_uadmin ( T, A_SHUTDOWN, AD_BOOT ) ;
}

/* reboot the system interactively with the uadmin(2) syscall */
static int objcmd_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return run_uadmin ( T, A_SHUTDOWN, AD_IBOOT ) ;
}

/* reboot the system fast with the uadmin(2) syscall */
static int objcmd_fast_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return run_uadmin ( T, A_SHUTDOWN, AD_FASTREBOOT ) ;
}

/* reboot the system with the reboot(3C) syscall */
static int objcmd_sys_reboot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return do_reboot ( T, RB_AUTOBOOT ) ;
}

/* signal all processes that are not in our own session with the
 * sigsendset(3C) syscall
 */
static int objcmd_kill_all ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i = SIGTERM ;
  procset_t pset ;

  if ( 1 < objc ) {
    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK ) {
      if ( 0 > i || NSIG <= i ) {
        return TCL_ERROR ;
      }
    } else {
      return TCL_ERROR ;
    }
  }

  /* define the 2 process sets */
  pset . p_op = POP_DIFF ;
  pset . p_lidtype = P_ALL ;
  pset . p_ridtype = P_SID ;
  pset . p_rid = P_MYID ;

  if ( sigsendset ( & pset, i ) ) {
    return psx_err ( T, errno, "sigsendset" ) ;
  }

  return TCL_OK ;
}

#else
#endif

/*
 * helper functions
 */

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

/* report the current value of the errno global error indicator variable */
static int objcmd_get_errno ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  const int e = errno ;
  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), e ) ;
  return TCL_OK ;
}

/* simple obj command that bitwise negates the given integer arg */
static int objcmd_bit_neg_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
      return TCL_ERROR ;
    }

    Tcl_SetIntObj ( Tcl_GetObjResult ( T ), i ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "num" ) ;
  return TCL_ERROR ;
}

/* simple obj command that bitwise negates the given long integer arg */
static int objcmd_bit_neg_long_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    long int i = 0 ;

    if ( Tcl_GetLongFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid long integer arg" ) ;
      return TCL_ERROR ;
    }

    Tcl_SetLongObj ( Tcl_GetObjResult ( T ), i ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "num" ) ;
  return TCL_ERROR ;
}

/* simple obj command that bitwise negates the given wide integer arg */
static int objcmd_bit_neg_wide_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    Tcl_WideInt i = 0 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid wide integer arg" ) ;
      return TCL_ERROR ;
    }

    Tcl_SetWideIntObj ( Tcl_GetObjResult ( T ), i ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "num" ) ;
  return TCL_ERROR ;
}

/* simple obj command that just bitwise ANDs all given integer args */
static int objcmd_bit_and_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i, j, r = ~ 0 ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_GetIntFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
        r &= j ;
      } else {
        Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
        return TCL_ERROR ;
      }
    }

    Tcl_SetIntObj ( Tcl_GetObjResult ( T ), r ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "num num [num ...]" ) ;
  return TCL_ERROR ;
}

/* simple obj command that just bitwise ANDs all given integer args */
static int objcmd_bit_or_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i, j, r = 0 ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_GetIntFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
        r |= j ;
      } else {
        Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
        return TCL_ERROR ;
      }
    }

    Tcl_SetIntObj ( Tcl_GetObjResult ( T ), r ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "num num [num ...]" ) ;
  return TCL_ERROR ;
}

/* simple obj command that just adds all given integer args */
static int objcmd_add_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i, j, r = 0 ;

  for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
    j = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
      r += j ;
    } else {
      Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
      return TCL_ERROR ;
    }
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), r ) ;
  return TCL_OK ;
}

/* simple obj command that just adds all given long integer args */
static int objcmd_add_long_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i ;
  long int j, r = 0 ;

  for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
    j = 0 ;

    if ( Tcl_GetLongFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
      r += j ;
    } else {
      Tcl_AddErrorInfo ( T, "invalid long integer arg" ) ;
      return TCL_ERROR ;
    }
  }

  Tcl_SetLongObj ( Tcl_GetObjResult ( T ), r ) ;
  return TCL_OK ;
}

/* simple obj command that just adds all given wide integer args */
static int objcmd_add_wide_int ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i ;
  Tcl_WideInt j, r = 0 ;

  for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
    j = 0 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
      r += j ;
    } else {
      Tcl_AddErrorInfo ( T, "invalid wide integer arg" ) ;
      return TCL_ERROR ;
    }
  }

  Tcl_SetWideIntObj ( Tcl_GetObjResult ( T ), r ) ;
  return TCL_OK ;
}

/* simple obj command that just adds all given double floating point number */
static int objcmd_add_double ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i ;
  double d, r = 0.0 ;

  for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
    d = 0.0 ;

    if ( Tcl_GetDoubleFromObj ( T, objv [ i ], & d ) == TCL_OK ) {
      r += d ;
    } else {
      Tcl_AddErrorInfo ( T, "invalid double floating point number" ) ;
      return TCL_ERROR ;
    }
  }

  Tcl_SetDoubleObj ( Tcl_GetObjResult ( T ), r ) ;
  return TCL_OK ;
}

static int objcmd_texit ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] &&
    Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK )
  {
    Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
    return TCL_ERROR ;
  }

  Tcl_Exit ( i ) ;
  return TCL_OK ;
}

static int objcmd_exit ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] &&
    Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK )
  {
    Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
    return TCL_ERROR ;
  }

  exit ( i ) ;
  return TCL_OK ;
}

static int objcmd_uexit ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] &&
    Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK )
  {
    Tcl_AddErrorInfo ( T, "invalid integer arg" ) ;
    return TCL_ERROR ;
  }

  _exit ( i ) ;
  return TCL_OK ;
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

static int objcmd_fs_getcwd ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_Obj * cwd = Tcl_FSGetCwd ( T ) ;

  if ( cwd ) {
    Tcl_SetObjResult ( T, cwd ) ;
    return TCL_OK ;
  }

  Tcl_AddErrorInfo ( T, "FSGetCwd() failed" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_chdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    return res_zero ( T, "FSChdir", Tcl_FSChdir ( objv [ 1 ] ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_remove ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_FSDeleteFile ( objv [ i ] ) != TCL_OK ) {
        Tcl_AddErrorInfo ( T, "FSDeleteFile() failed" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "file [file ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_mkdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_FSCreateDirectory ( objv [ i ] ) != TCL_OK ) {
        Tcl_AddErrorInfo ( T, "FSCreateDirectory() failed" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir [dir ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_rmdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i ;
    Tcl_Obj * o = NULL ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_FSRemoveDirectory ( objv [ i ], 1, & o ) != TCL_OK ) {
        Tcl_AddErrorInfo ( T, "FSCreateDirectory() failed for " ) ;
        Tcl_AppendObjToErrorInfo ( T, o ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir [dir ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_rename ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc && NULL != objv [ 1 ] && NULL != objv [ 2 ] ) {
    return res_zero ( T, "FSRenameFile",
      Tcl_FSRenameFile ( objv [ 1 ], objv [ 2 ] ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "src dest" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_copy_file ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;

    for ( i = 2 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_FSCopyFile ( objv [ 1 ], objv [ i ] ) != TCL_OK ) {
        Tcl_AddErrorInfo ( T, "FSCopyFile() failed" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "src dest [dest ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_copy_dir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i ;
    Tcl_Obj * o = NULL ;

    for ( i = 2 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      if ( Tcl_FSCopyDirectory ( objv [ 1 ], objv [ i ], & o ) != TCL_OK ) {
        Tcl_AddErrorInfo ( T, "FSCopyDirectory() failed for " ) ;
        Tcl_AppendObjToErrorInfo ( T, o ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "src dest [dest ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_readlink ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    Tcl_Obj * const rp = Tcl_FSLink ( objv [ 1 ], NULL, 0 ) ;

    if ( rp ) {
      Tcl_SetObjResult ( T, rp ) ;
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "FSLink() failed" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "link" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_link ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc && NULL != objv [ 1 ] && NULL != objv [ 2 ] ) {
    if ( Tcl_FSLink ( objv [ 1 ], objv [ 2 ], TCL_CREATE_HARD_LINK ) ) {
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "FSLink() failed" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "src dest" ) ;
  return TCL_ERROR ;
}

static int objcmd_fs_symlink ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc && NULL != objv [ 1 ] && NULL != objv [ 2 ] ) {
    if ( Tcl_FSLink ( objv [ 1 ], objv [ 2 ], TCL_CREATE_SYMBOLIC_LINK ) ) {
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "FSLink() failed" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "src dest" ) ;
  return TCL_ERROR ;
}

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

/*
 * wrappers for functions that operate on file descriptors
 */

/*
 * wrappers for IO functions
 */

/* wrapper for the close(2) syscall */
static int objcmd_close ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i, j ;

    for ( i = 1 ; objc > i ; ++ i ) {
      if ( Tcl_GetIntFromObj ( T, objv [ i ], & j ) == TCL_OK ) {
        if ( 0 > j ) {
          Tcl_AddErrorInfo ( T, "negative fd" ) ;
          return TCL_ERROR ;
        } else if ( close_fd ( j ) ) {
          return psx_err ( T, errno, "close" ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "non negative integer arg required" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "fd [fd ...]" ) ;
  return TCL_ERROR ;
}

/* flush (the buffers of) all open stdio (output) streams */
static int objcmd_fflush_all ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  (void) fflush ( NULL ) ;
  return TCL_OK ;
}

/* write(2) to a given fd */
static int objcmd_write ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i, j, fd = -1 ;
    const char * str = NULL ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & fd ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "fd must be non negative integer" ) ;
      return TCL_ERROR ;
    } else if ( 0 > fd ) {
      Tcl_AddErrorInfo ( T, "negative fd" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i ; ++ i ) {
      j = -1 ;
      str = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( 0 <= j && NULL != str ) {
        const ssize_t s = write ( fd, str, j ) ;

        if ( 0 > s || s != j ) {
          return psx_err ( T, errno, "write" ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "arg must be a string" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "fd string [string ...]" ) ;
  return TCL_ERROR ;
}

/*
 * sync open files corresonding to given fds
 */

/* helper function that syncs the open file corresponding to a given fd */
static int sync_fd ( Tcl_Interp * T, const int fd, const unsigned int what )
{
  switch ( what ) {
    case FSYNC_FDATASYNC :
#if defined (_POSIX_SYNCHRONIZED_IO) && (0 < _POSIX_SYNCHRONIZED_IO)
      if ( fdatasync ( fd ) ) { return psx_err ( T, errno, "fdatasync" ) ; }
#endif
      break ;
    case FSYNC_SYNCFS :
#if defined (OSLinux)
      if ( syncfs ( fd ) ) { return psx_err ( T, errno, "syncfs" ) ; }
#endif
      break ;
    case FSYNC_FSYNC :
    default :
      if ( fsync ( fd ) ) { return psx_err ( T, errno, "fsync" ) ; }
      break ;
  }

  return TCL_OK ;
}

static int sync_fds ( Tcl_Interp * T, const int objc, Tcl_Obj * const * objv,
  const unsigned int what )
{
  if ( 1 < objc ) {
    int i, j ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;

      if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ i ], & j ) && 0 <= j ) {
        if ( sync_fd ( T, j, what ) != TCL_OK ) {
          return TCL_ERROR ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "invalid fd" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "fd [fd ...]" ) ;
  return TCL_ERROR ;
}

/* fsync(2) the open files corresonding to the given fds */
static int objcmd_fsync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return sync_fds ( T, objc, objv, FSYNC_FSYNC ) ;
}

/* fdatasync(2) the open files corresonding to the given fds */
static int objcmd_fdatasync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
#if defined (_POSIX_SYNCHRONIZED_IO) && (0 < _POSIX_SYNCHRONIZED_IO)
  return sync_fds ( T, objc, objv, FSYNC_FDATASYNC ) ;
#else
  Tcl_AddErrorInfo ( T, "Platform does not support the fdatasync(2) syscall" ) ;
  return TCL_ERROR ;
#endif
}

/* syncfs(2) the fs of the open files corresonding to the given fds */
static int objcmd_syncfs ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
#if defined (OSLinux)
  return sync_fds ( T, objc, objv, FSYNC_SYNCFS ) ;
#else
  Tcl_AddErrorInfo ( T, "Platform does not support the syncfs(2) syscall" ) ;
  return TCL_ERROR ;
#endif
}

/*
 * wrappers for misc POSIX syscalls
 */

static int objcmd_sync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  sync () ;
  return TCL_OK ;
}

static int objcmd_pfsync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  return mpsync ( T, objc, objv, FSYNC_FSYNC ) ;
}

static int objcmd_pfdatasync ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
#if defined (_POSIX_SYNCHRONIZED_IO) && (0 < _POSIX_SYNCHRONIZED_IO)
  return mpsync ( T, objc, objv, FSYNC_FDATASYNC ) ;
#else
  /*
  return psx_err ( T, ENOSYS, "fdatasync" ) ;
  */
  Tcl_AddErrorInfo ( T, "Platform does not support the fdatasync(2) syscall" ) ;
  return TCL_ERROR ;
#endif
}

static int objcmd_psyncfs ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
#if defined (OSLinux)
  return mpsync ( T, objc, objv, FSYNC_SYNCFS ) ;
#else
  /*
  return psx_err ( T, ENOSYS, "syncfs" ) ;
  */
  Tcl_AddErrorInfo ( T, "Platform does not support the syncfs(2) syscall" ) ;
  return TCL_ERROR ;
#endif
}

static int objcmd_time ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetLongObj ( Tcl_GetObjResult ( T ), (long int) time ( NULL ) ) ;
  return TCL_OK ;
}

static int objcmd_gethostid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  Tcl_SetLongObj ( Tcl_GetObjResult ( T ), gethostid () ) ;
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

static int objcmd_getpgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  pid_t p = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid pid" ) ;
      return TCL_ERROR ;
    }

    p = (pid_t) i ;
  }

  p = getpgid ( p ) ;

  if ( 0 > p ) {
    return psx_err ( T, errno, "getpgid" ) ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) p ) ;
  return TCL_OK ;
}

static int objcmd_getsid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  pid_t p = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid pid" ) ;
      return TCL_ERROR ;
    }

    p = (pid_t) i ;
  }

  p = getsid ( p ) ;

  if ( 0 > p ) {
    return psx_err ( T, errno, "getsid" ) ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) p ) ;
  return TCL_OK ;
}

static int objcmd_sethostid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    long int i = 0 ;

    if ( Tcl_GetLongFromObj ( T, objv [ 1 ], & i ) == TCL_OK ) {
      return res_zero ( T, "sethostid", sethostid ( i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal host id" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "id" ) ;
  return TCL_ERROR ;
}

static int objcmd_setuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      return res_zero ( T, "setuid", setuid ( (uid_t) i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal uid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "uid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      return res_zero ( T, "setgid", setgid ( (gid_t) i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal gid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "gid" ) ;
  return TCL_ERROR ;
}

static int objcmd_seteuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      return res_zero ( T, "seteuid", seteuid ( (uid_t) i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal uid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "uid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setegid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    Tcl_WideInt i = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 <= i ) {
      return res_zero ( T, "setegid", setegid ( (gid_t) i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal gid" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "gid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setreuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    Tcl_WideInt r = -1, e = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & r ) != TCL_OK || 0 > r ) {
      Tcl_AddErrorInfo ( T, "illegal uid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & e ) != TCL_OK || 0 > e ) {
      Tcl_AddErrorInfo ( T, "illegal uid" ) ;
      return TCL_ERROR ;
    }

    return res_zero ( T, "setreuid", setreuid ( (uid_t) r, (uid_t) e ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "ruid euid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setregid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    Tcl_WideInt r = -1, e = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & r ) != TCL_OK || 0 > r ) {
      Tcl_AddErrorInfo ( T, "illegal gid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & e ) != TCL_OK || 0 > e ) {
      Tcl_AddErrorInfo ( T, "illegal gid" ) ;
      return TCL_ERROR ;
    }

    return res_zero ( T, "setregid", setregid ( (gid_t) r, (gid_t) e ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "rgid egid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setresuid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 3 < objc ) {
    Tcl_WideInt r = -1, e = -1, s = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & r ) != TCL_OK || 0 > r ) {
      Tcl_AddErrorInfo ( T, "illegal uid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & e ) != TCL_OK || 0 > e ) {
      Tcl_AddErrorInfo ( T, "illegal uid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & s ) != TCL_OK || 0 > s ) {
      Tcl_AddErrorInfo ( T, "illegal uid" ) ;
      return TCL_ERROR ;
    }

    return res_zero ( T, "setresuid",
      setresuid ( (uid_t) r, (uid_t) e, (uid_t) s ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "ruid euid suid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setresgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 3 < objc ) {
    Tcl_WideInt r = -1, e = -1, s = -1 ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & r ) != TCL_OK || 0 > r ) {
      Tcl_AddErrorInfo ( T, "illegal gid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & e ) != TCL_OK || 0 > e ) {
      Tcl_AddErrorInfo ( T, "illegal gid" ) ;
      return TCL_ERROR ;
    }

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & s ) != TCL_OK || 0 > s ) {
      Tcl_AddErrorInfo ( T, "illegal gid" ) ;
      return TCL_ERROR ;
    }

    return res_zero ( T, "setresgid",
      setresgid ( (gid_t) r, (gid_t) e, (gid_t) s ) ) ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "rgid egid sgid" ) ;
  return TCL_ERROR ;
}

static int objcmd_setpgid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  int i = 0 ;
  pid_t p = 0, pg = 0 ;

  if ( 1 < objc && NULL != objv [ 1 ] ) {
    i = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid pid" ) ;
      return TCL_ERROR ;
    }

    p = (pid_t) i ;
  }

  if ( 2 < objc && NULL != objv [ 2 ] ) {
    i = 0 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 2 ], & i ) != TCL_OK ) {
      Tcl_AddErrorInfo ( T, "invalid pid" ) ;
      return TCL_ERROR ;
    }

    pg = (pid_t) i ;
  }

  if ( setpgid ( p, pg ) ) {
    return psx_err ( T, errno, "setpgid" ) ;
  }

  return TCL_OK ;
}

static int objcmd_setsid ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  const pid_t p = setsid () ;

  if ( 0 > p ) {
    return psx_err ( T, errno, "setsid" ) ;
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
    return psx_err ( T, errno, "fork" ) ;
  }

  Tcl_SetIntObj ( Tcl_GetObjResult ( T ), (int) p ) ;
  return TCL_OK ;
}

static int objcmd_nodename ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  struct utsname uts ;

  if ( uname ( & uts ) ) {
    return psx_err ( T, errno, "uname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), uts . nodename, -1 ) ;
  return TCL_OK ;
}

static int objcmd_sysarch ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  struct utsname uts ;

  if ( uname ( & uts ) ) {
    return psx_err ( T, errno, "uname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), uts . machine, -1 ) ;
  return TCL_OK ;
}

static int objcmd_sysname ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  struct utsname uts ;

  if ( uname ( & uts ) ) {
    return psx_err ( T, errno, "uname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), uts . sysname, -1 ) ;
  return TCL_OK ;
}

static int objcmd_sysrelease ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  struct utsname uts ;

  if ( uname ( & uts ) ) {
    return psx_err ( T, errno, "uname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), uts . release, -1 ) ;
  return TCL_OK ;
}

static int objcmd_sysversion ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  struct utsname uts ;

  if ( uname ( & uts ) ) {
    return psx_err ( T, errno, "uname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), uts . version, -1 ) ;
  return TCL_OK ;
}

static int objcmd_gethostname ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  char buf [ 1 + HOST_NAME_MAX ] = { 0 } ;

  if ( gethostname ( buf, sizeof ( buf ) - 1 ) ) {
    return psx_err ( T, errno, "gethostname" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), buf, str_len ( buf ) ) ;
  return TCL_OK ;
}

static int objcmd_umask ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = 0 ;

    if ( TCL_OK != Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      Tcl_AddErrorInfo ( T, "integer arg >= 0 required" ) ;
      return TCL_ERROR ;
    }

    i &= 007777 ;
    Tcl_SetIntObj ( Tcl_GetObjResult ( T ),
      (int) umask ( 007777 & (mode_t) i ) ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "mask" ) ;
  return TCL_ERROR ;
}

static int objcmd_kill ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int s = 0 ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & s ) && 0 <= s && NSIG > s )
    {
      int i, p ;

      for ( i = 2 ; objc > i && NULL != objv [ i ] ; ++ i ) {
        if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & p ) == TCL_OK ) {
          if ( kill ( (pid_t) p, s ) ) {
            return psx_err ( T, errno, "kill" ) ;
          }
        } else {
          Tcl_AddErrorInfo ( T, "pid (integer) required" ) ;
          return TCL_ERROR ;
        }
      }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "illegal signal number specified" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "sig pid [pid ...]" ) ;
  return TCL_ERROR ;
}

/* killpg(2) */

static int objcmd_nice ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = 0 ;

    if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) ) {
      errno = 0 ;
      i = nice ( i ) ;

      if ( -1 == i && 0 != errno ) {
        return psx_err ( T, errno, "nice" ) ;
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

static int objcmd_msleep ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = -1 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 < i ) {
      Tcl_Sleep ( i ) ;
      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "positive integer arg required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "msecs" ) ;
  return TCL_ERROR ;
}

static int objcmd_sleep ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = -1 ;

    if ( Tcl_GetIntFromObj ( T, objv [ 1 ], & i ) == TCL_OK && 0 < i ) {
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

static int objcmd_getcwd ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  char cwd [ 1 + PATH_MAX ] = { '\0' } ;

  if ( getcwd ( cwd, sizeof ( cwd ) - 1 ) == NULL ) {
    return psx_err ( T, errno, "getcwd" ) ;
  }

  Tcl_SetStringObj ( Tcl_GetObjResult ( T ), cwd, -1 ) ;
  return TCL_OK ;
}

static int objcmd_chdir ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = -1 ;
    const char * const path = Tcl_GetStringFromObj ( objv [ 1 ], & i ) ;

    if ( ( 0 < i ) && path && * path ) {
      if ( chdir ( path ) ) {
        return psx_err ( T, errno, "chdir" ) ;
      }

      return TCL_OK ;
    }

    Tcl_AddErrorInfo ( T, "non empty path string required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir" ) ;
  return TCL_ERROR ;
}

static int objcmd_chroot ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc && NULL != objv [ 1 ] ) {
    int i = -1 ;
    const char * const path = Tcl_GetStringFromObj ( objv [ 1 ], & i ) ;

    if ( ( 0 < i ) && path && * path ) {
      if ( chdir ( path ) ) {
        return psx_err ( T, errno, "chdir" ) ;
      }

      return res_zero ( T, "chroot", chroot ( "." ) ) ;
    }

    Tcl_AddErrorInfo ( T, "non empty path string required" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dir" ) ;
  return TCL_ERROR ;
}

static int objcmd_isatty ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i, f ;

    for ( i = 1 ; objc > i ; ++ i ) {
      if ( TCL_OK == Tcl_GetIntFromObj ( T, objv [ i ], & f ) ) {
        if ( 0 > f ) {
          Tcl_AddErrorInfo ( T, "negative fd" ) ;
          return TCL_ERROR ;
        } else if ( 0 == isatty ( f ) ) {
          Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 0 ) ;
          return TCL_OK ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "fd must be non negative integer" ) ;
        return TCL_ERROR ;
      }
    }

    Tcl_SetBooleanObj ( Tcl_GetObjResult ( T ), 1 ) ;
    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "fd [fd ...]" ) ;
  return TCL_ERROR ;
}

static int objcmd_sethostname ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 1 < objc ) {
    int i = -1 ;
    const char * const name = Tcl_GetStringFromObj ( objv [ 1 ], & i ) ;

    if ( ( 0 < i ) && name && * name ) {
      return res_zero ( T, "sethostname", sethostname ( name, (size_t) i ) ) ;
    }

    Tcl_AddErrorInfo ( T, "illegal hostname" ) ;
    return TCL_ERROR ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "name" ) ;
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
  const int objc, Tcl_Obj * const * objv )
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
  const int objc, Tcl_Obj * const * objv )
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
  const int objc, Tcl_Obj * const * objv )
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
  const int objc, Tcl_Obj * const * objv )
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

/* binding for the truncate(2) syscall */
static int objcmd_truncate ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
  if ( 2 < objc ) {
    int i, j ;
    off_t o = 0 ;
    Tcl_WideInt s = 0 ;
    const char * path = NULL ;

    if ( Tcl_GetWideIntFromObj ( T, objv [ 1 ], & s ) == TCL_OK && 0 <= s ) {
      o = (off_t) s ;
    } else {
      Tcl_AddErrorInfo ( T, "non negative file size in bytes required" ) ;
      return TCL_ERROR ;
    }

    for ( i = 2 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;
      path = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && path && * path ) {
        if ( truncate ( path, o ) ) {
          return psx_err ( T, errno, "truncate" ) ;
        }
      } else {
        Tcl_AddErrorInfo ( T, "file name required" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "size file [file ...]" ) ;
  return TCL_ERROR ;
}

/* binding for the swapoff(2) syscall */
static int objcmd_swapoff ( ClientData cd, Tcl_Interp * T,
  const int objc, Tcl_Obj * const * objv )
{
#if defined (OSLinux) || defined (OSdragonfly) || defined (OSfreebsd) || \
  defined (OSnetbsd) || defined (OSopenbsd)
  if ( 1 < objc ) {
    int i, j ;
    const char * path = NULL ;

    for ( i = 1 ; objc > i && NULL != objv [ i ] ; ++ i ) {
      j = -1 ;
      path = Tcl_GetStringFromObj ( objv [ i ], & j ) ;

      if ( ( 0 < j ) && path && * path ) {
        if (
#if defined (OSnetbsd) || defined (OSopenbsd)
          swapctl ( SWAP_OFF, path, 0 )
#else
          swapoff ( path )
#endif
        ) { return psx_err ( T, errno, "swapoff" ) ; }
      } else {
        Tcl_AddErrorInfo ( T, "invalid swap device path" ) ;
        return TCL_ERROR ;
      }
    }

    return TCL_OK ;
  }

  Tcl_WrongNumArgs ( T, 1, objv, "dev [dev ...]" ) ;
#else
  Tcl_AddErrorInfo ( T, "Platform not supported" ) ;
#endif
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

/* reset given or all signals to their default actions */

/* tclsh application init function */
int Tcl_AppInit ( Tcl_Interp * T )
{
  /*
  Tcl_Namespace * nsp = NULL ;
  */

  /* add new FS object commands */
  (void) Tcl_CreateObjCommand ( T, "::fs::getcwd", objcmd_fs_getcwd, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::cwd", objcmd_fs_getcwd, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::chdir", objcmd_fs_chdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::cd", objcmd_fs_chdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::remove", objcmd_fs_remove, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rm", objcmd_fs_remove, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::mkdir", objcmd_fs_mkdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::md", objcmd_fs_mkdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rmdir", objcmd_fs_rmdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rmrec", objcmd_fs_rmdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rm_r", objcmd_fs_rmdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rename", objcmd_fs_rename, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::move", objcmd_fs_rename, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::mv", objcmd_fs_rename, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::copy_file", objcmd_fs_copy_file, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::copy", objcmd_fs_copy_file, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::cp", objcmd_fs_copy_file, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::copy_dir", objcmd_fs_copy_dir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::copyrec", objcmd_fs_copy_dir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::rcopy", objcmd_fs_copy_dir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::copy_r", objcmd_fs_copy_dir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::cp_r", objcmd_fs_copy_dir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::readlink", objcmd_fs_readlink, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::link", objcmd_fs_link, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::fs::symlink", objcmd_fs_symlink, NULL, NULL ) ;
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

  /* add new string commands */
  (void) Tcl_CreateCommand ( T, "::ux::system", strcmd_system, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execl", strcmd_execl, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execl0", strcmd_execl0, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execlp", strcmd_execlp, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::execlp0", strcmd_execlp0, NULL, NULL ) ;
  (void) Tcl_CreateCommand ( T, "::ux::pivot_root", strcmd_pivot_root, NULL, NULL ) ;
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
  (void) Tcl_CreateObjCommand ( T, "::ux::get_errno", objcmd_get_errno, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_neg_int", objcmd_bit_neg_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bitnegint", objcmd_bit_neg_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_neg_long_int", objcmd_bit_neg_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_neg_long", objcmd_bit_neg_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bitneglong", objcmd_bit_neg_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_neg_wide_int", objcmd_bit_neg_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_neg_wide", objcmd_bit_neg_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bitnegwide", objcmd_bit_neg_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_and_int", objcmd_bit_and_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bitandint", objcmd_bit_and_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bit_or_int", objcmd_bit_or_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::bitorint", objcmd_bit_or_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_int", objcmd_add_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::addint", objcmd_add_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_long_int", objcmd_add_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_long", objcmd_add_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::addlong", objcmd_add_long_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_wide_int", objcmd_add_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_wide", objcmd_add_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::addwide", objcmd_add_wide_int, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::add_double", objcmd_add_double, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::texit", objcmd_texit, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::tcl_exit", objcmd_texit, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::exit", objcmd_exit, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::uexit", objcmd_uexit, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::_exit", objcmd_uexit, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::close", objcmd_close, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::close_fd", objcmd_close, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::write", objcmd_write, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::fflush_all", objcmd_fflush_all, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sync", objcmd_sync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::fsync", objcmd_fsync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::fdatasync", objcmd_fdatasync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pfsync", objcmd_pfsync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pfdatasync", objcmd_pfdatasync, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pause", objcmd_pause, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::pause_forever", objcmd_pause_forever, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::time", objcmd_time, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::gethostid", objcmd_gethostid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getuid", objcmd_getuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::geteuid", objcmd_geteuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getgid", objcmd_getgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getegid", objcmd_getegid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getpid", objcmd_getpid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getppid", objcmd_getppid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getpgrp", objcmd_getpgrp, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getpgid", objcmd_getpgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sethostid", objcmd_sethostid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setuid", objcmd_setuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setgid", objcmd_setgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::seteuid", objcmd_seteuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setegid", objcmd_setegid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setreuid", objcmd_setreuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setregid", objcmd_setregid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setresuid", objcmd_setresuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setresgid", objcmd_setresgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getsid", objcmd_getsid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setpgid", objcmd_setpgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setsid", objcmd_setsid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::fork", objcmd_fork, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nodename", objcmd_nodename, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sysarch", objcmd_sysarch, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sysname", objcmd_sysname, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::osname", objcmd_sysname, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sysrelease", objcmd_sysrelease, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::osrelease", objcmd_sysrelease, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sysversion", objcmd_sysversion, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::osversion", objcmd_sysversion, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::gethostname", objcmd_gethostname, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::umask", objcmd_umask, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::kill", objcmd_kill, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nice", objcmd_nice, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::msleep", objcmd_msleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sleep", objcmd_sleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nanosleep", objcmd_nanosleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::nsleep", objcmd_nanosleep, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::delay", objcmd_delay, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::getcwd", objcmd_getcwd, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::cwd", objcmd_getcwd, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::chdir", objcmd_chdir, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::chroot", objcmd_chroot, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::isatty", objcmd_isatty, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::sethostname", objcmd_sethostname, NULL, NULL ) ;
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
  (void) Tcl_CreateObjCommand ( T, "::ux::truncate", objcmd_truncate, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::swapoff", objcmd_swapoff, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::reboot", objcmd_reboot, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::halt", objcmd_halt, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::poweroff", objcmd_poweroff, NULL, NULL ) ;

  /* add platform specific (object) commands to current interpreter */
#if defined (OSbsd)
  /* wrappers to syscalls common to ALL of the BSDs */
#endif

#if defined (OSLinux)
  (void) Tcl_CreateObjCommand ( T, "::ux::setfsuid", objcmd_setfsuid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::setfsgid", objcmd_setfsgid, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::hibernate", objcmd_hibernate, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::kexec", objcmd_kexec, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::cad_on", objcmd_cad_on, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::cad_off", objcmd_cad_off, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::syncfs", objcmd_syncfs, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::psyncfs", objcmd_psyncfs, NULL, NULL ) ;
#elif defined (OSdragonfly)
#elif defined (OSfreebsd)
  (void) Tcl_CreateObjCommand ( T, "::ux::powercycle", objcmd_powercycle, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::reroot", objcmd_reroot, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::unmount", objcmd_unmount, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::force_unmount", objcmd_force_unmount, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::swapon", objcmd_swapon, NULL, NULL ) ;
#elif defined (OSnetbsd)
#elif defined (OSopenbsd)
  (void) Tcl_CreateObjCommand ( T, "::ux::pledge", objcmd_pledge, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::unveil", objcmd_unveil, NULL, NULL ) ;
  (void) Tcl_CreateObjCommand ( T, "::ux::last_unveil", objcmd_last_unveil, NULL, NULL ) ;
#elif defined (OSaix)
#elif defined (OShpux)
#elif defined (OSsolaris) || defined (OSsunos5)
#endif

#if 0
  nsp = Tcl_FindNamespace ( T, "::fs", NULL, TCL_GLOBAL_ONLY ) ;
  /* pattern for exported command names */
  (void) Tcl_Export ( T, nsp, "[a-z]?*", 0 ) ;
  (void) Tcl_CreateEnsemble ( T, "::fs", nsp, TCL_ENSEMBLE_PREFIX ) ;
  nsp = Tcl_FindNamespace ( T, "::ux", NULL, TCL_GLOBAL_ONLY ) ;
  /* pattern for exported command names */
  (void) Tcl_Export ( T, nsp, "[a-z]?*", 0 ) ;
  /* create command ensembles containing the exported commands */
  (void) Tcl_CreateEnsemble ( T, "::x", nsp, TCL_ENSEMBLE_PREFIX ) ;
  /* group (process) id related commands together into an ensemble command */
  (void) Tcl_CreateEnsemble ( T, "::x::id", nsp, TCL_ENSEMBLE_PREFIX ) ;
  /* group file test related commands together into an ensemble command */
  (void) Tcl_CreateEnsemble ( T, "::x::test", nsp, TCL_ENSEMBLE_PREFIX ) ;
  (void) Tcl_CreateEnsemble ( T, "::ux", nsp, TCL_ENSEMBLE_PREFIX ) ;
#endif

  (void) Tcl_SetVar ( T, "tcl_rcFileName", "~/.tclrc", TCL_GLOBAL_ONLY ) ;
  /*
  (void) Tcl_ObjSetVar2 ( T,
    Tcl_NewStringObj ( "tcl_rcFileName", -1 ), NULL,
    Tcl_NewStringObj ( "~/.tclshrc", -1 ), TCL_GLOBAL_ONLY ) ;
  */

  /* read and run /path/to/tcl/lib/tcl8.6/init.tcl */
  if ( TCL_ERROR == Tcl_Init ( T ) ) {
    /*
    return TCL_ERROR ;
    */
    (void) fprintf ( stderr,
      "\n%s: Could not find and source the \"init.tcl\" initialization script!\n\n",
      ( progname && * progname ) ? progname : "WARNING" ) ;
  }

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
#ifdef USE_TCL_STUBS
  (void) Tcl_InitStubs ( T, "8.6", 0 ) ;
#endif

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

  /* initialize global vars */
  progname = ( ( 0 < argc ) && * argv && ** argv ) ? * argv : NULL ;

  /* T(cl,k)_Main create the new intereter for us */
#ifdef WANT_TK
  Tk_Main ( argc, argv, Tcl_AppInit ) ;
#else
  Tcl_Main ( argc, argv, Tcl_AppInit ) ;
#endif

  return 0 ;
}

