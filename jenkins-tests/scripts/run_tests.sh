#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

TARGET=$1

if [[ $TARGET != "bd9576" ]] && [[ $TARGET != "bd71837" ]] && [[ $TARGET != "bd71847" ]] && [[ $TARGET != "bd71850" ]] && [[ $TARGET != "bd71815" ]]

then
	echo "Unknown target \'$TARGET\'"
	exit 1
fi

RUNSCRIPT="$DIR/test.expect"

$RUNSCRIPT $TARGET
if [[ $? -ne 0 ]]
then
	echo "Failed to run tests for $TARGET"
	exit -1
else
	echo "Tests ran for $TARGET"
fi

exit 0
