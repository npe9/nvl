#!/bin/sh

SRCDIR=./src/pisces
IMAGEDIR=$HOME/hobbes_install
if [[ ! -e $IMAGEDIR ]]; then
		mkdir -p $IMAGEDIR
fi

if [[ ! -d $IMAGEDIR ]]; then
		echo bad imagedir $IMAGEDIR 1>&2
		exit 1
fi

cp -R $SRCDIR/xpmem/mod/xpmem.ko $IMAGEDIR
cp -R $SRCDIR/pisces/pisces.ko $IMAGEDIR
cp -R $SRCDIR/petlib/hw_status $IMAGEDIR
cp -R $SRCDIR/hobbes/lnx_inittask/lnx_init $IMAGEDIR
cp -R $SRCDIR/hobbes/shell/hobbes $IMAGEDIR
cp -R $SRCDIR/kitten/vmlwk.bin $IMAGEDIR
cp -R $SRCDIR/hobbes/lwk_inittask/lwk_init $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/pisces_cons $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/v3_cons_sc $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/v3_cons_nosc $IMAGEDIR
#cp -R $SRCDIR/hobbes/examples/apps/pmi/test_pmi_hello $IMAGEDIR
cp -R src/hpcg/bin/xhpcg $IMAGEDIR
cp -R scripts/* $IMAGEDIR
#cp -R $SRCDIR/hobbes/gui_demo/hobbes-gui $IMAGEDIR

#tar -zcvf hobbes_install.tar.gz ./hobbes_install
