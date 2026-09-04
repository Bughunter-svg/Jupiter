#include "network.h"
#include "ports.h"
#include "screen.h"
#include "string.h"
#include "memory.h"

/* ─── NE2000 register map (page 0) ──────────────────────────────── */
#define NE_BASE      0x300

#define NE_CMD       0x00  /* Command register (all pages)          */
#define NE_PSTART    0x01  /* Page start (page 0 write)             */
#define NE_PSTOP     0x02  /* Page stop  (page 0 write)             */
#define NE_BNRY      0x03  /* Boundary pointer                      */
#define NE_TPSR      0x04  /* TX page start                         */
#define NE_TBCR0     0x05  /* TX byte count low                     */
#define NE_TBCR1     0x06  /* TX byte count high                    */
#define NE_ISR       0x07  /* Interrupt status                      */
#define NE_RSAR0     0x08  /* Remote start addr low                 */
#define NE_RSAR1     0x09  /* Remote start addr high                */
#define NE_RBCR0     0x0A  /* Remote byte count low                 */
#define NE_RBCR1     0x0B  /* Remote byte count high                */
#define NE_RCR       0x0C  /* Receive config                        */
#define NE_TCR       0x0D  /* Transmit config                       */
#define NE_DCR       0x0E  /* Data config                           */
#define NE_IMR       0x0F  /* Interrupt mask                        */
#define NE_DMA_PORT  0x10  /* Remote DMA data port                  */
#define NE_RESET_PORT 0x1F /* Read→reset, write→reset              */

/* Page 1 registers (select with CMD bits 7:6 = 01) */
#define NE_P1_PAR0   0x01  /* Physical address registers 0-5        */
#define NE_P1_CURR   0x07  /* Current page pointer                  */

/* CMD register bits */
#define CMD_PAGE0    0x20  /* Select page 0 (bits 7:6 = 00 + STP)  */
#define CMD_PAGE1    0x60  /* Select page 1                         */
#define CMD_START    0x02  /* Start NIC                             */
#define CMD_STOP     0x01  /* Stop NIC                              */
#define CMD_NODMA    0x20  /* No remote DMA                         */
#define CMD_RDMA_RD  0x0A  /* Remote DMA read                       */
#define CMD_RDMA_WR  0x12  /* Remote DMA write                      */
#define CMD_TXP      0x04  /* Transmit packet                       */

/* ISR bits */
#define ISR_PRX      0x01  /* Packet received                       */
#define ISR_PTX      0x02  /* Packet transmitted                    */
#define ISR_RXE      0x04  /* Receive error                         */
#define ISR_TXE      0x08  /* Transmit error                        */
#define ISR_RST      0x80  /* Reset complete                        */

/* Ring-buffer pages (each page = 256 bytes) */
#define TX_PAGE_START  0x40   /* TX buffer: pages 0x40-0x46         */
#define RX_PAGE_START  0x46   /* RX ring starts here                */
#define RX_PAGE_STOP   0x80   /* RX ring ends here (exclusive)      */

#define MAX_PACKET_SIZE 1514
#define ARP_CACHE_SIZE  16

/* ─── State ──────────────────────────────────────────────────────── */
static network_device_t nic;
static int  net_ok   = 0;   /* 1 = hardware found & initialised    */
static int  net_init = 0;   /* 1 = init attempted                  */
static uint8_t rx_next = RX_PAGE_START; /* next page to read        */

typedef struct {
    ip_addr_t  ip;
    mac_addr_t mac;
    int        valid;
} arp_cache_entry_t;

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

/* ─── Low-level helpers ──────────────────────────────────────────── */
static inline void ne_wr(uint8_t reg, uint8_t val) {
    outb(nic.io_base + reg, val);
}
static inline uint8_t ne_rd(uint8_t reg) {
    return inb(nic.io_base + reg);
}

/* Busy-wait with timeout; returns 0 on timeout */
static int ne_wait_dma(void) {
    for (int t = 0; t < 0x10000; t++) {
        if (ne_rd(NE_ISR) & 0x40) return 1; /* RDC bit */
    }
    return 0; /* timeout */
}

/*
 * Read 'len' bytes from NIC buffer at NIC-address 'src_page:src_off'
 * into host RAM pointer 'dst'.
 */
static void ne_dma_read(uint8_t src_page, uint8_t src_off,
                        void *dst, uint16_t len) {
    uint16_t addr = ((uint16_t)src_page << 8) | src_off;

    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_NODMA | CMD_START);
    ne_wr(NE_RBCR0, len & 0xFF);
    ne_wr(NE_RBCR1, (len >> 8) & 0xFF);
    ne_wr(NE_RSAR0, addr & 0xFF);
    ne_wr(NE_RSAR1, (addr >> 8) & 0xFF);
    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_RDMA_RD | CMD_START);

    uint8_t *p = (uint8_t *)dst;
    for (uint16_t i = 0; i < len; i++)
        p[i] = inb(nic.io_base + NE_DMA_PORT);

    ne_wait_dma();
}

/*
 * Write 'len' bytes from host RAM 'src' into NIC buffer at page 'dst_page'.
 */
static void ne_dma_write(uint8_t dst_page, const void *src, uint16_t len) {
    uint16_t addr = (uint16_t)dst_page << 8;
    uint16_t wlen = (len < 64) ? 64 : len; /* NE2000 min TX = 64 bytes */

    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_NODMA | CMD_START);
    ne_wr(NE_RBCR0, wlen & 0xFF);
    ne_wr(NE_RBCR1, (wlen >> 8) & 0xFF);
    ne_wr(NE_RSAR0, addr & 0xFF);
    ne_wr(NE_RSAR1, (addr >> 8) & 0xFF);
    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_RDMA_WR | CMD_START);

    const uint8_t *p = (const uint8_t *)src;
    for (uint16_t i = 0; i < len;  i++) outb(nic.io_base + NE_DMA_PORT, p[i]);
    for (uint16_t i = len; i < wlen; i++) outb(nic.io_base + NE_DMA_PORT, 0x00);

    ne_wait_dma();
}

/* ─── Detect NE2000 ──────────────────────────────────────────────── */
/*
 * Standard NE2000 detection: reset, check ISR RST bit, do 32-byte
 * DMA loopback to confirm the chip is real.
 * Returns 1 if found.
 */
static int ne2000_detect(void) {
    /* Reset */
    uint8_t rst = inb(nic.io_base + NE_RESET_PORT);
    outb(nic.io_base + NE_RESET_PORT, rst);

    /* Wait for RST bit in ISR */
    for (int t = 0; t < 0x10000; t++) {
        if (inb(nic.io_base + NE_ISR) & ISR_RST) break;
        if (t == 0xFFFF - 1) return 0; /* no NIC */
    }

    /* Stop NIC, no DMA */
    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_STOP);
    for (volatile int d = 0; d < 5000; d++) asm volatile("pause");

    /* Configure DCR: 8-bit, FIFO threshold=2, normal */
    outb(nic.io_base + NE_DCR, 0x48);

    /* Clear remote byte count */
    outb(nic.io_base + NE_RBCR0, 0);
    outb(nic.io_base + NE_RBCR1, 0);

    /* Accept all packets (loopback mode for detection) */
    outb(nic.io_base + NE_RCR, 0x20);

    /* Internal loopback */
    outb(nic.io_base + NE_TCR, 0x02);

    /* Set ring buffer boundaries */
    outb(nic.io_base + NE_PSTART, RX_PAGE_START);
    outb(nic.io_base + NE_PSTOP,  RX_PAGE_STOP);
    outb(nic.io_base + NE_BNRY,   RX_PAGE_START);

    /* TX buffer */
    outb(nic.io_base + NE_TPSR, TX_PAGE_START);

    /* Page 1: set CURR, program PAR (we'll read MAC from PROM next) */
    outb(nic.io_base + NE_CMD, CMD_PAGE1 | CMD_NODMA | CMD_STOP);
    outb(nic.io_base + NE_P1_CURR, RX_PAGE_START + 1);
    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_STOP);

    return 1;
}

/* ─── Read MAC from PROM ─────────────────────────────────────────── */
/*
 * The NE2000 PROM occupies the first 32 bytes of NIC address space.
 * Bytes 0,2,4,6,8,10 hold the 6 MAC octets (every other byte).
 */
static int ne2000_read_mac(void) {
    uint8_t prom[32];

    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_STOP);

    outb(nic.io_base + NE_DCR, 0x48);
    outb(nic.io_base + NE_RBCR0, 32);
    outb(nic.io_base + NE_RBCR1, 0);
    outb(nic.io_base + NE_RSAR0, 0);
    outb(nic.io_base + NE_RSAR1, 0);

    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_RDMA_RD | CMD_STOP);

    for (int i = 0; i < 32; i++)
        prom[i] = inb(nic.io_base + NE_DMA_PORT);

    for (int i = 0; i < 6; i++)
        nic.mac_addr.addr[i] = prom[i * 2];

    if (nic.mac_addr.addr[0] == 0xFF &&
        nic.mac_addr.addr[1] == 0xFF &&
        nic.mac_addr.addr[2] == 0xFF &&
        nic.mac_addr.addr[3] == 0xFF &&
        nic.mac_addr.addr[4] == 0xFF &&
        nic.mac_addr.addr[5] == 0xFF) {
        return 0;
    }

    return 1;
}

/* ─── network_init ───────────────────────────────────────────────── */
void network_init(void) {
    net_init = 1;
    nic.io_base = NE_BASE;

    memset(arp_cache, 0, sizeof(arp_cache));

    if (!ne2000_detect()) {
        print("Network: No NE2000 found at 0x300 — network disabled\n");
        net_ok = 0;
        return;
    }

    /* Read MAC from PROM */
    if (!ne2000_read_mac()) {
        print("Network: Failed to read NE2000 MAC\n");
        net_ok = 0;
        return;
    }

    /* Program PAR0-5 in page 1 so NIC accepts frames for us */
    outb(nic.io_base + NE_CMD, CMD_PAGE1 | CMD_NODMA | CMD_STOP);
    for (int i = 0; i < 6; i++)
        outb(nic.io_base + NE_P1_PAR0 + i, nic.mac_addr.addr[i]);
    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_STOP);

    /* Normal RX/TX mode */
    outb(nic.io_base + NE_TCR, 0x00);
    outb(nic.io_base + NE_RCR, 0x04); /* Accept broadcast           */
    outb(nic.io_base + NE_ISR, 0xFF); /* Clear all interrupts       */
    outb(nic.io_base + NE_IMR, 0x00); /* Mask all (polling mode)    */

    /* Start NIC */
    outb(nic.io_base + NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_START);

    /* Default static IP config (no DHCP yet) */
    nic.ip_addr.addr[0] = 10; nic.ip_addr.addr[1] = 0;
    nic.ip_addr.addr[2] = 2;  nic.ip_addr.addr[3] = 15;

    nic.netmask.addr[0] = 255; nic.netmask.addr[1] = 255;
    nic.netmask.addr[2] = 255; nic.netmask.addr[3] = 0;

    nic.gateway.addr[0] = 10; nic.gateway.addr[1] = 0;
    nic.gateway.addr[2] = 2;  nic.gateway.addr[3] = 2;

    rx_next = RX_PAGE_START + 1;
    net_ok  = 1;

    /* Print MAC */
    print("Network: NE2000 @ 0x300  MAC ");
    char buf[4];
    for (int i = 0; i < 6; i++) {
        uint8_t b = nic.mac_addr.addr[i];
        /* Two hex digits */
        itoa((b >> 4) & 0xF, buf, 16); print(buf);
        itoa( b       & 0xF, buf, 16); print(buf);
        if (i < 5) print(":");
    }
    print("\n");
    print("Network: IP 10.0.2.15  GW 10.0.2.2\n");
}

/* ─── Send ───────────────────────────────────────────────────────── */
int network_send_packet(const void *data, size_t length) {
    if (!net_ok || length > MAX_PACKET_SIZE || length < 14) return -1;

    ne_dma_write(TX_PAGE_START, data, (uint16_t)length);

    uint16_t tx_len = (length < 64) ? 64 : (uint16_t)length;
    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_NODMA | CMD_START);
    ne_wr(NE_TPSR,  TX_PAGE_START);
    ne_wr(NE_TBCR0, tx_len & 0xFF);
    ne_wr(NE_TBCR1, (tx_len >> 8) & 0xFF);
    ne_wr(NE_CMD,   CMD_PAGE0 | CMD_NODMA | CMD_TXP | CMD_START);

    /* Poll for TX complete (with timeout) */
    for (int t = 0; t < 0x10000; t++) {
        uint8_t isr = ne_rd(NE_ISR);
        if (isr & ISR_PTX) { ne_wr(NE_ISR, ISR_PTX); return 0; }
        if (isr & ISR_TXE) { ne_wr(NE_ISR, ISR_TXE); return -1; }
    }
    return -1; /* timeout */
}

/* ─── Receive ────────────────────────────────────────────────────── */
int network_receive_packet(void *buffer, size_t buffer_size) {
    if (!net_ok) return -1;

    ne_wr(NE_CMD, CMD_PAGE0 | CMD_NODMA | CMD_START);
    if (!(ne_rd(NE_ISR) & ISR_PRX)) return 0; /* nothing ready */

    /* Read ring header (4 bytes) from current rx_next page */
    uint8_t hdr[4];
    ne_dma_read(rx_next, 0x00, hdr, 4);

    uint8_t  status   = hdr[0];
    uint8_t  next_pg  = hdr[1];
    uint16_t pkt_len  = (uint16_t)hdr[2] | ((uint16_t)hdr[3] << 8);

    /* Validate */
    if (!(status & 0x01) || pkt_len < 14 || pkt_len > (uint16_t)buffer_size) {
        /* Bad packet — advance */
        rx_next = next_pg;
        ne_wr(NE_BNRY, (rx_next == RX_PAGE_START) ? RX_PAGE_STOP - 1
                                                   : rx_next - 1);
        ne_wr(NE_ISR, ISR_PRX);
        return -1;
    }

    uint16_t data_len = pkt_len - 4; /* strip CRC */
    ne_dma_read(rx_next, 0x04, buffer, data_len);

    rx_next = next_pg;
    ne_wr(NE_BNRY, (rx_next == RX_PAGE_START) ? RX_PAGE_STOP - 1
                                               : rx_next - 1);
    ne_wr(NE_ISR, ISR_PRX);

    return (int)data_len;
}

/* ─── ARP ────────────────────────────────────────────────────────── */
void handle_arp_packet(const void *packet, size_t length) {
    if (length < sizeof(eth_header_t) + sizeof(arp_header_t)) return;

    const eth_header_t *eth = (const eth_header_t *)packet;
    const arp_header_t *arp = (const arp_header_t *)(eth + 1);

    if (ntohs(arp->htype) != ARP_HTYPE_ETHER) return;
    if (ntohs(arp->ptype) != ETH_TYPE_IP)      return;
    if (memcmp(arp->target_ip, nic.ip_addr.addr, IP_ALEN) != 0) return;

    /* Cache sender */
    ip_addr_t  sender_ip;  memcpy(sender_ip.addr,  arp->sender_ip,  IP_ALEN);
    mac_addr_t sender_mac; memcpy(sender_mac.addr, arp->sender_mac, ETH_ALEN);
    arp_cache_add(&sender_ip, &sender_mac);

    if (ntohs(arp->opcode) == ARP_OP_REQUEST) {
        uint8_t reply[sizeof(eth_header_t) + sizeof(arp_header_t)];
        eth_header_t *re  = (eth_header_t *)reply;
        arp_header_t *ra  = (arp_header_t *)(re + 1);

        memcpy(re->dest, eth->src,          ETH_ALEN);
        memcpy(re->src,  nic.mac_addr.addr, ETH_ALEN);
        re->type = htons(ETH_TYPE_ARP);

        ra->htype  = htons(ARP_HTYPE_ETHER);
        ra->ptype  = htons(ETH_TYPE_IP);
        ra->hlen   = ETH_ALEN;
        ra->plen   = IP_ALEN;
        ra->opcode = htons(ARP_OP_REPLY);
        memcpy(ra->sender_mac, nic.mac_addr.addr, ETH_ALEN);
        memcpy(ra->sender_ip,  nic.ip_addr.addr,  IP_ALEN);
        memcpy(ra->target_mac, arp->sender_mac,    ETH_ALEN);
        memcpy(ra->target_ip,  arp->sender_ip,     IP_ALEN);

        network_send_packet(reply, sizeof(reply));
        print("Network: ARP reply sent\n");
    }
}

void handle_ip_packet(const void *packet, size_t length) {
    if (length < sizeof(eth_header_t) + sizeof(ip_header_t)) return;
    const eth_header_t *eth = (const eth_header_t *)packet;
    const ip_header_t  *ip  = (const ip_header_t *)(eth + 1);
    (void)ip; /* future: ICMP/UDP/TCP dispatch here */
}

void send_arp_request(const ip_addr_t *target_ip) {
    uint8_t pkt[sizeof(eth_header_t) + sizeof(arp_header_t)];
    eth_header_t *eth = (eth_header_t *)pkt;
    arp_header_t *arp = (arp_header_t *)(eth + 1);

    memset(eth->dest, 0xFF, ETH_ALEN);
    memcpy(eth->src,  nic.mac_addr.addr, ETH_ALEN);
    eth->type = htons(ETH_TYPE_ARP);

    arp->htype  = htons(ARP_HTYPE_ETHER);
    arp->ptype  = htons(ETH_TYPE_IP);
    arp->hlen   = ETH_ALEN;
    arp->plen   = IP_ALEN;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, nic.mac_addr.addr, ETH_ALEN);
    memcpy(arp->sender_ip,  nic.ip_addr.addr,  IP_ALEN);
    memset(arp->target_mac, 0x00, ETH_ALEN);
    memcpy(arp->target_ip,  target_ip->addr,   IP_ALEN);

    network_send_packet(pkt, sizeof(pkt));
}

/* ─── ARP cache ──────────────────────────────────────────────────── */
void arp_cache_add(const ip_addr_t *ip, const mac_addr_t *mac) {
    /* Update existing entry */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            memcmp(arp_cache[i].ip.addr, ip->addr, IP_ALEN) == 0) {
            memcpy(arp_cache[i].mac.addr, mac->addr, ETH_ALEN);
            return;
        }
    }
    /* Find free slot */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            memcpy(arp_cache[i].ip.addr,  ip->addr,  IP_ALEN);
            memcpy(arp_cache[i].mac.addr, mac->addr, ETH_ALEN);
            arp_cache[i].valid = 1;
            return;
        }
    }
    /* Overwrite slot 0 (oldest eviction) */
    memcpy(arp_cache[0].ip.addr,  ip->addr,  IP_ALEN);
    memcpy(arp_cache[0].mac.addr, mac->addr, ETH_ALEN);
    arp_cache[0].valid = 1;
}

int arp_cache_lookup(const ip_addr_t *ip, mac_addr_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            memcmp(arp_cache[i].ip.addr, ip->addr, IP_ALEN) == 0) {
            memcpy(mac->addr, arp_cache[i].mac.addr, ETH_ALEN);
            return 1;
        }
    }
    return 0;
}
int network_is_local_ip(const ip_addr_t *ip) {
    for (int i = 0; i < IP_ALEN; i++) {
        if ((ip->addr[i] & nic.netmask.addr[i]) !=
            (nic.ip_addr.addr[i] & nic.netmask.addr[i])) {
            return 0;
        }
    }

    return 1;
}

int network_resolve_mac(const ip_addr_t *dest_ip, mac_addr_t *mac) {
    ip_addr_t next_hop;

    if (network_is_local_ip(dest_ip)) {
        memcpy(next_hop.addr, dest_ip->addr, IP_ALEN);
    } else {
        memcpy(next_hop.addr, nic.gateway.addr, IP_ALEN);
    }

    if (arp_cache_lookup(&next_hop, mac)) {
        return 1;
    }

    send_arp_request(&next_hop);

    uint8_t buffer[1514];

    for (int attempt = 0; attempt < 100000; attempt++) {
        int len = network_receive_packet(buffer, sizeof(buffer));

        if (len > 0) {
            eth_header_t *eth = (eth_header_t *)buffer;
            uint16_t type = ntohs(eth->type);

            if (type == ETH_TYPE_ARP) {
                handle_arp_packet(buffer, len);

                if (arp_cache_lookup(&next_hop, mac)) {
                    return 1;
                }
            } else if (type == ETH_TYPE_IP) {
                handle_ip_packet(buffer, len);
            }
        }

        asm volatile("pause");
    }

    return 0;
}
void arp_cache_print(void) {
    char buf[4];
    int found = 0;
    print("IP Address       HW Address\n");
    print("─────────────────────────────────────\n");
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) continue;
        /* IP */
        for (int j = 0; j < 4; j++) {
            itoa(arp_cache[i].ip.addr[j], buf, 10);
            print(buf);
            if (j < 3) print(".");
        }
        print("   ");
        /* MAC */
        for (int j = 0; j < 6; j++) {
            uint8_t b = arp_cache[i].mac.addr[j];
            itoa((b >> 4) & 0xF, buf, 16); print(buf);
            itoa( b       & 0xF, buf, 16); print(buf);
            if (j < 5) print(":");
        }
        print("\n");
        found++;
    }
    if (!found) print("(ARP cache is empty)\n");
}


/* ─── Utility ────────────────────────────────────────────────────── */
uint16_t ntohs(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
uint16_t htons(uint16_t v) { return ntohs(v); }

uint16_t calculate_checksum(const void *data, size_t length) {
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)data;
    while (length > 1) { sum += *p++; length -= 2; }
    if (length) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ─── Status print ───────────────────────────────────────────────── */
void network_print_info(void) {
    if (!net_init) { print("Network: not yet initialized\n"); return; }
    if (!net_ok)   { print("Network: NO HARDWARE DETECTED\n"); return; }

    char buf[4];
    print("Network : UP\n");
    print("MAC     : ");
    for (int i = 0; i < 6; i++) {
        uint8_t b = nic.mac_addr.addr[i];
        itoa((b>>4)&0xF, buf, 16); print(buf);
        itoa( b    &0xF, buf, 16); print(buf);
        if (i < 5) print(":");
    }
    print("\n");
    print("IP      : ");
    for (int i = 0; i < 4; i++) {
        itoa(nic.ip_addr.addr[i], buf, 10); print(buf);
        if (i < 3) print(".");
    }
    print("\n");
    print("Gateway : ");
    for (int i = 0; i < 4; i++) {
        itoa(nic.gateway.addr[i], buf, 10); print(buf);
        if (i < 3) print(".");
    }
    print("\n");
    print("ARP cache:\n");
    int found = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid) {
            print("  ");
            for (int j = 0; j < 4; j++) {
                itoa(arp_cache[i].ip.addr[j], buf, 10); print(buf);
                if (j < 3) print(".");
            }
            print(" -> ");
            for (int j = 0; j < 6; j++) {
                uint8_t b = arp_cache[i].mac.addr[j];
                itoa((b>>4)&0xF, buf, 16); print(buf);
                itoa( b    &0xF, buf, 16); print(buf);
                if (j < 5) print(":");
            }
            print("\n");
            found++;
        }
    }
    if (!found) print("  (empty)\n");
}

/* ─── Getters ────────────────────────────────────────────────────── */
network_device_t *get_network_device(void) { return &nic; }
int is_network_initialized(void)           { return net_ok; }
