#!/bin/bash

# Compile and install the module
make
sudo make load

# Install a device always present after installation for testing and debugging purpose
major=$( cat /proc/devices | grep mailslot | cut -d " " -f 1 )
minor=100
sudo mknod -m 666 /dev/test_dev c $major $minor

# See if the module has been correctly inserted
dmesg

