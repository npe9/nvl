#!/bin/sh

if [[ ! -e /tmp/hobbes ]]; then
		mkdir /tmp/hobbes
fi

cp -R /home/npe/hobbes_install/* /tmp/hobbes
