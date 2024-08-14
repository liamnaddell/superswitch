# P4: A language for computers which do not use CPU's or RAM.

P4's flashy website does not do justice to how amazing it's concepts are. P4 is
a beautiful expression of a world where optimization was pushed so far, the
concept of CPU and RAM became obsolete. 

P4 is a DSL for these strange machines. 

## Understanding P4's execution context.

P4 is blandly known as a "packet processing engine". It's a language for
parsing packet headers, looking up relevant fields in tables, and pushing those
packets to interfaces "ports". Other words used to describe it is a "macro
engine". 

Because the ideas expressible in P4 are so limited, P4 is used as the language
for an "NPU" or network processing unit. These devices are programmed
(microcoded) using P4. 

### NPU

An NPU is loaded with a P4 program, and is wired to incoming and outgoing
interfaces. This NPU will see ingress packets, perform the packet processing,
and egress the packets, without a CPU involved.

### TCAM

TCAM, or "Ternary Content Accessible Memory" is like RAM, but instead of
addressing by the *address* (index) of memory in a cell, you address by the 
*data* (value) in the cell. The "ternary" part means you can ignore certain
parts of memory when looking up data.

For example, Lets say I have the following bits in memory:

```
000000
010100
101110
111111
```

I could search TCAM for `1????0`, and I would get back `101110`. If you're like
me, at this point, you will notice this is essentially a hash map, implemented
with hardware, where the keys are bitmasks, and be absolutely amazed. 


### It gets better...

It's not just a hash map, implemented in hardware, using bitmasks to search up
keys. If you squint at it, it's actually also a database engine, implemented in
hardware, using bitmasks as keys. 

Instead of thinking in terms of memory cells and rows, what if instead, you
thought of 

```
000000
010100
101110
111111
```

as a TABLE of (KEY,PAIR) values WHERE you can SEARCH by defining KEYS.

Even better, consider this table of ip addresses and destination mac addresses.
We might use this table for storing the results of ARP queries. 

```
;schema
;uint32_t ip4 |   uint48_t mac    | uint8_t port
192.168.2.1   | de:ad:be:ef:00:00 |       1
10.0.0.1      | fe:ee:df:00:00:0d |       2
...
```

We could apply the binary mask 0xc0a80201?????????????? to search for the 48
bit mac address, and interface to use for an incoming packet with destination address 192.168.2.1. 

## Actually USING p4. 

Unfortunatly, acquiring a real P4 processor is really hard and super expensive.
In fact, even as a current cisco employee working close to the NPU, I don't
actually get to see / touch one. 

Most of what I know comes from a HAL used for writing and testing microservice based driver
middleware for a feature which are mostly written about in the strange
english-adjacent language Cisco employees use to obfuscate ideas to
outsiders.

As such, we will use BMV2. A software implementation of the P4 runtime, which
is way easier to use, but considerably less cool.


I enabled the debugger for bmv2 using --enable-debugger


But I'm not super happy with p4's behavioral model. It's kinda annoying to have
to deal with real interfaces / ports. 

What I really want is an abstract model, maybe something else can satisfy my
needs for a locally hosted, yet mostly abstract p4 implementation.

Petr4 seems not super well maintained, and "web interface" isn't exactly what
I'm looking for. Plus the readme doesn't explain how to route packets through
the Petr4 model, so I'm gonna assume you can't. 

In any case, let's try https://gist.github.com/austinmarton/1922600 next, and
go with the behavioral model.

I used a hex packet decoder to debug my C packet sender. I also ran into some
bizzare struct layout / data packing BS, no idea what's going on there.

This hex packet decoder ended up being full of lies though:
https://hpd.gasmi.net/

They tricked me into thinking we don't htons() ethertype, but we do...
-1 hr in the drain :(. I then got tricked by hexdump and fwrite into
beleiving that I needed to subtract one from an ip packet in order for it to be
valid. I have absolutely no idea why the hell hexdump + fwrite caused an extra
byte to be injected into my packet, but o-well :).

After way too much work, I finally got my first packet!

```
liam@gentoo ~ $ sudo tcpdump -xx -v -n -i veth0
dropped privs to pcap
tcpdump: listening on veth0, link-type EN10MB (Ethernet), snapshot length 262144 bytes
19:49:39.127662 IP truncated-ip - 1004 bytes missing! (tos 0x0, ttl 1, id 0, offset 0, flags [none], proto Options (0), length 1024,
 bad cksum b6ff (->e932)!)
    170.170.170.170 > 187.187.187.187:  ip-proto-0 1004
        0x0000:  0708 090a 0b0c 0102 0304 0506 0800 4500
        0x0010:  0400 0000 0000 0100 b6ff aaaa aaaa bbbb
        0x0020:  bbbb
1 packet captured
1 packet received by filter
0 packets dropped by kernel
```

This was 100% worth it for avoiding scapy though. Any torture is preferable to
scapy. It's really that bad. I still have bad cksum, but the length was
fixable. 

Ok, now it's time to ACTUALLY fix the checksum. This was a real joy /s. I found
an article which contains a C implementation from RFC 1071. This is an RFC on
how to compute the checksum (mostly in assembly) on the fascinating CPU
architectures of Motorola 68020, and Cray (yes, the supercomputer). Luckily
they contain a (typo's included) implementation for C. Thanks IETF, I can't
imagine how that one got in there.

I tried this, but it didn't work, so I dumped the memory of the inbound packet
in GDB, and saw the bit order was reversed, so I un-reversed all of the bit
orders coming in, *then*, after fixing another length problem sizeof(\*this) vs
sizeof(this). I saw that my checksums were conspicuously off by exactly 0x100. 

Luckily, I saw a little article snippet about how you can "fast compute"
checksums when decrementing the TTL by adding 0x100 to the sum. Sounds like a
TTL error to me! And would you look, the test.p4 file doesn't change the
checksum after decrementing TTL. Onto the next problem matee!!!


Ok, we need to actually write some P4 now! I found an article which explains to
call the `update_checksum` function, so I figured out the arguments, and wrote
it up, and got... 100.

Which is actually super close. My only mistake was including the previous
checksum in the checksum, which when you checksum a checksum, causes the
checksum to zero, since thats how checksums checksum. 

Ok, But I tried setting the checksum to zero before computing, and...
Apparently... p4 only lets you call a couple functions in the "update checksum"
context, namely, "update_checksum". So that's annoying. So I just moved the
"checksum = zero" logic to the ingress parser, which works BRILLIANTLY. No more
checksum issues :)) :0. I finally produced a valid IPv4 packet!

## Ok, what now?

Writing a real packet switch would be super time consuming, and not very 
entertaining. Plus, I've already done it before for a school project. I think I
need something just a little bit stupider.

I'm thinking of creating a switch that converts IPv4 packets into IPv6 packets.
In this manor, we'd convert the ipv4 source and dest addrs into ipv6 source and
dest, with prefix <ip4>::0. ~~I bet I could become a millionare if I market
this as a "SRV6 enabled super switch"~~. 

This shouldn't be too difficult, since for the most part, ipv4 and ipv6 have
similar-ish header structures. IPv6 even saves us the cksum calculation. Happy
days!

So, I added an ipv6 header to my `headers_t` struct, then added some logic for
converting ipv4 into ipv6 in my `parse_ipv4` function.

Apparently headers are structs + validity. So we need to set the validity of
our ipv6 header.
