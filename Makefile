all:
	p4c -b bmv2 switch.p4 -o switch.bmv2
	g++ -g -Wall -Werror sendp.cpp -o sendp
switch:
	simple_switch --interface 0@veth0 --interface 1@veth2 --interface 2@veth4 switch.bmv2/switch.json
