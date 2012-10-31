#!/bin/bash

if [ -z "$1" ] || [ ! -d "$2" ] || [ -z "$3" ]; then
    echo Usage "$0 [codename] [obj-dir] [mode]"
    exit 1
fi

local_dir=`cd $(dirname $0); pwd`
codename="$1"
obj_dir=`cd $2; pwd`
mode="$3"
prefix=${codename}-
gdb=ucore-mp64-gdb

term=urxvt
qemu=qemu-system-x86_64
smp=4
mem=512m

qemu_cmd="$qemu -smp $smp -m $mem \
        -hda ${obj_dir}/${prefix}bootimage \
        -serial file:${obj_dir}/${prefix}serial.log \
        -net nic,model=e1000 -net user,hostfwd=tcp::9999-:7 -net dump,file=${obj_dir}/${prefix}netdump.pcap"

if [ "$mode" = "gdb" ]; then
    $qemu_cmd -vga std -s -S &
    $gdb --command=${local_dir}/gdbinit
elif [ "$mode" = "console" ]; then
    $qemu_cmd -vga std -S -monitor stdio
elif [ "$mode" = "3w" ]; then
    $term -e $qemu_cmd -vga std -monitor stdio -s -S &
    $gdb --command=${local_dir}/gdbinit
fi
