#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>

// Network constants
#define ETH_ALEN 6
#define IP_ALEN 4
#define ETH_TYPE_IP 0x0800
#define ETH_TYPE_ARP 0x0806
#define ARP_HTYPE_ETHER 1
#define ARP_PTYPE_IP 0x0800
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

// MAC address type
typedef struct {
    uint8_t addr[ETH_ALEN];
} mac_addr_t;

// IP address type
typedef struct {
    uint8_t addr[IP_ALEN];
} ip_addr_t;

// Ethernet header
typedef struct {
    uint8_t dest[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

// ARP header
typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t opcode;
    uint8_t sender_mac[ETH_ALEN];
    uint8_t sender_ip[IP_ALEN];
    uint8_t target_mac[ETH_ALEN];
    uint8_t target_ip[IP_ALEN];
} __attribute__((packed)) arp_header_t;

// IP header
typedef struct {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src_ip[IP_ALEN];
    uint8_t dest_ip[IP_ALEN];
} __attribute__((packed)) ip_header_t;

// Network device structure
typedef struct {
    uint16_t io_base;
    mac_addr_t mac_addr;
    ip_addr_t ip_addr;
    ip_addr_t netmask;
    ip_addr_t gateway;
} network_device_t;

// Function prototypes
void network_init();
int network_send_packet(const void* data, size_t length);
int network_receive_packet(void* buffer, size_t buffer_size);
void handle_arp_packet(const void* packet, size_t length);
void handle_ip_packet(const void* packet, size_t length);
void send_arp_request(const ip_addr_t* target_ip);
void arp_cache_add(const ip_addr_t* ip, const mac_addr_t* mac);
int arp_cache_lookup(const ip_addr_t* ip, mac_addr_t* mac);
void network_print_info();

// Utility functions
uint16_t ntohs(uint16_t netshort);
uint16_t htons(uint16_t hostshort);
uint16_t calculate_checksum(const void* data, size_t length);

// Getter functions (Option 2) - REPLACED extern declarations with these
network_device_t* get_network_device();
int is_network_initialized();

#endif
