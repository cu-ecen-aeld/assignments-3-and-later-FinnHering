#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{usr/bin,usr/lib,usr/sbin,var/log,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
    sed -i 's/^CONFIG_TC=y.*/# CONFIG_TC is not set/' .config
    make
else
    cd busybox
fi

# TODO: Make and install busybox
make CONFIG_PREFIX=${OUTDIR}/rootfs/ install
cd ${OUTDIR}/rootfs/

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
LD_INTERPRETER=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | sed 's/^.*:\(.*\)].*$/\1/' | xargs basename | xargs -I {} find $SYSROOT -name {})
cp $LD_INTERPRETER ${OUTDIR}/rootfs/lib

LD_NEEDED_LIBRARIES=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | sed 's/.*\[\(.*\)\].*/\1/' | xargs -L 1 basename | xargs -I {} find $SYSROOT -name {})
cp $LD_NEEDED_LIBRARIES ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make

cd $FINDER_APP_DIR
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -r {finder.sh,finder-test.sh,writer,autorun-qemu.sh} ${OUTDIR}/rootfs/home/
cp -rL conf ${OUTDIR}/rootfs/home
# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio