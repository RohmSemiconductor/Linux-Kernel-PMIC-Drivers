#!/bin/bash

modprobe bd71827-power

mkdir /sys/kernel/config/device-tree/overlays/bd71815-simple-gauge
cat /intree-dtbos/bd71815_bbb_gauge_test.dtbo > /sys/kernel/config/device-tree/overlays/bd71815-simple-gauge/dtbo

