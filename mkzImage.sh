#!/bin/bash
echo $1
#export PATH=/mnt/aaa/HH6410CompileTool/arm-1176-linux-gnueabi/bin:$PATH
#export CROSS_COMPILE=arm-1176-linux-gnueabi-
#export ARCH=arm
export CROSS_COMPILE=arm-linux-
export PATH=$PATH:/mnt/aaa/HH6410CompileTool/arm-2008q3-real/bin
export ARCH=arm
export KBUILD_OUTPUT=/mnt/aaa/6410/s3c-linux-2.6.21/outimage
make $1 
#arm-1176-linux-gnueabi-gcc
#cp arch/arm/boot/zImage /home/tftp-file/
#ls -l /home/tftp-file/zImage
#cp arch/arm/boot/zImage /home/shared/
cp arch/arm/boot/zImage /home/nfs
ls -lh /home/nfs/zImage
