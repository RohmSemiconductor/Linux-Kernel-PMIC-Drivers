#!/bin/bash

#
# Main test file. Here it all starts. This is kicked by Jenkins when 
# it determines that testing is needed.
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
JENKINS_LOG_STORAGE=/var/jenkins/workspace/PMIC_testing_job/logs

#
# HW targets
#
#TARGETS="bd71847 bd71837 bd9576"
TARGETS="bd71847 bd71837 bd9576"
#TARGETS="bd9576"

#
# Git stuff
#
GIT_ROHM_POWER=$DIR/gits/Linux-Kernel-PMIC-Drivers/
#BOWER_BRANCHES="v4.9.99-BD71815AGW tests-devel"
#BOWER_BRANCHES="tests-devel"

FOLDER_DEFAULT="$GIT_ROHM_POWER"
TEST_GIT_DEFAULT="linux-next"
TEST_BRANCH_TARGET_DEFAULT="tests-devel"

#
# Commands
#
COMPILE=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/paalaa.sh
RUN_TESTS=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/run_tests.sh
REBOOT=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/powercycle_bbb.sh
SETUP_BBB_ENV=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/setup_bbb_env.sh
UPDATE_TGT_SCRIPTS=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/update-target-scripts.sh
POWERCTRL="$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/ip-power-control.sh"



ORIG_ARGS="$@"

echo "Script executed as '${BASH_SOURCE:-$0} $ORIG_ARGS'"

#
# Arguments:
# -f <folder>
# -g <git>
# -b <branch>
# -t <tag>
# -U (no-Update - don't update local git index)
# -C (no-Checkout)
# -P (no-pull)
# -h (help - print arguments)
#
OPTSTRING=":T:f:g:b:t:UCPBhRr"

function usage() {
	echo 'Usage:'
	echo './test.sh [-T target] [-f local-folder] [-g git-remote] [-b git-branch (or tag)] [-U] [-h] [-C] [-R] [-P] [-B] [-r]'
	echo 'test few PMIC drivers on break-out board connected to a BBB.'
	echo "-B means \"no-Build\" Eg. don't build the sources at all - just run the tests"
	echo "-C means "no-Checkout" Eg. don't checkout anything from git but use sources as in workarea"
	echo "-P means \"no-Pull\" Eg. don't pull/merge anything from remote git"
	echo "-R means \"no-Restart\" Eg. Don't update this script from git and restart"
	echo "-U means \"no-Update\" Eg. don't update local git index from remote"
	echo '-r skips reboot to speed-up manual testing'
	echo '-b is the branch we use'
	echo "-t is the tag we use"
	echo "-T test targets instead the default ones"
	echo '-f is (local) folder where our linux git is contained'
	echo '-g is the name of the git remote script uses'
	echo '-h prints this help text'
}

CHECKOUT_GIT=""
UPDATE_GIT=""
PULL_GIT=""
BUILD_SW=""
RESTART=""
TEST_TAG=""

while getopts "$OPTSTRING" opt; do
  case ${opt} in
    g )
      TEST_GIT=$OPTARG
      ;;
    T )
      TARGETS=$OPTARG
      ;;
    t )
      TEST_TAG=$OPTARG
#If TAG is given then we should not pull.
      PULL_GIT="no"
      ;;
    b )
      TEST_BRANCH_TARGET=$OPTARG
      ;;
    f )
      TEST_FOLDER=$OPTARG
      ;;
    C )
      CHECKOUT_GIT="no"
      echo "CHECKOUT_GIT - no"
      ;;
    P )
      PULL_GIT="no"
      echo "PULL_GIT - no"
      ;;
    B )
      BUILD_SW="no"
      echo "BUILD_SW - no"
      ;;
    U )
      UPDATE_GIT="no"
      echo "UPDATE_GIT - no"
      ;;
    R )
      RESTART="no"
      ;;
    r )
      REBOOT=''
      ;;
    h )
      usage
      exit 0
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      exit -1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      exit -1
      ;;
  esac
done
shift $((OPTIND -1))

MAIN_SCRIPT_TESTS=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/test.sh

#
# I hate it when git does not contain recent version of scripts. That will
# for sure happen to this script as it is not executed from git dir.
# Let's by default get this script from git folder.
#
# Well, consider changing this hack to symlink as symlink prevents us
# from accidentally overwriting changes.
#
# Actually, this hack works with symlink
#
if [[ $RESTART != "no" ]]
then
	#TBD - urogallus requires login... I don't want to script any git
	#passwds here. Maybe do key based login?
	cp $MAIN_SCRIPT_TESTS .
	echo "execing \"${BASH_SOURCE:-$0}\" \"$ORIG_ARGS -R\""
	exec "${BASH_SOURCE:-$0}" $ORIG_ARGS -R
fi

if [[ $TEST_FOLDER == "" ]]
then
	TEST_FOLDER="$FOLDER_DEFAULT"
fi

if [[ $TEST_GIT == "" ]]
then
	TEST_GIT=$TEST_GIT_DEFAULT
fi

if [[ $TEST_BRANCH_TARGET == "" ]]
then
	if [[ $TEST_TAG == "" ]]
	then
		TEST_BRANCH_TARGET=$TEST_BRANCH_TARGET_DEFAULT
	fi
else
	if [[ $TEST_TAG != "" ]]
	then
		echo "Both branch and tag given - aborting"
	      	exit -1
	fi
fi

export TIMESTAMP=`date +%Y%m%d_%s`
TESTLOG="$JENKINS_LOG_STORAGE/jenkins-log-$TIMESTAMP.txt"
export LOGS=$TESTLOG

date > $TESTLOG
PARSE_LOG=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/parse_logs.sh
COLLECT_MSGS=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/collect_messages.sh
COLLECT_AND_CLEAN_LOGS=$DIR/gits/tetrao-urogallus/jenkins-tests/scripts/tar_and_clean_logs.sh

function get_out() {
	if [ $1 -eq 0 ]
	then
		echo "SUCCESS" |tee -a $TESTLOG
	else
		echo "FAILED $1" |tee -a $TESTLOG
	#	mv $TESTLOG $JENKINS_LOG_STORAGE/testlogs/jenkins-log-$TIMESTAMP.txt
	fi
	#$PARSE_LOG
	$COLLECT_AND_CLEAN_LOGS
	#tar -cvzf $JENKINS_LOG_STORAGE/testlogs/$TESTLOG.tar.gz $TESTLOG 
	#rm -r $TESTLOG

	exit $1
}

function err_out() {
	if [  -z "$1" ]
	then
		ERR=-1
	else
		ERR="$1"
	fi
	get_out "$1"
}

function sukset_ulos() {
	get_out 0
}

echo "Starting test run using git $TEST_GIT tag/branch $TEST_BRANCH_TARGET" |tee -a $TESTLOG

FIRMWARE_TGT="$TEST_FOLDER/firmware/."

#
# Paths
#
FIRMWARE=$DIR/gits/tetrao-urogallus/bbb_linux_firmware/*
DTS_FOLDER="$TEST_FOLDER/arch/arm/boot/dts/"

echo "update target test scripts" |tee -a $TESTLOG
$UPDATE_TGT_SCRIPTS || err_out $?

echo "Setup host network etc - might be already done" |tee -a $TESTLOG
#
# this should be done when PC is started (start tftp and nfs & configure interfaces)
#
sudo $DIR/gits/tetrao-urogallus/jenkins-tests/scripts/dothings.sh |tee -a $TESTLOG

#
# Urogallus is not open for all to read. Skip it for now
#
#GIT_UROGALLUS=$DIR/gits/tetrao-urogallus/

echo "update git $TEST_GIT" |tee -a $TESTLOG 

echo "get into work folder '$TEST_FOLDER'" |tee -a $TESTLOG
cd $TEST_FOLDER || err_out $?



if [[ $CHECKOUT_GIT == "no" ]]
then
	echo "Skipping git checkout..." |tee -a $TESTLOG
else
	if [[ $TEST_TAG != "" ]]
	then
		if [[ $UPDATE_GIT != "no" ]]
		then
			echo "Fetching tags from  $TEST_GIT" |tee -a $TESTLOG
			git fetch $TEST_GIT --tags || err_out $?
		fi
		echo "Reset to tag $TEST_TAG" |tee -a $TESTLOG
		#git checkout $TEST_TAG || err_out $?
		git reset --hard $TEST_TAG  || err_out $?
	else

		if [[ $UPDATE_GIT == "no" ]]
		then
			echo "Skipping git update..." |tee -a $TESTLOG
		else
			echo "Updating local index" |tee -a $TESTLOG
			git fetch $TEST_GIT || err_out $?
		fi

		echo "Reset to branch $TEST_GIT/$TEST_BRANCH_TARGET" |tee -a $TESTLOG
		git reset --hard $TEST_GIT/$TEST_BRANCH_TARGET
		#git checkout $TEST_BRANCH_TARGET || err_out $?
	fi
fi

if [[ $PULL_GIT == "no" ]]
then
	echo "Skipping git pull..." |tee -a $TESTLOG
else
##	echo "Merging changes from remote (how to handle merge conflicts?)" |tee -a $TESTLOG
#	git pull || err_out $?
	if [[ $TEST_TAG != "" ]]
	then
		git reset --hard $TEST_TAG  || err_out $?
	else
		git reset --hard $TEST_GIT/$TEST_BRANCH_TARGET
	fi
fi

echo "copying BBB firmware" |tee -a $TESTLOG
cp $FIRMWARE $FIRMWARE_TGT || err_out $?

if [[ $BUILD_SW == "" ]]
then
	echo "compiling Linux" |tee -a $TESTLOG
	$COMPILE zImage || err_out $?

	echo "compiling device-tree" |tee -a $TESTLOG
	$COMPILE dtree || err_out $?

	echo "Setup BBB environments" |tee -a $TESTLOG
	$SETUP_BBB_ENV || err_out $?
else
	echo "Skipping Linux and test compilations..." |tee -a $TESTLOG
fi

for t in $TARGETS
do
	source "$DIR/gits/tetrao-urogallus/jenkins-tests/configs/$t.conf"

	sudo $POWERCTRL $POWER_PORT 0
	sleep 5
	sudo $POWERCTRL $POWER_PORT 1

	#
	#	We must reboot BBB before starting the tests as we want to have new kernel running (and start from known state.
	#	We could do some ping testing to detect when BBB has rebooted - but we still would have problem
	#	as SSH will fail untill userspace is properly up - ping OTOH will succeed already when uBoot enables net.
	#	I guess many SSH attempts with time-out would be the solution - but lazy me just writes sleep here for now.
	#	Let's see if 50 seconds is enough.
	$REBOOT
	if [[ $REBOOT != '' ]]
	then
		sleep 50
	fi

	echo "Run tests for $t" |tee -a $TESTLOG
	$RUN_TESTS $t || err_out $?

	echo "Parse logs..." |tee -a $TESTLOG
	MSGSTIME=`date +%Y%m%d_%s`
	export LOGS=`$COLLECT_MSGS $MSGSTIME`
	$PARSE_LOG $t || err_out $?

done
$REBOOT

sukset_ulos

