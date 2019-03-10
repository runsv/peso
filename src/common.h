/*
 * common includes and definitions
 */

#ifndef _HEADER_PESI_COMMON_H_
#define _HEADER_PESI_COMMON_H_		1

/* colors for output to terminals */
/*
#define ANSI_HIGHLIGHT_ON		"\033[1;39m"
#define ANSI_HIGHLIGHT_RED_ON		"\033[1;31m"
#define ANSI_HIGHLIGHT_GREEN_ON		"\033[1;32m"
#define ANSI_HIGHLIGHT_YELLOW_ON	"\033[1;33m"
#define ANSI_HIGHLIGHT_BLUE_ON		"\033[1;36m"
#define ANSI_HIGHLIGHT_OFF		"\033[0m"

#define CONSOLE_COLOR_BLACK		"\033[30m"
#define CONSOLE_COLOR_RED		"\033[31m"
#define CONSOLE_COLOR_GREEN		"\033[32m"
#define CONSOLE_COLOR_YELLOW		"\033[33m"
#define CONSOLE_COLOR_BLUE		"\033[34m"
#define CONSOLE_COLOR_MAGENTA		"\033[35m"
#define CONSOLE_COLOR_CYAN		"\033[36m"
#define CONSOLE_COLOR_WHITE		"\033[37m"
#define CONSOLE_ENDCOLOR		"\033[0m"
#define CONSOLE_CTL_SAVESTATE		"\033[s"
#define CONSOLE_CTL_RESTORESTATE	"\033[u"
*/

#if defined (__linux__) || defined (__linux) || defined (linux)
#  define OSLinux		1
#  define OSlinux		1
#  define _GNU_SOURCE		1
#  define _ALL_SOURCE		1
#  include <features.h>
/*
#  if defined (__GLIBC__)
#    define _GNU_SOURCE		1
#  endif
#  undef _FEATURES_H
*/
#endif

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <err.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <alloca.h>
#include <glob.h>
#include <fnmatch.h>
#include <wordexp.h>
#include <regex.h>
#include <ftw.h>
#include <termios.h>
#include <syslog.h>
#include <spawn.h>
#include <semaphore.h>
#include <mqueue.h>
#include <syscall.h>
#include <aio.h>
#include <sched.h>
#include <utime.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/swap.h>
#include <sys/statvfs.h>
#include <sys/termios.h>
#include <sys/ttydefaults.h>
#include <sys/reboot.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <utmp.h>
/*
#include <utmpx.h>
*/

#if defined (__GLIBC__)
#  include <sys/auxv.h>
#  include <execinfo.h>
#  include <mcheck.h>
#endif	/* __GLIBC__ */

/* accomodate OS differences */

/* find out if this is one of the BSDs */
/*
if defined(__FreeBSD__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
*/
#if defined (BSD) || defined (__FreeBSD__) || defined (__DragonFly__) \
 || defined (__OpenBSD__) || defined (__NetBSD__)
#  include <sys/queue.h>
#  include <paths.h>
#  define OSbsd		1
#  define OS		"BSD"
#endif

#if defined (BSD) && ! defined (__GNU__)
/* include <sys/param.h> */
#  include <sys/user.h>
#  include <sys/sysctl.h>
#  include <kvm.h>
#else
/* include <sys/param.h> */
#endif

#if defined (gnu_hurd)
#  define OSHurd	1
#  define OShurd	1
#  define OS		"Hurd"
#elif defined (__linux__) || defined (__linux) || defined (linux)
#  include <features.h>
#  include <asm/types.h>
#  include <error.h>
#  include <fstab.h>
#  include <mntent.h>
#  include <paths.h>
#  include <ucontext.h>
#  include <ifaddrs.h>
#  include <sys/vfs.h>
#  include <sys/sendfile.h>
#  include <sys/fsuid.h>
#  include <sys/timeb.h>
#  include <sys/io.h>
/* include <sys/perm.h>		*/
/* include <sys/memfd.h>	*/
#  include <sys/queue.h>
#  include <sys/sysinfo.h>
#  include <sys/signalfd.h>
#  include <sys/inotify.h>
#  include <sys/fanotify.h>
#  include <sys/sysmacros.h>
#  include <sys/prctl.h>
#  include <sys/kd.h>
#  include <sys/vt.h>
#  include <sys/klog.h>
#  include <sys/quota.h>
#  include <sys/xattr.h>
#  include <sys/capability.h>
#  include <xfs/xqm.h>
#  include <linux/version.h>
#  include <linux/magic.h>
#  include <linux/unistd.h>
#  include <linux/random.h>
#  include <linux/vt.h>
#  include <linux/kd.h>
#  include <linux/memfd.h>
#  include <linux/kexec.h>
#  include <linux/netlink.h>
#  include <linux/rtnetlink.h>
#  include <linux/loop.h>
#  include <linux/connector.h>
#  if defined (__GLIBC__)
#    include <argp.h>
#    include <bits/sigcontext.h>
/*    include <obstack.h>	*/
#  endif	/* __GLIBC__ */
#  define _LINUX_TIME_H
#  include <linux/cn_proc.h>
#  define OSLinux	1
#  define OSlinux	1
#  define OS		"Linux"
#elif defined (__ANDROID__)
#  define OSAndroid	1
#  define OSandroid	1
#  define OS		"Android"
/* elif defined (__FreeBSD__) || defined (__FreeBSD_kernel__) */
#elif defined (__FreeBSD__) || defined (__FreeBSD_version)
#  include <utmpx.h>
#  include <sys/sysctl.h>
#  include <sys/procctl.h>
#  include <sys/event.h>
#  define OSFreeBSD	1
#  define OSfreebsd	1
#  define OS		"FreeBSD"
#elif defined (__DragonFly__)
#  include <sys/procctl.h>
#  define OSDragonFlyBSD	1
#  define OSdragonflybsd	1
#  define OSDragonFly		1
#  define OSdragonfly		1
#  define OSdragon		1
#  define OSdfly		1
#  define OS		"DrgoanFly"
#elif defined (__NetBSD__) || defined (NetBSD) || defined (__NetBSD_Version__)
#  include <sys/event.h>
#  include <util.h>
#  include <tzfile.h>
#  include <machine/cpu.h>
#  include <sys/sysctl.h>
#  include <ttyent.h>
#  include <utmp.h>
#  include <utmpx.h>
#  define OSNetBSD	1
#  define OSnetbsd	1
#  define OS		"NetBSD"
#elif defined (__OpenBSD__) || defined (OpenBSD)
#  include <sys/event.h>
#  include <sys/tree.h>
#  include <sys/sysctl.h>
#  include <sys/reboot.h>
#  include <sys/tree.h>
#  include <machine/cpu.h>
#  include <paths.h>
#  include <util.h>
#  include <ttyent.h>
#  include <readpassphrase.h>
#  include <login_cap.h>
#  define OSOpenBSD	1
#  define OSopenbsd	1
#  define OS		"OpenBSD"
#elif defined (__MirBSD__)
#  define OSMirBSD	1
#  define OSmirbsd	1
#  define OS		"MirBSD"
#elif defined (MidnightBSD)
#  define OSMidnightBSD		1
#  define OSmidnightbsd		1
#  define OS		"MidnightBSD"
#elif defined (bsdi)
#  define OSbsdi	1
#elif defined (aix) || defined (_AIX)
#  include <utmpx.h>
#  define OSsysv	1
#  define OSaix		1
#  define OS		"AIX"
#elif defined (hpux) || defined (__hpux__)
#  include <utmpx.h>
#  define OSsysv	1
#  define OShpux	1
#  define OS		"HPUX"
#elif defined (__sun__) || defined (__sun) || defined (sun)
/*
//if defined (__sun__) && defined (__sparc__) && defined (__unix__) && defined (__svr4__)
*/
#  include <utmp.h>
#  include <utmpx.h>
#  include <utility.h>
#  include <siginfo.h>
#  include <libgen.h>
/*
#  include <libcontract.h>
*/
#  include <libscf.h>
#  include <libnvpair.h>
#  include <mp.h>
#  include <priv.h>
#  include <pool.h>
#  include <stropts.h>
#  include <door.h>
#  include <rctl.h>
#  include <ucontext.h>
#  include <ucred.h>
#  include <vdp.h>
#  include <volmgt.h>
#  include <zone.h>
#  include <xpol.h>
#  include <xti.h>
#  include <sys/conf.h>
#  include <sys/uadmin.h>
#  include <sys/mount.h>
#  include <sys/mnttab.h>
#  include <sys/vfstab.h>
#  include <sys/vtoc.h>
#  include <security/pam_appl.h>
#  include <security/pam_modules.h>
#  include <uuid/uuid.h>
#  include <project.h>
# if defined (__unix__) && defined (__svr4__)
#   define OSsunos	5
#   define OSsunos5	1
#   define OSsolaris	1
#   define OSsysv	1
#   define OS		"Solaris"
# else
#   define OSsunos	4
#   define OSsunos4	1
/*
#   define OSbsd	1
*/
#   define OS		"SunOS"
# endif
#elif defined (Darwin) || defined (__APPLE__) || (defined (APPLE) && defined (MACH))
#  include <sys/event.h>
#  define OSDarwin	1
#  define OSdarwin	1
/*
#   define OSbsd	1
*/
#  define OS		"Darwin"
#endif

/* feature test macros */
#if defined (SIGRTMIN) && defined (SIGRTMAX)
#  define HAVE_RT_SIGS		1
#endif

#if defined (__GLIBC__) && defined (_GNU_SOURCE)
#  define HAVE_EACCESS		1
#elif defined (__MUSL__) && defined (_GNU_SOURCE)
#  define HAVE_EACCESS		1
#elif defined (OSLinux) && defined (_GNU_SOURCE)
#  define HAVE_EACCESS		1
#else
#endif

/* characters with special meaning to the shell */
#define SHELL_META_CHARS	"#@$|&?!%\\=*~^:;<>()[]{}`\'\""
/*
#define SHELL_META_CHARS	"~;|&!?*@$`:=%#<>()[]{}\'\""
#define SHELL_META_CHARS	"~`!$^&*()=|\\{}[];\"'<>?"
*/

/* mark a function variable as unused */
#ifndef UNUSED
#  define UNUSED(x) UNUSED_ ## x __attribute__ ((unused))
#endif

#ifndef S_IXUGO
#  define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

#ifndef S_ISEXEC
#  define S_ISEXEC(m) (((m) & S_IXUSR) == S_IXUSR)
#endif

#ifndef NSIG
#  ifdef _NSIG
#    define NSIG	_NSIG
#  else
#    define NSIG	32
#  endif
#endif

#ifndef UNIX_PATH_MAX
#  ifdef OSLinux
#    define UNIX_PATH_MAX 108
#  else
/*   define UNIX_PATH_MAX 92 */
#  endif
#endif

#ifndef _PATH_DEV
#  define _PATH_DEV		"/dev/"
#endif

#ifndef _PATH_TMP
#  define _PATH_TMP		"/tmp/"
#endif

#ifndef _PATH_VARRUN
#  define _PATH_VARRUN		"/var/run/"
#endif

#ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL		"/dev/null"
#endif

#ifndef _PATH_TTY
#  define _PATH_TTY		"/dev/tty"
#endif

#ifndef _PATH_CONSOLE
#  define _PATH_CONSOLE		"/dev/console"
#endif

#ifndef _PATH_UTMP
#  define _PATH_UTMP		"/var/run/utmp"
#endif

#ifndef _PATH_WTMP
#  define _PATH_WTMP		"/var/log/wtmp"
#endif

#ifdef OSLinux
/* Virtual console master */
#  define VT_MASTER		"/dev/tty0"
/* Logical system console */
/*  define CONSOLE		"/dev/console" */
#  define CONSOLE		_PATH_CONSOLE
#endif

/* helper macros */
/*
//define UNCONST(a) ( (void *) (unsigned long) (const void *) (a) )
#define new(t) ( (t *) malloc( sizeof(t) ) )
#define newa(t,n) ( (t *) malloc( sizeof(t) * (n) ) )
#define SHIFT ( -- argc, ++ argv )
*/
#define XSTR(x) #x
#define STR(x) XSTR(x)
#define NELEMS(a) ( sizeof ( a ) / sizeof ( ( a ) [ 0 ] ) )
#define ARRAY_SIZE(a) ( sizeof ( a ) / sizeof ( ( a ) [ 0 ] ) )
#define ISCLR(word, bit)   ((word & (1 << (bit)) ? 0 : 1))
#define ISSET(word, bit)   ((word & (1 << (bit)) ? 1 : 0))
#define ISOTHER(word, bit) ((word & ~ (1 << (bit)) ? 1 : 0))
#define SETBIT(word, bit)   (word |= (1 << (bit)))
#define CLRBIT(word, bit)   (word &= ~ (1 << (bit)))
#define ISMEMBER(set, val) ((set) & (1 << (val)))
#define DELSET(set, val)   ((set) &= ~ (1 << (val)))
#define ADDSET(set, val)   ((set) |= (1 << (val)))
#define EMPTYSET(set)      ((set) = 0)
#define EXISTS(p) ( 0 == access ( p, F_OK ) )

#define STREQU(str1, str2) \
	(((str1)[0] == (str2)[0]) && (strcmp(str1, str2) == 0))
#define STRNEQU(str1, str2, cnt) \
	(((str1)[0] == (str2)[0]) && (strncmp(str1, str2, cnt) == 0))

#define print(s) (void) write( STDOUT_FILENO, s, strlen( s ) )
#define fd_print(fd, s) (void) write( fd, s, strlen( s ) )
#define make_char_dev(x, m, maj, min) mknod ( ( x ), S_IFCHR | ( m ), makedev ( ( maj ),( min ) ) )
#define make_block_dev(x, m, maj, min) mknod ( ( x ), S_IFBLK | ( m ), makedev ( ( maj ), ( min ) ) )

#define L_ADD_CONST(S,f) lua_pushinteger( S, f ) ; lua_setfield( S, -2, #f ) ;

#define L_ADD_INT_KEY(S,k,v)	(void) lua_pushliteral( S, k ) ; \
				lua_pushinteger( S, v ) ; \
				lua_rawset( S, -3 ) ;

#define L_CLEAR_STACK(S)	{ const int n = lua_gettop( S ) ; \
				if ( 0 < n ) lua_pop( S, n ) ; }

#define NOINTR(Call)	while ( 0 > ( Call ) && EINTR == errno ) { ; }
#define CLOSEFD(fd)	while ( close ( fd ) && ( EINTR == errno ) ) { ; }

#define BLOCK_SIGS \
	sigset_t ss ; \
	sigfillset( & ss ) ; \
	NOINTR(sigprocmask( SIG_BLOCK, & ss, & ss ))

#define UNBLOCK_SIGS \
	NOINTR(sigprocmask( SIG_SETMASK, & ss, NULL ))

#define RESET_SIG_ALL \
	{ int i ; sigset_t ss ; struct sigaction sa ; \
	(void) sigemptyset( & sa.sa_mask ) ; \
	sa.sa_flags = 0 ; \
	sa.sa_handler = SIG_DFL ; \
	for( i = 1 ; i < NSIG ; ++ i ) { NOINTR(sigaction( i, & sa, NULL )) } \
	(void) sigfillset( & ss ) ; \
	NOINTR(sigprocmask( SIG_UNBLOCK, & ss, & ss )) ; }

/* set a handler function for a given signal */
#define HANDLESIG(sa, sig, fun, flags) \
  do { sa . sa_handler = fun ; sa . sa_flags = flags ; \
    (void) sigemptyset ( & sa . sa_mask ) ; \
    (void) sigaction ( sig, & sa, NULL ) ; } while ( 0 )

/* set a siginfo style handler function for a given signal */
#define SETSIGINFO(sa, sig, fun, flags) \
  do { sa . sa_sigaction = fun ; sa . sa_flags = SA_SIGINFO | flags ; \
    (void) sigemptyset ( & sa . sa_mask ) ; \
    (void) sigaction ( sig, & sa, NULL ) ; } while ( 0 )

/* ignore a given signal */
#define IGNSIG(sa, sig, flags) \
  do { sa . sa_handler = SIG_IGN ; sa . sa_flags = flags ; \
    (void) sigemptyset ( & sa . sa_mask ) ; \
    (void) sigaction ( sig, & sa, NULL ) ; } while ( 0 )

/* restore the default behaviour for a given signal */
#define DFLSIG(sa, sig, flags) \
  do { sa . sa_handler = SIG_DFL ; sa . sa_flags = flags ; \
    (void) sigemptyset ( & sa . sa_mask ) ; \
    (void) sigaction ( sig, & sa, NULL ) ; } while ( 0 )

#define IAMCONFUSED { \
	(void) fprintf( stderr, "Confused by errors. Waiting for a signal...\n" ) ; \
	(void) pause() ; (void) fprintf( stderr, "Got signal. Continuing ...\n" ) ; }

#define COMPLAIN(a) \
	fprintf( stderr, "%s: %s: %s\n", __progname, a, strerror( errno ) )
#define COMPLAIN2(a,b) \
	fprintf( stderr, "%s: " a ": %s\n", __progname, b, strerror( errno ) )

#define SHOUT(a) \
	fprintf( stderr, "%s: %s\n", __progname, a )
#define SHOUT2(a,b) \
	fprintf( stderr, "%s: " a "\n", __progname, b )

#define MYWARN(a) \
	fprintf( stderr, "%s: %s: %s\n", __progname, a, strerror( errno ) )
#define MYWARN2(a,b) \
	fprintf( stderr, "%s: " a ": %s\n", __progname, b, strerror( errno ) )

/* include settings tunable by user */
/*
#include "config.h"
*/

#define HOSTNAME		"darkstar"
#define SYSINIT_VERSION		"0.01.6"

#ifndef SYSINIT_BASE
#  define SYSINIT_BASE		"/etc/pesi"
#endif

#ifndef RC_UP
#  define RC_UP			SYSINIT_BASE "/up"
#endif

#ifndef RC_DOWN
#  define RC_DOWN		SYSINIT_BASE "/down"
#endif

#ifndef RC_CRASH
#  define RC_CRASH		SYSINIT_BASE "/crash"
#endif

#ifndef SYSINIT_HOSTNAME
#  define SYSINIT_HOSTNAME	"darkstar"
#endif

#ifndef PATH
/*
#  define PATH "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin"
*/
#  define PATH "/bin:/sbin:/usr/bin:/usr/sbin"
#endif

#ifndef SHELL
#  ifdef _PATH_BSHELL
#    define SHELL _PATH_BSHELL
#  else
#    define SHELL		"/bin/sh"
#  endif
#endif

#ifndef LOGIN
#  ifdef _PATH_LOGIN
#    define LOGIN _PATH_LOGIN
#  else
#    define LOGIN		"/bin/login"
#  endif
#endif

#if defined (OSLinux) || defined (OSsolaris)
#  ifndef SULOGIN
#    define SULOGIN		"/sbin/sulogin"
#  endif
#endif

#if 0

/* Since Linux 2.5.18, but not in GLIBC until 2.19 */
#ifdef OSLinux
#  ifndef RB_SW_SUSPEND
#    define RB_SW_SUSPEND	0xd000fce2
#  endif
#endif

/* check for constants needed for reboot */
#ifndef RB_HALT_SYSTEM
#  ifdef OSLinux
#    define RB_HALT_SYSTEM  0xcdef0123
#    define RB_ENABLE_CAD   0x89abcdef
#    define RB_DISABLE_CAD  0
#    define RB_POWER_OFF    0x4321fedc
#    define RB_AUTOBOOT     0x01234567
#  elif defined(RB_HALT)
#    define RB_HALT_SYSTEM  RB_HALT
#  endif
#endif

/* Stop system and switch power off if possible. */
#ifndef RB_POWER_OFF
#  if defined (RB_POWERDOWN)
#    define RB_POWER_OFF  RB_POWERDOWN
#  elif defined (RB_POWEROFF)
#    define RB_POWER_OFF  RB_POWEROFF
#  elif defined (OSLinux)
#    define RB_POWER_OFF  0x4321fedc
#  else
#    warning "poweroff unsupported, using halt as fallback"
#    define RB_POWER_OFF  RB_HALT_SYSTEM
#  endif
#endif

#if defined (OSLinux)
#  define SYSTEM_HALT RB_HALT_SYSTEM
#  define SYSTEM_POWEROFF RB_POWER_OFF
#  define SYSTEM_REBOOT RB_AUTOBOOT
#elif defined (OSfreebsd) || defined (OSdragonfly)
#  define SYSTEM_HALT RB_HALT
#  define SYSTEM_POWEROFF (RB_HALT | RB_POWEROFF)
#  define SYSTEM_REBOOT RB_AUTOBOOT
#elif defined (OSnetbsd) || defined (OSopenbsd)
#  define SYSTEM_HALT RB_HALT
#  define SYSTEM_POWEROFF (RB_HALT | RB_POWERDOWN)
#  define SYSTEM_REBOOT RB_AUTOBOOT
#elif defined (OSsolaris)
/*  define SYSTEM_REBOOT RB_AUTOBOOT	*/
#  define SYSTEM_HALT AD_HALT
#  define SYSTEM_POWEROFF AD_POWEROFF
#  define SYSTEM_REBOOT AD_BOOT
#endif

/* signals we know about */

/* This signal is used for communication between pid #1
 * and the process supervisor */
#ifdef SIGRTMAX
#  define COMM_SIGNAL		(0 + SIGRTMAX)
#  define RECONF_SIGNAL		(SIGRTMAX - 1)
   /* this is used to toggle respawning of terminated services */
#  define RESPAWN_SIGNAL	(SIGRTMAX - 2)
#else
   /* deep shit */
#  warning "no real time signals ? building on OpenBSD ? good luck ..."
#endif

/* This signal tells process #1 to enter what systemd terms "emergency mode".
 * We go with systemd since BSD init, upstart, and System V init do not
 * have a defined convention. */
#ifdef SIGRTMIN
#  define EMERGENCY_SIGNAL	(2 + SIGRTMIN)
/* This signal tells process #1 to enter what BSD terms "multi user mode"
 * and what systemd terms "default mode".
 * On Linux and BSD, we go with systemd since upstart, BSD init, and
 * System V init do not have a defined convention. */
#  define NORMAL_SIGNAL		(0 + SIGRTMIN)

/* This signal tells process #1 to enter what BSD and SysV init term
 * "single user mode/runlevel" and what systemd terms "rescue mode".
 */
   /* On Linux, we go with systemd since upstart and System 5 init
    * do not have a defined convention. */
#  define RESCUE_SIGNAL		(1 + SIGRTMIN)

/* This signal tells process #1 to activate the "reboot" service,
 * i. e. in our case that means to call the (Lua) stage 3
 * procedure, then kill all remaining processes, call the
 * (Lua) stage 4 procedure thereafter and reboot the system.
 */
   /* On Linux, we go with systemd since upstart and System 5 init
    * do not have a defined convention.
    * (but BusyBox init and finit use SIGTERM) */
#  define REBOOT_SIGNAL		(5 + SIGRTMIN)

/* This signal tells process #1 to activate the "halt" service,
 * i. e. (in our case) call the (Lua) stage 3 procedure etc.,
 * as in the case of the reboot signal.
 */
   /* On Linux, we go with systemd since upstart and System 5 init
    * do not have a defined convention. */
#  define HALT_SIGNAL		(3 + SIGRTMIN)

/*
 * This signal tells process #1 to activate the "poweroff" service.
 */
   /* On Linux, we go with systemd since upstart and System 5 init do not
    * have a defined convention. */
#  define POWEROFF_SIGNAL	(4 + SIGRTMIN)

#else /* no real time signals ? */

   /* On the BSDs, it is SIGTERM. */
#  define RESCUE_SIGNAL		SIGTERM

   /* On the BSDs, it is SIGINT and overlaps secure-attention-key. */
#  define REBOOT_SIGNAL		SIGINT

   /* On the BSDs, it is SIGUSR1. (same for BusyBox init and finit) */
#  define HALT_SIGNAL		SIGUSR1

   /* On the BSDs, it is SIGUSR2. (same for BusyBox init and finit) */
#  define POWEROFF_SIGNAL	SIGUSR2

#endif /* SIGRTMIN */

#ifdef RESCUE_SIGNAL
#  define SINGLE_USER_SIGNAL RESCUE_SIGNAL
#endif

/* end of signal definitions */

#endif /* end if 0 */

extern char ** environ ;
/* extern int errno ; */
extern const char * const sys_siglist [] ;

#if defined (__GLIBC__)
extern char * __progname ;
#endif

#endif /* end of header */

