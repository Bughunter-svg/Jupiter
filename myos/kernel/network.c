#include "network.h"
#include "ports.h"
#include "screen.h"
#include "string.h"
#include "memory.h"

#define NE2000_BASE 0x300
#define MAX_PACKET_SIZE 1514
#define ARP_CACHE_SIZE 16

// Network device
static network_device_t nic;
static int network_initialized = 0;

// ARP cache
typedef struct {
    ip_addr_t ip;
    mac_addr_t mac;
    uint32_t timestamp;
} arp_cache_entry_t;

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];
static uint32_t arp_cache_time = 0;

// NE2000 registers
#define NE2000_CMD 0x00
#define NE2000_PSTART 0x01
#define NE2000_PSTOP 0x02
#define NE2000_BNRY 0x03
#define NE2000_TPSR 0x04
#define NE2000_TBCR0 0x05
#define NE2000_TBCR1 0x06
#define NE2000_ISR 0x07
#define NE2000_RSAR0 0x08
#define NE2000_RSAR1 0x09
#define NE2000_RBCR0 0x0A
#define NE2000_RBCR1 0x0B
#define NE2000_RCR 0x0C
#define NE2000_TCR 0x0D
#define NE2000_DCR 0x0E
#define NE2000_IMR 0x0F
#define NE2000_CURR 0x07
#define NE2000_DMA_PORT 0x10

// NE2000 functions
static void ne2000_write(uint8_t reg, uint8_t value) {
    outb(nic.io_base + reg, value);
}

static uint8_t ne2000_read(uint8_t reg) {
    return inb(nic.io_base + reg);
}

static void ne2000_select_page(uint8_t page) {
    ne2000_write(0x00, page);
}

static void ne2000_reset() {
    ne2000_write(0x1F, 0xFF);
    for (volatile int i = 0; i < 10000; i++);
}

void network_init() {
    // Initialize NIC
    nic.io_base = NE2000_BASE;
    
    // Reset NIC
    ne2000_reset();
    
    // Read MAC address from PROM
    ne2000_select_page(1);
    for (int i = 0; i < 6; i++) {
        nic.mac_addr.addr[i] = ne2000_read(0x01 + i);
    }
    
    // Configure NIC
    ne2000_select_page(0);
    ne2000_write(NE2000_RCR, 0x04); // Accept broadcast packets
    ne2000_write(NE2000_TCR, 0x00); // Normal transmit operation
    ne2000_write(NE2000_DCR, 0x48); // 8-bit DMA, byte order: DMA<->FIFO
    ne2000_write(NE2000_IMR, 0x11); // Enable RX and TX interrupts
    
    // Set page start/stop
    ne2000_select_page(1);
    ne2000_write(NE2000_PSTART, 0x4C);
    ne2000_write(NE2000_PSTOP, 0x80);
    
    // Set boundary and current pointers
    ne2000_select_page(0);
    ne2000_write(NE2000_BNRY, 0x4C);
    ne2000_write(NE2000_CURR, 0x4C);
    
    // Clear interrupt status
    ne2000_write(NE2000_ISR, 0xFF);
    
    // Set default IP configuration
    nic.ip_addr.addr[0] = 192;
    nic.ip_addr.addr[1] = 168;
    nic.ip_addr.addr[2] = 1;
    nic.ip_addr.addr[3] = 100;
    
    nic.netmask.addr[0] = 255;
    nic.netmask.addr[1] = 255;
    nic.netmask.addr[2] = 255;
    nic.netmask.addr[3] = 0;
    
    nic.gateway.addr[0] = 192;
    nic.gateway.addr[1] = 168;
    nic.gateway.addr[2] = 1;
    nic.gateway.addr[3] = 1;
    
    // Initialize ARP cache
    memset(arp_cache, 0, sizeof(arp_cache));
    
    network_initialized = 1;
    
    print("Network initialized: MAC ");
    for (int i = 0; i < 6; i++) {
        char buf[4];
        itoa(nic.mac_addr.addr[i], buf, 16);
        print(buf);
        if (i < 5) print(":");
    }
    print("\n");
}

int network_send_packet(const void* data, size_t length) {
    if (!network_initialized || length > MAX_PACKET_SIZE) return -1;
    
    ne2000_select_page(0);
    
    // Wait for NIC to be ready
    while (ne2000_read(NE2000_ISR) & 0x04);
    
    // Set transmit page start and byte count
    ne2000_write(NE2000_TPSR, 0x40);
    ne2000_write(NE2000_TBCR0, length & 0xFF);
    ne2000_write(NE2000_TBCR1, length >> 8);
    
    // Copy data to NIC buffer
    outb(nic.io_base + NE2000_CMD, 0x0A); // Start remote DMA write
    ne2000_write(NE2000_RSAR0, 0x00);
    ne2000_write(NE2000_RSAR1, 0x40);
    ne2000_write(NE2000_RBCR0, length & 0xFF);
    ne2000_write(NE2000_RBCR1, length >> 8);
    
    // Write data to DMA port
    uint8_t* ptr = (uint8_t*)data;
    for (size_t i = 0; i < length; i++) {
        outb(nic.io_base + NE2000_DMA_PORT, ptr[i]);
    }
    
    // Start transmission
    outb(nic.io_base + NE2000_CMD, 0x22);
    
    // Wait for transmission to complete
    while (!(ne2000_read(NE2000_ISR) & 0x02));
    ne2000_write(NE2000_ISR, 0x02);
    
    return 0;
}

int network_receive_packet(void* buffer, size_t buffer_size) {
    if (!network_initialized) return -1;
    
    ne2000_select_page(0);
    
    // Check if packet received
    if (!(ne2000_read(NE2000_ISR) & 0x01)) return 0;
    
    // Read packet header
    uint8_t header[4];
    ne2000_write(NE2000_RSAR0, 0x00);
    ne2000_write(NE2000_RSAR1, 0x4C);
    ne2000_write(NE2000_RBCR0, 4);
    ne2000_write(NE2000_RBCR1, 0);
    outb(nic.io_base + NE2000_CMD, 0x0A);
    
    for (int i = 0; i < 4; i++) {
        header[i] = inb(nic.io_base + NE2000_DMA_PORT);
    }
    
    uint8_t status = header[0];
    uint8_t next_ptr = header[1];
    uint16_t length = (header[3] << 8) | header[2];
    length -= 4; // Subtract CRC
    
    if (status & 0x80 || length > buffer_size || length < 14) {
        // Invalid packet
        ne2000_write(NE2000_BNRY, next_ptr);
        ne2000_write(NE2000_ISR, 0x01);
        return -1;
    }
    
    // Read packet data
    ne2000_write(NE2000_RSAR0, 0x04);
    ne2000_write(NE2000_RSAR1, 0x4C);
    ne2000_write(NE2000_RBCR0, length & 0xFF);
    ne2000_write(NE2000_RBCR1, length >> 8);
    outb(nic.io_base + NE2000_CMD, 0x0A);
    
    for (int i = 0; i < length; i++) {
        ((uint8_t*)buffer)[i] = inb(nic.io_base + NE2000_DMA_PORT);
    }
    
    // Update boundary pointer
    ne2000_write(NE2000_BNRY, next_ptr);
    ne2000_write(NE2000_ISR, 0x01);
    
    return length;
}

void handle_arp_packet(const void* packet, size_t length) {
    if (length < sizeof(eth_header_t) + sizeof(arp_header_t)) return;
    
    const eth_header_t* eth = (const eth_header_t*)packet;
    const arp_header_t* arp = (const arp_header_t*)(eth + 1);
    
    if (ntohs(arp->htype) != ARP_HTYPE_ETHER || ntohs(arp->ptype) != ETH_TYPE_IP) return;
    
    ip_addr_t target_ip;
    memcpy(target_ip.addr, arp->target_ip, IP_ALEN);
    
    // Check if this ARP is for us
    if (memcmp(target_ip.addr, nic.ip_addr.addr, IP_ALEN) != 0) return;
    
    if (ntohs(arp->opcode) == ARP_OP_REQUEST) {
        // Send ARP reply
        uint8_t reply[sizeof(eth_header_t) + sizeof(arp_header_t)];
        eth_header_t* reply_eth = (eth_header_t*)reply;
        arp_header_t* reply_arp = (arp_header_t*)(reply_eth + 1);
        
        // Build Ethernet header
        memcpy(reply_eth->dest, eth->src, ETH_ALEN);
        memcpy(reply_eth->src, nic.mac_addr.addr, ETH_ALEN);
        reply_eth->type = htons(ETH_TYPE_ARP);
        
        // Build ARP header
        reply_arp->htype = htons(ARP_HTYPE_ETHER);
        reply_arp->ptype = htons(ETH_TYPE_IP);
        reply_arp->hlen = ETH_ALEN;
        reply_arp->plen = IP_ALEN;
        reply_arp->opcode = htons(ARP_OP_REPLY);
        memcpy(reply_arp->sender_mac, nic.mac_addr.addr, ETH_ALEN);
        memcpy(reply_arp->sender_ip, nic.ip_addr.addr, IP_ALEN);
        memcpy(reply_arp->target_mac, arp->sender_mac, ETH_ALEN);
        memcpy(reply_arp->target_ip, arp->sender_ip, IP_ALEN);
        
        network_send_packet(reply, sizeof(reply));
    }
}

void handle_ip_packet(const void* packet, size_t length) {
    // Basic IP packet handling (placeholder for ICMP/TCP/UDP)
    (void)packet; // Silence unused parameter warning
    (void)length; // Silence unused parameter warning
}

void send_arp_request(const ip_addr_t* target_ip) {
    uint8_t packet[sizeof(eth_header_t) + sizeof(arp_header_t)];
    eth_header_t* eth = (eth_header_t*)packet;
    arp_header_t* arp = (arp_header_t*)(eth + 1);
    
    // Build Ethernet header (broadcast)
    memset(eth->dest, 0xFF, ETH_ALEN);
    memcpy(eth->src, nic.mac_addr.addr, ETH_ALEN);
    eth->type = htons(ETH_TYPE_ARP);
    
    // Build ARP header
    arp->htype = htons(ARP_HTYPE_ETHER);
    arp->ptype = htons(ETH_TYPE_IP);
    arp->hlen = ETH_ALEN;
    arp->plen = IP_ALEN;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, nic.mac_addr.addr, ETH_ALEN);
    memcpy(arp->sender_ip, nic.ip_addr.addr, IP_ALEN);
    memset(arp->target_mac, 0, ETH_ALEN);
    memcpy(arp->target_ip, target_ip->addr, IP_ALEN);
    
    network_send_packet(packet, sizeof(packet));
}

void arp_cache_add(const ip_addr_t* ip, const mac_addr_t* mac) {
    // Simple ARP cache implementation
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (memcmp(arp_cache[i].ip.addr, ip->addr, IP_ALEN) == 0) {
            memcpy(arp_cache[i].mac.addr, mac->addr, ETH_ALEN);
            arp_cache[i].timestamp = arp_cache_time++;
            return;
        }
    }
    
    // Find oldest entry
    int oldest = 0;
    for (int i = 1; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].timestamp < arp_cache[oldest].timestamp) {
            oldest = i;
        }
    }
    
    memcpy(arp_cache[oldest].ip.addr, ip->addr, IP_ALEN);
    memcpy(arp_cache[oldest].mac.addr, mac->addr, ETH_ALEN);
    arp_cache[oldest].timestamp = arp_cache_time++;
}

int arp_cache_lookup(const ip_addr_t* ip, mac_addr_t* mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (memcmp(arp_cache[i].ip.addr, ip->addr, IP_ALEN) == 0) {
            memcpy(mac->addr, arp_cache[i].mac.addr, ETH_ALEN);
            return 1;
        }
    }
    return 0;
}

// Utility functions
uint16_t ntohs(uint16_t netshort) {
    return ((netshort & 0xFF00) >> 8) | ((netshort & 0x00FF) << 8);
}

uint16_t htons(uint16_t hostshort) {
    return ntohs(hostshort);
}

uint16_t calculate_checksum(const void* data, size_t length) {
    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)data;
    
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    if (length > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)~sum;
}

// ==================== GETTER FUNCTIONS ====================

/**
 * Get pointer to the network device structure
 */
network_device_t* get_network_device() {
    return &nic;
}

/**
 * Check if network is initialized
 */
int is_network_initialized() {
    return network_initialized;
}

// ==================== NETWORK INFO FUNCTION ====================

/**
 * Print network information (uses getters)
 */
void network_print_info() {
    print("Network Status: ");
    if (is_network_initialized()) {
        print("INITIALIZED\n");
        
        network_device_t* net_dev = get_network_device();
        
        // Print MAC address
        print("MAC: ");
        for (int i = 0; i < 6; i++) {
            char buf[4];
            itoa(net_dev->mac_addr.addr[i], buf, 16);
            print(buf);
            if (i < 5) print(":");
        }
        print("\n");
        
        // Print IP address
        print("IP: ");
        for (int i = 0; i < 4; i++) {
            char buf[4];
            itoa(net_dev->ip_addr.addr[i], buf, 10);
            print(buf);
            if (i < 3) print(".");
        }
        print("\n");
        
        // Print gateway
        print("Gateway: ");
        for (int i = 0; i < 4; i++) {
            char buf[4];
            itoa(net_dev->gateway.addr[i], buf, 10);
            print(buf);
            if (i < 3) print(".");
        }
        print("\n");
    } else {
        print("NOT INITIALIZED\n");
    }
}
