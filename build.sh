#!/bin/bash
set -xe

export P4C_BIN_DIR="/opt/netronome/p4/bin"
export PATH_EXP=`realpath ./src/`

rm -rf code Makefile-nfp4build wire-eno1.pcapng *.txt
$P4C_BIN_DIR/nfp4build --output-nffw-filename $PATH_EXP/code/firmware.nffw -4 $PATH_EXP/spinner.p4 -c $PATH_EXP/spinner.c --sku nfp-4xxx-b0 --platform hydrogen --reduced-thread-usage  --shared-codestore --debug-info --nfp4c_p4_version 16 --nfp4c_p4_compiler p4c-nfp --nfirc_default_table_size 65536 --nfirc_no_all_header_ops --nfirc_implicit_header_valid --nfirc_no_zero_new_headers --nfirc_multicast_group_count 16 --nfirc_multicast_group_size 16 -DPIF_PLUGIN_INIT
