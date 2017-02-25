#!/bin/sh

if [[ $1 = "-s" ]]; then
		for i in c0-0c0s2n2 c0-0c0s2n3; do  
				ssh root@$i mkdir /tmp/hobbes
    				ssh root@$i /home/npe/nvl.me/src/nvl/pisces/scripts/config_node.sh
				#ssh root@$i cp "/home/ktpedre/hobbes_install_ktpedre/*" /tmp/hobbes
				#ssh root@$i cp "/home/ktpedre/hobbes_install/*" /tmp/hobbes
				#ssh root@$i cp "/home/npe/hobbes_install/*" /tmp/hobbes
				#ssh root@$i cp "/home/ktpedre/hobbes_install/*.xml" /tmp/hobbes
				#ssh root@$i cp "/home/npe/hobbes_install/*init" /tmp/hobbes
				ssh root@$i cp "/home/npe/hobbes_install/start_hobbes*" /tmp/hobbes
				#ssh root@$i cp "/home/npe/nvl.me/src/nvl/pisces/src/pisces/hobbes/lwk_inittask/lwk_init" /tmp/hobbes
				#ssh root@$i cp "/home/ktpedre/hobbes_install_ktpedre/*ko" /tmp/hobbes
				#ssh root@$i cp "/home/npe/hobbes_install/*ko" /tmp/hobbes
				ssh root@$i cp "/home/npe/nvl.me/src/nvl/pisces/src/hpcg/bin/xhpcg" /tmp/hobbes
				#ssh root@$i cp "/home/npe/nvl.me/src/nvl/pisces/src/pisces/hobbes/lwk_inittask/lwk_init" /tmp/hobbes
				ssh root@$i cp "/home/npe/nvl.me/src/nvl/pisces/cray/static-link/xpmem_chnl_serv" /tmp/hobbes
				ssh root@$i cp "/home/npe/nvl.me/src/nvl/pisces/src/pisces/kitten/vmlwk.bin" /tmp/hobbes
				ssh root@$i /tmp/hobbes/start_hobbes.sh
				#ssh root@$i /tmp/hobbes/start_hobbes2.sh
		done
fi

#aprun -N 1 -n 1 -L 26,27 ./hobbes launch_app enclave-1 -with-hio=/tmp/hobbes/stub  

aprun -n 2 -N 1 -L 26,27 ~/nvl.me/src/nvl/pisces/cray/static-link/xpmem_chnl_serv &
sleep 5
for i in c0-0c0s2n2 c0-0c0s2n3; do
		#./hobbes launch_app enclave-1 -with-hio=/tmp/hobbes/stub --heap 600 --stack 256 /tmp/hobbes/xhpcg &
		#./hobbes launch_app enclave-1 -with-hio=/tmp/hobbes/stub --heap 600 --stack 256 /tmp/hobbes_ktpedre/io-daemon-raw /tmp/hobbes_ktpedre/test_file.txt &
		ssh root@$i	/tmp/hobbes/hobbes launch_app enclave-1 --heap 600 --stack 256 /tmp/hobbes/xhpcg &
done
#sleep 10
if [[ $2 = "-c" ]]; then
		for i in c0-0c0s2n2; do
				echo CONSOLE: $i
sleep 1
				ssh root@$i /tmp/hobbes/hobbes console enclave-1
				ssh root@$i /tmp/hobbes/hobbes console enclave-1
				ssh root@$i /tmp/hobbes/hobbes console enclave-1
				echo END CONSOLE: $i
		done
fi
