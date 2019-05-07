/*
 * wrapper functions for System V ipc related syscalls
 * currently restricted to SysV message queues (since we need them)
 * and hence lacking support for SysV semaphores and shared memory.
 *
 * public domain code
 */

/* wrapper for the ftok(3) SysV ipc key generator function */
static int Sftok ( lua_State * L )
{
  const char * path = luaL_checkstring ( L, 1 ) ;

  if ( path && * path ) {
    int i, e = 0 ;

    i = luaL_optinteger ( L, 2, 'A' ) ;
    i = ( 0xff & i ) ? i : 'A' ;
    i = ftok ( path, i ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( 0 > i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* wrapper for msgget() */
static int Smsgget ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;
  int e = luaL_optinteger ( L, 2, 0 ) ;

  i = msgget ( i, e ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( 0 > i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

/* wrapper for msgctl() */
static int Smsgctl ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  /* FIXME: complete it */
  if ( 0 <=i ) {
    struct msqid_ds mqs ;
    int e = luaL_checkinteger ( L, 2 ) ;

    if ( IPC_STAT == e ) {
      i = msgctl ( i, IPC_STAT, & mqs ) ;
      e = errno ;
      lua_pushinteger ( L, i ) ;

      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    } else if ( IPC_SET == e ) {
    } else if ( IPC_RMID == e ) {
      i = msgctl ( i, IPC_RMID, & mqs ) ;
      e = errno ;
      lua_pushinteger ( L, i ) ;

      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    }
  }

  return 0 ;
}

/* send a message to a given SysV message queue */
static int Smsgsnd ( lua_State * L )
{
  const int mqid = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= mqid ) {
    int e = 0 ;
    struct mq_msg_s mbuf ;
    int i = luaL_checkinteger ( L, 2 ) ;
    const char * msg = luaL_checkstring ( L, 3 ) ;

    /* make sure the mtype field has an allowed value */
    i = ( 0 < i ) ? i : getpid () ;
    /* zero out the whole message buffer structure before use */
    (void) memset ( & mbuf, 0, sizeof ( struct mq_msg_s ) ) ;
    mbuf . mtype = i ;
    if ( msg && * msg ) {
      (void) strcopy ( mbuf . mtext, msg, sizeof ( mbuf . mtext ) - 1 ) ;
    }

    i = msgsnd ( mqid, & mbuf, sizeof ( mbuf . mtext ), IPC_NOWAIT ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* receive a message from a given SysV message queue */
static int Smsgrcv ( lua_State * L )
{
  const int mqid = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= mqid ) {
    struct mq_msg_s mbuf ;
    int e = 0, i = luaL_optinteger ( L, 2, 0 ) ;

    /* zero out the whole message buffer structure before use */
    (void) memset ( & mbuf, 0, sizeof ( struct mq_msg_s ) ) ;
    mbuf . mtype = 0 ;
    i = msgrcv ( mqid, & mbuf,
      sizeof ( mbuf . mtext ) - 1, i, IPC_NOWAIT | MSG_NOERROR ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( 0 > i ) {
      lua_pushinteger ( L, e ) ;
    } else {
      lua_pushinteger ( L, mbuf . mtype ) ;
    }

    if ( 0 < i && sizeof ( mbuf . mtext ) > i && '\0' != mbuf . mtext [ 0 ] )
    {
      /* due to the preceding memset() call this string is NUL terminated */
      (void) lua_pushstring ( L, mbuf . mtext ) ;
      return 3 ;
    }

    return 2 ;
  }

  return 0 ;
}

/* set the access permissions and size limit of a given SysV message queue */
static int Lmqv_set ( lua_State * L )
{
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) {
    const int mqid = luaL_checkinteger ( L, 1 ) ;

    if ( 0 <= mqid && 1 < n ) {
      int i = 0, e = 0 ;
      struct msqid_ds mqs ;

      /* do a IPC_STAT first to see if the given SysV message queue id
       * is valid (i. e. the queue exists and is readable for us)
       * and to fill the msqid_ds struct with the current values
       */
      i = msgctl ( mqid, IPC_STAT, & mqs ) ;
      e = errno ;

      if ( i ) {
        lua_pushinteger ( L, i ) ;
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      i = luaL_checkinteger ( L, 2 ) ;
      mqs . msg_perm . mode = ( 0 < i ) && ( 00777 & i ) ? i : 00600 ;
      if ( 2 < n ) { i = luaL_checkinteger ( L, 3 ) ; }
      if ( 0 <= i ) { mqs . msg_perm . uid = i ; }
      if ( 3 < n ) { i = luaL_checkinteger ( L, 4 ) ; }
      if ( 0 <= i ) { mqs . msg_perm . gid = i ; }
      if ( 4 < n ) { i = luaL_checkinteger ( L, 5 ) ; }
      if ( 0 < i ) { mqs . msg_qbytes = i ; }
      i = msgctl ( mqid, IPC_SET, & mqs ) ;
      e = errno ;
      lua_pushinteger ( L, i ) ;

      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    }
  }

  return 0 ;
}

/* get info about a given SysV message queue */
static int Lmqv_stat ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;
    struct msqid_ds mqs ;

    i = msgctl ( i, IPC_STAT, & mqs ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      /* msgctl() call failed */
      lua_pushinteger ( L, e ) ;
    } else {
      /* msgctl() call succeeded, create a Lua table containing the values
       * returned into the msqid_ds structure and return that new table
       * as result to the caller.
       */
      lua_newtable ( L ) ;
      /* msg_ctime field */
      (void) lua_pushliteral ( L, "ctime" ) ;
      lua_pushinteger ( L, mqs . msg_ctime ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_rtime field */
      (void) lua_pushliteral ( L, "rtime" ) ;
      lua_pushinteger ( L, mqs . msg_rtime ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_stime field */
      (void) lua_pushliteral ( L, "stime" ) ;
      lua_pushinteger ( L, mqs . msg_stime ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_lrpid field */
      (void) lua_pushliteral ( L, "lrpid" ) ;
      lua_pushinteger ( L, mqs . msg_lrpid ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_lspid field */
      (void) lua_pushliteral ( L, "lspid" ) ;
      lua_pushinteger ( L, mqs . msg_lspid ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_qnum field */
      (void) lua_pushliteral ( L, "qnum" ) ;
      lua_pushinteger ( L, mqs . msg_qnum ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_qbytes field */
      (void) lua_pushliteral ( L, "qbytes" ) ;
      lua_pushinteger ( L, mqs . msg_qbytes ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . mode field */
      (void) lua_pushliteral ( L, "mode" ) ;
      lua_pushinteger ( L, mqs . msg_perm . mode ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . uid field */
      (void) lua_pushliteral ( L, "uid" ) ;
      lua_pushinteger ( L, mqs . msg_perm . uid ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . gid field */
      (void) lua_pushliteral ( L, "gid" ) ;
      lua_pushinteger ( L, mqs . msg_perm . gid ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . cuid field */
      (void) lua_pushliteral ( L, "cuid" ) ;
      lua_pushinteger ( L, mqs . msg_perm . cuid ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . cgid field */
      (void) lua_pushliteral ( L, "cgid" ) ;
      lua_pushinteger ( L, mqs . msg_perm . cgid ) ;
      lua_rawset ( L, -3 ) ;
#if defined (OSLinux)
      /* msg_perm . __key field */
      (void) lua_pushliteral ( L, "key" ) ;
      lua_pushinteger ( L, mqs . msg_perm . __key ) ;
      lua_rawset ( L, -3 ) ;
      /* msg_perm . __seq field */
      (void) lua_pushliteral ( L, "seq" ) ;
      lua_pushinteger ( L, mqs . msg_perm . __seq ) ;
      lua_rawset ( L, -3 ) ;
      /* __msg_cbytes field */
      (void) lua_pushliteral ( L, "cbytes" ) ;
      lua_pushinteger ( L, mqs . __msg_cbytes ) ;
      lua_rawset ( L, -3 ) ;
#endif
    }

    return 2 ;
  }

  return 0 ;
}

/* remove a given SysV message queue without emptying it first */
static int Lmqv_remove ( lua_State * L )
{
  int i = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= i ) {
    int e = 0 ;
    /* struct msqid_ds mqs ;		*/

    i = msgctl ( i, IPC_RMID, NULL ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* empty a given SysV message queue
 * (i. e. delete all messages it contains)
 */
static int Lmqv_empty ( lua_State * L )
{
  const int mqid = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= mqid ) {
    ssize_t s = 0 ;
    struct mq_msg_s mbuf ;

    do {
      s = msgrcv ( mqid, & mbuf,
        sizeof ( mbuf . mtext ) - 1, 0, IPC_NOWAIT | MSG_NOERROR ) ;
    } while ( 0 <= s ) ;
  }

  return 0 ;
}

/* empty and delete a given SysV message queue
 * (i. e. delete all messages it contains before removing it)
 */
static int Lmqv_rm ( lua_State * L )
{
  const int mqid = luaL_checkinteger ( L, 1 ) ;

  if ( 0 <= mqid ) {
    int i, e ;
    struct mq_msg_s mbuf ;

    do {
      i = msgrcv ( mqid, & mbuf,
        sizeof ( mbuf . mtext ) - 1, 0, IPC_NOWAIT | MSG_NOERROR ) ;
    } while ( 0 <= i ) ;

    i = msgctl ( mqid, IPC_RMID, NULL ) ;
    e = errno ;
    lua_pushinteger ( L, i ) ;

    if ( i ) {
      lua_pushinteger ( L, e ) ;
      return 2 ;
    }

    return 1 ;
  }

  return 0 ;
}

/* empty and delete a given (by its key !) SysV message queue
 * (i. e. delete all messages it contains before removing it)
 */
static int Lmqv_close ( lua_State * L )
{
  int k = luaL_checkinteger ( L, 1 ) ;

  if ( 0 < k && IPC_PRIVATE != k ) {
    int e = 0 ;

    k = msgget ( k, 0 ) ;
    e = errno ;

    if ( 0 > k ) {
      lua_pushinteger ( L, k ) ;
      lua_pushinteger ( L, e ) ;
      return 2 ;
    } else {
      int i = 0 ;
      struct mq_msg_s mbuf ;

      do {
        i = msgrcv ( k, & mbuf,
          sizeof ( mbuf . mtext ) - 1, 0, IPC_NOWAIT | MSG_NOERROR ) ;
      } while ( 0 <= i ) ;

      i = msgctl ( k, IPC_RMID, NULL ) ;
      e = errno ;
      lua_pushinteger ( L, i ) ;

      if ( i ) {
        lua_pushinteger ( L, e ) ;
        return 2 ;
      }

      return 1 ;
    }
  }

  return 0 ;
}

/* open/create a SysV message queue */
static int Lmqv_open ( lua_State * L )
{
  int i = 0x01, e = 0 ;
  const int n = lua_gettop ( L ) ;

  if ( 0 < n ) { i = luaL_checkinteger ( L, 1 ) ; }
  if ( ( 1 < n ) && lua_isboolean ( L, 2 ) )
  { e = lua_toboolean ( L, 2 ) ; }

  i = ( 1 > i ) ? 0x01 : i ;
  i = mqv_open ( i, e ) ;
  e = errno ;
  lua_pushinteger ( L, i ) ;

  if ( 0 > i ) {
    lua_pushinteger ( L, e ) ;
    return 2 ;
  }

  return 1 ;
}

