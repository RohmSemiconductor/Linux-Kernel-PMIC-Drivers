#!/bin/bash

# I may invoke this manually w/o setting the NFS path. My environment has NFS
# in /home/mvaittin/nfs - jenkins should invoke this via scripts that do
# set the CFG_BBB_NFS_ROOT
if [ -z ${CFG_BBB_NFS_ROOT+x} ]
then
	export CFG_BBB_NFS_ROOT=/home/mvaittin/nfs
fi


if [ -z ${RUNCFG_COMPILER_ENV_PATH+x} ]
then
	source ../use-kernel-dir
	cd ../bb-compiler/
	source setcc
	cd -
else
	source "$RUNCFG_COMPILER_ENV_PATH" || exit -1
	export KERNEL_DIR="$RUNCFG_REMOTES_PATH"
fi
if [ "$1" == "clean" ] || [ "$1" == "debug_cfg" ] || [ "$1" == "install" ] || [ "$1" == "dtb" ];
then
	make "$1"
else
	make
fi;
