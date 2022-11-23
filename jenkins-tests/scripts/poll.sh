#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
source $DIR/configs/jenkins-env-config || exit -1
source $DIR/configs/jenkins-server-configs || exit -1

function end()
{
	rm -rf .mva_jenkins_polling
	echo "$*"
}


#
# I know. This is racy. It'd be better to open the file with exclusive flag
# but I've no idea how to do that with bash. It may be some exec <> macig
# would do the trick but I've no energy to start pondering it.. Let's just
# silently agree 80% is enough ^^;
#
cd $RUNCFG_JENKINS_SCRIPT_PATH
if [[ -f .mva_jenkins_polling ]]
then
	echo "Polling already ongoing"
	cat .mva_jenkins_polling
	exit -1
fi

date > .mva_jenkins_polling

git fetch $CFG_ROHM_PMIC_GIT

git rev-parse $CFG_ROHM_PMIC_GIT/$CFG_ROHM_PMIC_GIT_JENKINS_BRANCH

rm -rf .mva_jenkins_polling
