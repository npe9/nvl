#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use File::Copy;
use File::Find;

my $BASEDIR    = `pwd`; chomp($BASEDIR);
my $SRCDIR     = "src";
my $CONFIGDIR  = "config";
my $OVERLAYDIR = "overlays";
my $IMAGEDIR   = "image";

use vars qw/*name *dir *prune/;
*name   = *File::Find::name;
*dir    = *File::Find::dir;
*prune  = *File::Find::prune;

my $ISOLINUX = "";
my $LDLINUX = "";
sub wanted {
	if(!-l && $name =~ "/isolinux.bin" && $ISOLINUX eq ""){
		print $name."\n";
		$ISOLINUX = $name;
		}
	if(!-l && ($name =~ "/ldlinux.c32" || $name =~ "/linux.c32") && $LDLINUX eq ""){
		print $name."\n";
		$LDLINUX = $name;
		}
}
find({wanted => \&wanted}, '/usr/');
$ISOLINUX ne "" || die "couldn't find isolinux.bin";
$LDLINUX ne "" || die "couldn't find ldlinux.c32";

if (! -d $SRCDIR)     { mkdir $SRCDIR; }
if (! -d $IMAGEDIR)   { mkdir $IMAGEDIR; }

# List of packages to build and include in image
my @packages;

my %libhugetlbfs;
$libhugetlbfs{package_type} = "git";
$libhugetlbfs{basename}	= "libhugetlbfs";
$libhugetlbfs{clone_cmd}[0] = "git clone https://github.com/npe9/libhugetlbfs.git";
push(@packages, \%libhugetlbfs);

my %numactl;
$numactl{package_type}  = "tarball";
$numactl{version}	= "2.0.9";
$numactl{basename}	= "numactl-$numactl{version}";
$numactl{tarball}	= "$numactl{basename}.tar.gz";
$numactl{url}		= "ftp://oss.sgi.com/www/projects/libnuma/download/$numactl{tarball}";
push(@packages, \%numactl);

my %hwloc;
$hwloc{package_type}    = "tarball";
$hwloc{version}		= "1.8";
$hwloc{basename}	= "hwloc-$hwloc{version}";
$hwloc{tarball}		= "$hwloc{basename}.tar.gz";
$hwloc{url}		= "http://www.open-mpi.org/software/hwloc/v$hwloc{version}/downloads/$hwloc{tarball}";
push(@packages, \%hwloc);

my %ofed;
$ofed{package_type}	= "tarball";
$ofed{version}		= "3.12-1";
$ofed{basename}		= "OFED-$ofed{version}-rc2";
$ofed{tarball}		= "$ofed{basename}.tgz";
$ofed{url}		= "http://downloads.openfabrics.org/downloads/OFED/ofed-$ofed{version}/$ofed{tarball}";
push(@packages, \%ofed);

my %ompi;
$ompi{package_type}	= "git";
$ompi{basename}		= "ompi";
$ompi{clone_cmd}[0]	= "git clone https://github.com/npe9/ompi";
push(@packages, \%ompi);

my %pisces;
$pisces{package_type}	= "git";
$pisces{basename}	= "pisces";
$pisces{src_subdir}	= "pisces";
$pisces{clone_cmd}[0]	= "test -d petlib || git clone http://essex.cs.pitt.edu/git/petlib.git";
$pisces{clone_cmd}[1]	= "test -d xpmem || git clone http://essex.cs.pitt.edu/git/xpmem.git";
$pisces{clone_cmd}[2]	= "test -d kitten || git clone https://github.com/hobbesosr/kitten";
$pisces{clone_cmd}[3]	= "test -d palacios || git clone http://essex.cs.pitt.edu/git/palacios.git";
$pisces{clone_cmd}[4]	= "test -d hobbes || git clone http://essex.cs.pitt.edu/git/hobbes.git";
$pisces{clone_cmd}[5]	= "test -d pisces || git clone http://essex.cs.pitt.edu/git/pisces.git";
push(@packages, \%pisces);

my %curl;
$curl{package_type}	= "tarball";
$curl{version}		= "7.47.1";
$curl{basename}		= "curl-$curl{version}";
$curl{tarball}		= "$curl{basename}.tar.bz2";
$curl{url}		= "https://curl.haxx.se/download/$curl{tarball}";
push(@packages, \%curl);

my %hdf5;
$hdf5{package_type}	= "tarball";
$hdf5{version}		= "1.8.16";
$hdf5{basename}		= "hdf5-$hdf5{version}";
$hdf5{tarball}		= "$hdf5{basename}.tar.bz2";
$hdf5{url}		= "https://www.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8.16/src/$hdf5{tarball}";
push(@packages, \%hdf5);

my %netcdf;
$netcdf{package_type}	= "tarball";
$netcdf{version}	= "4.4.0";
$netcdf{basename}	= "netcdf-$netcdf{version}";
$netcdf{tarball}	= "$netcdf{basename}.tar.gz";
$netcdf{url}		= "ftp://ftp.unidata.ucar.edu/pub/netcdf/$netcdf{tarball}";
push(@packages, \%netcdf);

my %dtk;
$dtk{package_type}	= "git";
$dtk{basename}		= "Trilinos";
$dtk{src_subdir}	= "dtk";
$dtk{clone_cmd}[0]	= "git clone https://github.com/trilinos/Trilinos.git";
$dtk{clone_cmd}[1]	= "git clone https://github.com/ORNL-CEES/DataTransferKit.git";
$dtk{clone_cmd}[2]	= "git clone https://github.com/ORNL-CEES/DTKData.git";
push(@packages, \%dtk);

my %hpl;
$hpl{package_type}	= "git";
$hpl{basename}		= "hpl";
$hpl{clone_cmd}[0]	= "git clone https://github.com/npe9/hpl.git";
$hpl{clone_cmd}[1]	= "cd $hpl{basename} && git checkout kitten";
$hpl{clone_cmd}[2]  = "sed -i 's%\\(TOPdir.*\=\\).*\$%\\1 $BASEDIR/$SRCDIR/$hpl{basename}%'  $hpl{basename}/Make.Kitten";
push(@packages, \%hpl);

my %mpi_tutorial;
$mpi_tutorial{package_type}	= "git";
$mpi_tutorial{basename}		= "mpitutorial";
$mpi_tutorial{clone_cmd}[0]	= "git clone https://github.com/npe9/mpitutorial.git";
push(@packages, \%mpi_tutorial);

my %hpcg;
$hpcg{package_type}	= "git";
$hpcg{basename}		= "hpcg";
$hpcg{clone_cmd}[0]	= "git clone https://github.com/hpcg-benchmark/hpcg";
push(@packages, \%hpcg);

my %program_args = (
	build_kernel		=> 0,
	build_busybox		=> 0,
	build_dropbear		=> 0,
	build_libhugetlbfs	=> 0,
	build_numactl		=> 0,
	build_hwloc		=> 0,
	build_ofed		=> 0,
	build_ompi		=> 0,
	build_pisces		=> 0,
	build_curl		=> 0,
	build_hdf5		=> 0,
	build_netcdf		=> 0,
	build_dtk		=> 0,
	build_hpl		=> 0,
	build_mpi_tutorial		=> 0,
	build_hpcg		=> 0,

	build_image		=> 0,
	build_isoimage		=> 0,
	build_nvl_guest		=> 0
);

if ($#ARGV == -1) {
	usage();
	exit(1);
}

GetOptions(
	"help"			=> \&usage,
	"clean" => sub { $program_args{'clean'} = 1; },
	"force-configure" => sub { $program_args{'force_configure'} = 1; },
	"build-kernel"		=> sub { $program_args{'build_kernel'} = 1; },
	"build-busybox"		=> sub { $program_args{'build_busybox'} = 1; },
	"build-dropbear"	=> sub { $program_args{'build_dropbear'} = 1; },
	"build-libhugetlbfs"	=> sub { $program_args{'build_libhugetlbfs'} = 1; },
	"build-numactl"		=> sub { $program_args{'build_numactl'} = 1; },
	"build-hwloc"		=> sub { $program_args{'build_hwloc'} = 1; },
	"build-ofed"		=> sub { $program_args{'build_ofed'} = 1; },
	"build-ompi"		=> sub { $program_args{'build_ompi'} = 1; },
	"build-pisces"		=> sub { $program_args{'build_pisces'} = 1; },
	"build-curl"		=> sub { $program_args{'build_curl'} = 1; },
	"build-hdf5"		=> sub { $program_args{'build_hdf5'} = 1; },
	"build-netcdf"		=> sub { $program_args{'build_netcdf'} = 1; },
	"build-dtk"		=> sub { $program_args{'build_dtk'} = 1; },
	"build-hpl"		=> sub { $program_args{'build_hpl'} = 1; },
	"build-mpi-tutorial"		=> sub { $program_args{'build_mpi_tutorial'} = 1; },
	"build-hpcg"		=> sub { $program_args{'build_hpcg'} = 1; },
	"build-image"		=> sub { $program_args{'build_image'} = 1; },
	"build-isoimage"        => sub { $program_args{'build_isoimage'} = 1; },
	"build-nvl-guest"	=> sub { $program_args{'build_nvl_guest'} = 1; },
	"<>"			=> sub { usage(); exit(1); }
);

sub usage {
	print <<"EOT";
Usage: build.pl [OPTIONS...]
EOT
}

# Scans given directory and copies over library dependancies 
sub copy_libs {
	my $directory = shift;
	my %library_list;

	foreach my $file (`find $directory -type f -exec file {} \\;`) {
		if ($file =~ /(\S+):/) {
			foreach my $ldd_file (`ldd $1 2> /dev/null \n`) {
				if ($ldd_file =~ /(\/\S+\/)(\S+\.so\S*) \(0x/) {
					my $lib = "$1$2";
					$library_list{$lib} = $1;
					while (my $newfile = readlink($lib)) {
						$lib =~ m/(.*)\/(.*)/;
						my $dir = "$1";
						if ($newfile =~ /^\//) {
							$lib = $newfile;
							}
						else {
							$lib = "$dir/$newfile";
							}
						$lib =~ m/(.*)\/(.*)/;
						$library_list{"$1\/$2"} = $1;    # store in the library
						}
					}
				}
			}
		}

	foreach my $file (sort keys %library_list) {
		# Do not copy libraries that will be nfs mounted
		next if $file =~ /^\/cluster_tools/;

		# Do not copy over libraries that already exist in the image
		next if -e "$directory/$file";
		# Create the target directory in the image, if necessary
		if (!-e "$directory/$library_list{$file}") {
			my $tmp = "$directory/$library_list{$file}";
			if ($tmp =~ s#(.*/)(.*)#$1#) {
				system("mkdir -p $tmp") == 0 or die "failed to create $tmp";
				}
			}
		
		# Copy library to the image
		system("rsync -a $file $directory/$library_list{$file}/") == 0
			or die "failed to copy list{$file}";
		}
}

# Download any missing package tarballs and repositories
for (my $i=0; $i < @packages; $i++) {
	my %pkg = %{$packages[$i]};
	if ($pkg{package_type} eq "tarball") {
		if (! -e "$SRCDIR/$pkg{tarball}") {
			print "CNL: Downloading $pkg{tarball}\n";
			system ("wget --no-check-certificate --directory-prefix=$SRCDIR $pkg{url} -O $SRCDIR\/$pkg{tarball}") == 0 
                          or die "failed to wget $pkg{tarball}";
		}
	} elsif ($pkg{package_type} eq "git") {
		chdir "$SRCDIR" or die;
		if ($pkg{src_subdir}) {
			if (! -e "$pkg{src_subdir}") {
				mkdir $pkg{src_subdir} or die;
			}
			chdir $pkg{src_subdir} or die;
		}
		if (! -e "$pkg{basename}") {
			for (my $j=0; $j < @{$pkg{clone_cmd}}; $j++) {
				print "CNL: Running command '$pkg{clone_cmd}[$j]'\n";
				system ($pkg{clone_cmd}[$j]) == 0
                                  or die "failed to clone $pkg{clone_cmd}[$j]";
			}
		}
		chdir "$BASEDIR" or die;
	} else {
		die "Unknown package_type=$pkg{package_type}";
	}
}

# Unpack each downloaded tarball
for (my $i=0; $i < @packages; $i++) {
	my %pkg = %{$packages[$i]};
	next if $pkg{package_type} ne "tarball";

	if (! -d "$SRCDIR/$pkg{basename}") {
		print "CNL: Unpacking $pkg{tarball}\n";
		if ($pkg{tarball} =~ m/tar\.gz/) {
			system ("tar --directory $SRCDIR -zxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
		} elsif ($pkg{tarball} =~ m/tar\.bz2/) {
			system ("tar --directory $SRCDIR -jxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
		} elsif ($pkg{tarball} =~ m/tgz/) {
			system ("tar --directory $SRCDIR -zxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
		} else {
			die "Unknown tarball type: $pkg{basename}";
		}
	}
}

# Build libhugetlbfs
if ($program_args{build_libhugetlbfs}) {
	print "CNL: Building libhugetlbfs $libhugetlbfs{basename}\n";
	chdir "$SRCDIR/$libhugetlbfs{basename}" or die;
	system ("rm -rf ./_install") == 0 or die;
	system ("BUILDTYPE=NATIVEONLY make >/dev/null") == 0 or die "failed to make";
	system ("BUILDTYPE=NATIVEONLY make install DESTDIR=$BASEDIR/$SRCDIR/$libhugetlbfs{basename}/_install >/dev/null") == 0
          or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build numactl
if ($program_args{build_numactl}) {
	print "CNL: Building numactl $numactl{basename}\n";
	chdir "$SRCDIR/$numactl{basename}" or die;
	my $DESTDIR = "$BASEDIR/$SRCDIR/$numactl{basename}/_install/usr";
	$DESTDIR =~ s/\//\\\//g;
	system ("sed '/^prefix/s/\\/usr/$DESTDIR/' Makefile > Makefile.cnl") == 0 or die;
	system ("mv Makefile Makefile.orig") == 0 or die;
	system ("cp Makefile.cnl Makefile") == 0 or die;
	system ("make >/dev/null") == 0 or die "failed to make";
	system ("make install >/dev/null") == 0 or die "failed to install";
	system ("mv Makefile.orig Makefile") == 0 or die;
	system ("rm -rf ./_install/share") == 0 or die;  # don't need manpages
	chdir "$BASEDIR" or die;
}

# Build hwloc
if ($program_args{build_hwloc}) {
	print "CNL: Building hwloc $hwloc{basename}\n";
	chdir "$SRCDIR/$hwloc{basename}" or die;
	system ("rm -rf ./_install") == 0 or die;
	system ("./configure --prefix=/usr --enable-static --disable-shared >/dev/null") == 0 or die "failed to configure";
	system ("make >/dev/null") == 0 or die "failed to make";
	system ("make install DESTDIR=$BASEDIR/$SRCDIR/$hwloc{basename}/_install >/dev/null") == 0
	  or die "failed ot install";
	chdir "$BASEDIR" or die;
}

# Build OpenMPI
if ($program_args{build_ompi}) {
	print "CNL: Building OpenMPI $ompi{basename}\n";
	chdir "$SRCDIR/$ompi{basename}" or die;
	# This is a horrible hack. We're installing OpenMPI into /opt on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	#system ("LD_LIBRARY_PATH=$BASEDIR/$SRCDIR/slurm-install/lib ./configure --prefix=/opt/$ompi{basename} --disable-shared --enable-static --with-verbs=yes") == 0
	system("cp opal/mca/pmix/pmix120/pmix/include/pmi.h $BASEDIR/$SRCDIR/pisces/hobbes/libhobbes/include") == 0 or die "couldn't copy pmi.h";
	if(! -e "configure" ){
		system("./autogen.pl") == 0 || die "couldn't generate configure for openmpi";
	}
	if(! -e "./config.status" || $program_args{force_configure}){
	  system ("./configure --prefix=$BASEDIR/opt/simple_busybox/$ompi{basename} --enable-static --disable-shared --disable-dlopen --disable-oshmem --disable-java --disable-hwloc-pci --disable-mpi-io --disable-libompitrace --without-verbs --without-cuda --without-libfabric --without-portals4 --without-scif --without-usnic --without-knem --without-cma --without-x --without-lustre --without-mxm --without-psm --without-psm2 --without-ucx --without-blcr --without-dmtcp --without-valgrind  --enable-mca-no-build=maffinity,paffinity,btl-openib,btl-portals,btl-portals4,btl-scif,btl-sm,btl-tcp,btl-usnic,btl-libfabric,pmix-pmix112,pmix-isolated,pmix-pmix120,coll-tuned,,pmix-whitedb,rte-orte,reachable-netlink,ess-pmi,pmix-xpmem,pmix-s2,pmix-pisces,pmix-s1 --disable-getpwuid --with-orte=no --enable-debug  --enable-mca-static=btl-vader --with-xpmem=$BASEDIR/$SRCDIR/pisces/xpmem --with-alps=yes") == 0
		or die "failed to configure";
	}
	system ("make -j 4 LIBS=\" $ENV{LIBS} -lalpsutil -lalpslli -lwlm_detect -lugni -lrt -lutil  \" LDFLAGS=\"$ENV{LDFLAGS} -L$BASEDIR/$SRCDIR/numactl/.libs -L/opt/cray/alps/5.2.1-2.0502.9041.11.6.gem/lib64/ -L/opt/cray/ugni/5.0-1.0502.9685.4.24.gem/lib64/ -L/opt/cray/wlm_detect/1.0-1.0502.53341.1.1.gem/lib64/ -L$BASEDIR/cray/static-link -L$BASEDIR/$SRCDIR/pisces/hobbes/libhobbes  -all-static\" >/dev/null") == 0 or die "failed to make";
	#system ("make \"V=1\" -j 4 LDFLAGS=\"$ENV{LDFLAGS} -L$BASEDIR/$SRCDIR/numactl/.libs -L/opt/cray/alps/5.2.1-2.0502.9041.11.6.gem/lib64/ -L/opt/cray/ugni/5.0-1.0502.9685.4.24.gem/lib64/ -L/opt/cray/wlm_detect/1.0-1.0502.53341.1.1.gem/lib64/ -all-static\" >/dev/null") == 0 or die "failed to make";
	system ("make install ") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build Pisces
if ($program_args{build_pisces}) {
	print "CNL: Building Pisces\n";

	# Step 0: Add a convenience script to the base pisces dir that updates all pisces repos to the latest
	copy "$BASEDIR/$CONFIGDIR/pisces/update_pisces.sh", "$SRCDIR/$pisces{src_subdir}" or die;
	system ("chmod +x $SRCDIR/$pisces{src_subdir}/update_pisces.sh");

	# STEP 1: Configure and build Kitten... this will fail because
	# Palacios has not been built yet, but Palacios can't be built
	# until Kitten is configured in built. TODO: FIXME
	print "CNL: STEP 1: Building pisces/kitten stage 1\n";
	chdir "$SRCDIR/$pisces{src_subdir}/kitten" or die;
	if (-e ".config") {
		print "CNL: pisces/kitten aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: pisces/kitten using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/pisces/kitten_config", ".config" or die;
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system ("make"); # This will always fail
	chdir "$BASEDIR" or die;
	print "CNL: STEP 1: Done building pisces/kitten stage 1\n";

	# STEP 2: configure and build Palacios
	print "CNL: STEP 2: Building pisces/palacios\n";
	chdir "$SRCDIR/$pisces{src_subdir}/palacios" or die;
	if (-e ".config") {
		print "CNL: pisces/palacios aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: pisces/palacios using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/pisces/palacios_config", ".config" or die;
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system "make clean";
	system ("CFLAGS=\" -Wno-missing-field-initializers \" make \"V=1\"") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 2: Done building pisces/palacios\n";

	# STEP 3: Rebuild Kitten... this will now succeed since Palacios has been built.
	print "CNL: STEP 3: Building pisces/kitten stage 2\n";
	chdir "$SRCDIR/$pisces{src_subdir}/kitten" or die;
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 3: Done building pisces/kitten stage 2\n";

	# STEP 4: Build petlib. Pisces depends on this.
	print "CNL: STEP 4: Building pisces/petlib\n";
	chdir "$SRCDIR/$pisces{src_subdir}/petlib" or die;
	system "make clean";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 4: Done building pisces/petlib\n";

	# STEP 5: Build XPMEM for host Linux. Pisces depends on this.
	print "CNL: STEP 5: Building pisces/xpmem\n";
	chdir "$SRCDIR/$pisces{src_subdir}/xpmem/mod" or die;
	# where is kevin's kernel? /home/ktpedre/cray_cnos/linux-3.0.101-0.31.1_1.0502.8394

	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/mod LINUX_KERN=/home/ktpedre/cray_cnos/linux-3.0.101-0.31.1_1.0502.8394 make clean";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/mod LINUX_KERN=/home/ktpedre/cray_cnos/linux-3.0.101-0.31.1_1.0502.8394 make") == 0
          or die "failed to make";
	chdir "$BASEDIR" or die;
	chdir "$SRCDIR/$pisces{src_subdir}/xpmem/lib" or die;
	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/lib make clean";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/lib make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 5: Done building pisces/xpmem\n";

	# Step 6: Build Pisces for Kitten
	print "CNL: STEP 6: Building pisces/pisces\n";
	chdir "$SRCDIR/$pisces{src_subdir}/pisces" or die "Couldn't change directory to $SRCDIR/$pisces{src_subdir}/pisces";
	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/pisces KERN_PATH=/home/ktpedre/cray_cnos/linux-3.0.101-0.31.1_1.0502.8394 make clean XPMEM=y";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/pisces KERN_PATH=/home/ktpedre/cray_cnos/linux-3.0.101-0.31.1_1.0502.8394 make XPMEM=y") == 0
	  or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 6: Done building pisces/pisces\n";

	# Step 7: Build WhiteDB
	print "CNL: STEP 7: Building pisces/hobbes/whitedb-0.7.3\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/whitedb-0.7.3" or die;
	system ("autoreconf -fvi") == 0 or die "failed to autoreconf";
	system ("./configure --enable-locking=wpspin") == 0 or die "failed to configure";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 7: Done building pisces/hobbes/whitedb-0.7.3\n";

	# Step 8: Build libhobbes.a
	print "CNL: STEP 8: Building pisces/hobbes/libhobbes\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/libhobbes" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die "failed to make";
	-e "lib" or system("mkdir lib") == 0 or die "couldn't make libhobbes lib directory";
	-e "include" or system("mkdir include") == 0 or die "couldn't make libhobbes include directory";
	#system ("cp libhobbes.a lib/libpmi.a") == 0 or die "couldn't set up pmi library";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 8: Done building pisces/hobbes/libhobbes\n";

	# Step 9: Build libhobbes lnx_init
	print "CNL: STEP 9: Building pisces/hobbes/lnx_inittask/lnx_init\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/lnx_inittask" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	# this will allways die
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 9: Done building pisces/hobbes/lnx_inittask/lnx_init\n";

	# Step 10: Build libhobbes shell
	print "CNL: STEP 10: Building pisces/hobbes/shell\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/shell" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 10: Done building pisces/hobbes/shell\n";

	# Step 11: Build Hobbes Kitten init_task
	print "CNL: STEP 11: Building pisces/hobbes/lwk_inittask\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/lwk_inittask" or die;
	system "KITTEN_PATH=../../kitten XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean";
	system ("KITTEN_PATH=../../kitten XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 11: Done building pisces/hobbes/lwk_inittask\n";

	# Step 13: Build cray forwarding layer 
	print "CNL: STEP 13: Building cray forwarding\n";
	chdir "$BASEDIR/cray/static-link" or die;
	system("make") == 0 or die "couldn't make forwarding layer";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 11: Done building cray forwarding\n";


	# Step 12: Build Hobbes PMI Hello Example App
	#print "CNL: STEP 12: Building pisces/hobbes/examples/apps/pmi/test_pmi_hello\n";
	#chdir "$SRCDIR/$pisces{src_subdir}/hobbes/examples/apps/pmi" or die;
	#system ("make clean") == 0 or die "failed to clean";
	#system ("make") == 0 or die "failed to make";
	#chdir "$BASEDIR" or die;
	#print "CNL: STEP 12: Done building pisces/hobbes/examples/apps/pmi/test_pmi_hello\n";

}

# Build Curl
if ($program_args{build_curl}) {
	print "CNL: Building Curl $curl{basename}\n";
	chdir "$SRCDIR/$curl{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --disable-shared --enable-static") == 0
          or die "failed to configure";
	system ("make V=1 curl_LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build HDF5
if ($program_args{build_hdf5}) {
	print "CNL: Building HDF5 $hdf5{basename}\n";
	chdir "$SRCDIR/$hdf5{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --enable-parallel --disable-shared --enable-static") == 0
          or die "failed to configure";
	system ("make V=1 LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build NetCDF
if ($program_args{build_netcdf}) {
	print "CNL: Building NetCDF $netcdf{basename}\n";
	chdir "$SRCDIR/$netcdf{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --disable-option-checking --with-hdf5=/opt/simple_busybox/install --disable-testsets CXX=mpicxx CC=mpicc F77=mpif77 FC=mpif90 CPPFLAGS=\"-I/opt/simple_busybox/install/include\" LIBS=\"-L/opt/simple_busybox/install/lib -lhdf5 -Wl,-rpath,/opt/simple_busybox/install/lib -ldl\" --cache-file=/dev/null --enable-static --disable-shared") == 0
          or die "failed to configure";
	system ("make V=1 LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build DTK
if ($program_args{build_dtk}) {
	print "CNL: Building Data Transfer Kit\n";

	my $DTK_BASEDIR  = "$BASEDIR/$SRCDIR/$dtk{src_subdir}";
	my $DTK_BUILDDIR = "$BASEDIR/$SRCDIR/$dtk{src_subdir}/BUILD";

	# Steup symbolic links
	system ("ln -sf $DTK_BASEDIR/DataTransferKit $DTK_BASEDIR/Trilinos/DataTransferKit");
	system ("ln -sf $DTK_BASEDIR/DTKData $DTK_BASEDIR/Trilinos/DataTransferKit/DTKData");

	# Create BUILD directory
	system("mkdir -p $DTK_BUILDDIR");
	copy "$BASEDIR/$CONFIGDIR/dtk/configure_dtk.sh", "$DTK_BUILDDIR" or die;
	system ("chmod +x $DTK_BUILDDIR/configure_dtk.sh");

	# Setup Build directory (run cmake)
	chdir "$DTK_BUILDDIR" or die;
	system "PATH_TO_TRILINOS=$DTK_BASEDIR/Trilinos ./configure_dtk.sh";

	# Build Trilinos and DTK
	system ("make -j 4") == 0 or die "failed to make";

	chdir "$BASEDIR" or die;
}


# Build Hpl
if ($program_args{build_hpl}) {
	print "CNL: Building Hpl\n";

	my $HPL_BASEDIR  = "$BASEDIR/$SRCDIR/$hpl{basename}";
	local $ENV{PATH} = "$ENV{PATH}:$BASEDIR/opt/simple_busybox/ompi/bin";
	chdir "$HPL_BASEDIR" or die "couldn't find hpl directory";
	print $HPL_BASEDIR."\n";
	# Build HPL or die
	system ("make arch=Kitten") == 0 or die "failed to make";

	chdir "$BASEDIR" or die;
}


# Build Mpi Tutorial
if ($program_args{build_mpi_tutorial}) {
	print "CNL: Building Mpi Tutorial\n";

	my $MPIT_BASEDIR  = "$BASEDIR/$SRCDIR/$mpi_tutorial{basename}/tutorials/mpi-hello-world/code";
	local $ENV{PATH} = "$ENV{PATH}:$BASEDIR/opt/simple_busybox/ompi/bin";
	chdir "$MPIT_BASEDIR" or die "couldn't find MPI tutorial directory";
	print $MPIT_BASEDIR."\n";
	# Build MPI Tutorial or die
	system ("make clean") == 0 or die "failed to make";
	system ("make") == 0 or die "failed to make";

	chdir "$BASEDIR" or die;
}

# Build Hpcg 
if ($program_args{build_hpcg}) {
	print "CNL: Building Hpcg\n";


	my $HPCG_BASEDIR  = "$BASEDIR/$SRCDIR/$hpcg{basename}";
	local $ENV{PATH} = "$ENV{PATH}:$BASEDIR/opt/simple_busybox/ompi/bin";
	print $HPCG_BASEDIR."\n";
	system("cp config/hpcg/Make.Kitten_MPI_Static $HPCG_BASEDIR/setup") == 0 || die "couldn't copy hpcg config";
	chdir "$HPCG_BASEDIR" or die "couldn't find HPCG directory";
	print $HPCG_BASEDIR."\n";
	# Build HPCG or die
	system ("make clean");
	# jury rig a fix 
	system ("make arch=Kitten_MPI_Static"); #== 0 or die "failed to make";
  system ("g++ -DHPCG_NO_OPENMP -Wl,-wrap,open -Wl,-wrap=ioctl -I./src -I./src/Kitten_MPI_Static -O3 -ffast-math -ftree-vectorize -ftree-vectorizer-verbose=0 -g -static -L/opt/cray/udreg/2.3.2-1.0502.9275.1.25.gem/lib64/ -L../../cray/static-link -L../numactl/.libs -L../ompi/ompi/.libs -L../ompi/opal/.libs -L../pisces/xpmem/lib -L../pisces/hobbes/libhobbes -L/opt/cray/ugni/5.0-1.0502.9685.4.24.gem/lib64/ -L/opt/cray/alps/5.2.1-2.0502.9041.11.6.gem/lib64/ -L/opt/cray/pmi/5.0.7-1.0000.10678.155.29.gem/lib64/ -L/opt/cray/wlm_detect/1.0-1.0502.53341.1.1.gem/lib64 src/main.o src/CG.o src/CG_ref.o src/TestCG.o src/ComputeResidual.o src/ExchangeHalo.o src/GenerateGeometry.o src/GenerateProblem.o src/GenerateProblem_ref.o src/CheckProblem.o src/OptimizeProblem.o src/ReadHpcgDat.o src/ReportResults.o src/SetupHalo.o src/SetupHalo_ref.o src/TestSymmetry.o src/TestNorms.o src/WriteProblem.o src/YAML_Doc.o src/YAML_Element.o src/ComputeDotProduct.o src/ComputeDotProduct_ref.o src/finalize.o src/init.o src/mytimer.o src/ComputeSPMV.o src/ComputeSPMV_ref.o src/ComputeSYMGS.o src/ComputeSYMGS_ref.o src/ComputeWAXPBY.o src/ComputeWAXPBY_ref.o src/ComputeMG_ref.o src/ComputeMG.o src/ComputeProlongation_ref.o src/ComputeRestriction_ref.o src/GenerateCoarseProblem.o src/ComputeOptimalShapeXYZ.o src/MixedBaseCounter.o src/CheckAspectRatio.o src/OutputFile.o -o bin/xhpcg -lhobbes -ludreg -pthread -L/opt/cray/xpmem/0.1-2.0502.55507.3.2.gem/lib64 -lxpmem -L/opt/cray/udreg/2.3.2-1.0502.9275.1.25.gem/lib64 -ludreg -Wl,-rpath -Wl,/opt/cray/xpmem/0.1-2.0502.55507.3.2.gem/lib64 -Wl,-rpath -Wl,/opt/cray/udreg/2.3.2-1.0502.9275.1.25.gem/lib64 -Wl,-rpath -Wl,/ufs/home/npe/nvl.me/src/nvl/pisces/opt/simple_busybox/ompi/lib -Wl,--enable-new-dtags -L/ufs/home/npe/nvl.me/src/nvl/pisces/opt/simple_busybox/ompi/lib -lmpi -lopen-pal -lm -lnuma -L/opt/cray/ugni/5.0-1.0502.9685.4.24.gem/lib64 -lugni -L/opt/cray/xpmem/0.1-2.0502.55507.3.2.gem/lib64 -lxpmem -lmunge -lutil -lrt -lmpi -lopen-pal -lrt -lxpmem -lhobbes -lalps -lalpsutil -lxpmem_chnl_client -lhobbes -lxpmem -lalpslli -lugni -lwlm_detect  -ludreg") == 0 or die "couldn't manually compile hpcg";
	chdir "$BASEDIR" or die;
}




##############################################################################
# Build Initramfs Image
##############################################################################
if ($program_args{build_image}) {
	#system ("rm -rf $IMAGEDIR/*";

	# Create some directories needed for stuff
	system ("mkdir -p $IMAGEDIR/etc");

	# Instal linux kernel modules
#	system("rsync -a $SRCDIR/$kernel{basename}/_install/\* $IMAGEDIR/") == 0
	#system("rsync -a /lib/modules/$kernel{version} $IMAGEDIR/lib/modules/") == 0
#		or die "Failed to rsync linux modules to $IMAGEDIR";

	# Install numactl into image
	system("rsync -a $SRCDIR/$numactl{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync numactl to $IMAGEDIR";

	# Install hwloc into image
	system("rsync -a $SRCDIR/$hwloc{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync hwloc to $IMAGEDIR";

	# Install libhugetlbfs into image
	system("rsync -a $SRCDIR/$libhugetlbfs{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync libhugetlbfs to $IMAGEDIR";

	# Install OpenMPI into image
	mkdir "$IMAGEDIR/opt/simple_busybox";
	system("cp -R $BASEDIR/opt/simple_busybox/$ompi{basename} $IMAGEDIR/opt/simple_busybox") == 0
		or die "Failed to rsync OpenMPI to $IMAGEDIR";

	# Install Pisces / Hobbes / Leviathan into image
	system("cp -R $SRCDIR/pisces/xpmem/mod/xpmem.ko $IMAGEDIR/opt/hobbes") == 0
		or die "error 1";
	system("cp -R $SRCDIR/pisces/pisces/pisces.ko $IMAGEDIR/opt/hobbes") == 0
		or die "error 2";
	system("cp -R $SRCDIR/pisces/petlib/hw_status $IMAGEDIR/opt/hobbes") == 0
		or die "error 2";
	system("cp -R $SRCDIR/pisces/hobbes/lnx_inittask/lnx_init $IMAGEDIR/opt/hobbes") == 0
		or die "error 4";
	system("cp -R $SRCDIR/pisces/hobbes/shell/hobbes $IMAGEDIR/opt/hobbes") == 0
		or die "error 5";
	system("cp -R $SRCDIR/pisces/kitten/vmlwk.bin $IMAGEDIR/opt/hobbes") == 0
		or die "error 6";
	system("cp -R $SRCDIR/pisces/hobbes/lwk_inittask/lwk_init $IMAGEDIR/opt/hobbes") == 0
		or die "error 7";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/pisces_cons $IMAGEDIR/opt/hobbes") == 0
		or die "error 8";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/v3_cons_sc $IMAGEDIR/opt/hobbes") == 0
		or die "error 9";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/v3_cons_nosc $IMAGEDIR/opt/hobbes") == 0
		or die "error 10";
	#system("cp -R $SRCDIR/pisces/hobbes/examples/apps/pmi/test_pmi_hello $IMAGEDIR/opt/hobbes") == 0
	#	or die "error 11";
	system("cp -R $SRCDIR/test/null/null $IMAGEDIR/opt/hobbes") == 0
		or die "error 12";
	system("cp -R $SRCDIR/mpitutorial/tutorials/mpi-hello-world/code/mpi_hello_world $IMAGEDIR/opt/hobbes") == 0
		or die "error 13";
	system("cp -R $SRCDIR/hpcg/bin/xhpcg $IMAGEDIR/opt/hobbes") == 0
		or die "error 14";

	# Install Hobbes Enclave DTK demo files
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/DataTransferKitSTKMeshAdapters_STKInlineInterpolation.exe $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/input.xml $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/cube_mesh.exo $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/pincell_mesh.exo $IMAGEDIR/opt/hobbes_enclave_demo");

	# Files copied from build host
	system ("cp /etc/localtime $IMAGEDIR/etc");
	if ( -e "/etc/redhat-release" ) {
		system ("cp /lib64/libnss_files.so.* $IMAGEDIR/lib64");
	} elsif ( -e "/etc/debian_version" ) {
		system ("cp /lib/x86_64-linux-gnu/libnss_files.so.* $IMAGEDIR/lib/x86_64-linux-gnu");
	} else {
		die "unknown linux distribution"
	}
	system ("cp /usr/bin/ldd $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/strace $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/ssh $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/scp $IMAGEDIR/usr/bin");
	system ("cp -R /usr/share/terminfo $IMAGEDIR/usr/share");

	# Infiniband files copied from build host
	#system ("cp -R /etc/libibverbs.d $IMAGEDIR/etc");
	#system ("cp /usr/lib64/libcxgb4-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libocrdma-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libcxgb3-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libnes-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmthca-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmlx4-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmlx5-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/bin/ibv_devices $IMAGEDIR/usr/bin");
	#system ("cp /usr/bin/ibv_devinfo $IMAGEDIR/usr/bin");
	#system ("cp /usr/bin/ibv_rc_pingpong $IMAGEDIR/usr/bin");

	# Find and copy all shared library dependencies
	copy_libs($IMAGEDIR);

	# Build the guest initramfs image
	# Fixup permissions, need to copy everything to a tmp directory
	system ("cp -R $IMAGEDIR $IMAGEDIR\_tmp");
	system ("sudo chown -R root.root $IMAGEDIR\_tmp");
	system ("sudo chmod +s $IMAGEDIR\_tmp/bin/busybox_root");
	system ("sudo chmod 777 $IMAGEDIR\_tmp/tmp");
	system ("sudo chmod +t $IMAGEDIR\_tmp/tmp");
	chdir  "$IMAGEDIR\_tmp" or die;
	system ("sudo find . | sudo cpio -H newc -o > $BASEDIR/initramfs.cpio");
	chdir  "$BASEDIR" or die;
	system ("cat initramfs.cpio | gzip > initramfs.gz");
	system ("rm initramfs.cpio");
	system ("sudo rm -rf $IMAGEDIR\_tmp");

	# As a convenience, copy Linux bzImage to top level
	#system ("cp $SRCDIR/$kernel{basename}/arch/x86/boot/bzImage bzImage");
}

##############################################################################
# Build a Palacios Guest Image for the NVL (xml config file + isoimage)
##############################################################################
if ($program_args{build_nvl_guest}) {
	system ("../../nvl/palacios/utils/guest_creator/build_vm config/nvl_guest.xml -o image.img");
}
