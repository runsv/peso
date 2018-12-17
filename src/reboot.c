/*
 * call the OS specific version of reboot(2)
 * run the platform specific reboot(2) syscall with correct parameters
 */

/* not included in Lua module ? */
#ifndef USED_BY_PESI

#include "feat.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include "os.h"

#if defined (OSLinux)
#  include <linux/reboot.h>
#elif defined (OSsunos)
#  include <sys/uadmin.h>
#else
#endif

#endif /* not included in Lua module */

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

int do_reboot ( const int how )
{
  int i = 0 ;

  switch ( how ) {
      case 'c' :
#if defined (RB_DISABLE_CAD)
        i = RB_DISABLE_CAD ;
#else
        errno = ENOSYS ;
        return -1 ;
#endif
        break ;
      case 'C' :
#if defined (RB_ENABLE_CAD)
        i = RB_ENABLE_CAD ;
#else
        errno = ENOSYS ;
        return -1 ;
#endif
        break ;
      case 's' :
      case 'S' :
#if defined (RB_SW_SUSPEND)
        sync () ;
        i = RB_SW_SUSPEND ;
#else
        errno = ENOSYS ;
        return -1 ;
#endif
        break ;
      case 'k' :
      case 'K' :
#if defined (RB_KEXEC)
        sync () ;
        i = RB_KEXEC ;
#else
        errno = ENOSYS ;
        return -1 ;
#endif
        break ;
      case 'p' :
      case 'P' :
        sync () ;
#if defined (AD_POWEROFF)
        i = AD_POWEROFF ;
#elif defined (RB_POWER_OFF)
        (void) reboot ( RB_POWER_OFF ) ;
#  if defined (RB_HALT_SYSTEM)
        i = RB_HALT_SYSTEM ;
#  endif
#elif defined (RB_HALT)
        i = RB_HALT ;
#  if defined (RB_POWEROFF)
        i |= RB_POWEROFF ;
#  elif defined (RB_POWERDOWN)
        i |= RB_POWEROFF ;
#  endif
#endif
        break ;
      case 'h' :
      case 'H' :
        sync () ;
#if defined (AD_HALT)
        i = AD_HALT ;
#elif defined (RB_HALT_SYSTEM)
        i = RB_HALT_SYSTEM ;
#elif defined (RB_HALT)
        i = RB_HALT ;
#endif
        break ;
      case 'r' :
      case 'R' :
      default :
        sync () ;
#if defined (AD_BOOT)
        i = AD_BOOT ;
#elif defined (RB_AUTOBOOT)
        i = RB_AUTOBOOT ;
#endif
        break ;
  }

#if defined (OSsunos)
  return uadmin ( A_SHUTDOWN, i, NULL ) ;
#elif defined (OSnetbsd)
  return reboot ( i, NULL ) ;
#else
  return reboot ( i ) ;
#endif
}

#if defined (OSsunos)

int sys_reboot ( const int how )
{
  int cmd = AD_BOOT ;

  sync () ;

  switch ( how ) {
    case 'o' :
    case 'p' :
    case 'P' :
      cmd = AD_POWEROFF ;
      break ;
    case 'h' :
    case 'H' :
      cmd = AD_HALT ;
      break ;
    case 'r' :
    case 'R' :
    default :
      cmd = AD_BOOT ;
      break ;
  }

  return uadmin ( A_SHUTDOWN, cmd, NULL ) ;
}

#elif defined (OSLinux)

int sys_reboot ( const int how )
{
  int cmd = RB_AUTOBOOT ;

  switch ( how ) {
    case 'c' :
#ifdef RB_DISABLE_CAD
      cmd = RB_DISABLE_CAD ;
#else
      errno = ENOSYS ;
      return -1 ;
#endif
      break ;
    case 'C' :
#ifdef RB_ENABLE_CAD
      cmd = RB_ENABLE_CAD ;
#else
      errno = ENOSYS ;
      return -1 ;
#endif
      break ;
    case 'k' :
    case 'K' :
#ifdef RB_KEXEC
      sync () ;
      cmd = RB_KEXEC ;
#else
      errno = ENOSYS ;
      return -1 ;
#endif
      break ;
    case 'o' :
    case 'p' :
    case 'P' :
      sync () ;
      (void) reboot ( RB_POWER_OFF ) ;
    case 'h' :
    case 'H' :
      sync () ;
      cmd = RB_HALT_SYSTEM ;
      break ;
    case 's' :
    case 'S' :
#ifdef RB_SW_SUSPEND
      sync () ;
      cmd = RB_SW_SUSPEND ;
#else
      errno = ENOSYS ;
      return -1 ;
#endif
      break ;
    case 'r' :
    case 'R' :
    default :
      sync () ;
      cmd = RB_AUTOBOOT ;
      break ;
  }

  return reboot ( cmd ) ;
}

/*
int os_reboot_with_cmd ( char * cmd )
{
  sync () ;
  return reboot ( LINUX_REBOOT_MAGIC, LINUX_REBOOT_MAGIC2,
    LINUX_REBOOT_CMD_RESTART2, cmd ) ;
}
*/

#elif defined (OSbsd)

int sys_reboot ( const int how )
{
  int cmd = RB_AUTOBOOT ;

  sync () ;

  switch ( how ) {
    case 'o' :
    case 'p' :
    case 'P' :
      cmd = RB_HALT ;
#if defined (RB_POWEROFF)
      cmd |= RB_POWEROFF ;
#elif defined (RB_POWERDOWN)
      cmd |= RB_POWERDOWN ;
#endif
      break ;
    case 'h' :
    case 'H' :
      cmd = RB_HALT ;
      break ;
    case 'r' :
    case 'R' :
    default :
      cmd = RB_AUTOBOOT ;
      break ;
  }

  return reboot ( cmd
#if defined (OSnetbsd)
    , NULL
#endif
    ) ;
}

#else

int sys_reboot ( const int how )
{
  sync () ;
  /*
  (void) reboot ( 0 ) ;
  */

  return -1 ;
}

#endif

