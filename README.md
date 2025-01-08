<p align="center">
<h1 align="center">Spinner</h1>
    <h5 align="center">Enabling In-network Flow Clustering Entirely in a Programmable Data Plane</h5>
    <p align="center">
        <a href="#overview">Overview</a> &#183;
        <a href="#compilation">Compilation</a> &#183;
        <a href="#loading">Loading</a> &#183;
        <a href="#usage">Usage</a>
    </p>
</p>


## Overview
Spinner is a flow clustering algorithm designed to fit existing architectural constraints of SmartNICs, and that can reach line rate for most packet sizes with complexity O(k).

The figure below llustrates the algorithm’s main idea.

![clustering-1](https://github.com/thiagobmi/Spinner-noms/assets/118558122/af3019e6-6eb0-4bbb-b677-ecbb30115d58)

## Compilation 
To perform the compilation via the command line in Linux, the following command was used:

```
$ P4C_BIN_DIR/nfp4build --output-nffw-filename $PATH_EXP/code/firmware.nffw -4 $PATH_EXP/spinner.p4 -c $PATH_EXP/spinner.c \
--sku nfp-4xxx-b0 --platform hydrogen --reduced-thread-usage --shared-codestore --debug-info --nfp4c_p4_version 16 \
--nfp4c_p4_compiler p4c-nfp --nfirc_default_table_size 65536 --nfirc_no_all_header_ops --nfirc_implicit_header_valid \
--nfirc_no_zero_new_headers --nfirc_multicast_group_count 16 --nfirc_multicast_group_size 16 -DPIF_PLUGIN_INIT \
```
- **options:**
  - ```-DPIF_PLUGIN_INIT```: initializes and enables the use of system calls  ```pif_plugin_init_master() { ... }``` e ```pif_plugin_init() {}```
  - ```plugin="$PATH_EXP/spinner.c"``` : compiles with Spinner's micro-c plugin;

## Loading

The following command was used to load the driver:
```
$ P4C_BIN_DIR/rtecli -p 20206 design-load -f $PATH_EXP/code/firmware.nffw -c $PATH_EXP/myconfig.p4cfg
```
- **options:**
  - ```-c $PATH_EXP/myconfig.p4cfg```: sets the custom table passed as argument as the default table.
 
## Usage
### Server 3
For practical purposes, the compilation and firmware loading processes for the Spinner have been streamlined into three main scripts.
Therefore, after configuring your environment and installing the appropriate smartNIC drivers, the Spinner can be utilized by following the steps below:

1. Delete the old firmware and configuration files:
> ```
> $ ./clean.sh
> ```
  
2. Compile the Spinner code:
> ```
> $ ./build.sh
> ```

3. Finally, load the firmware onto the smartNIC:
> ```
> $ ./load.sh
> ```

After being loaded onto the SmartNIC, Spinner will automatically initiate the process of identifying and clustering packet flows passing through the board.

### Server 1
The generation of massive traffic was achieved using the MoonGen traffic generator. The following command was employed:


- **-tx/-rx**: the source and destination ports;
- **--src-ip/--dst-ip**: the source and destination IPs;
- **-sipv/-dipv**: the presence of variation (0.0.0.1) or absence (0.0.0.0) in the source/destination IPs;
- **--pkt-size**: the packet size in bytes;
- **-pps**: the number of packets sent per second.

```
/opt/MoonGen/build/MoonGen /opt/MoonGen/examples/netronome-packetgen/packetgen.lua \
-tx 1 -rx 1  -sipv 10.0.1.10 --sipv 0.0.0.0 --src-ip 10.0.1.11 \
 --src-ip-vary 10.10.10.10  --pkt-size [X] -pps [Y] \
```

### Server 2
On server 2, we created two namespaces, one for receiving packets and another for sending. Subsequently, we employed iperf3 to generate legitimate TCP traffic.


- **-s**: execute in server mode.
- **-c**: execute in client mode.
- **-t**: the duration in seconds.

```
ip netns exec receiver iperf3 -s &
ip netns exec sender iperf3 -c 30.30.30.2 -t 55
```

## Cite
```
@INPROCEEDINGS{10575413,
  author={Cannarozzo, Luigi and Morais, Thiago Bortoluzzi and de Souza, Paulo Silas Severo and Gobatto, Leonardo Reinehr and Lamb, Ivan Peter and Duarte, Pedro Arthur P. R. and Azambuja, José Rodrigo Furlanetto and Lorenzon, Arthur Francisco and Rossi, Fábio Diniz and Cordeiro, Weverton and Luizelli, Marcelo Caggiani},
  booktitle={NOMS 2024-2024 IEEE Network Operations and Management Symposium}, 
  title={Spinner: Enabling In-network Flow Clustering Entirely in a Programmable Data Plane}, 
  year={2024},
  volume={},
  number={},
  pages={1-9},
  keywords={Performance evaluation;Machine learning algorithms;Decision making;Prototypes;Clustering algorithms;Machine learning;Throughput;P4;in-network clustering;SmartNICs},
  doi={10.1109/NOMS59830.2024.10575413}}
```
