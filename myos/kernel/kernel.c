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

void print_memory_info();
void manual_timer_test();
void test_process();
void test_network_detection();
void test_mac_address();
void test_network_packets();
void check_network_status();
void memory_test(void);
void execute_command(char *input);

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

void test_process()
{
    print(">>> PID 1 STARTED <<<\n");

    for (volatile int i = 0; i < 5000000; i++)
        asm volatile("pause");

    print(">>> PID 1 FINISHED <<<\n");
    print(">>> PID 1 RETURNING NOW <<<\n");
}

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

static void vm_multi_page_test(void)
{
    volatile uint32_t *memory;
    volatile uint32_t *memory2;
    size_t i;

    print("\nMulti-Page Virtual Memory Test\n");
    print("===============================\n");

    memory = (volatile uint32_t *)vm_alloc_pages(4);

    if (!memory) {
        print("Allocation: FAIL\n");
        return;
    }

    print("Base address: ");
    print_hex((uint32_t)memory);
    print("\n");

    if ((uint32_t)&memory[1024] !=
        (uint32_t)memory + 0x1000U) {

        print("Contiguous virtual range: FAIL\n");

        vm_free_pages((void *)memory, 4);
        return;
    }

    print("Contiguous virtual range: PASS\n");

    for (i = 0; i < 4; i++) {
        volatile uint32_t *page =
            (volatile uint32_t *)(
                (uint32_t)memory +
                (uint32_t)(i * 0x1000U)
            );

        *page = 0x4A555000U + (uint32_t)i;
    }

    for (i = 0; i < 4; i++) {
        volatile uint32_t *page =
            (volatile uint32_t *)(
                (uint32_t)memory +
                (uint32_t)(i * 0x1000U)
            );

        if (*page != 0x4A555000U + (uint32_t)i) {

            print("Mapping/read/write: FAIL\n");

            vm_free_pages((void *)memory, 4);
            return;
        }
    }

    print("Mapping/read/write: PASS\n");

    if (vm_free_pages((void *)memory, 4) != 0) {
        print("Free: FAIL\n");
        return;
    }

    print("Free: PASS\n");

    memory2 = (volatile uint32_t *)vm_alloc_pages(4);

    if (!memory2) {
        print("Reallocation: FAIL\n");
        return;
    }

    if ((uint32_t)memory2 != (uint32_t)memory) {
        print("Virtual range reuse: FAIL\n");
        vm_free_pages((void *)memory2, 4);
        return;
    }

    print("Virtual range reuse: PASS\n");

    vm_free_pages((void *)memory2, 4);

    print("Multi-page VMM test: PASS\n");
}

static void kernel_vmm_heap_test(void)
{
    size_t before;
    size_t after_alloc;
    size_t after_free;
    size_t i;
    volatile uint32_t *a;
    volatile uint32_t *b;
    volatile uint32_t *c;
    volatile uint32_t *test;
    int passed = 1;

    print("\nKernel VMM Heap Test\n");
    print("=====================\n");

    before = vm_get_used_pages();

    a = (volatile uint32_t *)kvmalloc(64);
    b = (volatile uint32_t *)kvmalloc(4096);
    c = (volatile uint32_t *)kvmalloc(10000);

    if (!a || !b || !c) {
        print("Allocation: FAIL\n");

        if (a)
            kvfree((void *)a);
        if (b)
            kvfree((void *)b);
        if (c)
            kvfree((void *)c);

        return;
    }

    after_alloc = vm_get_used_pages();

    if (after_alloc != before + 6)
        passed = 0;

    if (passed)
        print("Allocation: PASS\n");
    else
        print("Allocation: FAIL\n");

    *a = 0x11111111U;
    *b = 0x22222222U;
    *c = 0x33333333U;

    if (*a != 0x11111111U)
        passed = 0;
    if (*b != 0x22222222U)
        passed = 0;
    if (*c != 0x33333333U)
        passed = 0;

    *((volatile uint8_t *)c + 9999) = 0x5AU;

    if (*((volatile uint8_t *)c + 9999) != 0x5AU)
        passed = 0;

    if (passed)
        print("Mapping/read/write: PASS\n");
    else
        print("Mapping/read/write: FAIL\n");

    kvfree((void *)a);
    kvfree((void *)b);
    kvfree((void *)c);

    after_free = vm_get_used_pages();

    if (after_free != before)
        passed = 0;

    if (after_free == before)
        print("VMM page reclamation: PASS\n");
    else
        print("VMM page reclamation: FAIL\n");

    for (i = 0; i < 16; i++) {
        test = (volatile uint32_t *)kvmalloc(1234);

        if (!test) {
            passed = 0;
            break;
        }

        *test = 0x4A550000U + (uint32_t)i;

        if (*test != 0x4A550000U + (uint32_t)i)
            passed = 0;

        kvfree((void *)test);

        if (vm_get_used_pages() != before)
            passed = 0;
    }

    if (passed)
        print("Repeated allocations: PASS\n");
    else
        print("Repeated allocations: FAIL\n");

    print("Kernel VMM heap test: ");

    if (passed)
        print("PASS\n");
    else
        print("FAIL\n");
}

static void kernel_vmm_heap_stress_test(void)
{
    size_t before;
    size_t after_alloc;
    size_t after_free;
    size_t i;
    size_t sizes[] = {
        1,
        100,
        4095,
        4096,
        4097,
        8192,
        16384,
        65536
    };

    void *blocks[8] = {0};

    void *a = (void *)0;
    void *b = (void *)0;
    void *c = (void *)0;
    void *d = (void *)0;
    void *e = (void *)0;
    void *f = (void *)0;

    volatile uint8_t *test;

    int passed = 1;
    int variable_passed = 1;
    int fragmentation_passed = 1;

    print("\nKernel VMM Heap Stress Test\n");
    print("============================\n");

    before = vm_get_used_pages();

    for (i = 0; i < 8; i++) {
        blocks[i] = kvmalloc(sizes[i]);

        if (!blocks[i]) {
            print("Size allocation failed: ");
            print_int((int)sizes[i]);
            print(" bytes\n");

            variable_passed = 0;
            break;
        }

        test = (volatile uint8_t *)blocks[i];

        test[0] = (uint8_t)(0x10U + i);
	if (test[0] != (uint8_t)(0x10U + i)) {
	    print("Size read/write failed: ");
	    print_int((int)sizes[i]);
	    print(" bytes\n");
	    variable_passed = 0;
	    break;
	}
	if (sizes[i] > 1) {
	    test[sizes[i] - 1] = (uint8_t)(0x80U + i);
	    if (test[sizes[i] - 1] != (uint8_t)(0x80U + i)) {
		print("Size read/write failed: ");
		print_int((int)sizes[i]);
		print(" bytes\n");
		variable_passed = 0;
		break;
	    }
	}

        print("Size ");
        print_int((int)sizes[i]);
        print(": PASS\n");
    }

    if (variable_passed)
        print("Variable-size allocations: PASS\n");
    else
        print("Variable-size allocations: FAIL\n");

    after_alloc = vm_get_used_pages();

    for (i = 0; i < 8; i++) {
        if (blocks[i])
            kvfree(blocks[i]);
    }

    after_free = vm_get_used_pages();

    if (after_free == before)
        print("Stress allocation reclamation: PASS\n");
    else {
        print("Stress allocation reclamation: FAIL\n");
        passed = 0;
    }

    if (after_alloc <= before)
        variable_passed = 0;

    a = kvmalloc(100);
    b = kvmalloc(100);
    c = kvmalloc(100);
    d = kvmalloc(100);

    if (!a || !b || !c || !d) {

        fragmentation_passed = 0;

        if (a)
            kvfree(a);

        if (b)
            kvfree(b);

        if (c)
            kvfree(c);

        if (d)
            kvfree(d);

    } else {

        kvfree(b);
        kvfree(d);

        e = kvmalloc(100);
        f = kvmalloc(100);

        if (!e || !f) {

            fragmentation_passed = 0;

            if (e)
                kvfree(e);

            if (f)
                kvfree(f);

        } else {

            if (e == a || e == c)
                fragmentation_passed = 0;

            if (f == a || f == c)
                fragmentation_passed = 0;

            kvfree(e);
            kvfree(f);
        }

        kvfree(a);
        kvfree(c);
    }

    if (vm_get_used_pages() != before)
        fragmentation_passed = 0;

    if (fragmentation_passed)
        print("Fragmentation and hole reuse: PASS\n");
    else
        print("Fragmentation and hole reuse: FAIL\n");

    if (!variable_passed)
        passed = 0;

    if (!fragmentation_passed)
        passed = 0;

    print("Kernel VMM heap stress test: ");

    if (passed)
        print("PASS\n");
    else
        print("FAIL\n");
}

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

static void kmalloc_vmm_test(void)
{
    size_t before;
    size_t after_alloc;
    size_t after_free;
    size_t allocation_size = 0x00400000U;
    volatile uint32_t *block;
    int passed = 1;

    print("\nKMalloc VMM Fallback Test\n");
    print("=========================\n");

    before = vm_get_used_pages();

    print("Initial VMM pages: ");
    print_int((int)before);
    print("\n");

    block = (volatile uint32_t *)kmalloc(allocation_size);

    if (!block) {
        print("VMM fallback allocation: FAIL\n");
        return;
    }

    if ((uint32_t)block < VM_START ||
        (uint32_t)block >= VM_END) {
        print("VMM fallback address: FAIL\n");
        passed = 0;
    } else {
        print("VMM fallback address: PASS\n");
    }

    block[0] = 0x4A555049U;

    if (block[0] != 0x4A555049U) {
        print("Read/write: FAIL\n");
        passed = 0;
    } else {
        print("Read/write: PASS\n");
    }

    after_alloc = vm_get_used_pages();

    if (after_alloc > before) {
        print("VMM pages allocated: PASS\n");
    } else {
        print("VMM pages allocated: FAIL\n");
        passed = 0;
    }

    kfree((void *)block);

    after_free = vm_get_used_pages();

    if (after_free == before) {
        print("VMM reclamation: PASS\n");
    } else {
        print("VMM reclamation: FAIL\n");
        passed = 0;
    }

    print("kmalloc VMM fallback: ");

    if (passed)
        print("PASS\n");
    else
        print("FAIL\n");
}

void execute_command(char *input)
{
    char *args[10];

    int argc = 0;
    int i = 0;
    int in_word = 0;

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

    if (in_word && i < LINE_SIZE)
        input[i] = '\0';

    if (argc == 0)
        return;

    if (strcmp(args[0], "help") == 0) {

        print("=============== JUPITER OS HELP =========================\n");
        print("| [FILE]    create, read, delete, ls                     |\n");
        print("| [FILE]    append, info, cp, edit                       |\n");
        print("| [SYSTEM]  clear, echo, meminfo, ps, calc               |\n");
        print("| [MEMORY]  memtest, pmtest, pmm, paging, pf, vmtest      |\n");
        print("| [MEMORY]  vmtest2, kheaptest, kheapstress              |\n");
        print("| [SYSTEM]  run, time, timer, sleep, memmap, uptime       |\n");
        print("| [INFO]    cpuinfo, osinfo, status, df                  |\n");
        print("| [NETWORK] ping, ifconfig, arp                          |\n");
        print("| [NETWORK] net, net test, net send                      |\n");
        print("| [USER]    whoami, logout                               |\n");
        print("| [HELP]    help                                         |\n");
        print("==========================================================\n");

    }

    else if (strcmp(args[0], "clear") == 0) {

        clear_screen();

    }

    else if (strcmp(args[0], "calc") == 0) {

        calculator(argc, args);

    }

    else if (strcmp(args[0], "echo") == 0 && argc > 1) {

        for (int i = 1; i < argc; i++) {

            print(args[i]);

            if (i < argc - 1)
                print(" ");
        }

        print("\n");

    }

    else if (strcmp(args[0], "meminfo") == 0) {

        print_memory_info();

    }

    else if (strcmp(args[0], "memmap") == 0) {

        mem_print_map();

    }

    else if (strcmp(args[0], "create") == 0 &&
             argc > 1) {

        if (fs_create(args[1], "")) {
            launch_editor(args[1]);
        }

    }

    else if (strcmp(args[0], "read") == 0 &&
             argc > 1) {

        const char *content =
            fs_read(args[1]);

        print(content);
        print("\n");

    }

    else if (strcmp(args[0], "delete") == 0 &&
             argc > 1) {

        fs_delete(args[1]);

    }

    else if (strcmp(args[0], "ls") == 0) {

        fs_list();

    }

    else if (strcmp(args[0], "ps") == 0) {

        list_processes();

    }

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

    else if (strcmp(args[0], "yield") == 0) {

        yield();

    }

    else if (strcmp(args[0], "time") == 0) {

        print("Uptime: ");
        print_hex(get_ticks());
        print(" ms\n");

    }

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

    else if (strcmp(args[0], "timer") == 0) {

        print("Timer ticks: ");
        print_hex(get_ticks());
        print("\n");

    }

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

    else if (strcmp(args[0], "pmm") == 0) {

        pmm_print_stats();

    }

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

    else if (strcmp(args[0], "info") == 0 &&
             argc > 1) {

        fs_info(args[1]);

    }

    else if (strcmp(args[0], "pf") == 0) {

        volatile uint32_t *fault_address =
            (volatile uint32_t *)PAGE_FAULT_TEST_ADDRESS;

        volatile uint32_t value;

        print("Triggering page fault...\n");

        *fault_address = 0x4A555049U;

        value = *fault_address;

        print("Recovered page value: ");
        print_hex(value);
        print("\n");

        if (value == 0x4A555049U)
            print("Page fault recovery test: PASS\n");
        else
            print("Page fault recovery test: FAIL\n");

    }

    else if (strcmp(args[0], "cp") == 0 &&
             argc > 2) {

        fs_copy(
            args[1],
            args[2]
        );

    }

    else if (strcmp(args[0], "vmtest2") == 0) {

        vm_multi_page_test();

    }

    else if (strcmp(args[0], "kheaptest") == 0) {

        kernel_vmm_heap_test();

    }

    else if (strcmp(args[0], "kheapstress") == 0) {

        kernel_vmm_heap_stress_test();

    }

    else if (strcmp(args[0], "kmallocvmm") == 0) {
    	kmalloc_vmm_test();
    }

    else if (strcmp(args[0], "edit") == 0 &&
             argc > 1) {

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

    else if (strcmp(args[0], "memtest") == 0) {

        memory_test();

    }

    else if (strcmp(args[0], "vmtest") == 0) {

        volatile uint32_t *page1;
        volatile uint32_t *page2;
        volatile uint32_t *page3;

        int passed = 1;

        print("\nVirtual Memory Test\n");
        print("===================\n");

        page1 =
            (volatile uint32_t *)vm_alloc_page();

        page2 =
            (volatile uint32_t *)vm_alloc_page();

        page3 =
            (volatile uint32_t *)vm_alloc_page();

        print("Page 1: ");
        print_hex((uint32_t)page1);
        print("\n");

        print("Page 2: ");
        print_hex((uint32_t)page2);
        print("\n");

        print("Page 3: ");
        print_hex((uint32_t)page3);
        print("\n");

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

        *page1 = 0x11111111U;
        *page2 = 0x22222222U;
        *page3 = 0x33333333U;

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

    else if (strcmp(args[0], "arp") == 0) {

        cmd_arp(argc, args);

    }

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

    else if (strcmp(args[0], "logout") == 0) {

        print("Logging out...\n");
        print("System will reboot to login screen\n");

        for (volatile int i = 0;
             i < 3000000;
             i++) {}

        asm volatile("jmp kmain");

    }

    else if (strcmp(args[0], "shutdown") == 0) {

        print("System shutting down...\n");

        while (1)
            asm volatile("hlt");

    }

    else if (strcmp(args[0], "reboot") == 0) {

        print("Rebooting system...\n");

        for (volatile int i = 0;
             i < 3000000;
             i++) {}

        asm volatile("jmp kmain");

    }

    else {

        print("Unknown command: ");
        print(args[0]);
        print("\n");
    }
}

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

void kmain(unsigned int magic,
           unsigned int *mb_info)
{
    (void)mb_info;

    static int boot_count = 0;

    boot_count++;

    if (magic != 0x2BADB002) {

        print(
            "Error: Not loaded by Multiboot-compliant loader\n"
        );

        return;
    }

    char line[LINE_SIZE];

    clear_screen();

    show_boot_animation();
    show_splash();

    print("Boot #");
    print_hex(boot_count);
    print("\n");

    mem_init();

    mem_detect_multiboot(mb_info);

    pmm_init(mb_info);

    init_paging();

    vm_init();

    fs_init();

    init_scheduler();

    init_interrupts();

    init_timer();

    disable_interrupts();

    init_users();

    show_login_screen();

    print("Welcome to JupiterOS Shell!\n");
    print("Interrupts: DISABLED (Safe Mode)\n");

    network_init();

    print("System ready\n");

    while (1) {

        manual_timer_test();

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

        get_line(
            line,
            LINE_SIZE
        );

        if (line[0] == 0)
            continue;

        execute_command(line);

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

        for (int i = 0;
             i < 10000;
             i++)
            asm volatile("pause");
    }
}