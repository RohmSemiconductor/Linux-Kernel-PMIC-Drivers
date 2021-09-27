#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

#JENKINS_LOG_STORAGE=/var/jenkins/workspace/PMIC_testing_job/logs

#echo "moving old tar(s): mv $RUNCFG_LOGDIR/testlogs//results_*.tar.gz $RUNCFG_LOGDIR/oldlogs/."
mv $RUNCFG_LOGDIR/testlogs//results_*.tar.gz $RUNCFG_LOGDIR/oldlogs/.

D2=`date +%Y%m%d_%s`

#echo "Creating new tar: tar -cvzf $RUNCFG_LOGDIR/testlogs/results_$D2.tar.gz $LOGS"
tar -cvzf $RUNCFG_LOGDIR/testlogs/results_$D2.tar.gz $LOGS
#|| exit -1

#echo "cleaning logs rm -rf $LOGS"
rm -rf $LOGS

