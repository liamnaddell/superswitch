all:
	g++ sendp.cpp -o sendp
	p4c -b bmv2 switch.p4 -o switch.bmv2
