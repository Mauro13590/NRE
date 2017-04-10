#!/bin/sh

if [ ! -d "$GEM5_DIR" ]; then
    echo "Please specify the gem5 directory via GEM5_DIR." >&2
    exit 1
fi

# Exec,ExecPC,Faults,I82094AA,LocalApic
gem5args=" --remote-gdb-port=1234 --debug-flags=Faults"
# gem5args="$gem5args --debug-start=3071984000"
gem5args="$gem5args $GEM5_DIR/configs/example/fs.py"
gem5args="$gem5args --command-line=\"novga serial\""
gem5args="$gem5args --caches --l2cache --cpu-type detailed --cpu-clock 1GHz --mem-size=256MB"
gem5args="$gem5args -n 2"

imgs=`mktemp -d`
mkdir $imgs/binaries
mkdir $imgs/disks

# we use the bootloader as the kernel
ln -s `readlink -f $1/bin/apps/bootloader` $imgs/binaries/x86_64-vmlinux-2.6.22.9
# create disks
dd if=/dev/zero of=$imgs/disks/x86root.img count=1024 bs=1024
dd if=/dev/zero of=$imgs/disks/linux-bigswap2.img count=1024 bs=1024

trap "" INT

if [ "$GUEST_DBG" != "" ]; then
    cmds=`mktemp`
    echo "target remote localhost:1234" > $cmds

    echo $gem5args | M5_PATH=$imgs xargs $1/tools/ignoreint/ignoreint $GEM5_DIR/build/X86/gem5.opt > log.txt 2>&1 &
    gdb --tui -command=$cmds $1/bin/apps/hypervisor-elf64

    killall gem5.opt
    rm -f $cmds
elif [ "$GEM5_DBG" != "" ]; then
    cmds=`mktemp`
    echo "b main" > $cmds
    echo "run $gem5args >log.txt" >> $cmds

    M5_PATH=$imgs gdb --tui -command=$cmds $GEM5_DIR/build/X86/gem5.debug

    rm -f $cmds
else
    echo $gem5args | M5_PATH=$imgs xargs $GEM5_DIR/build/X86/gem5.opt > log.txt &
    while [ "`lsof -i :3456`" = "" ]; do
        sleep 1
    done
    telnet 127.0.0.1 3456
fi

rm -rf $imgs
