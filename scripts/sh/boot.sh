#!/bin/sh
# -*- shell -*-

##
## shell helper function/subroutine definitions for system boot
##

net_start () {
  ## static IP configuration via iproute
  ip link set dev eth0 up
  ip addr add 192.168.1.2/24 brd + dev eth0
  ip route add default via 192.168.1.1
}

lvm_start () {
  dmsetup mknodes
  vgscan --ignorelockingfailure
  vgchange -ay --ignorelockingfailure
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

## set default host and domain names
set_host_name () {
  local name="panda"
  local dom="dark.net"
  local f=/proc/sys/kernel/hostname

  #echo "$HOSTNAME" >| /proc/sys/kernel/hostname
  ## directly set the host name without calling "hostname"
  if test -f "$f" -a -w "$f" ; then
    echo -e "[$PNAME] Setting host name to $name ...\t\c"
    echo -n "$name" > "$f" && echo "ok" || echo "ERROR"
  fi

  ## directly set the domain name with calling "domainname"
  f=/proc/sys/kernel/domainname

  if test -f "$f" -a -w "$f" ; then
    echo -e "[$PNAME] Setting domain name to $dom ...\t\c"
    echo -n "$dom" > "$f" && echo "ok" || echo "ERROR"
  fi
}

start_log () {
  echo "[$PANME] $*" >> "$BOOTLOG"
}

source_start_scripts () {
  if test -d /etc/runit/sh ; then
    for i in /etc/runit/sh/S?* ; do
      test -f "$i" -a -r "$i" -a -s "$i" && . "$i" start
    done
  fi
}

## bring up the loopback interface
lo_up () {
  :
}

## mount procfs on /proc
mount_procfs () {
  if mountpoint -q /proc ; then
    echo "/proc is already a mount point, remounting ..."
  else
    mkdir -pm 0755 /sys && mount -wt proc proc /proc
  fi
}

## mount sysfs on /sys
mount_sysfs () {
  if mountpoint -q /sys ; then
    echo "/sys is already a mount point"
  else
    mkdir -pm 0755 /sys && mount -wt sysfs sysfs /sys
sysfs_opts=nodev,noexec,nosuid
mount -n -t sysfs -o ${sysfs_opts} sysfs /sys
  fi
}

## mount tmpfs on /run so that runtime data are not written to disk
mount_run () {
  if mountpoint -q /run ; then
    echo "/run is already a mount point"
  else
    mkdir -pm 0755 /run && mount -wt tmpfs -o mode=0755 tmpfs /run
  fi
}

## setup kernel support for securityfs
mount_secfs () {
  local d=/sys/kernel/security

  test -d "$d" || return 0
  grep -qs securityfs /proc/filesystems || return 0

  if mountpoint -q "$d" ; then
    echo "$d is already a mount point"
  else
	ebegin "Mounting security filesystem on $d ... "
	mount -n -t securityfs -o ${sysfs_opts} \
		securityfs /sys/kernel/security
	eend $?
  fi
}

## set up kernel support for debugfs
mount_debugfs () {
  local d=/sys/kernel/debug

  test -d "$d" || return 0
  grep -qs securityfs /proc/filesystems || return 0

  if mountpoint -q "$d" ; then
    echo "$d is already a mount point"
  else
    echo "Mounting $d"
  fi
}

## mount misc aux fs under /sys
mount_sys_misc () {
  if [ -d /sys/kernel/debug ] && ! mountinfo -q /sys/kernel/debug ; then
	if grep -qs debugfs /proc/filesystems ; then
			ebegin "Mounting debug filesystem"
			mount -n -t debugfs -o ${sysfs_opts} debugfs /sys/kernel/debug
			eend $?
		fi
  fi

  ## set up kernel support for configfs
  if [ -d /sys/kernel/config ] && ! mountinfo -q /sys/kernel/config; then
		if grep -qs configfs /proc/filesystems; then
			ebegin "Mounting config filesystem"
			mount -n -t configfs -o  ${sysfs_opts} configfs /sys/kernel/config
			eend $?
		fi
  fi

  ## set up kernel support for fusectl
  if [ -d /sys/fs/fuse/connections ] \
		&& ! mountinfo -q /sys/fs/fuse/connections; then
		if grep -qs fusectl /proc/filesystems; then
			ebegin "Mounting fuse control filesystem"
			mount -n -t fusectl -o ${sysfs_opts} \
				fusectl /sys/fs/fuse/connections
			eend $?
		fi
  fi

  ## set up Kernel Support for SELinux
  if [ -d /sys/fs/selinux ] && ! mountinfo -q /sys/fs/selinux; then
		if grep -qs selinuxfs /proc/filesystems; then
			ebegin "Mounting SELinux filesystem"
			mount -t selinuxfs selinuxfs /sys/fs/selinux
			eend $?
		fi
  fi

  ## set up Kernel Support for persistent storage
  if [ -d /sys/fs/pstore ] && ! mountinfo -q /sys/fs/pstore; then
		if grep -qs 'pstore$' /proc/filesystems; then
			ebegin "Mounting persistent storage (pstore) filesystem"
			mount -t pstore pstore -o ${sysfs_opts} /sys/fs/pstore
			eend $?
		fi
  fi

  ## set up kernel support for efivarfs
  if [ -d /sys/firmware/efi/efivars ] &&
		! mountinfo -q /sys/firmware/efi/efivars; then
		ebegin "Mounting efivarfs filesystem"
		mount -n -t efivarfs -o ${sysfs_opts} \
			efivarfs /sys/firmware/efi/efivars 2> /dev/null
		eend 0
  fi
}

mount_binfmt () {
  local p
  echo "Mounting binfmt fs ..."
  mountpoint -q /proc/sys/fs/binfmt_misc ||
    mount -t binfmt_misc binfmt /proc/sys/fs/binfmt_misc || return 1

  for path in /usr/lib/binfmt.d /etc/binfmt.d /run/binfmt.d
  do
        test -d "$path" || continue
        [[ -z "$(ls $path)" ]] && continue
        grep "^:" $path/* | \
            while read -r line; do
                printf "%s" "$line" > /proc/sys/fs/binfmt_misc/register || return 1
            done
  done

  return 0
}

## mount cgroupfs
mount_cgroups () {
  local d=/sys/fs/cgroup

  test -d "$d" || return 0

  if mountpoint -q "$d" ; then
    echo "$d is already a mount point"
  else
    echo "Mounting cgroupfs on $d ..."
  fi
}

## mount devtmpfs on /dev
mount_dev () {
  :
}

seed_run () {
  local f

  mkdir -p -m 0755 /run/shm

  for f in /run/utmp ${BOOTLOG:=/run/boot.log}
  do
    : > "$f"
  done
}

seed_dev () {
  mkdir -pm 0755 /dev/shm /dev/mqueue
}

## modules-load.d(5) compatible kernel module loader
modLoad () {
  ## [-n] [-v]
  {
    ## parameters passed as modules-load= or rd.modules-load= in kernel command line
    sed -nr 's/,/\n/;s/(.* |^)(rd\.)?modules-load=([^ ]*).*/\3/p' /proc/cmdline

    ## find files /{etc,run,usr/lib}/modules-load.d/*.conf in that order
    find -L /etc/modules-load.d /run/modules-load.d /usr/lib/modules-load.d \
       	-maxdepth 1 -name '*.conf' -printf '%p %P\n' 2>/dev/null |
    ## load each basename only once
	sort -k2 -s | uniq -f1 | cut -d' ' -f1 |
    ## read the files, output all non-empty, non-comment lines
	tr '\012' '\0' | xargs -0 -r grep -h -v -e '^[#;]' -e '^$'
  } |
  ## Call modprobe on the list of modules
  tr '\012' '\0' | xargs -0 -r modprobe -ab "$@"
}

mount_pseudofs () {
  mount_procfs
  mount_sysfs
  mount_run
  mount_sys_misc
  seed_run
  mount_dev
  seed_dev
}

## create all kinds of files according to a given config file
create_files () {
   local conf=${1:-/etc/sysconfig/createfiles}

   test -f "$conf" -a -s "$conf" -a -r "$conf" || return 0
   local name type perm usr grp dtype maj min junk
   ## input to file descriptor 9 and output to stdin (redirection)
   exec 9>&0 < "$conf"

   while read name type perm usr grp dtype maj min junk
   do
      ## ignore comments and blank lines
      case "${name:-}" in
        "" | \#* ) continue ;;
      esac

      if test -e "${name:-}" ; then
        ## ignore existing files for now
        echo "$name already exists ..."
      else
        ## create stuff based on its type
        case "${type:-}" in
          dir )
            mkdir -p -m 0755 "${name}"
            ;;
          file )
            : > "${name}"
            ;;
          dev )
            case "${dtype:-}" in
              p* )
                mknod -m ${perm:-0600} "${name}" p
                ;;
              c* )
                mknod -m ${perm:-0600} "${name}" c ${maj:-} ${min:-}
                ;;
              b* )
                mknod -m ${perm:-0600} "${name}" b ${maj:-} ${min:-}
                ;;
              * )
                log_warning_msg "\nUnknown device type: ${dtype}"
                ;;
            esac
            ;;
          * )
            log_warning_msg "\nUnknown type: ${type}"
            continue
            ;;
        esac
      fi

      ## set up the permissions now, also for already existing files
      if test -e "${name:-}" ; then
        chown ${usr:-0}:${grp:-0} "${name}"
        chmod ${perm:-'a+rX,u+w,go-w'} "${name}"
      fi
   done

   ## close file descriptor 9 (end redirection)
   exec 0>&9 9>&-

   return 0
}

mount_virtfs () {
  local r=0

  if ! mountpoint -q /run ; then
    mount /run || r=1
  fi

  mkdir -p -m 0755 /run/lock /run/shm
  chmod 1777 /run/shm

  log_info_msg "Mounting virtual file systems: ${INFO}/run "

  if ! mountpoint -q /proc ; then
    log_info_msg2 " ${INFO}/proc"
    mount -o "nosuid,noexec,nodev" /proc || r=1
  fi

  if ! mountpoint -q /sys ; then
    log_info_msg2 " ${INFO}/sys"
    mount -o "nosuid,noexec,nodev" /sys || r=1
  fi

  if ! mountpoint -q /dev ; then
    log_info_msg2 " ${INFO}/dev"
    mount -o "mode=0755,nosuid" /dev || r=1
  fi

  ln -sfn /run/shm /dev/shm
  ev_ret "$r"
}

seed_fs () {
  : > /var/run/utmp
}

start_localnet () {
  test -f /etc/sysconfig/network && . /etc/sysconfig/network
  test -f /etc/hostname && HOSTNAME=`cat /etc/hostname`

  log_info_msg "Bringing up the loopback interface ... "
  ip addr add 127.0.0.1/8 label lo dev lo
  ip link set lo up
  ev_ret "$?"

  log_info_msg "Setting hostname to ${HOSTNAME} ..."
  hostname "${HOSTNAME:-darkstar}"
  ev_ret "$?"
}

load_modules () {
  ## assure that the kernel has module support
  test -f /proc/modules || return 0
  local FILE

  for FILE in /etc/sysconfig/modules /etc/sysconfig/modules.d/*.conf
  do
    ## continue with next if there's no modules file or there are no
    ## valid entries
    test -f "$FILE" -a -s "$FILE" -a -r "$FILE" || continue
    egrep -qv '^($|#)' $FILE || continue
    log_info_msg "Loading modules: "
    break
  done

  for FILE in /etc/sysconfig/modules /etc/sysconfig/modules.d/*?.conf
  do
    test -f "$FILE" -a -s "$FILE" -a -r "$FILE" || continue
    egrep -qv '^($|#)' "$FILE" || continue

    while read module args
    do
         ## ignore comments and blank lines
         case "$module" in
          "" | "#"* ) continue ;;
         esac

         ## attempt to load the module, passing any arguments provided
         modprobe ${module} ${args} > /dev/null

         # Print the module name if successful, otherwise take note.
         if test 0 -eq "$?" ; then
           log_info_msg2 " ${module}"
         else
          failedmod="${failedmod} ${module}"
         fi
     done < "$FILE"
  done

  ## print a message about successfully loaded modules on the correct line.
  log_success_msg2
  ## print a failure message with a list of any modules that
  ## may have failed to load.
  if test -n "${failedmod:-}" ; then
     log_failure_msg "Failed to load modules:${failedmod}"
  fi

  return 0
}

## load required Linux kernel modules
more_modules () {
  set -- $( uname -a )
  local f v="${3:-x}" s="${MODS:-/etc/rc.d/rc.modules}"

  case "${v%%.?*}" in
    2 ) ;;
    3 ) ;;
    4 ) ;;
  esac

  ## match the kernel version by release
  case "${v%.?*}" in
    2.2 ) ;;
    2.3 ) ;;
    2.4 ) ;;
    2.5 ) ;;
    2.6 ) ;;
    3.0 ) ;;
    3.1 ) ;;
    3.2 ) ;;
    3.3 ) ;;
    3.4 ) ;;
    3.5 ) ;;
    3.6 ) ;;
    3.7 ) ;;
    3.8 ) ;;
    3.9 ) ;;
    3.10 ) ;;
    3.11 ) ;;
    3.12 ) ;;
    3.13 ) ;;
    3.14 ) ;;
    3.15 ) ;;
    3.16 ) ;;
    3.17 ) ;;
    3.18 ) ;;
    3.19 ) ;;
    4.0 ) ;;
    4.1 ) ;;
    4.2 ) ;;
    4.3 ) ;;
    4.4 ) ;;
    4.5 ) ;;
    4.6 ) ;;
    4.7 ) ;;
    4.8 ) ;;
    4.9 ) ;;
    4.10 ) ;;
    4.11 ) ;;
    4.12 ) ;;
    4.13 ) ;;
    4.14 ) ;;
    4.15 ) ;;
    4.16 ) ;;
    4.17 ) ;;
    4.18 ) ;;
    4.19 ) ;;
    4.20 ) ;;
  esac

  ## match the kernel version exactly
  case "${v:-}" in
    2.6.39 ) ;;
    3.19.8 ) ;;
    4.1.12 ) ;;
    4.1.52 ) ;;
    4.3.6 ) ;;
    4.4.166 ) ;;
    4.9.144 ) ;;
    4.14.87 ) ;;
    4.19.8 ) ;;
  esac

  ## see if module load scripts exist and run them
  for f in "$s" "${s}-${v%.?*}" "${s}-${v}" ; do
    if test -f "$f" -a -s "$f" -a -r "$f" ; then
      #echo "Loading the kernel modules listed in $f ... "
      echo "Running kernel module load script $f ... "
      #$sh "$f" &
      #( . "$f" ) &
      . "$f"
      #break
    fi
  done

  return 0
}

restore_clock () {
  ## read the clock config from first arg or try to use default config file
  local f=${1:-/etc/sysconfig/clock}
  test -f "$f" -a -s "$f" -a -r "$f" && . "$f"

  case "${UTC:-}" in
    yes | true | 1 )
      CLOCKMODE=utc
      CLOCKPARAMS="${CLOCKPARAMS} --utc"
      ;;
    no | false | 0 )
      CLOCKMODE=localtime
      CLOCKPARAMS="${CLOCKPARAMS} --localtime"
      ;;
  esac

  hwclock --hctosys ${CLOCKPARAMS} > /dev/null
  log_info_msg "Setting hardware clock to ${CLOCKMODE} ... "
  ev_ret "$?"
}

check_fs () {
  local m

  if test -f /fastboot ; then
     m="/fastboot found, will omit "
     m="${msg} file system checks as requested.\n"
     log_info_msg "$m"
     return 0
  fi

  log_info_msg "Mounting root file system in read-only mode ... "
  mount -n -o remount,ro / > /dev/null

  if test "${?}" -eq 0 ; then
     log_success_msg2
  else
     log_failure_msg2
     msg="\n\nCannot check root "
     msg="${msg}filesystem because it could not be mounted "
     msg="${msg}in read-only mode.\n\n"
     msg="${msg}After you press Enter, this system will be "
     msg="${msg}halted and powered off.\n\n"
     log_failure_msg "${msg}"

     log_info_msg "Press Enter to continue ... "
     wait_for_user
     #/etc/rc.d/init.d/halt stop
  fi

  if [ -f /forcefsck ]; then
     msg="\n/forcefsck found, forcing file"
     msg="${msg} system checks as requested."
     log_success_msg "$msg"
     options="-f"
  else
     options=
  fi

  log_info_msg "Checking file systems ... "

  ## note: -a option used to be -p; but this fails e.g. on fsck.minix
  if is_true "$VERBOSE_FSCK" ; then
     fsck ${options} -a -A -C -T
  else
     fsck ${options} -a -A -C -T > /dev/null
  fi

  error_value="$?"

  if test "${error_value}" -eq 0 ; then
     log_success_msg2
  fi

  if test "${error_value}" -eq 1 ; then
     msg="\nWARNING:\n\nFile system errors "
     msg="${msg}were found and have been corrected.\n"
     msg="${msg}You may want to double-check that "
     msg="${msg}everything was fixed properly."
     log_warning_msg "$msg"
  fi

  if test "${error_value}" -eq 2 -o "${error_value}" -eq 3 ; then
     msg="\nWARNING:\n\nFile system errors "
     msg="${msg}were found and have been been "
     msg="${msg}corrected, but the nature of the "
     msg="${msg}errors require this system to be rebooted.\n\n"
     msg="${msg}After you press enter, "
     msg="${msg}this system will be rebooted\n\n"
     log_failure_msg "$msg"

     log_info_msg "Press Enter to continue ... "
     wait_for_user
     reboot -f
  fi

  if test "${error_value}" -gt 3 -a "${error_value}" -lt 16 ; then
     msg="\nFAILURE:\n\nFile system errors "
     msg="${msg}were encountered that could not be "
     msg="${msg}fixed automatically.  This system "
     msg="${msg}cannot continue to boot and will "
     msg="${msg}therefore be halted until those "
     msg="${msg}errors are fixed manually by a "
     msg="${msg}System Administrator.\n\n"
     msg="${msg}After you press Enter, this system will be "
     msg="${msg}halted and powered off.\n\n"
     log_failure_msg "$msg"

     log_info_msg "Press Enter to continue ... "
     wait_for_user
     #/etc/rc.d/init.d/halt stop
  fi

  if test "${error_value}" -ge 16 ; then
     msg="\nFAILURE:\n\nUnexpected Failure "
     msg="${msg}running fsck.  Exited with error "
     msg="${msg} code: ${error_value}."
     log_failure_msg "$msg"
     return ${error_value}
  fi

  return 0
}

mount_fs () {
  log_info_msg "Remounting root file system in read-write mode ... "
  mount -o remount,rw / > /dev/null
  ev_ret "$?"
  ## remove fsck-related file system watermarks.
  rm -f /fastboot /forcefsck
  ## make sure /dev/pts exists
  mkdir -p -m 755 /dev/pts
  ## this will mount all filesystems that do not have _netdev in
  ## their option list.  _netdev denotes a network filesystem.
  log_info_msg "Mounting remaining file systems ... "
  mount -a -O no_netdev > /dev/null
  ev_ret "$?"
  return "$failed"
}

clean_fs () {
  log_info_msg "Cleaning file systems: "

  if test "${SKIPTMPCLEAN:-}" = "" ; then
    log_info_msg2 " /tmp"
    chmod 1777 /tmp
    cd /tmp &&
    find . -xdev -mindepth 1 ! -name lost+found -delete || failed=1
  fi

  : > /var/run/utmp

  if grep -q '^utmp:' /etc/group ; then
    chmod 664 /var/run/utmp
    chgrp utmp /var/run/utmp
  fi

  ev_ret "$failed"

  if egrep -qv '^(#|$)' /etc/sysconfig/createfiles 2>/dev/null; then
    log_info_msg "Creating files and directories... "
    create_files      # Always returns 0
    ev_ret "$?"
  fi

  return $failed
}

swap_on () {
  log_info_msg "Activating all swap files/partitions..."
  swapon -a
  ev_ret "$?"
}

run_sysctl () {
  if test -f /etc/sysctl.conf ; then
    log_info_msg "Setting kernel runtime parameters ..."
    sysctl -q -p
    ev_ret "$?"
  fi
}

fix_mtab () {
  local f=/proc/self/mounts

  if test -f "$f" ; then
    echo -e "Fixing mtab file:\tln -sf $f /etc/mtab ...\t\c"
    ln -sf "$f" /etc/mtab
    ev_ret "$?"
  fi
}

## set up the console vts
setup_console () {
  local r=0

  test -f /etc/sysconfig/console && . /etc/sysconfig/console

  ## see if we need to do anything
  if test -z "${KEYMAP:-}" -a -z "${KEYMAP_CORRECTIONS:-}" -a \
    -z "${FONT:-}" -a -z "${LEGACY_CHARSET:-}" && ! is_true "${UNICODE:-}"
  then
    return 0
  fi

  ## there should be no bogus failures below this line!
  log_info_msg "Setting up Linux console ..."

  ## figure out if a framebuffer console is used
  test -d /sys/class/graphics/fb0 && use_fb=1 || use_fb=0

  ## figure out the command to set the console into the desired mode
  is_true "${UNICODE:-}" &&
  MODE_COMMAND="echo -en '\033%G' && kbd_mode -u" ||
  MODE_COMMAND="echo -en '\033%@\033(K' && kbd_mode -a"

  ## on framebuffer consoles, font has to be set for each vt in
  ## UTF-8 mode. This doesn't hurt in non-UTF-8 mode also.
  ! is_true "${use_fb}" || test -z "${FONT:-}" ||
    MODE_COMMAND="${MODE_COMMAND} && setfont ${FONT}"

  ## apply that command to all consoles mentioned in
  ## /etc/inittab. Important: in the UTF-8 mode this should
  ## happen before setfont, otherwise a kernel bug will
  ## show up and the unicode map of the font will not be used.
  for TTY in `grep '^[^#].*respawn:/sbin/agetty' /etc/inittab |
    grep -o '\btty[[:digit:]]*\b'`
  do
    openvt -f -w -c ${TTY#tty} -- sh -c "${MODE_COMMAND}" || r=1
  done

  ## set the font (if not already set above) and the keymap
  if test 1 -eq "${use_fb:-}" -a -n "${FONT:-}" ; then
    setfont "$FONT" || r=1
  fi

  if test -n "${KEYMAP:-}" ; then
    loadkeys "${KEYMAP}" > /dev/null 2>&1 || r=1
  fi

  if test -n "${KEYMAP_CORRECTIONS:-}" ; then
    loadkeys ${KEYMAP_CORRECTIONS} > /dev/null 2>&1 || r=1
  fi

  ## convert the keymap from $LEGACY_CHARSET to UTF-8
  if test -n "${LEGACY_CHARSET:-}" ; then
    dumpkeys -c "$LEGACY_CHARSET" | loadkeys -u > /dev/null 2>&1 || r=1
  fi

  ## if any of the commands above failed, the trap at the
  ## top would set $r to 1 (non zero)
  ev_ret "$r"
  return "$r"
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

udev_start () {
  log_info_msg "Populating /dev with device nodes... "

  if ! grep -q '[[:space:]]sysfs' /proc/mounts
  then
     log_failure_msg2
     msg="FAILURE:\n\nUnable to create "
     msg="${msg}devices without a SysFS filesystem\n\n"
     msg="${msg}After you press Enter, this system "
     msg="${msg}will be halted and powered off.\n\n"
     log_info_msg "$msg"
     log_info_msg "Press Enter to continue..."
     wait_for_user
     /etc/rc.d/init.d/halt stop
  fi

  ## udev handles uevents itself, so we don't need to have
  ## the kernel call out to any binary in response to them
  test -f /proc/sys/kernel/hotplug && echo > /proc/sys/kernel/hotplug
  ## Start the udev daemon to continually watch for, and act on, uevents
  udevd --daemon --resolve-names=never
  ## now traverse /sys in order to "coldplug" devices that have
  ## already been discovered
  udevadm trigger --action=add    --type=subsystems
  udevadm trigger --action=add    --type=devices
  udevadm trigger --action=change --type=devices

  ## now wait for udevd to process the uevents we triggered
  if ! is_true "$OMIT_UDEV_SETTLE" ; then
     udevadm settle
  fi

  ## if any LVM based partitions are on the system, ensure they
  ## are activated so they can be used.
  test -x /sbin/vgchange && /sbin/vgchange -a y > /dev/null
  log_success_msg2
}

udev_retry () {
  log_info_msg "Retrying failed uevents, if any ... "

  ## As of udev-186, the --run option is no longer valid
  #rundir=$(/sbin/udevadm info --run)
  rundir=/run/udev
  ## from Debian: "copy the rules generated before / was mounted read-write":
  for file in ${rundir}/tmp-rules--*; do
    dest=${file##*tmp-rules--}
    test "$dest" = '*' && break
    cat "$file" >> /etc/udev/rules.d/$dest
    rm -f "$file"
  done

  ## re-trigger the uevents that may have failed,
  ## in hope they will succeed now
  sed -e 's/#.*$//' /etc/sysconfig/udev_retry | /bin/grep -v '^$' | \
  while read line ; do
    for subsystem in $line ; do
      udevadm trigger --subsystem-match=$subsystem --action=add
    done
  done

  ## now wait for udevd to process the uevents we triggered
  if ! is_true "$OMIT_UDEV_RETRY_SETTLE" ; then
    udevadm settle
  fi

  ev_ret "$?"
}

