#!/bin/bash

#source ../use-kernel-dir
export KERNEL_DIR=/home/mvaittin/gits/torvalds/linux


cd /home/mvaittin/gits/tetrao-urogallus/bb-compiler/
source setcc
cd -

if [ "$1" == "clean" ] || [ "$1" == "debug_cfg" ] || [ "$1" == "install" ] || [ "$1" == "dtb" ];
then
	make "$1"
else
	make
fi;
