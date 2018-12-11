#!/bin/sh
# -*- shell -*-

##
## common rc shell functions/subroutines
##

## output color definitions
#NORMAL="\\033[0;39m"         # Standard console grey
#SUCCESS="\\033[1;32m"        # Success is green
#WARNING="\\033[1;33m"        # Warnings are yellow
#FAILURE="\\033[1;31m"        # Failures are red
#INFO="\\033[1;36m"           # Information is light cyan
#BRACKET="\\033[1;34m"        # Brackets are blue

msg () {
  ## bold
  echo -e "\033[1m=> $*\033[m\n"
}

msg_ok () {
  ## bold/green
  echo -e "\033[1m\033[32m OK\033[m\n"
}

msg_warn () {
  ## bold/yellow
  echo -e "\033[1m\033[33mWARNING: $*\033[m"
}

msg_error () {
  ## bold/red
  echo -e "\033[1m\033[31mERROR: $*\033[m\n"
}

Log () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

Log_ok () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

Log_warn () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

Log_warn () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

start_log () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

## check a given return value
ev_ret () {
  local r=${1:-$?}

  if test 0 -eq "$r" ; then
    echo -e "\tOK"
  else
    echo -e "\tFAILED"
  fi

  return 0
}

## reverse a given "list"
## (a string containing a white space separated sequence of substrings)
rev () {
  if test 0 -lt "$#" ; then
    local i r=

    for i in ${1:-} ; do
      r="$i $r"
    done

    echo $r
  fi
}

is_frs () {
  test 0 -lt "$#" || return 2
  local i

  for i ; do
    test -f "$i" -a -s "$i" -a -r "$i" || return 1
  done

  return 0
}

## secure default process environment
## (shell settings, default permissions, process limits, signals)
def_env () {
  set +auf
  umask 022
  ulimit -Sc 0

  ## ignore keyboard interrupts
  #trap : SIGINT SIGQUIT
  trap : 2 3

  #PATH=/bin:/sbin:/usr/bin:/usr/sbin
  PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
  PATH=${PATH}:/busybox:/usr/busybox:/toybox:/usr/toybox
  export PATH
  #test -t 0 -o -t 1 -o -t 2 && export TERM=linux
  cd /
}

source_start_scripts () {
  if test -d /etc/runit/sh ; then
    for i in /etc/runit/sh/S?* ; do
      test -f "$i" -a -r "$i" -a -s "$i" && . "$i" start
    done
  fi
}

bootlogd_start () {
  echo "Starting bootlogd ... "
  bootlogd -p /run/bootlogd.pid -l /var/log/boot.log || return 1
}

bootlogd_stop () {
  test -f /run/bootlogd.pid || return 0
  echo "Stopping bootlogd ... "
  touch /var/log/boot.log
  kill $(< /run/bootlogd.pid)
  rm -f /run/bootlogd.pid
}

