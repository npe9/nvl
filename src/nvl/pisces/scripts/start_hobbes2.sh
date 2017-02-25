#!/bin/sh

cd /tmp/hobbes
#cp -R /home/ktpedre/hobbes_install_ktpedre ./hobbes_ktpedre
#cd ./hobbes_ktpedre

echo "Unloading petos, pisces, and xpmem modules"
/sbin/rmmod petos
/sbin/rmmod pisces
/sbin/rmmod xpmem

echo "Loading Hobbes PetOS Driver"
/sbin/insmod /tmp/hobbes_ktpedre/petos.ko
chmod 777 /dev/petos

echo "Loading Hobbes XEMEM Driver 'xpmem.ko ns=1 xpmem_follow_page=1'"
/sbin/insmod /tmp/hobbes_ktpedre/xpmem.ko ns=1 xpmem_follow_page=1

echo "Loading Hobbes Pisces Driver '/sbin/insmod pisces.ko'"
/sbin/insmod /tmp/hobbes_ktpedre/pisces.ko

echo "Launching Hobbes Master Daemon 'lnx_init --cpulist=0,31'"
export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0
#/tmp/hobbes_ktpedre/lnx_init --cpulist=0,8,9,10,31 ${@:1} &
# Don't use cores from socket 0, also core 31 from socket 1 can't be offlined
/tmp/hobbes_ktpedre/lnx_init --cpulist=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,31 ${@:1} &
echo $! > leviathan.pid

echo "Sleeping 10 seconds before restarting Cray ALPS Daemon"
sleep 10
echo "Restarting Cray ALPS Daemon"
killall apinit

echo "Sleeping 10 seconds before starting a Hobbes Pisces enclave"
sleep 10
echo "Starting a Hobbes Pisces enclave 'hobbes create_enclave kitten_enclave.xml'"
unset HOBBES_ENCLAVE_ID
unset HOBBES_APP_ID
cd /tmp/hobbes_ktpedre
./hobbes create_enclave kitten_enclave_ktpedre.xml

echo "Done."
