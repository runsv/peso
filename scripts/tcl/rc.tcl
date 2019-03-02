#!/bin/runtcl
# -*- tcl -*-

##
## boot and halt the system
##

# TclX may be useful but is not required to run this script so far
#package require Tclx

##
## config
##

# host name to use
#set host_name "darkstar"
set host_name "panda"

# search path for external commands
set path "/bin:/sbin:/usr/bin:/usr/sbin"

# use busybox ?
set use_busybox 0

# use start-stop-daemon to start daemons ?
set use_ssd 0

# path to the busybox binary
set bbox "/bin/busybox"
set mdev "$bbox mdev"

# path to the start-stop-daemon binary
#set ssd "$busybox start-stop-daemon"
set ssd "/sbin/start-stop-daemon"

set modules {
  mod
  mod2
  mod3
  mod4
  mod5
}

set modargs {
  {modA arg arg2 arg3}
  {modB arg arg2 arg3}
  {modC arg arg2 arg3}
}

#array set modhash {
#  "modA" "arg arg2 arg3"
#  "modB" "arg arg2 arg3"
#}

proc want_modules {os ver arch} {
  if { "Linux" eq $os } then {
    set ver [split $ver .]
    set v [lindex $ver 0]
    set m [lindex $ver 1]
    set p [lindex $ver 2]
  } elseif { "FreeBSD" eq $os } then {
  }
}

##
## other global variables
##

set pname $argv0
set fs [list]
set pseudofs [list]

##
## helper functions
##

# try to catch all errors that can occur when running "cmd"
proc try_catch {cmd} {
  set res 0
  global errorInfo

  if { [catch { uplevel 1 $cmd } res] } {
    puts stderr "\nError in \"$cmd\" :\t$res\n$errorInfo\n"
    return
  }

  return $res
}

proc run_ext {} {
  if { [catch {pesi fork} pid] } {
    puts "can not fork"
  } else {
    pesi waitpid $pid
  }
}

proc prot_run {cmd} {
  puts -nonewline "running $cmd ...\t"

  if { [catch {uplevel 1 $cmd} result] } {
    puts "FAILED"
  } else {
    puts "ok"
  }
}

proc do_run {cmd} {
  if { [catch { uplevel 1 $cmd } result] } {
  } else {
  }
}

proc fcomp {what, path, path2} {
}

proc ftest {what, path} {
  # use try
  file lstat $path st
}

# checks if a given directory is a mount point
proc is_mount_point {dir} {
  if { [file isdirectory $dir] } {
    file lstat $dir st

    if { "directory" eq $st(type) } then {
      if { 0 < $st(ino) && 4 > $st(ino) } {
        return 1
      } elseif { 3 < $st(ino) } then {
        file lstat "${dir}/.." st2

        if { $st2(dev) != $st(dev) } then {
          return 1
        } elseif { $st2(ino) == $st(ino) } then {
          return 1
        }
      }
    }
  }

  return 0
}

# needs the pseudofs mounted
# better use a shell script wrapper /sbin/hotplug around busybox mdev
# instead of mdev proper
proc mdev_start {} {
  set i

  ebegin "Starting mdev ($mdev)"
  # Create a file so that our rc system knows it's still in sysinit.
  # Existance means init scripts will not directly run.
  # rc will remove the file when done with sysinit.
  #: > "/dev/.rcsysinit"
  # Avoid race conditions, serialize hotplug events.
  #echo > "/dev/mdev.seq"

  if test -f /proc/sys/kernel/hotplug ; then
    ebegin "Setting up /sbin/mdev as hotplug agent"
    echo /sbin/mdev > /proc/sys/kernel/hotplug
    eend "$?"
  fi

  einfo "Loading kernel modules for detected hardware ..."
  env -i /sbin/mdev -s
  # mdev -s does not poke network interfaces or usb devices so we need to do it here.
  for i in /sys/class/net/?*/uevent ; do
    printf 'add' > "$i"
  done 2> /dev/null

  for i in /sys/bus/usb/devices/?* ; do
    case "${i##*/}" in
      [0-9]*-[0-9]* )
        printf 'add' > "$i/uevent"
        ;;
    esac
  done

  # Load kernel modules, run twice.
  #find /sys -name 'modalias' -type f -exec cat '{}' + | sort -u | xargs modprobe -b -a 2> /dev/null
  #find /sys -name 'modalias' -type f -exec cat '{}' + | sort -u | xargs modprobe -b -a 2> /dev/null
}

mdev_stop {} {
  set h "/proc/sys/kernel/hotplug"

  if {[file isfile $h]} then {
    # just empty the file
    set f [open $h w]
    close $f
    puts "Disabled mdev hotplug agent"
  }
}

proc get_fs {} {
}

proc mount_procfs {} {
  if { ! [file exists "/proc"] } { catch {file mkdir "/proc"} }

  if { [is_mount_point "/proc"] } then {
    puts "/proc is already mounted"
  } elseif { [file isdirectory "/proc"] } {
    puts "Mounting procfs on /proc ..."
  }
}

proc mount_run {} {
}

proc mount_sysfs {} {
}

proc mount_dev {} {
  ebegin "Mounting devtmpfs on /dev"

  ## First umount /dev and friends if it is already mounted.
  ## It may be leftover from initramfs mount --move, we don't
  ## want to use it.
  umount /dev/pts /dev/shm /dev/mqueue /dev > /dev/null 2>&1

  if fstabinfo --quiet /dev ; then
    mount -n /dev
  else
    mount -n -t tmpfs -o "exec,nosuid,mode=0755,size=10M" mdev-tmpfs /dev
  fi

  mkdir -p -m 0755 /dev/pts /dev/shm /dev/mqueue /dev/mapper

  if ! fstabinfo --mount /dev/pts ; then
    ebegin "Mounting /dev/pts"
    mount -n -t devpts -o noexec,nosuid,gid=5,mode=0620 devpts /dev/pts
    eend "$?"
  fi

  if ! fstabinfo --mount /dev/shm ; then
    ebegin "Mounting /dev/shm"
    mount -n -t tmpfs -o noexec,nosuid,nodev,mode=1777 shm-tmpfs /dev/shm
    eend "$?"
  fi

  if ! fstabinfo --mount /dev/mqueue && grep -q mqueue /proc/filesystems
  then
    ebegin "Mounting /dev/mqueue"
    mount -n -t mqueue -o noexec,nosuid,nodev,mode=1777 mqueue /dev/mqueue
    eend "$?"
  fi
}

proc seed_run {} {
}

proc seed_dev {} {
  # some basic nodes
  if { [file exists "/proc/kcore"] } then {}
  test ! -e "/dev/console" && mknod "/dev/console" c 5 1
  test ! -e "/dev/null" && mknod "/dev/null" c 1 3
  test ! -e "/dev/tty" && mknod "/dev/tty" c 5 0
  test ! -e "/dev/tty1" && mknod "/dev/tty1" c 4 1
  test ! -e "/dev/urandom" && mknod "/dev/urandom" c 1 9
  test ! -e "/dev/random" && mknod "/dev/random" c 1 8
  test ! -e "/dev/zero" && mknod "/dev/zero" c 1 5

  ln -snf "/proc/self/fd" "/dev/fd"
  ln -snf "fd/0" "/dev/stdin"
  ln -snf "fd/1" "/dev/stdout"
  ln -snf "fd/2" "/dev/stderr"

  if { [file exists "/proc/kcore"] } then { symlink "/proc/kcore" "/dev/core" }
}

proc mount_dev_misc {} {
}

proc mount_cgroups {} {
}

proc mount_pseudofs {os} {
  if { "Linux" eq $os } then {
    mount_procfs
    mount_run
    mount_sysfs
    mount_dev
  } elseif { "DragonFly" eq $os } then {
  } elseif { "FreeBSD" eq $os } then {
  } elseif { "NetBSD" eq $os } then {
  } elseif { "OpenBSD" eq $os } then {
  }
}

proc load_modules {os ver arch} {
  if { "Linux" eq $os } then {
    want_modules $os $ver $arch
  } elseif { "FreeBSD" eq $os } then {
  }
}

proc swap_files {on} {
  if { $on } then {
  } else {
    puts "Deactivating additional swap space"
    if { [file exists "/proc/swaps"] } then {
    }
  }
}

proc sys_boot {} {
  pesi hostname $host_name
  mount_pseudofs $os
}

proc sys_halt {how} {
}

proc setup_env {} {
  #sys umask 0077
}

set dirs { "/" "/proc" "/sys" "/dev" "/dev/pts" "/dev/shm" "/run" "/sys/fs"
  "/sys/fs/cgroup" "/bin" "/bin/.." "/etc" "/usr" "/usr/local"
  "/var" "/tmp"
}

foreach x $dirs {
  file lstat $x st
  puts "${x}\t$st(ino)\t$st(dev)"

  if { [is_mount_point $x] } then {
    puts "$x is MP\n"
  } else {
    puts "$x is NO MP\n"
  }
}

#file stat "hohohohohoh" st
#file isdirectory "hohohohohoh"

proc main {what} {
  setup_env

  if { "boot" eq $what } then {
    puts "\nStarting the init process ...\n"
  } else {
    puts "haha"
  }
}

##
## main
##

if { 0 < $argc } then {
} else {
  main boot
}

puts "\n\n$pname: All done\n\n"
exit 0

