#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "filesystem.h"
#include "process.h"
#include "timer.h"
#include "interrupts.h"
#include "splash.h"
#include "system.h"
#include "ports.h"
#include "editor.h"
#include "network.h"
#include "login.h"
#include <stddef.h>
#include "netcmds.h"
#define LINE_SIZE 128

// Forward declarations
void print_memory_info();
void manual_timer_test();
void test_process();
void test_network_detection();
void test_mac_address();
void test_network_packets();
void check_network_status();
void execute_command(char *input);

void print_memory_info() {
    print("Memory: 64MB total (simulated)\n");
    print("Free: 56MB\n");
    print("Used: 8MB\n");
}

// Simulate interrupts manually for testing
void manual_timer_test() {
    static int ticks = 0;
    ticks++;
    
    if (ticks % 500000 == 0) {
        print("[TICK:");
        print_hex(ticks);
        print("] ");
    }
}

void test_process() {
    while(1) {
        print("Background process running! \n");
        for(int i=0; i<1000000; i++) asm volatile("pause");
    }
}

void test_network_detection() {
    print("Testing NE2000 detection...\n");
    
    // Test if we can read from the NIC
    uint16_t base = 0x300;
    
    // Try to read from various registers to detect NIC
    for (int i = 0; i < 0x10; i++) {
        uint8_t value = inb(base + i);
        char buf[8];
        print("Register 0x");
        itoa(i, buf, 16);
        print(buf);
        print(": 0x");
        itoa(value, buf, 16);
        print(buf);
        print("\n");
    }
}

void test_mac_address() {
    print("Testing MAC address reading...\n");
    
    // Test MAC address reading (similar to network_init but just for testing)
    outb(0x300 + 0x00, 0x01); // Select page 1
    
    print("MAC Address: ");
    for (int i = 0; i < 6; i++) {
        uint8_t mac_byte = inb(0x300 + 0x01 + i);
        char buf[4];
        itoa(mac_byte, buf, 16);
        print(buf);
        if (i < 5) print(":");
    }
    print("\n");
    
    outb(0x300 + 0x00, 0x00); // Back to page 0
}

void test_network_packets() {
    print("\n=== Network Packet Test ===\n");
    
    network_device_t* net_dev = get_network_device();
    
    // Test 1: Send ARP request to gateway
    print("Sending ARP request to gateway...\n");
    send_arp_request(&net_dev->gateway);
    
    // Test 2: Try to receive packets
    print("Listening for packets (5 seconds)...\n");
    
    uint8_t buffer[1514];
    int packets_received = 0;
    
    for (int attempt = 0; attempt < 50; attempt++) {
        int length = network_receive_packet(buffer, sizeof(buffer));
        
        if (length > 0) {
            packets_received++;
            print("Packet received! Length: ");
            char len_buf[16];
            itoa(length, len_buf, 10);
            print(len_buf);
            print(" bytes\n");
            
            // Check packet type
            if (length >= (int)sizeof(eth_header_t)) {
                eth_header_t* eth = (eth_header_t*)buffer;
                uint16_t eth_type = ntohs(eth->type);
                
                if (eth_type == ETH_TYPE_ARP) {
                    print("-> ARP Packet\n");
                    handle_arp_packet(buffer, length);
                } else if (eth_type == ETH_TYPE_IP) {
                    print("-> IP Packet\n");
                    handle_ip_packet(buffer, length);
                } else {
                    print("-> Unknown Ethernet Type: 0x");
                    char type_buf[8];
                    itoa(eth_type, type_buf, 16);
                    print(type_buf);
                    print("\n");
                }
            }
        }
        
        // Small delay between checks
        for (volatile int i = 0; i < 100000; i++) {}
    }
    
    if (packets_received == 0) {
        print("No packets received during test period.\n");
    } else {
        print("Total packets received: ");
        char count_buf[16];
        itoa(packets_received, count_buf, 10);
        print(count_buf);
        print("\n");
    }
}

void check_network_status() {
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
    } else {
        print("NOT INITIALIZED\n");
    }
}

void execute_command(char *input) {
    // Simple argument parsing
    char* args[10];
    int argc = 0;
    int i = 0;
    int in_word = 0;
    
    // Parse input into arguments
    while (input[i] && argc < 10) {
        if (input[i] != ' ') {
            if (!in_word) {
                args[argc++] = &input[i];
                in_word = 1;
            }
        } else {
            input[i] = '\0';
            in_word = 0;
        }
        i++;
    }
    
    // Ensure last argument is terminated
    if (in_word && i < LINE_SIZE) {
        input[i] = '\0';
    }
    
    if (argc == 0) return;

    // Now use args[0] as command, args[1] as first argument, etc.
    if (strcmp(args[0], "help") == 0) {
        print("=============== JUPITER OS HELP ===============\n");
        print("| [FILE]    create, read, delete, ls         |\n");
        print("| [FILE]    append, info, cp, edit           |\n");
        print("| [SYSTEM]  clear, echo, meminfo, ps, calc   |\n");
        print("| [SYSTEM]  run, time, timer, sleep          |\n");
        print("| [INFO]    cpuinfo, osinfo, status, df      |\n");
        print("| [NETWORK] ping, ifconfig, arp              |\n");
        print("| [NETWORK] net, net test, net send          |\n");
        print("| [USER]    whoami, logout                   |\n");
        print("| [HELP]    help                             |\n");
        print("==============================================\n");
    }
    else if (strcmp(args[0], "clear") == 0) {
        clear_screen();
    }
    else if (strcmp(args[0], "calc") == 0) {
        calculator(argc, args);
    }
    else if (strcmp(args[0], "echo") == 0 && argc > 1) {
        for (int i=1; i<argc; i++){
            print(args[i]);
            if (i<argc - 1) print(" ");
        }
        print("\n");
    }
    else if (strcmp(args[0], "meminfo") == 0) {
        print_memory_info();
    }
    else if (strcmp(args[0], "create") == 0 && argc > 2) {
        // For create, manually build the content string
        // Find where the content starts in the original input
        char* content_start = args[2];
        // Restore spaces between words
        for (int j = 2; j < argc - 1; j++) {
            char* space_pos = args[j] + strlen(args[j]);
            if (space_pos < &input[LINE_SIZE - 1]) {
                *space_pos = ' ';  // Restore space
            }
        }
        fs_create(args[1], content_start);
    }
    else if (strcmp(args[0], "read") == 0 && argc > 1) {
        const char* content = fs_read(args[1]);
        print(content); print("\n");
    }
    else if (strcmp(args[0], "delete") == 0 && argc > 1) {
        fs_delete(args[1]);
    }
    else if (strcmp(args[0], "ls") == 0) {
        fs_list();
    }
    else if (strcmp(args[0], "ps") == 0) {
        list_processes();
    }
    else if (strcmp(args[0], "run") == 0 && argc > 1) {
        print("Process execution coming soon!\n");
    }
    else if (strcmp(args[0], "time") == 0) {
        print("Uptime: "); print_hex(get_ticks()); print(" ms\n");
    }
    else if (strcmp(args[0], "sleep") == 0 && argc > 1) {
        int ms = 0;
        char* p = args[1];
        while (*p >= '0' && *p <= '9') {
            ms = ms * 10 + (*p - '0');
            p++;
        }
        print("Sleeping for "); print_hex(ms); print(" ms...\n");
        sleep(ms); print("Awake!\n");
    }
    else if (strcmp(args[0], "timer") == 0) {
        print("Timer ticks: "); print_hex(get_ticks()); print("\n");
    }
    else if (strcmp(args[0], "append") == 0 && argc > 2) {
        // For append, manually build the content string
        char* content_start = args[2];
        // Restore spaces between words
        for (int j = 2; j < argc - 1; j++) {
            char* space_pos = args[j] + strlen(args[j]);
            if (space_pos < &input[LINE_SIZE - 1]) {
                *space_pos = ' ';  // Restore space
            }
        }
        fs_append(args[1], content_start);
    }
    else if (strcmp(args[0], "info") == 0 && argc > 1) {
        fs_info(args[1]);
    }
    else if (strcmp(args[0], "cp") == 0 && argc > 2) {
        fs_copy(args[1], args[2]);
    }
    else if (strcmp(args[0], "edit") == 0 && argc > 1) {
        launch_editor(args[1]);
    }
    else if (strcmp(args[0], "cpuinfo") == 0) {
        show_cpuinfo();
    }
    else if (strcmp(args[0], "osinfo") == 0) {
        show_osinfo();
    }
    else if (strcmp(args[0], "status") == 0) {
        show_status();
    }
    else if (strcmp(args[0], "df") == 0) {
        show_diskinfo();
    }
        else if (strcmp(args[0], "ping") == 0) {
        cmd_ping(argc, args);
    }
    else if (strcmp(args[0], "ifconfig") == 0) {
        cmd_ifconfig();
    }
    else if (strcmp(args[0], "arp") == 0) {
        cmd_arp(argc, args);
    }
    else if (strcmp(args[0], "net") == 0) {
        if (argc > 1 && strcmp(args[1], "test") == 0) {
            test_network_packets();
        } else if (argc > 2 && strcmp(args[1], "send") == 0) {
            /* net send <ip> <msg>  →  shift args so args[1]=ip, args[2]=msg */
            cmd_netsend(argc - 1, args + 1);
        } else {
            check_network_status();
        }
    }
    else if (strcmp(args[0], "whoami") == 0) {
        int current_user = get_current_user_id();
        if (current_user >= 0) {
            print(get_username(current_user));
            if (is_user_admin(current_user)) {
                print(" (Administrator)");
            }
            print("\n");
        } else {
            print("Not logged in\n");
        }
    }
    else if (strcmp(args[0], "logout") == 0) {
        print("Logging out...\n");
        // For now, just reboot to show login screen again
        print("System will reboot to login screen\n");
        for (volatile int i = 0; i < 3000000; i++) {}
        asm volatile("jmp kmain"); // Simple reboot
    }
    else if (strcmp(args[0], "shutdown") == 0) {
        print("System shutting down...\n");
        while(1) { asm volatile("hlt"); } // Halt forever
    }
    else if (strcmp(args[0], "reboot") == 0) {
        print("Rebooting system...\n");
        for (volatile int i = 0; i < 3000000; i++) {}
        asm volatile("jmp kmain"); // Simple reboot
    }
    else {
        print("Unknown command: "); print(args[0]); print("\n");
    }
}

void kmain(unsigned int magic, unsigned int *mb_info) {
    (void)mb_info;
    
    static int boot_count = 0;
    boot_count++;
    
    if (magic != 0x2BADB002) {
        print("Error: Not loaded by Multiboot-compliant loader\n");
        return;
    }  
    char line[LINE_SIZE];
    clear_screen();
    
    // Boot animation and splash
    show_boot_animation();
    show_splash();
    
    print("Boot #"); print_hex(boot_count); print("\n");
    
    // Initialize systems
    fs_init();
    init_scheduler();
    init_interrupts();
    init_timer();
    enable_interrupts();
    
    // Initialize users and show login screen
    init_users();
    show_login_screen();
    
    print("Welcome to JupiterOS Shell!\n");
    print("Interrupts: DISABLED (Safe Mode)\n");
    
    // Test network hardware
    print("Testing network hardware...\n");
    test_network_detection();
    test_mac_address();
    
    // Initialize network
    network_init();
    
    print("System ready\n");

    // Main shell loop
    while(1) {
        manual_timer_test();
        
        // Show username in prompt if logged in
        int current_user = get_current_user_id();
        if (current_user >= 0) {
            print(get_username(current_user));
            print("@jupiteros> ");
        } else {
            print("> ");
        }
        
        get_line(line, LINE_SIZE);
        if (line[0] == 0) continue;
        execute_command(line);
        
        // Check for network packets in the main loop
        uint8_t buffer[1514];
        int length = network_receive_packet(buffer, sizeof(buffer));
        
        if (length > 0) {
            // Handle received packet
            if (length >= (int)sizeof(eth_header_t)) {
                eth_header_t* eth = (eth_header_t*)buffer;
                uint16_t eth_type = ntohs(eth->type);
                
                if (eth_type == ETH_TYPE_ARP) {
                    handle_arp_packet(buffer, length);
                } else if (eth_type == ETH_TYPE_IP) {
                    handle_ip_packet(buffer, length);
                }
            }
        }
        
        // Small delay to prevent CPU hogging
        for (int i = 0; i < 10000; i++) asm volatile("pause");
    }
}
