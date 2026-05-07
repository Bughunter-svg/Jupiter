#include "system.h"
#include "screen.h"
#include "string.h"
#include "memory.h"
#include "process.h"
#include "timer.h"
#include "filesystem.h"

/* ─── cpuinfo ────────────────────────────────────────────────────── */
void show_cpuinfo(void) {
    /* Read CPU vendor string via CPUID instruction */
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];
    vendor[12] = '\0';

    asm volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    /* GCC may reorder, so use explicit byte copies */
    int i;
    for (i = 0; i < 4; i++) vendor[i]     = (char)((ebx >> (i * 8)) & 0xFF);
    for (i = 0; i < 4; i++) vendor[4 + i] = (char)((edx >> (i * 8)) & 0xFF);
    for (i = 0; i < 4; i++) vendor[8 + i] = (char)((ecx >> (i * 8)) & 0xFF);

    /* Max CPUID level is in eax after leaf 0 */
    uint32_t max_leaf = eax;

    print("CPU Vendor  : "); print(vendor); print("\n");

    /* Get brand string if supported (leaves 0x80000002-4) */
    uint32_t ext_max;
    asm volatile ("cpuid" : "=a"(ext_max) : "a"(0x80000000) : "ebx","ecx","edx");
    if (ext_max >= 0x80000004) {
        char brand[49];
        brand[48] = '\0';
        uint32_t regs[12];
        uint32_t leaf;
        for (leaf = 0; leaf < 3; leaf++) {
            asm volatile (
                "cpuid"
                : "=a"(regs[leaf*4+0]), "=b"(regs[leaf*4+1]),
                  "=c"(regs[leaf*4+2]), "=d"(regs[leaf*4+3])
                : "a"(0x80000002 + leaf)
            );
        }
        memcpy(brand, regs, 48);
        /* Trim leading spaces */
        char *b = brand;
        while (*b == ' ') b++;
        print("CPU Model   : "); print(b); print("\n");
    }

    /* Feature flags from leaf 1 */
    if (max_leaf >= 1) {
        uint32_t f_eax, f_ebx, f_ecx, f_edx;
        asm volatile (
            "cpuid"
            : "=a"(f_eax), "=b"(f_ebx), "=c"(f_ecx), "=d"(f_edx)
            : "a"(1)
        );
        print("Architecture: 32-bit Protected Mode (x86)\n");
        print("Features    :");
        if (f_edx & (1 << 0))  print(" FPU");
        if (f_edx & (1 << 23)) print(" MMX");
        if (f_edx & (1 << 25)) print(" SSE");
        if (f_edx & (1 << 26)) print(" SSE2");
        if (f_ecx & (1 << 0))  print(" SSE3");
        if (f_ecx & (1 << 28)) print(" AVX");
        print("\n");
        (void)f_eax; (void)f_ebx;
    }
}

/* ─── meminfo – reads live heap stats ───────────────────────────── */
void show_meminfo(void) {
    size_t total = mem_get_total();
    size_t used  = mem_get_used();
    size_t free_ = mem_get_free();

    char buf[16];

    print("Memory Total : ");
    itoa((int)(total / 1024), buf, 10); print(buf); print(" KB\n");

    print("Memory Used  : ");
    itoa((int)(used  / 1024), buf, 10); print(buf); print(" KB\n");

    print("Memory Free  : ");
    itoa((int)(free_ / 1024), buf, 10); print(buf); print(" KB\n");

    print("Heap Base    : 0x100000\n");
    print("Heap Limit   : 0x500000  (4 MB)\n");
}

/* ─── osinfo ─────────────────────────────────────────────────────── */
void show_osinfo(void) {
    print("OS Name  : JupiterOS\n");
    print("Version  : v1.1 Alpha\n");
    print("Built on : " __DATE__ " " __TIME__ "\n");
    print("Arch     : i386 Protected Mode\n");
    print("Shell    : JupiterSH v1.1\n");
    print("Features : FS | Memory | Processes | Network | Login\n");
}

/* ─── status – live uptime + process count ──────────────────────── */
void show_status(void) {
    char buf[16];
    unsigned long ticks = get_ticks();
    /* Timer fires at 100 Hz, so ticks / 100 = seconds */
    unsigned long seconds = ticks / 100;
    unsigned long minutes = seconds / 60;
    seconds %= 60;

    print("System Status : ONLINE\n");

    print("Uptime        : ");
    itoa((int)minutes, buf, 10); print(buf); print("m ");
    itoa((int)seconds, buf, 10); print(buf); print("s\n");

    print("Process Count : ");
    itoa(get_process_count(), buf, 10); print(buf); print("\n");

    print("Memory Used   : ");
    itoa((int)(mem_get_used() / 1024), buf, 10); print(buf); print(" KB\n");
}

/* ─── df – filesystem usage ─────────────────────────────────────── */
void show_diskinfo(void) {
    int file_count = fs_get_file_count();
    int bytes_used = fs_get_bytes_used();
    int total_cap  = MAX_FILES * MAX_FILE_SIZE;   /* bytes */

    char buf[16];

    print("Filesystem : RAM-FS (volatile)\n");

    print("Total Cap  : ");
    itoa(total_cap / 1024, buf, 10); print(buf); print(" KB\n");

    print("Used       : ");
    itoa(bytes_used, buf, 10); print(buf); print(" bytes\n");

    print("Free       : ");
    itoa(total_cap - bytes_used, buf, 10); print(buf); print(" bytes\n");

    print("Files      : ");
    itoa(file_count, buf, 10); print(buf); print(" / ");
    itoa(MAX_FILES, buf, 10);  print(buf); print("\n");
}

/* ─── calculator ─────────────────────────────────────────────────── */
void calculator(int argc, char *args[]) {
    if (argc != 4) {
        print("Usage: calc <num1> <op> <num2>\n");
        print("Ops  : + - * /\n");
        return;
    }

    /* Detect floating-point */
    int has_decimal = 0;
    for (int i = 1; i < argc && !has_decimal; i++) {
        for (char *p = args[i]; *p; p++)
            if (*p == '.') { has_decimal = 1; break; }
    }

    char op = args[2][0];

    if (has_decimal) {
        float a = atof_simple(args[1]);
        float b = atof_simple(args[3]);
        float r = 0;
        switch (op) {
            case '+': r = a + b; break;
            case '-': r = a - b; break;
            case '*': r = a * b; break;
            case '/':
                if (b == 0.0f) { print("Error: Division by zero!\n"); return; }
                r = a / b; break;
            default: print("Error: Unknown operator\n"); return;
        }
        print_float(a); print_char(' '); print_char(op);
        print_char(' '); print_float(b); print(" = "); print_float(r); print("\n");
    } else {
        int a = atoi_simple(args[1]);
        int b = atoi_simple(args[3]);
        int r = 0;
        switch (op) {
            case '+': r = a + b; break;
            case '-': r = a - b; break;
            case '*': r = a * b; break;
            case '/':
                if (b == 0) { print("Error: Division by zero!\n"); return; }
                r = a / b; break;
            default: print("Error: Unknown operator\n"); return;
        }
        print_int(a); print_char(' '); print_char(op);
        print_char(' '); print_int(b); print(" = "); print_int(r); print("\n");
    }
}
