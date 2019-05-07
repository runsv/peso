/*
 * functions to configure/shutdown network interfaces
 */

#if 0
/* network commands of general utility */
struct ifconfigtable {
  char * name ;
  int ioctlname ;
  struct sockaddr * where ;
} ;

static const struct ifconfigtable param_table [ ] = {
  { "addr", SIOCSIFADDR, & req . ifr_addr },
  { "netmask", SIOCSIFNETMASK, & req . ifr_netmask },
  { "broadcast", SIOCSIFBRDADDR, & req . ifr_broadaddr },
  { "pointopoint", SIOCSIFDSTADDR, & req . ifr_dstaddr },
  { NULL, 0, NULL }
} ;

static int Lnet_config ( lua_State * L )
{
  int i, flags = 0 ;
  int fd = -1 ;
  struct ifreq req ;
  struct rtentry rte ;
  char string [ 128 ] = { 0 } ;

  switch ( argv [ 0 ] [ 0 ] ) {
      case 'i' : /* ifconfig */
        if ( argc == 1 || argc > 4 ) goto wna ;

	strncpy ( req . ifr_name, argv [ 1 ], IFNAMSIZ - 1 ) ; /* ifname */

	if ( 0 > ioctl ( fd, SIOCGIFFLAGS, & req ) ) goto ioctlerr ;

	flags = req . ifr_flags ;

	if ( argc == 2 ) { /* report about one interface */
	    sprintf ( string,"%s: %s", req . ifr_name,
		    flags & IFF_UP ? "up" : "down" ) ;

	    if ( ioctl ( fd, SIOCGIFADDR, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string," addr %s ",
		    inet_ntoa ( ( * (struct sockaddr_in *)
			       & req . ifr_addr ) . sin_addr ) ) ;

	    if ( ioctl ( fd, SIOCGIFNETMASK, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string," mask %s ",
		    inet_ntoa ( ( * (struct sockaddr_in *)
			       & req . ifr_netmask ) . sin_addr ) ) ;

	    if ( ioctl ( fd, SIOCGIFBRDADDR, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string," bcast %s\n",
		    inet_ntoa ( ( * (struct sockaddr_in *)
			       & req . ifr_broadaddr ) . sin_addr ) ) ;

	    if ( ioctl ( fd, SIOCGIFDSTADDR, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string,"\tpointopoint %s ",
		    inet_ntoa ( ( * (struct sockaddr_in *)
			       & req . ifr_addr ) . sin_addr ) ) ;
	    Tcl_AppendResult ( interp, string, NULL ) ;

	    if ( ioctl ( fd, SIOCGIFHWADDR, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string, " hwaddr %02x:%02x:%02x:%02x:%02x:%02x ",
		    req . ifr_hwaddr . sa_data [ 0 ] & 0xff,
		    req . ifr_hwaddr . sa_data [ 1 ] & 0xff,
		    req . ifr_hwaddr . sa_data [ 2 ] & 0xff,
		    req . ifr_hwaddr . sa_data [ 3 ] & 0xff,
		    req . ifr_hwaddr . sa_data [ 4 ] & 0xff,
		    req . ifr_hwaddr . sa_data [ 5 ] & 0xff ) ;
	    Tcl_AppendResult ( interp, string, NULL ) ;

	    if ( ioctl ( fd, SIOCGIFMTU, & req ) < 0 ) goto ioctlerr ;
	    sprintf ( string, " mtu %i ", (int) req . ifr_mtu ) ;
	    Tcl_AppendResult ( interp, string, NULL ) ;

	    return TCL_OK ;
	}

	if ( argc == 3 ) { /* up or down */
	    i = 0 ; /* number of successfull operations */

	    if ( 0 == strcmp ( argv [ 2 ], "up" ) ) {
		i++ ; flags |= IFF_UP ;
	    }

	    if ( 0 == strcmp ( argv [ 2 ], "down" ) ) {
		i ++ ; flags &= ~ ( IFF_UP ) ;
	    }

	    if ( i ) {
		req . ifr_flags = flags ;
		if ( ioctl ( fd, SIOCSIFFLAGS, & req ) < 0 ) goto ioctlerr ;
		return TCL_OK ;
	    }
	}

	if ( argc == 4 ) {
	    struct in_addr addr ;

	    if ( 0 == inet_aton ( argv [ 3 ], & addr ) ) goto wna ;

	    for ( i = 0 ; param_table [ i ] . name ; i ++ ) {
		if ( 0 == strcmp ( param_table [ i ] . name, argv [ 2 ] ) ) {
		    (* (struct sockaddr_in *) ( param_table [ i ] . where ) )
			. sin_addr = addr ;
		    param_table [ i ] . where -> sa_family = AF_INET ;
		    if ( ioctl ( fd, param_table [ i ] . ioctlname, & req ) )
			goto ioctlerr ;
		    return TCL_OK ;
		}
	    }
	}
  }

  return TCL_ERROR ;
}

/* maximal number of interfaces */
#define MAX_IFS	64

/* First, we find all shaper devices and down them.
 * Then we down all real interfaces.
 */
static int Lif_down_all ( lua_State * L )
{
  int numif, shaper, i = 0 ;
  struct ifreq ifr [ MAX_IFS ] ;
  struct ifconf ifc ;
  const int fd = socket ( AF_INET, SOCK_DGRAM, 0 ) ;

  if ( 0 > fd ) {
    (void) fprintf ( stderr, "ifdown: " ) ;
    perror ( "socket(2)" ) ;
    return -1 ;
  }

  ifc . ifc_len = sizeof ( ifr ) ;
  ifc . ifc_req = ifr ;

  if ( 0 > ioctl ( fd, SIOCGIFCONF, & ifc ) ) {
	(void) fprintf ( stderr, "ifdown: " ) ;
	perror ( "SIOCGIFCONF" ) ;
	(void) close_fd ( fd ) ;
	return -1 ;
  }

  numif = ifc . ifc_len / sizeof ( struct ifreq ) ;

  for ( shaper = 1 ; shaper >= 0 ; -- shaper ) {
	for ( i = 0 ; i < numif ; ++ i ) {
			if ( ( strncmp ( ifr [ i ] . ifr_name, "shaper", 6 ) == 0 )
			    != shaper) continue;

			if ( strcmp ( ifr [ i ] . ifr_name, "lo" ) == 0 )
				continue ;

			if ( strchr ( ifr [ i ] . ifr_name, ':' ) != NULL )
				continue ;

			/* read interface flags */
			if ( ioctl ( fd, SIOCGIFFLAGS, & ifr [ i ] ) < 0 ) {
				(void) fprintf ( stderr, "ifdown: shutdown " ) ;
				perror ( ifr [ i ] . ifr_name ) ;
				continue ;
			}

#ifdef ifr_flags
# define IRFFLAGS	ifr_flags
#else
/* Present on kFreeBSD */
# define IRFFLAGS	ifr_flagshigh
#endif
        if ( ifr [ i ] . IRFFLAGS & IFF_UP ) {
		ifr [ i ] . IRFFLAGS &= ~ ( IFF_UP ) ;
		if ( ioctl ( fd, SIOCSIFFLAGS, & ifr [ i ] ) < 0 ) {
			fprintf ( stderr, "ifdown: shutdown " ) ;
			perror ( ifr [ i ] . ifr_name ) ;
		}
        }
#undef IRFFLAGS
		}
  }

  (void) close_fd ( fd ) ;
  return 0 ;
}
#endif

/* if_* - basic ifconfig like operations on a given interface */
static int Lif_up ( lua_State * L )
{
#if defined (OSLinux)
  const int n = lua_gettop ( L ) ;

  if ( 0 < n && 0 != lua_isstring ( L, 1 ) ) {
    int fd, i, r = 0 ;
    struct ifreq ifr ;
    struct sockaddr_in * sin = (struct sockaddr_in *) & ifr . ifr_addr ;
    const char * ifname = luaL_checkstring ( L, 1 ) ;
    const char * addr = ( 1 < n ) ? luaL_checkstring ( L, 2 ) : NULL ;
    const char * mask = ( 2 < n ) ? luaL_checkstring ( L, 3 ) : NULL ;

    if ( ! ( ifname && * ifname ) ) {
      return luaL_argerror ( L, 1, "invalid interface name" ) ;
    }

    fd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_IP ) ;

    if ( 0 > fd ) {
      i = errno ;
      lua_pushinteger ( L, -9 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }

    (void) memset( & ifr, 0, sizeof ( ifr ) ) ;
    ifr . ifr_addr . sa_family = AF_INET ;
    (void) strcopy ( ifr . ifr_name, ifname, IFNAMSIZ - 1 ) ;

    if ( addr && * addr ) {
      if ( 1 == inet_pton ( AF_INET, addr, & sin -> sin_addr ) ) {
        r = ioctl ( fd, SIOCSIFADDR, & ifr ) ;
      }

      if ( mask && strcmp ( addr, "0.0.0.0" ) &&
        ( 1 == inet_pton ( AF_INET, mask, & sin -> sin_addr ) ) )
      {
        /* set netmask of non-zero ip address */
        r += ioctl ( fd, SIOCSIFNETMASK, & ifr ) ;
      }
    }

    i = ioctl ( fd, SIOCGIFFLAGS, & ifr ) ;
    r += i ;

    if ( 0 == i ) {
      ifr . ifr_flags |= IFF_UP ;
      r += ioctl ( fd, SIOCSIFFLAGS, & ifr ) ;
    }

    i = errno ;
    (void) close_fd ( fd ) ;
    lua_pushinteger ( L, r ) ;

    if ( r ) {
      lua_pushinteger ( L, ( 0 < i ) ? i : errno ) ;
      return 2 ;
    }

    return 1 ;
  }

  return luaL_error ( L, "interface name expected" ) ;
#else
  return luaL_error ( L, "OS not supported" ) ;
#endif
}

static int Lif_down ( lua_State * L )
{
#if defined (OSLinux)
  const char * ifname = luaL_checkstring ( L, 1 ) ;

  if ( ifname && * ifname ) {
    int i, r = 0 ;
    struct ifreq ifr ;
    const int fd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_IP ) ;

    if ( 0 > fd ) {
      i = errno ;
      lua_pushinteger ( L, -9 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }

    (void) memset( & ifr, 0, sizeof ( ifr ) ) ;
    ifr . ifr_addr . sa_family = AF_INET ;
    (void) strcopy ( ifr . ifr_name, ifname, IFNAMSIZ - 1 ) ;
    r = ioctl ( fd, SIOCGIFFLAGS, & ifr ) ;

    if ( 0 == r ) {
      ifr . ifr_flags &= ~ ( IFF_UP ) ;
      r += ioctl ( fd, SIOCSIFFLAGS, & ifr ) ;
    }

    i = errno ;
    (void) close_fd ( fd ) ;
    lua_pushinteger ( L, r ) ;

    if ( r ) {
      lua_pushinteger ( L, ( 0 < i ) ? i : errno ) ;
      return 2 ;
    }

    return 1 ;
  }

  return luaL_argerror ( L, 1, "invalid interface name" ) ;
#else
  return luaL_error ( L, "OS not supported" ) ;
#endif
}

/*
 * some route(8) functionality.
 * this is *way* incomplete for now.
 * it can only be used to set the netmask and default gateway
 * of network interfaces.
 */

/* sets the netmask */
static int Lroute_add_netmask ( lua_State * L )
{
#if defined (OSLinux)
  const char * ifname = luaL_checkstring ( L, 1 ) ;
  const char * addr = luaL_checkstring ( L, 2 ) ;
  const char * mask = luaL_checkstring ( L, 3 ) ;

  if ( addr && mask && ifname && * addr && * mask && * ifname ) {
    int i, fd ;
    struct rtentry rte ;

    (void) memset ( & rte, 0, sizeof ( rte ) ) ;
    rte . rt_dst . sa_family = AF_INET ;
    rte . rt_genmask . sa_family = AF_INET ;
    rte . rt_gateway . sa_family = AF_INET ;
    rte . rt_flags = RTF_UP ;
    rte . rt_metric = 0 ;
    rte . rt_dev = (char *) ifname ;

    i = inet_aton ( addr,
      & ( ( * (struct sockaddr_in *) & rte . rt_dst ) . sin_addr ) ) ;

    if ( 0 == i ) {
      return luaL_argerror ( L, 2, "invalid ip address" ) ;
    }

    i = inet_aton ( mask,
      & ( ( * (struct sockaddr_in *) & rte . rt_genmask ) . sin_addr ) ) ;

    if ( 0 == i ) {
      return luaL_argerror ( L, 3, "invalid netmask" ) ;
    }

    fd = socket ( AF_INET, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_RAW ) ;

    if ( 0 > fd ) {
      i = errno ;
      lua_pushinteger ( L, -3 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }

    i = ioctl ( fd, SIOCADDRT, & rte ) ;
    if ( i ) { i = errno ? errno : -1 ; }
    (void) close_fd ( fd ) ;
    if ( 0 < i ) { errno = i ; }
    return res_zero ( L, i ) ;
  }

  return luaL_error ( L, "3 non empty string args expected" ) ;
#else
  return luaL_error ( L, "OS not supported" ) ;
#endif
}

/* sets the default gateway */
static int Lroute_add_defgw ( lua_State * L )
{
#if defined (OSLinux)
  const char * ifname = luaL_checkstring ( L, 1 ) ;
  const char * gw = luaL_checkstring ( L, 2 ) ;

  if ( gw && ifname && * gw && * ifname ) {
    int i, fd ;
    struct rtentry rte ;

    (void) memset ( & rte, 0, sizeof ( rte ) ) ;
    rte . rt_dst . sa_family = AF_INET ;
    rte . rt_genmask . sa_family = AF_INET ;
    rte . rt_gateway . sa_family = AF_INET ;
    rte . rt_flags = RTF_UP | RTF_GATEWAY ;
    rte . rt_metric = 1 ;
    rte . rt_dev = (char *) ifname ;

    i = inet_aton ( gw,
      & ( ( * (struct sockaddr_in *) & rte . rt_gateway ) . sin_addr ) ) ;

    if ( 0 == i ) {
      return luaL_argerror ( L, 2, "invalid ip address" ) ;
    }

    fd = socket ( AF_INET, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_RAW ) ;

    if ( 0 > fd ) {
      i = errno ;
      lua_pushinteger ( L, -3 ) ;
      lua_pushinteger ( L, i ) ;
      return 2 ;
    }

    i = ioctl ( fd, SIOCADDRT, & rte ) ;
    if ( i ) { i = errno ? errno : -1 ; }
    (void) close_fd ( fd ) ;
    if ( 0 < i ) { errno = i ; }
    return res_zero ( L, i ) ;
  }

  return luaL_error ( L, "2 non empty string args expected" ) ;
#else
  return luaL_error ( L, "OS not supported" ) ;
#endif
}

/*
static int Lroute_delete ( lua_State * L )
{
  return 0 ;
}
*/

