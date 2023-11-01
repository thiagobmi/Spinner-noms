#!/bin/bash

export P4C_BIN_DIR="/opt/netronome/p4/bin"
export PATH_EXP=`realpath .`

$P4C_BIN_DIR/rtecli -p 20206 design-load -f $PATH_EXP/code/firmware.nffw -c $PATH_EXP/myconfig.p4cfg
