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

function install_fail() {
	echo "Install Failed"
	echo "You may want to clean-up by removing \'$LOCAL_WORKDIR\'"
	exit -1
}

function ask_continue() {
	read -n1 -r -p "$*" RESP

	if [ ! "$RESP" = "y" ]
	then
		echo ""
		pr_debug "exiting"
		return 1;
	fi
	echo ""
	pr_debug continuing...
	return 0;
}

if [ -d $LOCAL_WORKDIR ]
then
	ask_continue "Found existing installation dir - continue (y/n)?" || exit 0
#	read
#	if [ ${REPLY} != "y" ]
#	then
#		echo "exiting"
#		exit 0;
#	fi
else
	echo "creating install dir $LOCAL_WORKDIR"
	mkdir -p "$LOCAL_WORKDIR" || exit -1
fi

if [ ! -d "$LOCAL_WORKDIR"/gits ]
then
	echo "create git dir"
	mkdir "$LOCAL_WORKDIR"/gits || install_fail
fi


cd "$LOCAL_WORKDIR"/gits || install_fail

if [ -d "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME" ]
then
	ask_continue "dir $LOCAL_WORKDIR/gits/$GIT_FOLDER_NAME exists - won't setup git. continue (y/n)" || exit 0
else
	echo "clone ROHM PMIC git"
	git clone --origin "$ROHM_PMIC_GIT" "$ROHM_PMIC_GIT_URL" "$GIT_FOLDER_NAME" || install_fail
fi

cd "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME"

pr_debug "git fetch $ROHM_PMIC_GIT"
git fetch "$ROHM_PMIC_GIT" || install_fail

pr_debug "checkout $ROHM_PMIC_GIT/$ROHM_PMIC_GIT_JENKINS_BRANCH"
git checkout "$ROHM_PMIC_GIT"/"$ROHM_PMIC_GIT_JENKINS_BRANCH" || install_fail

source "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME"/"$CONFIG_FOLDER"/jenkins-server-configs

if [ -f "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME"/$CONFIG_FOLDER/jenkins-env-config ]
then
	ask_continue "found existing jenkins config, overwrite (y/n)?" || cp $DIR/jenkins-env-config-default "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME"/$CONFIG_FOLDER/jenkins-env-config || install_fail
fi

cd "$LOCAL_WORKDIR"/gits/"$GIT_FOLDER_NAME" || install_fail

git remote show "$TORVALDS_GIT" > /dev/null
if [ $? -eq 0 ]
then
	ask_continue "remote $TORVALDS_GIT found, skipping git setup. Continue (y/n)?" || exit 0
else
	echo "add torvalds git"
	git remote add "$TORVALDS_GIT" "$TORVALDS_GIT_URL" || install_fail
fi

git remote show "$LINUX_NEXT_GIT" > /dev/null
if [ $? -eq 0 ]
then
	ask_continue "remote $LINUX_NEXT_GIT found, skipping git setup. Continue (y/n)?" || exit 0
else
	echo "add linux-next git"
	git remote add "$LINUX_NEXT_GIT" "$LINUX_NEXT_GIT_URL" || install_fail
fi

git fetch "$TORVALDS_GIT" || install_fail
git fetch "$LINUX_NEXT_GIT" || install_fail

#git checkout "


