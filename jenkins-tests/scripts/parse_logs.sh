#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

LOGDIR="$CFG_BBB_NFS_ROOT"

SYSTEM_LOG="var/log/messages"

TEST_LOGS="testlogs/$1_test_log.txt"

#echo "Sleep a little to give NFS time to update logs - should probably implement sync..."
#sleep 10
echo "cutting messages: tail -n5000 $LOGDIR/$SYSTEM_LOG"

echo "" >> $DIR/messages_cut$TIMESTAMP
echo "" >> $DIR/messages_cut$TIMESTAMP
echo "" >> $DIR/messages_cut$TIMESTAMP
echo "********************************************************" >> $DIR/messages_cut$TIMESTAMP
echo "RUN FOR $1" >> $DIR/messages_cut$TIMESTAMP
tail -n5000 $LOGDIR/$SYSTEM_LOG >> $DIR/messages_cut$TIMESTAMP

#export LOGS="$LOGS $DIR/messages_cut$TIMESTAMP"
#echo LOGS now $LOGS

echo "checking for test completion string: grep 'HELPPO HOMMA' $LOGDIR/$TEST_LOGS"
grep "HELPPO HOMMA" $LOGDIR/$TEST_LOGS || (echo "Log parser did not find test completion string"; exit -1); 

