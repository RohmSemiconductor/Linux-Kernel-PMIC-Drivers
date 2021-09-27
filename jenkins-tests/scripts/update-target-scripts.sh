#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

TEST_GIT_BASE="$DIR/../../"
TARGET_TEST_DIR="$TEST_GIT_BASE/jenkins-tests/scripts/target_scripts/"
#NFS_DIR=/home/mvaittin/nfs/
NFS_DIR="$CFG_BBB_NFS_ROOT"

echo "Copying target files to NFS"
sudo cp -r "$TARGET_TEST_DIR/"* "$NFS_DIR/." || exit -1
