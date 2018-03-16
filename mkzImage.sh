#!/bin/bash
echo $1
#export PATH=/mnt/aaa/HH6410CompileTool/arm-1176-linux-gnueabi/bin:$PATH
#export CROSS_COMPILE=arm-1176-linux-gnueabi-
#export ARCH=arm
export CROSS_COMPILE=arm-linux-
export PATH=$PATH:/mnt/aaa/HH6410CompileTool/arm-2008q3-real/bin
export ARCH=arm
make $1 
#arm-1176-linux-gnueabi-gcc
#cp arch/arm/boot/zImage /home/tftp-file/
#ls -l /home/tftp-file/zImage
#cp arch/arm/boot/zImage /home/shared/
cp arch/arm/boot/zImage /home/nfs
ls -lh /home/nfs/zImage
