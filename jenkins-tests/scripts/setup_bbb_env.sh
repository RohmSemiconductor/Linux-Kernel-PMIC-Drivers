#!/bin/bash

#
# This file contains steps which initialize BBB SW for testing a specific linux version.
# Eg, this step is common for all test targets (BD71837, BD71847, ...) so this is only
# ran before testing the first target.
# 
# This needs to be ran again if Linux version is updated though.


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

TEST_GIT_BASE="$DIR/../../"
OVERLAY_SRC="$DIR/../../overlay_merger"
NFS_DIR=/home/mvaittin/nfs/

#
# bd718x7 does not necessarily exist in all tested branches... TBD - how to do testing correctly?
#
#TESTS="bd71837 bd71847 bd71828"
#TESTS="bd71837 bd71847 bd71828 bd99954"
TESTS="bd71837 bd71847 bd99954 bd9576 bd71815"

cd $OVERLAY_SRC || exit -1
echo "Compiling overlay merger..."
./build.sh || exit -1

echo "Installing overlay merger..."
sudo ./build.sh install || exit -1

cd -

for t in $TESTS
do
	cd "$TEST_GIT_BASE/$t" || exit -1

	echo "Compiling $t tests..."
	./build.sh || exit -1

	echo "Installing $t tests..."
	sudo ./build.sh install || exit -1

	cd -
done

