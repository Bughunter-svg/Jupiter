/*
 * netcmds.c – High-level network commands for JupiterOS
 *   ping <ip>              ICMP echo request/reply
 *   ifconfig               show NIC configuration
 *   arp                    show / manage ARP cache
 *   net send <ip> <msg>    send a UDP datagram (port 9 discard)
 */

#include "netcmds.h"
#include "network.h"
#include "screen.h"
#include "string.h"
#include "timer.h"
#include "ports.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Internal packet structures
 * ═══════════════════════════════════════════════════════════════════ */

/* ICMP header */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

/* UDP header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0
#define PING_PAYLOAD_SIZE  32
#define PING_TIMEOUT_MS    3000   /* ms – approx via busy-wait           */
#define PING_BUSY_LOOPS    500000 /* loops ≈ 1 ms on ~500 MHz QEMU       */

/* ═══════════════════════════════════════════════════════════════════
 *  Small helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Print a dotted-decimal IP from a 4-byte array */
static void print_ip(const uint8_t ip[4]) {
    char buf[4];
    for (int i = 0; i < 4; i++) {
        itoa(ip[i], buf, 10);
        print(buf);
        if (i < 3) print(".");
    }
}

/* Print a colon-separated MAC from a 6-byte array */
static void print_mac(const uint8_t mac[6]) {
    char buf[3];
    for (int i = 0; i < 6; i++) {
        uint8_t b = mac[i];
        itoa((b >> 4) & 0xF, buf, 16); print(buf);
        itoa( b       & 0xF, buf, 16); print(buf);
        if (i < 5) print(":");
    }
}

/* Print a decimal integer */
static void print_dec(int n) {
    char buf[12];
    itoa(n, buf, 10);
    print(buf);
}

/*
 * Parse "a.b.c.d" into a 4-byte array.
 * Returns 1 on success, 0 on bad format.
 */
static int parse_ip(const char *s, uint8_t out[4]) {
    int octet = 0, idx = 0, digits = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (*s - '0');
            digits++;
            if (octet > 255 || digits > 3) return 0;
        } else if (*s == '.') {
            if (idx >= 3 || digits == 0) return 0;
            out[idx++] = (uint8_t)octet;
            octet = 0; digits = 0;
        } else {
            return 0;
        }
    }
    if (idx != 3 || digits == 0) return 0;
    out[idx] = (uint8_t)octet;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Build & send an IP packet (Ethernet + IP + payload)
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Build a full Ethernet+IP frame in 'buf', filling Ethernet src/dst,
 * IP header, and copying 'payload' of 'payload_len' bytes.
 * Returns total frame length, or -1 on ARP miss.
 *
 * 'buf' must be at least 1514 bytes.
 */
static int build_ip_frame(uint8_t *buf,
                           const uint8_t dst_ip[4],
                           uint8_t proto,
                           const void *payload, uint16_t payload_len) {
    network_device_t *dev = get_network_device();

    /* ── Resolve destination MAC (ARP cache or broadcast) ── */
    ip_addr_t dst;
    mac_addr_t dst_mac;
    memcpy(dst.addr, dst_ip, IP_ALEN);
    if (!network_resolve_mac(&dst, &dst_mac)) {
        return -1;
    }

    /* ── Ethernet header ── */
    eth_header_t *eth = (eth_header_t *)buf;
    memcpy(eth->dest, dst_mac.addr,        6);
    memcpy(eth->src,  dev->mac_addr.addr,  6);
    eth->type = htons(ETH_TYPE_IP);

    /* ── IP header ── */
    ip_header_t *ip = (ip_header_t *)(buf + sizeof(eth_header_t));
    uint16_t ip_total = (uint16_t)(sizeof(ip_header_t) + payload_len);

    ip->ver_ihl        = 0x45;          /* version=4, IHL=5 (20 bytes) */
    ip->tos            = 0;
    ip->total_length   = htons(ip_total);
    ip->identification = htons(0x1234); /* static ID – fine for our use */
    ip->flags_fragment = 0;
    ip->ttl            = 64;
    ip->protocol       = proto;
    ip->checksum       = 0;
    memcpy(ip->src_ip,  dev->ip_addr.addr, 4);
    memcpy(ip->dest_ip, dst_ip,            4);
    ip->checksum = calculate_checksum(ip, sizeof(ip_header_t));

    /* ── Payload ── */
    memcpy(buf + sizeof(eth_header_t) + sizeof(ip_header_t),
           payload, payload_len);

    return (int)(sizeof(eth_header_t) + ip_total);
}

/* ═══════════════════════════════════════════════════════════════════
 *  ICMP helpers
 * ═══════════════════════════════════════════════════════════════════ */

static uint16_t ping_seq = 0;
static const uint16_t PING_ID = 0x4A50;  /* 'JP' */

/* Build an ICMP echo-request + 32-byte payload; return size of icmp chunk */
static uint16_t build_icmp_echo(uint8_t *icmp_buf, uint16_t seq) {
    icmp_header_t *hdr = (icmp_header_t *)icmp_buf;
    hdr->type     = ICMP_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(PING_ID);
    hdr->seq      = htons(seq);

    /* Fill payload with 'A'-'Z' pattern */
    uint8_t *payload = icmp_buf + sizeof(icmp_header_t);
    for (int i = 0; i < PING_PAYLOAD_SIZE; i++)
        payload[i] = (uint8_t)('A' + (i % 26));

    uint16_t total = (uint16_t)(sizeof(icmp_header_t) + PING_PAYLOAD_SIZE);
    hdr->checksum = calculate_checksum(icmp_buf, total);
    return total;
}

/* ═══════════════════════════════════════════════════════════════════
 *  cmd_ping
 * ═══════════════════════════════════════════════════════════════════ */
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
        for (char *p = args[2]; *p >= '0' && *p <= '9'; p++)
            count = count * 10 + (*p - '0');
        if (count < 1 || count > 20) count = 4;
    }

    print("PING ");
    print_ip(target_ip);
    print(": ");
    print_dec(PING_PAYLOAD_SIZE);
    print(" bytes of data\n");

    int sent = 0, received = 0, total_ms = 0;

    for (int i = 0; i < count; i++) {
        uint16_t seq = ++ping_seq;

        /* Build ICMP chunk */
        uint8_t icmp_chunk[sizeof(icmp_header_t) + PING_PAYLOAD_SIZE];
        uint16_t icmp_len = build_icmp_echo(icmp_chunk, seq);

        /* Embed in full Ethernet+IP frame */
        uint8_t frame[1514];
        int frame_len = build_ip_frame(frame, target_ip,
                                       IP_PROTO_ICMP,
                                       icmp_chunk, icmp_len);
        if (frame_len < 0) {
            print("ping: could not build frame\n");
            break;
        }

        /* Record send time (tick-based) */
        uint32_t t_start = (uint32_t)get_ticks();
        network_send_packet(frame, (size_t)frame_len);
        sent++;

        /* ── Wait for ICMP echo-reply ── */
        int reply_ok = 0;
        uint32_t elapsed_ms = 0;

        for (int attempt = 0; attempt < PING_TIMEOUT_MS; attempt++) {
            /* Busy-wait ~1 ms */
            for (volatile int d = 0; d < PING_BUSY_LOOPS; d++)
                asm volatile("pause");

            uint8_t rx_buf[1514];
            int rx_len = network_receive_packet(rx_buf, sizeof(rx_buf));
            if (rx_len < (int)(sizeof(eth_header_t) +
                               sizeof(ip_header_t) +
                               sizeof(icmp_header_t))) {
                elapsed_ms++;
                continue;
            }

            eth_header_t  *re  = (eth_header_t *)rx_buf;
            if (ntohs(re->type) != ETH_TYPE_IP) { elapsed_ms++; continue; }

            ip_header_t   *rip = (ip_header_t *)(rx_buf + sizeof(eth_header_t));
            if (rip->protocol != IP_PROTO_ICMP) { elapsed_ms++; continue; }

            uint8_t ip_hlen = (rip->ver_ihl & 0x0F) * 4;
            icmp_header_t *ric = (icmp_header_t *)
                                 (rx_buf + sizeof(eth_header_t) + ip_hlen);

            if (ric->type != ICMP_ECHO_REPLY)   { elapsed_ms++; continue; }
            if (ntohs(ric->id)  != PING_ID)     { elapsed_ms++; continue; }
            if (ntohs(ric->seq) != seq)          { elapsed_ms++; continue; }

            /* Got our reply */
            uint32_t t_end = (uint32_t)get_ticks();
            elapsed_ms = (uint32_t)(t_end - t_start) * 10; /* ticks→ms @100Hz */
            reply_ok = 1;
            break;
        }

        if (reply_ok) {
            received++;
            total_ms += (int)elapsed_ms;
            print_dec(PING_PAYLOAD_SIZE + 8);
            print(" bytes from ");
            print_ip(target_ip);
            print(": icmp_seq=");
            print_dec(seq);
            print(" ttl=64 time=");
            print_dec((int)elapsed_ms);
            print(" ms\n");
        } else {
            print("Request timeout for icmp_seq ");
            print_dec(seq);
            print("\n");
        }

        /* ~1 second gap between pings */
        for (volatile int d = 0; d < 1000 * PING_BUSY_LOOPS; d++)
            asm volatile("pause");
    }

    /* ── Statistics ── */
    int lost = sent - received;
    int loss_pct = (sent > 0) ? (lost * 100 / sent) : 0;

    print("\n--- ");
    print_ip(target_ip);
    print(" ping statistics ---\n");
    print_dec(sent);     print(" packets transmitted, ");
    print_dec(received); print(" received, ");
    print_dec(loss_pct); print("% packet loss\n");

    if (received > 0) {
        print("avg rtt = ");
        print_dec(total_ms / received);
        print(" ms\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  cmd_ifconfig
 * ═══════════════════════════════════════════════════════════════════ */
void cmd_ifconfig(void) {
    if (!is_network_initialized()) {
        print("ifconfig: network not initialized\n");
        return;
    }

    network_device_t *dev = get_network_device();

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

/* ═══════════════════════════════════════════════════════════════════
 *  cmd_arp  –  show / flush ARP cache
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * We need to peek into the ARP cache. Rather than duplicating the
 * struct, we expose a small iteration helper via network.h.
 * Here we call arp_cache_lookup in a sweep over known IPs – but
 * that's impractical. Instead we add arp_cache_print() to network.c.
 *
 * For now we call the existing network_print_info() which already
 * prints the cache, and we wrap it with a cleaner header.
 */
void cmd_arp(int argc, char *args[]) {
    (void)argc; (void)args;

    if (!is_network_initialized()) {
        print("arp: network not initialized\n");
        return;
    }

    /* network_print_info() already prints the ARP cache section */
    print("ARP cache for eth0:\n");
    network_print_info();
}

/* ═══════════════════════════════════════════════════════════════════
 *  cmd_netsend  –  send a short UDP message (port 9 "discard")
 * ═══════════════════════════════════════════════════════════════════ */
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
    uint16_t   msg_len  = (uint16_t)strlen(msg);
    uint16_t   udp_len  = (uint16_t)(sizeof(udp_header_t) + msg_len);

    /* Build UDP + message chunk */
    uint8_t udp_chunk[sizeof(udp_header_t) + 256];
    if (msg_len > 256) { print("net send: message too long (max 256)\n"); return; }

    udp_header_t *udp = (udp_header_t *)udp_chunk;
    udp->src_port = htons(1234);
    udp->dst_port = htons(9);     /* RFC 863 discard port */
    udp->length   = htons(udp_len);
    udp->checksum = 0;            /* optional for UDP/IPv4 */
    memcpy(udp_chunk + sizeof(udp_header_t), msg, msg_len);

    /* Wrap in Ethernet+IP */
    uint8_t frame[1514];
    int frame_len = build_ip_frame(frame, dst_ip,
                                   IP_PROTO_UDP,
                                   udp_chunk, udp_len);
    if (frame_len < 0) {
        print("net send: frame build failed\n");
        return;
    }

    int rc = network_send_packet(frame, (size_t)frame_len);
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
