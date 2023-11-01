#!/bin/bash
set -xe

export P4C_BIN_DIR="/opt/netronome/p4/bin"
# !!!!!! Do not forget to modify this path whenever you move this file !!!!!! #
#export PATH_EXP="/root/documentation/clustering"
export PATH_EXP=`realpath .`

# bataan,hydrogen,starfighter1,lithium,beryllium,carbon
# Works with: hydrogen lithium beryllium carbon
#-c $PATH_EXP/v2_heavyHitter.c
#--------------------------heavyHitter version--------------------------#

#A lot of problem happen if we let cache files so try to supress them before building, clean.sh script do it alone.
rm -rf code Makefile-nfp4build wire-eno1.pcapng *.txt
$P4C_BIN_DIR/nfp4build --output-nffw-filename $PATH_EXP/code/firmware.nffw -4 $PATH_EXP/clustering.p4 -c $PATH_EXP/clustering.c --sku nfp-4xxx-b0 --platform hydrogen --reduced-thread-usage  --shared-codestore --debug-info --nfp4c_p4_version 16 --nfp4c_p4_compiler p4c-nfp --nfirc_default_table_size 65536 --nfirc_no_all_header_ops --nfirc_implicit_header_valid --nfirc_no_zero_new_headers --nfirc_multicast_group_count 16 --nfirc_multicast_group_size 16 -DPIF_PLUGIN_INIT
