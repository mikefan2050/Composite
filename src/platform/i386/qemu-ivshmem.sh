#!/bin/sh
if [ $# != 2 ]; then
  echo "Usage: $0 <module> <core>"
  exit 1
fi

taskset -c $2 qemu-system-i386 --enable-kvm -m 256 -device ivshmem,size=512M,shm=ivshmem -cpu host -nographic -kernel kernel.img -no-reboot -initrd $1

