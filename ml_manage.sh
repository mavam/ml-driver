#!/bin/sh
module="ml_driver"
devname="usb_device"
device="ml"
mode=664
group="plugdev"

replace_nodes() 
{
	# Remove stale nodes
	rm -f /dev/${device}[0-3]

	major=$(awk "\$2==\"${devname}\" {print \$1}" /proc/devices)
	echo $major

	for i in 0 1 2 3; do
		mknod -m ${mode} "/dev/${device}${i}" c ${major} ${i}
		echo mknod -m ${mode} "/dev/${device}${i}" c ${major} ${i}

	done

	chgrp ${group} /dev/${device}[0-3]
}

load()
{	
	shift

	rmmod usbhid

	insmod ./${module}.ko $* || exit 1

	# The following screws up the properly created /dev/ml0 device on my system!
	# replace_nodes	
}

unload()
{
	rmmod ${module} || exit 1
	rm -f /dev/${device}[0-3]
}

case $1 in
	load)
	load $*
	;;
	unload)
	unload
	;;
	reload)
	unload
	load $*
	;;
	*)
	echo "usage: $0 <load|unload|reload>"
	;;
esac
