#!/bin/bash

I=0

for IFACE in "${CFG_ETH_IF_TARGETS[@]}"
do
	ip addr add ${CFG_ETH_IF_SERVER_IPS[$I]} dev $IFACE
	ip link set $IFACE up
	I=$(($I + 1))
done

#ip addr add 192.168.255.126/24 dev $CFG_ETH_IF_TARGETS
#ip link set enp0s25 up
systemctl restart tftp
systemctl restart nfs-server
