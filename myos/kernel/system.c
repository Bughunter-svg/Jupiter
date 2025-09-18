#include "system.h"
#include "screen.h"
#include "string.h"

void show_cpuinfo() {
    print("CPU: i386-compatible\n");
    print("Architecture: 32-bit x86\n");
    print("FPU: Present\n");
    print("Vendor: JupiterOS Virtual CPU\n");
}

void show_meminfo() {
    print("Memory Total: 64MB\n");
    print("Memory Used: 8MB\n");
    print("Memory Free: 56MB\n");
    print("Kernel Size: 2MB\n");
    print("Heap Available: 32MB\n");
}

void show_osinfo() {
    print("JupiterOS v1.0 Alpha\n");
    print("Build Date: " __DATE__ "\n");
    print("Features: Filesystem, Memory Mgmt, Processes\n");
    print("Shell: JupiterSH v1.0\n");
}

void show_status() {
    print("System Status: ONLINE\n");
    print("Uptime: ");
    // Would show actual uptime when interrupts work
    print("0 seconds\n");
    print("Processes: 1 running\n");
    print("Files: 0 open\n");
}

void show_diskinfo() {
    print("Disk: RAM Filesystem\n");
    print("Total Space: 1MB\n");
    print("Used Space: 0.1MB\n");
    print("Free Space: 0.9MB\n");
    print("Files: 0\n");
}

void calculator(int argc, char* args[]) {
    if (argc != 4) {
        print("Usage: calc <number1> <operator> <number2>\n");
        print("Operators: + - * /\n");
        return;
    }
    
    // Check if arguments have decimal points
    int has_decimal = 0;
    for (int i = 1; i < argc; i++) {
        char* ptr = args[i];
        while (*ptr) {
            if (*ptr == '.') {
                has_decimal = 1;
                break;
            }
            ptr++;
        }
        if (has_decimal) break;
    }
    
    if (has_decimal) {
        // FLOATING POINT CALCULATION
        float num1 = atof_simple(args[1]);
        char op = args[2][0];
        float num2 = atof_simple(args[3]);
        float result = 0;
        
        switch (op) {
            case '+': result = num1 + num2; break;
            case '-': result = num1 - num2; break;
            case '*': result = num1 * num2; break;
            case '/':
                if (num2 != 0.0f) result = num1 / num2;
                else { print("Error: Division by zero!\n"); return; }
                break;
            default:
                print("Error: Invalid operator!\n"); return;
        }
        
        print_float(num1);
        print(" ");
        print_char(op);
        print(" ");
        print_float(num2);
        print(" = ");
        print_float(result);
        print("\n");
    } else {
        // INTEGER CALCULATION (original code)
        int num1 = atoi_simple(args[1]);
        char op = args[2][0];
        int num2 = atoi_simple(args[3]);
        int result = 0;
        
        switch (op) {
            case '+': result = num1 + num2; break;
            case '-': result = num1 - num2; break;
            case '*': result = num1 * num2; break;
            case '/':
                if (num2 != 0) result = num1 / num2;
                else { print("Error: Division by zero!\n"); return; }
                break;
            default:
                print("Error: Invalid operator!\n"); return;
        }
        
        print_int(num1);
        print(" ");
        print_char(op);
        print(" ");
        print_int(num2);
        print(" = ");
        print_int(result);
        print("\n");
    }
}
