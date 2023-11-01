
/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

extern void do_clustering();


/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

typedef bit<9>  egressSpec_t;
typedef bit<48> macAddr_t;
typedef bit<32> ip4Addr_t;

header ethernet_t {
    macAddr_t dstAddr;
    macAddr_t srcAddr;
    bit<16>   etherType;
}

header ipv4_t {
    bit<4>    version;
    bit<4>    ihl;
    bit<6>    diffserv;
    bit<2>    ecn;
    bit<16>   totalLen;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   fragOffset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdrChecksum;
    ip4Addr_t srcAddr;
    ip4Addr_t dstAddr;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4>  dataOffset;
    bit<4>  res;
    bit<8>  flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header udp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> length_;
    bit<16> checksum;
}

header debug_t {
    bit<32> v1;
    bit<32> v2;
    bit<32> v3;
    bit<32> v4;
    bit<32> v5;
    bit<32> v6;
    bit<32> v7;
    bit<32> v8;
    bit<32> v9;
    bit<32> v10;    
}


header intrinsic_metadata_t {
	bit<64> ingress_global_timestamp;
    bit<64> current_global_timestamp;
}

struct metadata {
	intrinsic_metadata_t intrinsic_metadata;
}

/*************** TEST ***************/
header print_t {
	bit<16> index;
	bit<32> label;
  bit<32> ip1;
  bit<32> ip2;
  bit<8> protocol;
}
// register<bit<64>>(10) myRegister;
register<bit<32>>(5000) flow_label;
// register<bit<32>>(5000) flow_ip1;
// register<bit<32>>(5000) flow_ip2;
// register<bit<8>>(5000) flow_protocol;
register<bit<32>>(10) debug_reg;


/************************************/

struct headers {
    ethernet_t  ethernet;
    ipv4_t      ipv4;
    tcp_t       tcp;
    udp_t       udp;
/*************** TEST ***************/
    print_t       print;
/************************************/
    debug_t       debug;
    intrinsic_metadata_t    intrinsic_metadata;

}


const bit<16> TYPE_IPV4 = 0x800;
const bit<8> PROTO_TCP = 8w6;
const bit<8> PROTO_UDP = 8w17;
/*************************************************************************
****************************** P A R S E R  ******************************
*************************************************************************/


parser MyParser(packet_in packet,
                out headers hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {

    state start {
	    transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            TYPE_IPV4   : parse_ipv4;
        }
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            PROTO_TCP   : parse_tcp;
            PROTO_UDP   : parse_udp;
        }
    }

    state parse_tcp {
        packet.extract(hdr.tcp);
        transition accept;
    }

    state parse_udp {
        packet.extract(hdr.udp);
        transition accept;
    }
}

/*************************************************************************
************   C H E C K S U M    V E R I F I C A T I O N   *************
*************************************************************************/

control MyVerifyChecksum(inout headers hdr, inout metadata meta) {
    apply {  }
}


/*************************************************************************
*****************  I N G R E S S   P R O C E S S I N G   *****************
*************************************************************************/

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    action forward(egressSpec_t port) {
        standard_metadata.egress_spec = (bit<16>)port;
        /*************** TEST ***************/
		// myRegister.write(1, hdr.addi.meta2);
        /************************************/
        do_clustering();
        //TODO verify if it's 0
        flow_label.write((bit<32>)hdr.print.index, hdr.print.label);
        debug_reg.write((bit<32>)0, (bit<32>)hdr.debug.v1);
        debug_reg.write((bit<32>)1, (bit<32>)hdr.debug.v2);
        debug_reg.write((bit<32>)2, (bit<32>)hdr.debug.v3);
        debug_reg.write((bit<32>)3, (bit<32>)hdr.debug.v4);
        debug_reg.write((bit<32>)4, (bit<32>)hdr.debug.v5);
        debug_reg.write((bit<32>)5, (bit<32>)hdr.debug.v6);
        debug_reg.write((bit<32>)6, (bit<32>)hdr.debug.v7);
        debug_reg.write((bit<32>)7, (bit<32>)hdr.debug.v8);
        debug_reg.write((bit<32>)8, (bit<32>)hdr.debug.v9);
        debug_reg.write((bit<32>)9, (bit<32>)hdr.debug.v10);

        //flow_ip1.write((bit<32>)hdr.print.index, hdr.print.ip1);
        //flow_ip2.write((bit<32>)hdr.print.index, hdr.print.ip2);
        //flow_protocol.write((bit<32>)hdr.print.index, hdr.print.protocol);
    }

    table ip_forward {
        key = {
            standard_metadata.ingress_port : exact;
        }
        actions = {
            NoAction;
            forward;
        }
        default_action = NoAction;
    }

    apply {
                           
        hdr.debug.setValid();
        hdr.debug.v1 = hdr.intrinsic_metadata.ingress_global_timestamp[63:32];
        hdr.debug.v2 = hdr.intrinsic_metadata.ingress_global_timestamp[31:0];
        hdr.debug.v3 = hdr.intrinsic_metadata.current_global_timestamp[63:32];
        hdr.debug.v4 = hdr.intrinsic_metadata.current_global_timestamp[31:0];
        hdr.debug.v5 = hdr.intrinsic_metadata.current_global_timestamp[63:32] - hdr.intrinsic_metadata.ingress_global_timestamp[63:32];
        hdr.debug.v6 = 0;
        hdr.debug.v7 = 0;
        hdr.debug.v8 = 0;
        hdr.debug.v9 = 0;
        hdr.debug.v10 = 0;
        ip_forward.apply();
        hdr.debug.setInvalid();

    }
}


/*************************************************************************
******************  E G R E S S   P R O C E S S I N G   ******************
*************************************************************************/

control MyEgress(inout headers hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {

    apply {  }
}

/*************************************************************************
**************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control MyComputeChecksum(inout headers hdr, inout metadata meta) {
    apply {
        update_checksum(
            hdr.ipv4.isValid(),
            {
                hdr.ipv4.version,
                hdr.ipv4.ihl,
                hdr.ipv4.diffserv,
		        hdr.ipv4.ecn,
                hdr.ipv4.totalLen,
                hdr.ipv4.identification,
                hdr.ipv4.flags,
                hdr.ipv4.fragOffset,
                hdr.ipv4.ttl,
                hdr.ipv4.protocol,
                hdr.ipv4.srcAddr,
                hdr.ipv4.dstAddr
            },
                hdr.ipv4.hdrChecksum,
                HashAlgorithm.csum16
        );
    }
}


/*************************************************************************
***************************  D E P A R S E R  ****************************
*************************************************************************/

control MyDeparser(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.tcp);
        packet.emit(hdr.udp);
        packet.emit(hdr.print);
        packet.emit(hdr.debug);
    }
}

/*************************************************************************
*****************************  S W I T C H  ******************************
*************************************************************************/

V1Switch(
    MyParser(),
    MyVerifyChecksum(),
    MyIngress(),
    MyEgress(),
    MyComputeChecksum(),
    MyDeparser()
) main;
