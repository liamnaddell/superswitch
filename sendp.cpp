/* GPL v3 */
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cassert>


struct ether {
public:
  ether (uint8_t dst[6], uint8_t src[6], uint16_t ethertype);
private:
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
};

ether::ether (uint8_t dst[6], uint8_t src[6], uint16_t ethertype) {
    memcpy(this->dst,dst,6);
    memcpy(this->src,src,6);
    this->ethertype = htons(ethertype);
}

struct ip4 {
    /* 4 and 4 */
    uint8_t version_ihl;
    uint8_t dscp;
    uint16_t len;
    uint16_t ident;
    /* flags and frag_offset 3 and 13 */
    uint16_t flags_frag_ofs;
    uint8_t ttl;
    uint8_t proto;
    uint16_t cksum;
    uint32_t src;
    uint32_t dst;
public:
    ip4(uint8_t dscp, uint16_t len, 
	uint16_t ident, uint16_t flags_frag_ofs, uint8_t ttl,
	uint8_t proto, uint32_t src, uint32_t dst);
};


#if 0
static uint16_t
in_cksum(uint16_t *addr, int count)
{
  uint32_t sum = 0;
  while (count > 1) {
      sum += * addr++;
      count -= 2;
  }

  if (count > 0)
    sum += * (unsigned char *) addr;

  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);
  uint16_t checksum = ~sum;

  return checksum;
}
#endif

static uint16_t
in_cksum(uint16_t *addr, int len)
{
    int nleft = len;
    uint16_t *w = addr;
    uint32_t sum = 0;
    uint16_t answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += htons(*w++);
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(u_char *)(&answer) = *(u_char *)w ;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}

ip4::ip4(uint8_t dscp, uint16_t len, 
    uint16_t ident, uint16_t flags_frag_ofs, uint8_t ttl,
    uint8_t proto, uint32_t src, uint32_t dst): dscp(dscp),
    len(htons(len)), ident(ident), flags_frag_ofs(flags_frag_ofs), 
    ttl(ttl), proto(htons(proto)),cksum(0),src(src),dst(dst)
{
  version_ihl=4<<4 | 5;
  uint16_t c_ck = in_cksum((uint16_t *) this,sizeof(*this));
  cksum=htons(c_ck);
}

#define BUF_SIZ		1024

int main(int argc, char *argv[]) {
    int sockfd;
    struct ifreq if_idx;
    struct ifreq if_mac;
    int tx_len = 0;
    char sendbuf[BUF_SIZ];
    struct sockaddr_ll socket_address;
    char ifName[IFNAMSIZ];
    strcpy(ifName,"veth0");

    uint8_t e_src[6] = {1,2,3,4,5,6};
    uint8_t e_dst[6] = {7,8,9,0xa,0xb,0xc};
    ether eth(e_dst,e_src,0x0800);
    ip4 ip(0,20,0,0,1,253,0xaaaaaaaa,0xbbbbbbbb);

    /* Open RAW socket to send on */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	perror("socket");
	return 1;
    }

    /* Get the index of the interface to send on */
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
	perror("SIOCGIFINDEX");
	return 1;
    }


    /* Get the MAC address of the interface to send on */
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
	perror("SIOCGIFHWADDR");
	return 1;
    }

    memcpy(sendbuf+tx_len,&eth,sizeof(ether));
    tx_len+=sizeof(ether);

    memcpy(sendbuf+tx_len,((char *) &ip),sizeof(ip4));
    tx_len+=sizeof(ip4);

    FILE *pkt_dsk = fopen("pkt","wb");
    assert(pkt_dsk != 0);

    fwrite(sendbuf,1,tx_len,pkt_dsk);
    fclose(pkt_dsk);


    /* Index of the network device */
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    /* Address length*/
    socket_address.sll_halen = ETH_ALEN;

    memcpy(socket_address.sll_addr,e_dst,6);

    /* Send packet */
    if (sendto(sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	printf("Send failed\n");

    return 0;
}
