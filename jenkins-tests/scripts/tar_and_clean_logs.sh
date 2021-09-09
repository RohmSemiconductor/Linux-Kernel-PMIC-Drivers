#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

JENKINS_LOG_STORAGE=/var/jenkins/workspace/PMIC_testing_job/logs

echo "moving old tar(s): mv $JENKINS_LOG_STORAGE/testlogs//results_*.tar.gz $JENKINS_LOG_STORAGE/oldlogs/."
mv $JENKINS_LOG_STORAGE/testlogs//results_*.tar.gz $JENKINS_LOG_STORAGE/oldlogs/.

D2=`date +%Y%m%d_%s`

echo "Creating new tar: tar -cvzf $JENKINS_LOG_STORAGE/testlogs/results_$D2.tar.gz $LOGS"
tar -cvzf $JENKINS_LOG_STORAGE/testlogs/results_$D2.tar.gz $LOGS
#|| exit -1

echo "cleaning logs rm -rf $LOGS"
rm -rf $LOGS

