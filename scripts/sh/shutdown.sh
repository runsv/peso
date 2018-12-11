#!/bin/sh

##
## shell functions/subroutines for system shutdown
##

deactivate_vgs () {
  _group=${1:-All}
  vgs=$( vgs | wc -l )

  if test "$vgs" -gt 0 ; then
    msg "Deactivating $_group LVM Volume Groups ... "
    vgchange -an
  fi
}

prepare () {
  mkdir -pm 0755 /run/lock /run/sv /run/log /run/Log /run/monit /run/utmps \
    /run/sv/perp /run/sv/six /run/sv/encore /run/sv/runit \
    /run/log/perp /run/log/six /run/log/encore /run/log/runit \
    /var/log/sv/six /var/log/sv/encore /var/log/sv/perp /var/log/sv/perpd

  #cp /etc/monitrc /run/monit
  d=/var/sv
  #test -d "$d" && $bb cp -rL $d/runit $d/perp $d/encore $d/six /run/sv
  test -d "$d" && cp -rL "$d"/???* /run/sv
}

save_log () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

## bring down the loopback interface
lo_down () {
  :
}

stop_localnet () {
  log_info_msg "Bringing down the loopback interface ... "
  ip link set lo down
  ev_ret "$?"
}

save_clock () {
  log_info_msg "Setting hardware clock ... "
  hwclock --systohc ${CLOCKPARAMS} > /dev/null
  ev_ret "$?"
}

unmount_fs () {
  local KILLDELAY=0

  ## don't unmount virtual file systems like /run
  log_info_msg "Unmounting all other currently mounted file systems ... "
  while test "$KILLDELAY" -lt 3 && \
   ! umount -a -d -r -t notmpfs,nosysfs,nodevtmpfs,noproc,nodevpts > /dev/null
  do
    ev_ret "$?"
    KILLDELAY=$(( KILLDELAY + 1 ))
    do_stop_sendsignals
  done

  ev_ret "$?"
  sync
  KILLDELAY=0
  ## make sure / is mounted read only (umount bug)
  log_info_msg "Remonting root file system in read-only mode..."

  while test "$KILLDELAY" -lt 3 && ! mount -o remount,ro /
  do
    ev_ret "$?"
    KILLDELAY=$((KILLDELAY+1))
    do_stop_sendsignals
  done

  ev_ret "$?"

  ## make all LVM volume groups unavailable, if appropriate
  ## this fails if swap or / are on an LVM partition
  #if [ -x /sbin/vgchange ]; then /sbin/vgchange -an > /dev/null; fi
}

swap_off () {
  log_info_msg "Deactivating all swap files/partitions..."
  swapoff -a
  ev_ret "$?"
}

bootlogd_stop () {
  test -f /run/bootlogd.pid || return 0
  echo "Stopping bootlogd ..."
  touch /var/log/boot.log
  kill $(< /run/bootlogd.pid)
  rm -f /run/bootlogd.pid
}

udev_stop () {
  udevadm control --exit
  udevadm info --cleanup-db
}

## we were just sourced to load function definitions
return 0

