#!/bin/bash

source ../../configs/jenkins-env-config
source ../../configs/jenkins-server-config

$RUNCFG_JENKINS_SCRIPT_PATH/paalaa.sh $*
