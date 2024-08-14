#include <core.p4>
#include <v1model.p4>

//TODO: Formatting.

typedef bit<48> EthernetAddress;
typedef bit<32> IPv4Address;
typedef bit<128> IPv6Address;

header ethernet_t {
    EthernetAddress dst_addr;
    EthernetAddress src_addr;
    bit<16>         ether_type;
}

//TODO: Field naming.
header ipv4_t {
    bit<4>      version;
    bit<4>      ihl;
    bit<8>      diffserv;
    bit<16>     total_len;
    bit<16>     identification;
    bit<3>      flags;
    bit<13>     frag_offset;
    bit<8>      ttl;
    bit<8>      protocol;
    bit<16>     hdr_checksum;
    IPv4Address src_addr;
    IPv4Address dst_addr;
}

header ipv6_t {
    bit<4> version;
    bit<8> traffic_class;
    bit<20> flow_label;
    bit<16> payload_length;
    bit<8> next_header;
    bit<8> hop_limit;
    IPv6Address source_address;
    IPv6Address dest_address;
}

header packet_data {
    varbit<256> data;
}

struct headers_t {
    ethernet_t ethernet;
    ipv4_t      ipv4;
    ipv6_t      ipv6;
    packet_data data;
}


struct metadata_t {
}

//TODO: Invalid checksum.
error {
    IPv4IncorrectVersion,
    IPv4OptionsNotSupported
}

parser my_parser(packet_in packet,
                out headers_t hd,
                inout metadata_t meta,
                inout standard_metadata_t standard_meta)
{
    state start {
        packet.extract(hd.ethernet);
        transition select(hd.ethernet.ether_type) {
            0x0800:  parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        packet.extract(hd.ipv4);
        verify(hd.ipv4.version == 4w4, error.IPv4IncorrectVersion);
        verify(hd.ipv4.ihl == 4w5, error.IPv4OptionsNotSupported);
	//TODO: A better place to put this?
	hd.ethernet.ether_type = 0x86DD;
	hd.ipv6.setValid();
	hd.ipv6.version = 4w6;
	hd.ipv6.traffic_class = hd.ipv4.diffserv;
	hd.ipv6.flow_label = 0;
	hd.ipv6.payload_length = hd.ipv4.total_len - 20;
	hd.ipv6.next_header = hd.ipv4.protocol;
	hd.ipv6.hop_limit = hd.ipv4.ttl;
	hd.ipv6.source_address = (bit<128>) hd.ipv4.src_addr << 96;
	hd.ipv6.dest_address = (bit<128>) hd.ipv4.dst_addr << 96;

        transition accept;
    }
    state parse_data {
	//Extract packet data
	packet.extract(hd.data,(bit<32>) hd.ipv6.payload_length);
        transition accept;
    }
}

control my_deparser(packet_out packet,
                   in headers_t hdr)
{
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv6);
	# TODO: Data.
    }
}

control my_verify_checksum(inout headers_t hdr,
                         inout metadata_t meta)
{
    //TODO:
    apply { }
}

control my_compute_checksum(inout headers_t hdr,
                          inout metadata_t meta)
{
    apply {
	update_checksum(true,hdr.ipv4,hdr.ipv4.hdr_checksum,HashAlgorithm.csum16);
    }
}

control my_ingress(inout headers_t hdr,
                  inout metadata_t meta,
                  inout standard_metadata_t standard_metadata)
{
    bool dropped = false;

    action drop_action() {
        mark_to_drop(standard_metadata);
        dropped = true;
    }

    action to_port_action(bit<9> port) {
        hdr.ipv4.ttl = hdr.ipv4.ttl - 1;
        standard_metadata.egress_spec = port;
    }

    table ipv4_match {
        key = {
            hdr.ipv4.dst_addr: lpm;
        }
        actions = {
            drop_action;
            to_port_action;
        }
        size = 1024;
        default_action = drop_action;
    }

    apply {
        ipv4_match.apply();
        if (dropped) return;
    }
}

control my_egress(inout headers_t hdr,
                 inout metadata_t meta,
                 inout standard_metadata_t standard_metadata)
{
    apply { }
}

V1Switch(my_parser(),
         my_verify_checksum(),
         my_ingress(),
         my_egress(),
         my_compute_checksum(),
         my_deparser()) main;

