ip addr add 192.168.255.126/24 dev enp0s25
ip link set enp0s25 up
systemctl start tftp
systemctl start nfs-server
