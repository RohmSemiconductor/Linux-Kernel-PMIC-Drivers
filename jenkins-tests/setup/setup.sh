#!/bin/bash

source jenkins-env-config-default
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
INSTALL_START_PATH=$DIR/../

function pr_debug() {
	echo "$*"
}

#function pr_debug() {
#	;
#}

function install_succes() {
	echo "Test environment created in $CFG_LOCAL_WORKDIR"
	exit 0
}

function install_fail() {
	echo "Install Failed"
	echo "You may want to clean-up by removing \'$CFG_LOCAL_WORKDIR\'"
	exit -1
}

function folder_exists_check() {

	if [ ! -d "$1" ]
	then
		echo "Folder $1 not found"
		return 1
	fi

	return 0
}

function ask_continue() {
	read -n1 -r -p "$*" RESP

	if [ ! "$RESP" = "y" ]
	then
		echo ""
		return 1;
	fi
	echo ""
	return 0;
}


function git_remote_add() {
	local GIT_NAME="$1"
	local GIT_URL="$2"

	git remote get-url "$GIT_NAME" > /dev/null
#	git remote show "$GIT_NAME" > /dev/null
	if [ $? -eq 0 ]
	then
		ask_continue "remote $GIT_NAME found, skipping git setup. Continue (y/n)?" || exit 0
	else
		echo "add $GIT_NAME git"
		pr_debug "git remote add $GIT_NAME '$GIT_URL'"
		git remote add "$GIT_NAME" "$GIT_URL" || install_fail
		git fetch "$GIT_NAME" || install_fail
	fi
}


#
# Print configs for debug purposes:

echo "root folder: $CFG_LOCAL_WORKDIR"
echo "urogallus folder: $CFG_BBB_REPO_PATH, name: $CFG_BBB_REPO_NAME"
echo "Compiler-path: $CFG_BBB_COMPILER_PATH"
#

# Fetch BBB firmware, compilers etc.
#
if [ ! -d "$CFG_BBB_REPO_PATH"/"$CFG_BBB_REPO_NAME" ]
then
	ask_continue "No BBB repo found, clone now (requires password) (y/n)?" || exit 0
	if [ ! -d "$CFG_BBB_REPO_PATH" ]
	then
		echo "Creating dir $CFG_BBB_REPO_PATH"
		mkdir -p "$CFG_BBB_REPO_PATH" || install_fail
	fi
	pr_debug "Cloning $CFG_BBB_REPO_URL to $CFG_BBB_REPO_PATH"
	cd "$CFG_BBB_REPO_PATH"
	git clone $CFG_BBB_REPO_URL || install_fail
	cd "$CFG_BBB_REPO_NAME"
	git checkout "$CFG_BBB_REPO_BRANCH" || install_fail

	echo "BBB Tools ready"
fi

#
# Sanity check
#
echo "Check for BBB repo"
folder_exists_check "$CFG_BBB_REPO_PATH"/"$CFG_BBB_REPO_NAME" || install_fail
echo "Check for BBB compiler"
folder_exists_check "$CFG_BBB_COMPILER_PATH" || install_fail

#
#Create local workdir(s)
#
if [ -d $CFG_LOCAL_WORKDIR ]
then
	ask_continue "Found existing installation dir - continue (y/n)?" || exit 0
else
	echo "creating install dir $CFG_LOCAL_WORKDIR"
	mkdir -p "$CFG_LOCAL_WORKDIR" || exit -1
fi

if [ ! -d "$CFG_LOCAL_WORKDIR"/gits ]
then
	echo "create git dir"
	mkdir "$CFG_LOCAL_WORKDIR"/gits || install_fail
fi

folder_exists_check "$CFG_LOCAL_WORKDIR"/gits || install_fail

cd "$CFG_LOCAL_WORKDIR"/gits || install_fail

#
# Chek out the tests from ROHM Power to a test folder.
#
echo check out the tests
#git_remote_add "$CFG_ROHM_PMIC_GIT" "$CFG_ROHM_PMIC_GIT_URL"

if [ -d "$CFG_LOCAL_WORKDIR"/gits/"$CFG_TESTS_FOLDER" ]
then
	ask_continue "dir $CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER exists - won't setup git. continue (y/n)" || exit 0
else
	git clone --origin "$CFG_ROHM_PMIC_GIT" "$CFG_ROHM_PMIC_GIT_URL" "$CFG_TESTS_FOLDER" || install_fail
	#git checkout "$CFG_ROHM_PMIC_GIT"/"$CFG_ROHM_PMIC_GIT_JENKINS_BRANCH" || install_fail
fi

cd "$CFG_LOCAL_WORKDIR"/gits/"$CFG_TESTS_FOLDER" || install_fail
pr_debug "Update index for $CFG_ROHM_PMIC_GIT in $CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER"
git fetch $CFG_ROHM_PMIC_GIT
pr_debug "checkout $CFG_ROHM_PMIC_GIT/$CFG_ROHM_PMIC_GIT_JENKINS_BRANCH"
git checkout "$CFG_ROHM_PMIC_GIT"/"$CFG_ROHM_PMIC_GIT_JENKINS_BRANCH" || install_fail

source "$CFG_CONFIG_PATH"/jenkins-server-configs || install_fail

#
# Sanity checks which require server configs to be sourced
#
echo "Check for BBB firmware"
folder_exists_check "$CFG_BBB_REPO_PATH"/"$CFG_BBB_REPO_NAME"/"$CFG_BBB_FIRMWARE_FOLDER" || install_fail

cd "$CFG_LOCAL_WORKDIR"/gits || install_fail
#
# Checkout Linux driver source repositories (ROHM Power, Torvalds, Linux Next)
# NOTE: Test scripts are also in ROHM Power but we checkout it in different
# folder in order to be able to have both the test scripts and drivers checked
# out at the same time. Splitting is no fun. TODO: Consider moving tests etc to
# a different repo.
if [ -d "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME" ]
then
	ask_continue "dir $CFG_LOCAL_WORKDIR/gits/$CFG_GIT_FOLDER_NAME exists - won't setup git. continue (y/n)" || exit 0
else
	mkdir -p "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME" || install_fail
	cd "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME" || install_fail

	git init
	git_remote_add "$CFG_ROHM_PMIC_GIT" "$CFG_ROHM_PMIC_GIT_URL"

#$CFG_ROHM_PMIC_GIT_URL
#	echo "clone ROHM PMIC git"
#	git clone --origin "$CFG_ROHM_PMIC_GIT" "$CFG_ROHM_PMIC_GIT_URL" "$CFG_GIT_FOLDER_NAME" || install_fail
fi

cd "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME" || install_fail

#pr_debug "git fetch $CFG_ROHM_PMIC_GIT"
#git fetch "$CFG_ROHM_PMIC_GIT" || install_fail


NEW_RUNCONFIG_FILE="$CFG_CONFIG_PATH"/jenkins-env-config

if [ -f "$NEW_RUNCONFIG_FILE" ]
then
	ask_continue "found existing jenkins config, keep old (y/n)?"
	if [ $? -ne 0 ]
	then
		cp $DIR/jenkins-env-config-default "$NEW_RUNCONFIG_FILE" || install_fail

		echo add RUNCONFIGS
		echo "# Following paths are generated by the setup script" >> $NEW_RUNCONFIG_FILE
		echo "" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_JENKINS_ROOT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_JENKINS_SCRIPT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_SCRIPT_FOLDER'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_JENKINS_TARGET_SCRIPT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_TARGET_SCRIPT_FOLDER'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_JENKINS_TARGET_CONFIG_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_TARGET_CONFIG_FOLDER'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_REMOTES_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_GIT_FOLDER_NAME'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_LOGDIR='$CFG_LOCAL_WORKDIR/logs'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_BBB_REPO_ROOT='$CFG_BBB_REPO_PATH/$CFG_BBB_REPO_NAME'" >> $NEW_RUNCONFIG_FILE
		echo "export RUNCFG_COMPILER_ENV_PATH='$CFG_BBB_REPO_PATH/$CFG_BBB_REPO_NAME/$CFG_BBB_COMPILER_ENV_SCRIPT'" >> $NEW_RUNCONFIG_FILE
	fi
else
	cp $DIR/jenkins-env-config-default "$NEW_RUNCONFIG_FILE" || install_fail

	#
	# Here we could echo absolute paths as helper env variables to config.
	# That would simplify actual test scripts which would no longer need to
	# compose full paths based on install root and folder names.
	# Let's see what is correct way of clean-up this mess of env variables...

	echo add RUNCONFIGS
	echo "# Following paths are generated by the setup script" >> $NEW_RUNCONFIG_FILE
	echo "" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_JENKINS_ROOT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_JENKINS_SCRIPT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_SCRIPT_FOLDER'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_JENKINS_CONFIG_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_CONFIG_FOLDER'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_JENKINS_TARGET_SCRIPT_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_TARGET_SCRIPT_FOLDER'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_JENKINS_TARGET_CONFIG_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_TARGET_CONFIG_FOLDER'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_REMOTES_PATH='$CFG_LOCAL_WORKDIR/gits/$CFG_GIT_FOLDER_NAME'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_LOGDIR='$CFG_LOCAL_WORKDIR/logs'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_BBB_REPO_ROOT='$CFG_BBB_REPO_PATH/$CFG_BBB_REPO_NAME'" >> $NEW_RUNCONFIG_FILE
	echo "export RUNCFG_COMPILER_ENV_PATH='$CFG_BBB_REPO_PATH/$CFG_BBB_REPO_NAME/$CFG_BBB_COMPILER_ENV_SCRIPT'" >> $NEW_RUNCONFIG_FILE
fi

cd "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME" || install_fail

git_remote_add "$CFG_TORVALDS_GIT" "$CFG_TORVALDS_GIT_URL"
git_remote_add "$CFG_LINUX_NEXT_GIT" "$CFG_LINUX_NEXT_GIT_URL"

#
# Create folder for BBB firmware binaries
if [ ! -d "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME"/firmware ]
then
	mkdir "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME"/firmware
fi

#git remote show "$CFG_TORVALDS_GIT" > /dev/null
#if [ $? -eq 0 ]
#then
#	ask_continue "remote $CFG_TORVALDS_GIT found, skipping git setup. Continue (y/n)?" || exit 0
#else
#	echo "add torvalds git"
#	pr_debug "git remote add $CFG_TORVALDS_GIT '$CFG_TORVALDS_GIT_URL'"
#	git remote add "$CFG_TORVALDS_GIT" "$CFG_TORVALDS_GIT_URL" || install_fail
#fi

#git remote show "$CFG_LINUX_NEXT_GIT" > /dev/null
#if [ $? -eq 0 ]
#then
#	ask_continue "remote $CFG_LINUX_NEXT_GIT found, skipping git setup. Continue (y/n)?" || exit 0
#else
#	echo "add linux-next git"
#	pr_debug "git remote add $CFG_LINUX_NEXT_GIT '$CFG_LINUX_NEXT_GIT_URL'"
#	git remote add "$CFG_LINUX_NEXT_GIT" "$CFG_LINUX_NEXT_GIT_URL" || install_fail
#fi

#git fetch "$CFG_TORVALDS_GIT" || install_fail
#git fetch "$CFG_LINUX_NEXT_GIT" || install_fail

if [ ! -f "$CFG_LOCAL_WORKDIR"/gits/"$CFG_TESTS_FOLDER"/"$CFG_JENKINS_ROOT_FOLDER"/"$CFG_SCRIPT_FOLDER"/test.sh ]
then
	echo "Main test script '$CFG_LOCAL_WORKDIR/gits/$CFG_TESTS_FOLDER/$CFG_JENKINS_ROOT_FOLDER/$CFG_SCRIPT_FOLDER/test.sh' not found!"
	install_fail
fi

if [ -f "$CFG_LOCAL_WORKDIR"/test.sh ]
then
	ask_continue "Old $CFG_LOCAL_WORKDIR/test.sh found, overwrite (y/n)?" || exit 0
fi

cp "$CFG_LOCAL_WORKDIR"/gits/"$CFG_TESTS_FOLDER"/"$CFG_JENKINS_ROOT_FOLDER"/"$CFG_SCRIPT_FOLDER"/test.sh "$CFG_LOCAL_WORKDIR"/.

if [ -e "$CFG_LOCAL_WORKDIR"/configs ]
then
	echo "Config dir exists - won't overwrite. If configs are old/incorrect, remove them and create links:"
	echo "ln -s '$CFG_CONFIG_PATH' '$CFG_LOCAL_WORKDIR/configs'"
else
	echo "creating link to configs"
	ln -s "$CFG_CONFIG_PATH" "$CFG_LOCAL_WORKDIR"/configs
fi

if [ ! -d "$CFG_LOCAL_WORKDIR"/logs/testlogs ]
then
	mkdir -p "$CFG_LOCAL_WORKDIR"/logs/testlogs
fi

if [ ! -d "$CFG_LOCAL_WORKDIR"/logs/oldlogs ]
then
	mkdir "$CFG_LOCAL_WORKDIR"/logs/oldlogs
fi

if [ ! -f "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME"/.config ]
then
	cp "$CFG_LOCAL_WORKDIR"/gits/"$CFG_TESTS_FOLDER"/default-jenkins-kernel-config "$CFG_LOCAL_WORKDIR"/gits/"$CFG_GIT_FOLDER_NAME"/.config
fi

install_succes
#git checkout "


