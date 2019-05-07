/*
 * time related wrapper functions
 * ADD: clock_gettime
 */

extern int daylight ;
extern long int timezone ;
extern char * tzname [ 2 ] ;

static int Stzset ( lua_State * L )
{
  tzset () ;
  return 0 ;
}

static int Ltzget ( lua_State * L )
{
  lua_newtable ( L ) ;
  tzset () ;

  (void) lua_pushliteral ( L, "daylight" ) ;
  lua_pushboolean ( L, daylight ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "timezone" ) ;
  lua_pushinteger ( L, timezone ) ;
  lua_rawset ( L, -3 ) ;

  if ( tzname [ 0 ] && tzname [ 0 ] [ 0 ] ) {
    (void) lua_pushliteral ( L, "tzname" ) ;
    (void) lua_pushstring ( L, tzname [ 0 ] ) ;
    lua_rawset ( L, -3 ) ;
  }

  if ( tzname [ 1 ] && tzname [ 1 ] [ 0 ] ) {
    (void) lua_pushliteral ( L, "daylight_tzname" ) ;
    (void) lua_pushstring ( L, tzname [ 1 ] ) ;
    lua_rawset ( L, -3 ) ;
  }

  return 1 ;
}

static int Stime ( lua_State * L )
{
  lua_pushinteger ( L, time ( NULL ) ) ;
  return 1 ;
}

static int Sstime ( lua_State * L )
{
  time_t t = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= t ) {
    return res_zero ( L, stime ( & t ) ) ;
  }

  return luaL_argerror ( L, 1, "non negative integer required" ) ;
}

static int Sctime ( lua_State * L )
{
  const time_t t = time ( NULL ) ;
  const char * ts = ctime ( & t ) ;

  if ( ts && * ts ) {
    lua_pushstring ( L, ts ) ;
    return 1 ;
  }

  return 0 ;
}

static int Sftime ( lua_State * L )
{
  struct timeb tb ;

  (void) ftime ( & tb ) ;
  lua_pushinteger ( L, tb . time ) ;
  lua_pushinteger ( L, tb . millitm ) ;
  lua_pushinteger ( L, tb . timezone ) ;
  lua_pushinteger ( L, tb . dstflag ) ;
  return 4 ;
}

static int Sclock ( lua_State * L )
{
  lua_pushinteger ( L, clock () ) ;
  return 1 ;
}

static int Stimes ( lua_State * L )
{
  struct tms tm ;
  const clock_t c = times ( & tm ) ;

  if ( 0 > c ) {
    const int i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  lua_pushinteger ( L, c ) ;
  lua_pushinteger ( L, tm . tms_utime ) ;
  lua_pushinteger ( L, tm . tms_stime ) ;
  lua_pushinteger ( L, tm . tms_cutime ) ;
  lua_pushinteger ( L, tm . tms_cstime ) ;
  return 5 ;
}

static int res_usage ( lua_State * L, const int what )
{
  struct rusage us ;

  if ( getrusage ( what, & us ) ) {
    const int i = errno ;
    lua_pushnil ( L ) ;
    lua_pushinteger ( L, i ) ;
    return 2 ;
  }

  /* create a new table to hold the result */
  lua_newtable ( L ) ;

  (void) lua_pushliteral ( L, "utime_sec" ) ;
  lua_pushinteger ( L, us . ru_utime . tv_sec ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "utime_usec" ) ;
  lua_pushinteger ( L, us . ru_utime . tv_usec ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "stime_sec" ) ;
  lua_pushinteger ( L, us . ru_stime . tv_sec ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "stime_usec" ) ;
  lua_pushinteger ( L, us . ru_stime . tv_usec ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "maxrss" ) ;
  lua_pushinteger ( L, us . ru_maxrss ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "minflt" ) ;
  lua_pushinteger ( L, us . ru_minflt ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "maxflt" ) ;
  lua_pushinteger ( L, us . ru_majflt ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "inblock" ) ;
  lua_pushinteger ( L, us . ru_inblock ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "oublock" ) ;
  lua_pushinteger ( L, us . ru_oublock ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "nvcsw" ) ;
  lua_pushinteger ( L, us . ru_nvcsw ) ;
  lua_rawset ( L, -3 ) ;

  (void) lua_pushliteral ( L, "nivcsw" ) ;
  lua_pushinteger ( L, us . ru_nivcsw ) ;
  lua_rawset ( L, -3 ) ;

  return 1 ;
}

static int Lgetrusage_proc ( lua_State * L )
{
  return res_usage ( L, RUSAGE_SELF ) ;
}

static int Lgetrusage_children ( lua_State * L )
{
  return res_usage ( L, RUSAGE_CHILDREN ) ;
}

#ifdef RUSAGE_THREAD
static int Lgetrusage_thread ( lua_State * L )
{
  return res_usage ( L, RUSAGE_THREAD ) ;
}
#endif

static int Sgettimeofday ( lua_State * L )
{
  struct timeval tv ;

  if ( gettimeofday ( & tv, NULL ) ) {
    const int i = errno ;
    lua_pushinteger ( L, -1 ) ;
    lua_pushinteger ( L, i ) ;
  } else {
    lua_pushinteger ( L, tv . tv_sec ) ;
    lua_pushinteger ( L, tv . tv_usec ) ;
  }

  return 2 ;
}

static int Ssettimeofday ( lua_State * L )
{
  struct timeval tv ;

  tv . tv_sec = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;
  tv . tv_usec = (lua_Unsigned) luaL_optinteger ( L, 2, 0 ) ;
  return res_zero ( L, settimeofday ( & tv, NULL ) ) ;
}

#if defined (_POSIX_TIMERS) && (0 < _POSIX_TIMERS)
static int Sclock_getcpuclockid ( lua_State * L )
{
  int i = 0 ;
  clockid_t ci ;

  i = luaL_optinteger ( L, 1, 0 ) ;
  i = ( 0 < i ) ? i : 0 ;
  i = clock_getcpuclockid ( i, & ci ) ;
  lua_pushinteger ( L, i ) ;
  lua_pushinteger ( L, ci ) ;
  return 2 ;
}

static int Sclock_getres ( lua_State * L )
{
  int i, e = 0 ;
  struct timespec ts ;

  i = luaL_optinteger ( L, 1, CLOCK_REALTIME ) ;
  i = clock_getres ( i, & ts ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  } else {
    lua_pushinteger ( L, ts . tv_sec ) ;
    lua_pushinteger ( L, ts . tv_nsec ) ;
    return 3 ;
  }

  return 1 ;
}

static int Sclock_gettime ( lua_State * L )
{
  int i, e = 0 ;
  struct timespec ts ;

  i = luaL_optinteger ( L, 1, CLOCK_REALTIME ) ;
  i = clock_gettime ( i, & ts ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  } else {
    lua_pushinteger ( L, ts . tv_sec ) ;
    lua_pushinteger ( L, ts . tv_nsec ) ;
    return 3 ;
  }

  return 1 ;
}

static int Sclock_settime ( lua_State * L )
{
  int i, e = 0 ;
  struct timespec ts ;

  ts . tv_sec = (lua_Unsigned) luaL_checkinteger ( L, 1 ) ;
  ts . tv_nsec = (lua_Unsigned) luaL_checkinteger ( L, 2 ) ;
  i = luaL_optinteger ( L, 3, CLOCK_REALTIME ) ;
  i = clock_settime ( i, & ts ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}
#endif

/*
 * adjust the system clock
 */

#if defined (OSLinux)

static int hwclock_runs_in_utc ( void )
{
  FILE * fp = fopen ( "/etc/adjtime", "r" ) ;

  if ( fp ) {
    int i = 0, r = 0 ;
    char buf [ 128 ] = { 0 } ;

    while ( NULL != fgets ( buf , sizeof ( buf ) - 1, fp ) ) {
      ++ i ;
      if ( 3 >= i ) {
        char * str = strtok ( buf, " \t\n" ) ;
        if ( str && * str ) {
          r = strcmp ( "LOCAL", str ) ? 1 : 0 ;
        }
        break ;
      }
    }

    (void) fclose ( fp ) ;
    return r ;
  }

  return 1 ;
}

static int initialize_system_clock_timezone ( void )
{
  int i = 0 ;
  const time_t now = time ( NULL ) ;
  const struct tm * t = localtime ( & now ) ;
  const int seconds_west = - t -> tm_gmtoff ;
  const struct timeval * ztv = NULL ;
  struct timezone tz = { 0, 0 } ;

  if ( hwclock_runs_in_utc () ) {
    /* prevent the next call from adjusting the system clock */
    i += settimeofday ( ztv, & tz ) ;
  }

  /* Set the RTC/FAT local time offset, and (if not UTC) adjust the system
   * clock from local-time-as-if-UTC to UTC.
   */
  tz . tz_minuteswest = seconds_west / 60 ;
  i += settimeofday ( ztv, & tz ) ;

  return i ;
}

static int Lhwclock_runs_in_utc ( lua_State * L )
{
  lua_pushboolean ( L, hwclock_runs_in_utc () ) ;
  return 1 ;
}

static int Linitialize_system_clock_timezone ( lua_State * L )
{
  lua_pushinteger ( L, initialize_system_clock_timezone () ) ;
  return 1 ;
}

#elif defined (OSfreebsd) || defined (OSdragonfly)

static int hwclock_runs_in_utc ( void )
{
  int local = 0 ;
  size_t siz = sizeof ( local ) ;
  int oid [ CTL_MAXNAME ] = { 0 } ;
  size_t len = sizeof ( oid ) / sizeof ( * oid ) ;
  struct stat st ;

  sysctlnametomib ( "machdep.wall_cmos_clock", oid, & len ) ;
  sysctl ( oid, len, & local, & siz, 0, 0 ) ;
  if ( local ) { return 1 ; }

  //return 0 > access ( "/etc/wall_cmos_clock", F_OK ) ;
  return 0 > stat ( "/etc/wall_cmos_clock", & st ) ;
}

static void initialize_system_clock_timezone ( const char * pname )
{
  const struct timeval * ztv = NULL ;
  const int utc = hwclock_runs_in_UTC () ;
  const time_t now = time ( NULL ) ;
  const struct tm * l = localtime ( & now ) ;
  const int seconds_west = - l -> tm_gmtoff ;
  struct timezone tz = { 0, 0 } ;

  if ( utc ) {
    /* zero out the tz_minuteswest if it is non-zero */
    settimeofday ( ztv, & tz ) ;
  } else {
    int old_seconds_west = 0 ;
    int disable_rtc_set = 0, old_disable_rtc_set = 0 ;
    int wall_cmos_clock = ! utc, old_wall_cmos_clock = 0 ;
    size_t siz = 0 ;
    struct timeval tv = { 0, 0 } ;

    siz = sizeof ( old_disable_rtc_set ) ;
    sysctlbyname ( "machdep.disable_rtc_set", & old_disable_rtc_set, & siz,
      & disable_rtc_set, sizeof ( disable_rtc_set ) ) ;

    siz = sizeof ( old_wall_cmos_clock ) ;
    sysctlbyname ( "machdep.wall_cmos_clock", & old_wall_cmos_clock, & siz,
      & wall_cmos_clock, sizeof ( wall_cmos_clock ) ) ;

    siz = sizeof ( old_seconds_west ) ;
    sysctlbyname ( "machdep.adjkerntz", & old_seconds_west, & siz,
      & seconds_west, sizeof ( seconds_west ) ) ;

    if ( 0 == old_wall_cmos_clock ) { old_seconds_west = 0 ; }

    if ( disable_rtc_set != old_disable_rtc_set ) {
      sysctlbyname ( "machdep.disable_rtc_set", 0, 0,
        & old_disable_rtc_set, sizeof ( old_disable_rtc_set ) ) ;
    }

    /* Adjust the system clock from local-time-as-if-UTC to UTC,
     * and zero out the tz_minuteswest if it is non-zero
     */
    gettimeofday ( & tv, 0 ) ;
    tv . tv_sec += seconds_west - old_seconds_west ;
    settimeofday ( & tv, & tz ) ;

    if ( seconds_west != old_seconds_west ) {
      if ( pname && * pname ) {
        (void) fprintf ( stderr,
          "\n%s: WARNING:\tTimezone wrong.\n"
          "Please put machdep.adjkerntz=%i and machdep.wall_cmos_clock=1 in loader.conf.\n\n"
          , pname, seconds_west
        ) ;
      } else {
        (void) fprintf ( stderr,
          "\nWARNING:\tTimezone wrong.\n"
          "Please put machdep.adjkerntz=%i and machdep.wall_cmos_clock=1 in loader.conf.\n\n"
          , seconds_west
        ) ;
      }
    }
  }
}

static int Lhwclock_runs_in_utc ( lua_State * L )
{
  lua_pushboolean ( L, hwclock_runs_in_utc () ) ;
  return 1 ;
}

static int Linitialize_system_clock_timezone ( lua_State * L )
{
  return 0 ;
}

#endif

