echo "Loading xpmem.ko ns=1"
if ! /sbin/rmmod xpmem.ko ; then
		echo couldn\'t remove xpmem module 1>&2
		exit 1
fi

if ! /sbin/insmod /tmp/hobbes/xpmem.ko ns=1 xpmem_follow_page=1 ; then
		echo couldn\'t add xpmem module 2>&1
		exit 1
fi

echo "Loading pisces.ko"
if ! /sbin/insmod /tmp/hobbes/pisces.ko ; then
		echo couldn\'t add pisces module 2>&1
		exit 1
fi

echo "Launching Hobbes Master Daemon"
export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0
/tmp/hobbes/lnx_init ${@:1} &
echo $! > leviathan.pid

echo "Done."
