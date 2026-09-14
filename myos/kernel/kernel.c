#include "vm.h"
#include "paging.h"
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
#include "memory.h"
#include "netcmds.h"

#define LINE_SIZE 128

#define PAGING_TEST_VIRTUAL_ADDRESS 0x00400000U
#define PAGING_TEST_VALUE           0x4A555049U
#define PAGE_FAULT_TEST_ADDRESS     0x00800000U


/* Forward declarations */
void print_memory_info();
void manual_timer_test();
void test_process();
void test_network_detection();
void test_mac_address();
void test_network_packets();
void check_network_status();
void memory_test(void);
void execute_command(char *input);


/*
 * ============================================================
 * Memory information
 * ============================================================
 */

void print_memory_info()
{
    size_t total = mem_get_total();
    size_t used = mem_get_used();
    size_t free = mem_get_free();

    print("Memory Information:\n");

    print("Total: ");
    print_hex((unsigned int)total);
    print(" bytes\n");

    print("Used: ");
    print_hex((unsigned int)used);
    print(" bytes\n");

    print("Free: ");
    print_hex((unsigned int)free);
    print(" bytes\n");
}


/*
 * ============================================================
 * Manual timer test
 * ============================================================
 */

void manual_timer_test()
{
    static int ticks = 0;

    ticks++;

    if (ticks % 500000 == 0) {
        print("[TICK:");
        print_hex(ticks);
        print("] ");
    }
}


/*
 * ============================================================
 * Process test
 * ============================================================
 */

void test_process()
{
    print(">>> PID 1 STARTED <<<\n");

    for (volatile int i = 0; i < 5000000; i++)
        asm volatile("pause");

    print(">>> PID 1 FINISHED <<<\n");
    print(">>> PID 1 RETURNING NOW <<<\n");
}


/*
 * ============================================================
 * Network detection test
 * ============================================================
 */

void test_network_detection()
{
    print("Testing NE2000 detection...\n");

    uint16_t base = 0x300;

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


/*
 * ============================================================
 * MAC address test
 * ============================================================
 */

void test_mac_address()
{
    print("Testing MAC address reading...\n");

    outb(0x300 + 0x00, 0x01);

    print("MAC Address: ");

    for (int i = 0; i < 6; i++) {
        uint8_t mac_byte = inb(0x300 + 0x01 + i);
        char buf[4];

        itoa(mac_byte, buf, 16);
        print(buf);

        if (i < 5)
            print(":");
    }

    print("\n");

    outb(0x300 + 0x00, 0x00);
}


/*
 * ============================================================
 * Network packet test
 * ============================================================
 */

void test_network_packets()
{
    print("\n=== Network Packet Test ===\n");

    network_device_t *net_dev = get_network_device();

    print("Sending ARP request to gateway...\n");
    send_arp_request(&net_dev->gateway);

    print("Listening for packets (5 seconds)...\n");

    uint8_t buffer[1514];
    int packets_received = 0;

    for (int attempt = 0; attempt < 50; attempt++) {

        int length =
            network_receive_packet(buffer, sizeof(buffer));

        if (length > 0) {
            packets_received++;

            print("Packet received! Length: ");

            char len_buf[16];

            itoa(length, len_buf, 10);
            print(len_buf);
            print(" bytes\n");

            if (length >= (int)sizeof(eth_header_t)) {

                eth_header_t *eth =
                    (eth_header_t *)buffer;

                uint16_t eth_type =
                    ntohs(eth->type);

                if (eth_type == ETH_TYPE_ARP) {

                    print("-> ARP Packet\n");

                    handle_arp_packet(
                        buffer,
                        length
                    );

                } else if (eth_type == ETH_TYPE_IP) {

                    print("-> IP Packet\n");

                    handle_ip_packet(
                        buffer,
                        length
                    );

                } else {

                    print("-> Unknown Ethernet Type: 0x");

                    char type_buf[8];

                    itoa(
                        eth_type,
                        type_buf,
                        16
                    );

                    print(type_buf);
                    print("\n");
                }
            }
        }

        for (volatile int i = 0; i < 100000; i++) {}
    }

    if (packets_received == 0) {

        print(
            "No packets received during test period.\n"
        );

    } else {

        print("Total packets received: ");

        char count_buf[16];

        itoa(
            packets_received,
            count_buf,
            10
        );

        print(count_buf);
        print("\n");
    }
}


/*
 * ============================================================
 * Network status
 * ============================================================
 */

void check_network_status()
{
    print("Network Status: ");

    if (is_network_initialized()) {

        print("INITIALIZED\n");

        network_device_t *net_dev =
            get_network_device();

        print("MAC: ");

        for (int i = 0; i < 6; i++) {

            char buf[4];

            itoa(
                net_dev->mac_addr.addr[i],
                buf,
                16
            );

            print(buf);

            if (i < 5)
                print(":");
        }

        print("\n");

        print("IP: ");

        for (int i = 0; i < 4; i++) {

            char buf[4];

            itoa(
                net_dev->ip_addr.addr[i],
                buf,
                10
            );

            print(buf);

            if (i < 3)
                print(".");
        }

        print("\n");

    } else {

        print("NOT INITIALIZED\n");
    }
}


/*
 * ============================================================
 * Paging mapping test
 * ============================================================
 */

static void paging_mapping_test(void)
{
    void *physical_page;
    volatile uint32_t *mapped_page;
    int passed;

    physical_page = pmm_alloc_page();

    if (!physical_page) {
        print("Dynamic mapping test: FAIL\n");
        return;
    }

    if (map_page(
            PAGING_TEST_VIRTUAL_ADDRESS,
            (uint32_t)physical_page,
            PAGE_PRESENT | PAGE_WRITABLE) < 0) {

        pmm_free_page(physical_page);

        print("Dynamic mapping test: FAIL\n");

        return;
    }

    mapped_page =
        (volatile uint32_t *)PAGING_TEST_VIRTUAL_ADDRESS;

    *mapped_page = PAGING_TEST_VALUE;

    passed =
        (*mapped_page == PAGING_TEST_VALUE);

    if (unmap_page(PAGING_TEST_VIRTUAL_ADDRESS) < 0) {

        print("Dynamic mapping test: FAIL\n");

        return;
    }

    pmm_free_page(physical_page);

    if (passed)
        print("Dynamic mapping test: PASS\n");
    else
        print("Dynamic mapping test: FAIL\n");
}


/*
 * ============================================================
 * Command execution
 * ============================================================
 */

void execute_command(char *input)
{
    char *args[10];

    int argc = 0;
    int i = 0;
    int in_word = 0;


    /*
     * Parse input into arguments.
     */
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


    /*
     * Ensure last argument is terminated.
     */
    if (in_word && i < LINE_SIZE)
        input[i] = '\0';


    if (argc == 0)
        return;


    /*
     * ========================================================
     * HELP
     * ========================================================
     */

    if (strcmp(args[0], "help") == 0) {

        print("=============== JUPITER OS HELP =========================\n");
        print("| [FILE]    create, read, delete, ls                     |\n");
        print("| [FILE]    append, info, cp, edit                       |\n");
        print("| [SYSTEM]  clear, echo, meminfo, ps, calc               |\n");
        print("| [MEMORY]  memtest, pmtest, pmm, paging, pf, vmtest      |\n");
        print("| [SYSTEM]  run, time, timer, sleep, memmap, uptime       |\n");
        print("| [INFO]    cpuinfo, osinfo, status, df                  |\n");
        print("| [NETWORK] ping, ifconfig, arp                          |\n");
        print("| [NETWORK] net, net test, net send                      |\n");
        print("| [USER]    whoami, logout                               |\n");
        print("| [HELP]    help                                         |\n");
        print("==========================================================\n");
    }


    /*
     * ========================================================
     * CLEAR
     * ========================================================
     */

    else if (strcmp(args[0], "clear") == 0) {

        clear_screen();
    }


    /*
     * ========================================================
     * CALCULATOR
     * ========================================================
     */

    else if (strcmp(args[0], "calc") == 0) {

        calculator(argc, args);
    }


    /*
     * ========================================================
     * ECHO
     * ========================================================
     */

    else if (strcmp(args[0], "echo") == 0 && argc > 1) {

        for (int i = 1; i < argc; i++) {

            print(args[i]);

            if (i < argc - 1)
                print(" ");
        }

        print("\n");
    }


    /*
     * ========================================================
     * MEMORY INFORMATION
     * ========================================================
     */

    else if (strcmp(args[0], "meminfo") == 0) {

        print_memory_info();
    }


    /*
     * ========================================================
     * MEMORY MAP
     * ========================================================
     */

    else if (strcmp(args[0], "memmap") == 0) {

        mem_print_map();
    }


    /*
     * ========================================================
     * CREATE
     * ========================================================
     */

    else if (strcmp(args[0], "create") == 0 &&
             argc > 1) {

        if (fs_create(args[1], "")) {
            launch_editor(args[1]);
        }
    }


    /*
     * ========================================================
     * READ
     * ========================================================
     */

    else if (strcmp(args[0], "read") == 0 &&
             argc > 1) {

        const char *content =
            fs_read(args[1]);

        print(content);
        print("\n");
    }


    /*
     * ========================================================
     * DELETE
     * ========================================================
     */

    else if (strcmp(args[0], "delete") == 0 &&
             argc > 1) {

        fs_delete(args[1]);
    }


    /*
     * ========================================================
     * LIST FILES
     * ========================================================
     */

    else if (strcmp(args[0], "ls") == 0) {

        fs_list();
    }


    /*
     * ========================================================
     * PROCESS LIST
     * ========================================================
     */

    else if (strcmp(args[0], "ps") == 0) {

        list_processes();
    }


    /*
     * ========================================================
     * RUN PROCESS
     * ========================================================
     */

    else if (strcmp(args[0], "run") == 0 &&
             argc > 1) {

        int pid =
            create_process(
                test_process,
                args[1],
                1
            );

        if (pid >= 0)
            print("Process created successfully.\n");
    }


    /*
     * ========================================================
     * YIELD
     * ========================================================
     */

    else if (strcmp(args[0], "yield") == 0) {

        yield();
    }


    /*
     * ========================================================
     * TIME
     * ========================================================
     */

    else if (strcmp(args[0], "time") == 0) {

        print("Uptime: ");
        print_hex(get_ticks());
        print(" ms\n");
    }


    /*
     * ========================================================
     * SLEEP
     * ========================================================
     */

    else if (strcmp(args[0], "sleep") == 0 &&
             argc > 1) {

        int ms = 0;

        char *p = args[1];

        while (*p >= '0' && *p <= '9') {

            ms =
                ms * 10 +
                (*p - '0');

            p++;
        }

        print("Sleeping for ");
        print_hex(ms);
        print(" ms...\n");

        sleep(ms);

        print("Awake!\n");
    }


    /*
     * ========================================================
     * TIMER
     * ========================================================
     */

    else if (strcmp(args[0], "timer") == 0) {

        print("Timer ticks: ");
        print_hex(get_ticks());
        print("\n");
    }


    /*
     * ========================================================
     * UPTIME
     * ========================================================
     */

    else if (strcmp(args[0], "uptime") == 0) {

        unsigned long ticks = get_ticks();

        unsigned long total_seconds =
            ticks / 100;

        unsigned long hours =
            total_seconds / 3600;

        unsigned long minutes =
            (total_seconds % 3600) / 60;

        unsigned long seconds =
            total_seconds % 60;

        print("JupiterOS uptime: ");

        print_int((int)hours);
        print("h ");

        print_int((int)minutes);
        print("m ");

        print_int((int)seconds);
        print("s\n");
    }


    /*
     * ========================================================
     * APPEND
     * ========================================================
     */

    else if (strcmp(args[0], "append") == 0 &&
             argc > 2) {

        char *content_start = args[2];

        for (int j = 2; j < argc - 1; j++) {

            char *space_pos =
                args[j] + strlen(args[j]);

            if (space_pos <
                &input[LINE_SIZE - 1]) {

                *space_pos = ' ';
            }
        }

        fs_append(
            args[1],
            content_start
        );
    }


    /*
     * ========================================================
     * PMM
     * ========================================================
     */

    else if (strcmp(args[0], "pmm") == 0) {

        pmm_print_stats();
    }


    /*
     * ========================================================
     * PAGING
     * ========================================================
     */

    else if (strcmp(args[0], "paging") == 0) {

        if (paging_is_enabled()) {

            print("Paging: ENABLED\n");

        } else {

            print("Paging: DISABLED\n");
            print("Dynamic mapping test: FAIL\n");

            return;
        }

        paging_mapping_test();
    }


    /*
     * ========================================================
     * FILE INFO
     * ========================================================
     */

    else if (strcmp(args[0], "info") == 0 &&
             argc > 1) {

        fs_info(args[1]);
    }


    /*
     * ========================================================
     * PAGE FAULT TEST
     * ========================================================
     */

    else if (strcmp(args[0], "pf") == 0) {

        volatile uint32_t *fault_address =
            (volatile uint32_t *)PAGE_FAULT_TEST_ADDRESS;

        volatile uint32_t value;

        print("Triggering page fault...\n");

        /*
         * First access is a WRITE.
         *
         * The page is intentionally unmapped.
         * The page fault handler allocates a physical
         * page and maps it.
         *
         * After iret, the CPU retries this instruction.
         */
        *fault_address = 0x4A555049U;

        /*
         * Read the value back from the newly mapped page.
         */
        value = *fault_address;

        print("Recovered page value: ");
        print_hex(value);
        print("\n");

        if (value == 0x4A555049U)
            print("Page fault recovery test: PASS\n");
        else
            print("Page fault recovery test: FAIL\n");
    }


    /*
     * ========================================================
     * COPY
     * ========================================================
     */

    else if (strcmp(args[0], "cp") == 0 &&
             argc > 2) {

        fs_copy(
            args[1],
            args[2]
        );
    }


    /*
     * ========================================================
     * EDIT
     * ========================================================
     */

    else if (strcmp(args[0], "edit") == 0 &&
             argc > 1) {

        launch_editor(args[1]);
    }


    /*
     * ========================================================
     * CPU INFO
     * ========================================================
     */

    else if (strcmp(args[0], "cpuinfo") == 0) {

        show_cpuinfo();
    }


    /*
     * ========================================================
     * OS INFO
     * ========================================================
     */

    else if (strcmp(args[0], "osinfo") == 0) {

        show_osinfo();
    }


    /*
     * ========================================================
     * STATUS
     * ========================================================
     */

    else if (strcmp(args[0], "status") == 0) {

        show_status();
    }


    /*
     * ========================================================
     * DISK INFO
     * ========================================================
     */

    else if (strcmp(args[0], "df") == 0) {

        show_diskinfo();
    }


    /*
     * ========================================================
     * PING
     * ========================================================
     */

    else if (strcmp(args[0], "ping") == 0) {

        cmd_ping(argc, args);
    }


    /*
     * ========================================================
     * IFCONFIG
     * ========================================================
     */

    else if (strcmp(args[0], "ifconfig") == 0) {

        cmd_ifconfig();
    }


    /*
     * ========================================================
     * MEMORY TEST
     * ========================================================
     */

    else if (strcmp(args[0], "memtest") == 0) {

        memory_test();
    }


    /*
     * ========================================================
     * VM TEST
     * ========================================================
     */

    else if (strcmp(args[0], "vmtest") == 0) {

        volatile uint32_t *page1;
        volatile uint32_t *page2;
        volatile uint32_t *page3;

        int passed = 1;

        print("\nVirtual Memory Test\n");
        print("===================\n");


        /*
         * Allocate three virtual pages.
         */
        page1 =
            (volatile uint32_t *)vm_alloc_page();

        page2 =
            (volatile uint32_t *)vm_alloc_page();

        page3 =
            (volatile uint32_t *)vm_alloc_page();


        /*
         * Display virtual addresses.
         */
        print("Page 1: ");
        print_hex((uint32_t)page1);
        print("\n");

        print("Page 2: ");
        print_hex((uint32_t)page2);
        print("\n");

        print("Page 3: ");
        print_hex((uint32_t)page3);
        print("\n");


        /*
         * Verify allocation.
         */
        if (!page1 ||
            !page2 ||
            !page3) {

            print("Allocation: FAIL\n");

            if (page1)
                vm_free_page((void *)page1);

            if (page2)
                vm_free_page((void *)page2);

            if (page3)
                vm_free_page((void *)page3);

            return;
        }


        /*
         * Write different values to each page.
         */
        *page1 = 0x11111111U;
        *page2 = 0x22222222U;
        *page3 = 0x33333333U;


        /*
         * Verify mappings.
         */
        if (*page1 != 0x11111111U)
            passed = 0;

        if (*page2 != 0x22222222U)
            passed = 0;

        if (*page3 != 0x33333333U)
            passed = 0;


        if (passed)
            print("Mapping/read/write: PASS\n");
        else
            print("Mapping/read/write: FAIL\n");


        /*
         * Free all pages.
         */
        if (vm_free_page((void *)page1) < 0)
            passed = 0;

        if (vm_free_page((void *)page2) < 0)
            passed = 0;

        if (vm_free_page((void *)page3) < 0)
            passed = 0;


        if (passed)
            print("Free/reclaim: PASS\n");
        else
            print("Free/reclaim: FAIL\n");


        print("VMM test: ");

        if (passed)
            print("PASS\n");
        else
            print("FAIL\n");
    }


    /*
     * ========================================================
     * PMM ALLOCATION TEST
     * ========================================================
     */

    else if (strcmp(args[0], "pmtest") == 0) {

        size_t before =
            pmm_get_free_pages();

        void *page1 =
            pmm_alloc_page();

        void *page2 =
            pmm_alloc_page();


        print("\nPMM Allocation Test\n");
        print("===================\n");

        print("Free before: ");
        print_int((int)before);
        print("\n");

        print("Page 1: ");
        print_hex((uint32_t)page1);
        print("\n");

        print("Page 2: ");
        print_hex((uint32_t)page2);
        print("\n");

        print("Free after allocation: ");
        print_int(
            (int)pmm_get_free_pages()
        );
        print("\n");


        pmm_free_page(page1);
        pmm_free_page(page2);


        print("Free after free: ");
        print_int(
            (int)pmm_get_free_pages()
        );
        print("\n");
    }


    /*
     * ========================================================
     * ARP
     * ========================================================
     */

    else if (strcmp(args[0], "arp") == 0) {

        cmd_arp(argc, args);
    }


    /*
     * ========================================================
     * NETWORK
     * ========================================================
     */

    else if (strcmp(args[0], "net") == 0) {

        if (argc > 1 &&
            strcmp(args[1], "test") == 0) {

            test_network_packets();

        } else if (argc > 2 &&
                   strcmp(args[1], "send") == 0) {

            cmd_netsend(
                argc - 1,
                args + 1
            );

        } else {

            check_network_status();
        }
    }


    /*
     * ========================================================
     * WHOAMI
     * ========================================================
     */

    else if (strcmp(args[0], "whoami") == 0) {

        int current_user =
            get_current_user_id();

        if (current_user >= 0) {

            print(
                get_username(current_user)
            );

            if (is_user_admin(current_user))
                print(" (Administrator)");

            print("\n");

        } else {

            print("Not logged in\n");
        }
    }


    /*
     * ========================================================
     * LOGOUT
     * ========================================================
     */

    else if (strcmp(args[0], "logout") == 0) {

        print("Logging out...\n");
        print("System will reboot to login screen\n");

        for (volatile int i = 0;
             i < 3000000;
             i++) {}

        asm volatile("jmp kmain");
    }


    /*
     * ========================================================
     * SHUTDOWN
     * ========================================================
     */

    else if (strcmp(args[0], "shutdown") == 0) {

        print("System shutting down...\n");

        while (1)
            asm volatile("hlt");
    }


    /*
     * ========================================================
     * REBOOT
     * ========================================================
     */

    else if (strcmp(args[0], "reboot") == 0) {

        print("Rebooting system...\n");

        for (volatile int i = 0;
             i < 3000000;
             i++) {}

        asm volatile("jmp kmain");
    }


    /*
     * ========================================================
     * UNKNOWN COMMAND
     * ========================================================
     */

    else {

        print("Unknown command: ");
        print(args[0]);
        print("\n");
    }
}


/*
 * ============================================================
 * Basic memory allocation test
 * ============================================================
 */

void memory_test()
{
    print("Memory allocation test:\n");

    void *a = kmalloc(1024);
    void *b = kmalloc(4096);
    void *c = kmalloc(16384);

    if (a && b && c) {

        print("Allocated: 21 KB\n");

    } else {

        print("Allocation failed\n");
    }

    print_memory_info();
}


/*
 * ============================================================
 * Kernel entry point
 * ============================================================
 */

void kmain(unsigned int magic,
           unsigned int *mb_info)
{
    (void)mb_info;

    static int boot_count = 0;

    boot_count++;


    /*
     * Verify Multiboot magic.
     */
    if (magic != 0x2BADB002) {

        print(
            "Error: Not loaded by Multiboot-compliant loader\n"
        );

        return;
    }


    char line[LINE_SIZE];


    /*
     * Clear screen and show boot UI.
     */
    clear_screen();

    show_boot_animation();
    show_splash();

    print("Boot #");
    print_hex(boot_count);
    print("\n");


    /*
     * ========================================================
     * Initialize memory subsystem
     * ========================================================
     */

    mem_init();

    mem_detect_multiboot(mb_info);

    pmm_init(mb_info);

    init_paging();

    vm_init();


    /*
     * ========================================================
     * Initialize remaining systems
     * ========================================================
     */

    fs_init();

    init_scheduler();

    init_interrupts();

    init_timer();

    disable_interrupts();


    /*
     * ========================================================
     * Users / login
     * ========================================================
     */

    init_users();

    show_login_screen();

    print("Welcome to JupiterOS Shell!\n");
    print("Interrupts: DISABLED (Safe Mode)\n");


    /*
     * ========================================================
     * Network
     * ========================================================
     */

    network_init();

    print("System ready\n");


    /*
     * ========================================================
     * Main shell
     * ========================================================
     */

    while (1) {

        manual_timer_test();


        /*
         * Display username in prompt.
         */
        int current_user =
            get_current_user_id();

        if (current_user >= 0) {

            print(
                get_username(current_user)
            );

            print("@jupiteros> ");

        } else {

            print("> ");
        }


        /*
         * Read command.
         */
        get_line(
            line,
            LINE_SIZE
        );


        if (line[0] == 0)
            continue;


        /*
         * Execute command.
         */
        execute_command(line);


        /*
         * ====================================================
         * Check for network packets
         * ====================================================
         */

        uint8_t buffer[1514];

        int length =
            network_receive_packet(
                buffer,
                sizeof(buffer)
            );

        if (length > 0) {

            if (length >=
                (int)sizeof(eth_header_t)) {

                eth_header_t *eth =
                    (eth_header_t *)buffer;

                uint16_t eth_type =
                    ntohs(eth->type);


                if (eth_type == ETH_TYPE_ARP) {

                    handle_arp_packet(
                        buffer,
                        length
                    );

                } else if (eth_type == ETH_TYPE_IP) {

                    handle_ip_packet(
                        buffer,
                        length
                    );
                }
            }
        }


        /*
         * Small delay to prevent CPU hogging.
         */
        for (int i = 0;
             i < 10000;
             i++)
            asm volatile("pause");
    }
}