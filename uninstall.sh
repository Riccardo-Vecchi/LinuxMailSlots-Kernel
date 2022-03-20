#!/bin/bash

# Remove the testing device
sudo rm /dev/test_dev

# Uninstall the module and clear the directory
sudo make unload
make clean

# See if the module has been correctly deleted
dmesg

