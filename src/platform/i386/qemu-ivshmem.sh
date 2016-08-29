#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')

qemu-system-i386 --enable-kvm -m 128 -device ivshmem,size=128M,shm=ivshmem -nographic -kernel kernel.img -no-reboot -initrd "$(echo $MODULES | tr ' ' ',')"

