#!/bin/bash

#
# This file contains steps which initialize BBB SW for testing a specific linux version.
# Eg, this step is common for all test targets (BD71837, BD71847, ...) so this is only
# ran before testing the first target.
#
# This needs to be ran again if Linux version is updated though.


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

#
# We expect tests and the overlay-merger to be in same repository where this
# script is stored. This may not be true in the future. TODO: We could
# remove the custom overlay merging code and use the overlay code maintained
# by Geert at Renesas repo.

TEST_GIT_BASE="$DIR/../../"
OVERLAY_SRC="$DIR/../../overlay_merger"
NFS_DIR="$CFG_BBB_NFS_ROOT"

echo "setup_bbb_env.sh - NFS dir set $CFG_BBB_NFS_ROOT"

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
sudo -E ./build.sh install || exit -1

cd -

for t in $TESTS
do
	cd "$TEST_GIT_BASE/$t" || exit -1

	echo "Compiling $t tests..."
	./build.sh || exit -1

	echo "Installing $t tests..."
	sudo -E ./build.sh install || exit -1

	cd -
done

