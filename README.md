# What is this?

This is the microcode for a forwarding ASIC written in p4 that converts ipv4 packets into ipv6 packets.

## Dependencies

In order to build+install, you need p4c and bmv2 installed and available in $PATH.

## How to run.

* Run `make` to build the switch, and run ./eth to set up the virtual ethernet ports. Then, run `make switch` to start the switch process.
* Use `simple_switch_CLI`, then enter `table_add ipv4_match to_port_action 187.187.0.0/16 => 2` to add a route. 
* Use `tcpdump -xx -i veth2` to check the output of virtual port 1. Then, run ./sendp to send traffic to the router!
* You can check port1, the ingress port using tcpdump to watch the ipv4 packet coming in, and the egressing packet should be an IPv6 packet with an identical payload!
