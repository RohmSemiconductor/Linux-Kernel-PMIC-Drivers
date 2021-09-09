#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

JENKINS_LOG_STORAGE=/var/jenkins/workspace/PMIC_testing_job/logs

#
# Hard-coding path is bad, Okay?
# Well, this NFS path is hardcoded also in test makefiles so I might as well hard-code it here.
#
LOGDIR=/home/mvaittin/nfs

SYSTEM_LOG="var/log/messages"

MSGSTIME=$1
sleep 10
tail -n5000 $LOGDIR/$SYSTEM_LOG > $DIR/messages_cut$MSGSTIME

LOGS="$LOGS $DIR/messages_cut$MSGSTIME"
echo $LOGS

