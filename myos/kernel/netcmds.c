#include "netcmds.h"
#include "network.h"
#include "screen.h"
#include "string.h"
#include "timer.h"
#include "ports.h"

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0
#define PING_PAYLOAD_SIZE 32
#define PING_TIMEOUT_LOOPS 300
#define PING_GAP_LOOPS 100

static void print_ip(const uint8_t ip[4]) {
    char buf[4];

    for (int i = 0; i < 4; i++) {
        itoa(ip[i], buf, 10);
        print(buf);
        if (i < 3)
            print(".");
    }
}

static void print_mac(const uint8_t mac[6]) {
    char buf[3];

    for (int i = 0; i < 6; i++) {
        uint8_t b = mac[i];

        itoa((b >> 4) & 0xF, buf, 16);
        print(buf);

        itoa(b & 0xF, buf, 16);
        print(buf);

        if (i < 5)
            print(":");
    }
}

static void print_dec(int n) {
    char buf[12];
    itoa(n, buf, 10);
    print(buf);
}

static int parse_ip(const char *s, uint8_t out[4]) {
    int octet = 0;
    int idx = 0;
    int digits = 0;

    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (*s - '0');
            digits++;

            if (octet > 255 || digits > 3)
                return 0;
        } else if (*s == '.') {
            if (idx >= 3 || digits == 0)
                return 0;

            out[idx++] = (uint8_t)octet;
            octet = 0;
            digits = 0;
        } else {
            return 0;
        }
    }

    if (idx != 3 || digits == 0)
        return 0;

    out[idx] = (uint8_t)octet;
    return 1;
}

static int build_ip_frame(uint8_t *buf,
                          const uint8_t dst_ip[4],
                          uint8_t proto,
                          const void *payload,
                          uint16_t payload_len) {
    network_device_t *dev = get_network_device();

    ip_addr_t dst;
    mac_addr_t dst_mac;

    memcpy(dst.addr, dst_ip, IP_ALEN);

    if (!network_resolve_mac(&dst, &dst_mac))
        return -1;

    eth_header_t *eth = (eth_header_t *)buf;

    memcpy(eth->dest, dst_mac.addr, ETH_ALEN);
    memcpy(eth->src, dev->mac_addr.addr, ETH_ALEN);
    eth->type = htons(ETH_TYPE_IP);

    ip_header_t *ip =
        (ip_header_t *)(buf + sizeof(eth_header_t));

    uint16_t ip_total =
        (uint16_t)(sizeof(ip_header_t) + payload_len);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(ip_total);
    ip->identification = htons(0x1234);
    ip->flags_fragment = htons(0);
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;

    memcpy(ip->src_ip, dev->ip_addr.addr, IP_ALEN);
    memcpy(ip->dest_ip, dst_ip, IP_ALEN);

    ip->checksum =
        calculate_checksum(ip, sizeof(ip_header_t));

    memcpy(buf + sizeof(eth_header_t) + sizeof(ip_header_t),
           payload,
           payload_len);

    return (int)(sizeof(eth_header_t) + ip_total);
}

static uint16_t ping_seq = 0;
static const uint16_t PING_ID = 0x4A50;

static uint16_t build_icmp_echo(uint8_t *icmp_buf, uint16_t seq) {
    icmp_header_t *hdr =
        (icmp_header_t *)icmp_buf;

    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(PING_ID);
    hdr->seq = htons(seq);

    uint8_t *payload =
        icmp_buf + sizeof(icmp_header_t);

    for (int i = 0; i < PING_PAYLOAD_SIZE; i++)
        payload[i] = (uint8_t)('A' + (i % 26));

    uint16_t total =
        (uint16_t)(sizeof(icmp_header_t) +
                   PING_PAYLOAD_SIZE);

    hdr->checksum =
        calculate_checksum(icmp_buf, total);

    return total;
}

void cmd_ping(int argc, char *args[]) {
    if (argc < 2) {
        print("Usage: ping <ip_address> [count]\n");
        print("  e.g. ping 10.0.2.2\n");
        print("       ping 10.0.2.2 4\n");
        return;
    }

    if (!is_network_initialized()) {
        print("ping: network not initialized\n");
        return;
    }

    uint8_t target_ip[4];

    if (!parse_ip(args[1], target_ip)) {
        print("ping: invalid IP address '");
        print(args[1]);
        print("'\n");
        return;
    }

    int count = 4;

    if (argc >= 3) {
        count = 0;

        for (char *p = args[2];
             *p >= '0' && *p <= '9';
             p++) {
            count = count * 10 + (*p - '0');
        }

        if (count < 1 || count > 20)
            count = 4;
    }

    print("PING ");
    print_ip(target_ip);
    print(": ");
    print_dec(PING_PAYLOAD_SIZE);
    print(" bytes of data\n");

    int sent = 0;
    int received = 0;
    int total_ms = 0;

    for (int i = 0; i < count; i++) {
        uint16_t seq = ++ping_seq;

        uint8_t icmp_chunk[
            sizeof(icmp_header_t) + PING_PAYLOAD_SIZE
        ];

        uint16_t icmp_len =
            build_icmp_echo(icmp_chunk, seq);

        uint8_t frame[1514];

        int frame_len =
            build_ip_frame(frame,
                           target_ip,
                           IP_PROTO_ICMP,
                           icmp_chunk,
                           icmp_len);

        if (frame_len < 0) {
            print("ping: could not build frame\n");
            break;
        }

        if (network_send_packet(frame,
                                (size_t)frame_len) != 0) {
            print("ping: transmit failed\n");
            break;
        }

        sent++;

        int reply_ok = 0;
        int elapsed_loops = 0;

        for (int attempt = 0;
             attempt < PING_TIMEOUT_LOOPS;
             attempt++) {

            uint8_t rx_buf[1514];

            int rx_len =
                network_receive_packet(rx_buf,
                                       sizeof(rx_buf));

            if (rx_len <= 0) {
                asm volatile("pause");
                continue;
            }

            eth_header_t *re =
                (eth_header_t *)rx_buf;

            uint16_t eth_type =
                ntohs(re->type);

            if (eth_type == ETH_TYPE_ARP) {
                handle_arp_packet(rx_buf, rx_len);
                continue;
            }

            if (eth_type != ETH_TYPE_IP)
                continue;

            if (rx_len <
                (int)(sizeof(eth_header_t) +
                      sizeof(ip_header_t) +
                      sizeof(icmp_header_t)))
                continue;

            ip_header_t *rip =
                (ip_header_t *)(rx_buf +
                                sizeof(eth_header_t));

            if ((rip->ver_ihl >> 4) != 4)
                continue;

            if (rip->protocol != IP_PROTO_ICMP)
                continue;

            if (memcmp(rip->src_ip,
                       target_ip,
                       IP_ALEN) != 0)
                continue;

            network_device_t *dev =
                get_network_device();

            if (memcmp(rip->dest_ip,
                       dev->ip_addr.addr,
                       IP_ALEN) != 0)
                continue;

            uint8_t ip_hlen =
                (uint8_t)((rip->ver_ihl & 0x0F) * 4);

            if (ip_hlen < 20)
                continue;

            if (rx_len <
                (int)(sizeof(eth_header_t) +
                      ip_hlen +
                      sizeof(icmp_header_t)))
                continue;

            icmp_header_t *ric =
                (icmp_header_t *)(rx_buf +
                                  sizeof(eth_header_t) +
                                  ip_hlen);

            if (ric->type != ICMP_ECHO_REPLY)
                continue;

            if (ric->code != 0)
                continue;

            if (ntohs(ric->id) != PING_ID)
                continue;

            if (ntohs(ric->seq) != seq)
                continue;

            reply_ok = 1;
            elapsed_loops = attempt;
            break;
        }

        if (reply_ok) {
            int elapsed_ms =
                elapsed_loops / 1000;

            received++;
            total_ms += elapsed_ms;

            print_dec(PING_PAYLOAD_SIZE + 8);
            print(" bytes from ");
            print_ip(target_ip);
            print(": icmp_seq=");
            print_dec(seq);
            print(" ttl=");
            print_dec(64);
            print(" time=");
            print_dec(elapsed_ms);
            print(" ms\n");
        } else {
            print("Request timeout for icmp_seq ");
            print_dec(seq);
            print("\n");
        }

        if (i + 1 < count) {
            for (volatile int d = 0;
                 d < PING_GAP_LOOPS;
                 d++)
                asm volatile("pause");
        }
    }

    int lost = sent - received;
    int loss_pct =
        (sent > 0) ? (lost * 100 / sent) : 0;

    print("\n--- ");
    print_ip(target_ip);
    print(" ping statistics ---\n");

    print_dec(sent);
    print(" packets transmitted, ");

    print_dec(received);
    print(" received, ");

    print_dec(loss_pct);
    print("% packet loss\n");

    if (received > 0) {
        print("avg rtt = ");
        print_dec(total_ms / received);
        print(" ms\n");
    }
}

void cmd_ifconfig(void) {
    if (!is_network_initialized()) {
        print("ifconfig: network not initialized\n");
        return;
    }

    network_device_t *dev =
        get_network_device();

    print("eth0      Link encap:Ethernet\n");

    print("          HWaddr ");
    print_mac(dev->mac_addr.addr);
    print("\n");

    print("          inet addr:");
    print_ip(dev->ip_addr.addr);
    print("  Mask:");
    print_ip(dev->netmask.addr);
    print("\n");

    print("          Gateway:");
    print_ip(dev->gateway.addr);
    print("\n");

    print("          io_base:0x");

    char buf[8];

    itoa(dev->io_base, buf, 16);
    print(buf);

    print("  IRQ:9\n");
    print("          UP BROADCAST RUNNING  MTU:1500\n");
}

void cmd_arp(int argc, char *args[]) {
    (void)argc;
    (void)args;

    if (!is_network_initialized()) {
        print("arp: network not initialized\n");
        return;
    }

    print("ARP cache for eth0:\n");
    network_print_info();
}

void cmd_netsend(int argc, char *args[]) {
    if (argc < 3) {
        print("Usage: net send <ip> <message>\n");
        print("  e.g. net send 10.0.2.2 hello\n");
        return;
    }

    if (!is_network_initialized()) {
        print("net send: network not initialized\n");
        return;
    }

    uint8_t dst_ip[4];

    if (!parse_ip(args[1], dst_ip)) {
        print("net send: invalid IP '");
        print(args[1]);
        print("'\n");
        return;
    }

    const char *msg = args[2];

    uint16_t msg_len =
        (uint16_t)strlen(msg);

    if (msg_len > 256) {
        print("net send: message too long (max 256)\n");
        return;
    }

    uint16_t udp_len =
        (uint16_t)(sizeof(udp_header_t) +
                   msg_len);

    uint8_t udp_chunk[
        sizeof(udp_header_t) + 256
    ];

    udp_header_t *udp =
        (udp_header_t *)udp_chunk;

    udp->src_port = htons(1234);
    udp->dst_port = htons(9);
    udp->length = htons(udp_len);
    udp->checksum = 0;

    memcpy(udp_chunk + sizeof(udp_header_t),
           msg,
           msg_len);

    uint8_t frame[1514];

    int frame_len =
        build_ip_frame(frame,
                       dst_ip,
                       IP_PROTO_UDP,
                       udp_chunk,
                       udp_len);

    if (frame_len < 0) {
        print("net send: frame build failed\n");
        return;
    }

    int rc =
        network_send_packet(frame,
                            (size_t)frame_len);

    if (rc == 0) {
        print("UDP packet sent to ");
        print_ip(dst_ip);
        print(" port 9 (");

        char buf[4];
        itoa(msg_len, buf, 10);
        print(buf);

        print(" bytes)\n");
    } else {
        print("net send: transmit failed\n");
    }
}
