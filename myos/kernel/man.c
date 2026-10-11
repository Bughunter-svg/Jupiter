#include "man.h"
#include "screen.h"
#include "string.h"

typedef struct {
    const char *name;
    const char *body;
} man_entry_t;

static void man_header(const char *topic)
{
    print(topic);
    print("(1)              JupiterOS Manual              ");
    print(topic);
    print("(1)\n\n");
}

static void man_show_topic(const char *topic, const char *body)
{
    man_header(topic);
    print(body);
    print("\n");
}

static const man_entry_t man_pages[] = {
    {
        "help",
        "NAME\n"
        "    help - show a compact command summary\n"
        "SYNOPSIS\n"
        "    help\n"
        "DESCRIPTION\n"
        "    Prints command categories and common commands.\n"
        "    Detailed documentation is in the manual pages.\n"
        "SEE ALSO\n"
        "    man(1), ls(1), exec(1), ps(1)\n"
    },
    {
        "man",
        "NAME\n"
        "    man - read JupiterOS manual pages\n"
        "SYNOPSIS\n"
        "    man\n"
        "    man <topic>\n"
        "    man 1\n"
        "DESCRIPTION\n"
        "    Without arguments, shows usage.  With a command name,\n"
        "    displays that command's manual page.  man 1 lists all\n"
        "    section 1 topics.\n"
        "EXAMPLES\n"
        "    man exec\n"
        "    man ls\n"
        "    man 1\n"
        "SEE ALSO\n"
        "    help(1)\n"
    },
    {
        "exec",
        "NAME\n"
        "    exec - load and execute a program from the RAM filesystem\n"
        "SYNOPSIS\n"
        "    exec <filename>\n"
        "    exec <filename> &\n"
        "DESCRIPTION\n"
        "    Loads a flat binary (org 0x00400000) from the RAM\n"
        "    filesystem and creates a ring-3 process.  Without &,\n"
        "    runs in the foreground and waits until exit.  With &,\n"
        "    starts a background job and returns immediately.\n"
        "    echo cannot run in background: shell and ring-3 share\n"
        "    the same keyboard port without multiplexing.\n"
        "EXAMPLES\n"
        "    exec hello\n"
        "    exec ticker &\n"
        "    exec echo\n"
        "SEE ALSO\n"
        "    jobs(1), fg(1), wait(1), ps(1)\n"
    },
    {
        "jobs",
        "NAME\n"
        "    jobs - list shell child processes\n"
        "SYNOPSIS\n"
        "    jobs\n"
        "DESCRIPTION\n"
        "    Lists PID, state, and name for processes whose parent\n"
        "    is the shell.  Includes zombies until reaped with fg\n"
        "    or wait.  Does not reap processes.\n"
        "SEE ALSO\n"
        "    fg(1), exec(1), ps(1), wait(1)\n"
    },
    {
        "fg",
        "NAME\n"
        "    fg - wait for a background job to finish\n"
        "SYNOPSIS\n"
        "    fg <pid>\n"
        "DESCRIPTION\n"
        "    Waits for the given child process to exit and reaps it.\n"
        "    Works for running or already-zombie background jobs.\n"
        "SEE ALSO\n"
        "    jobs(1), exec(1), wait(1)\n"
    },
    {
        "bg",
        "NAME\n"
        "    bg - resume a suspended job\n"
        "SYNOPSIS\n"
        "    bg <pid>\n"
        "DESCRIPTION\n"
        "    Not supported.  The scheduler does not suspend jobs\n"
        "    for later resume; use fg to wait for completion.\n"
        "SEE ALSO\n"
        "    fg(1), jobs(1)\n"
    },
    {
        "ls",
        "NAME\n"
        "    ls - list RAM filesystem files\n"
        "SYNOPSIS\n"
        "    ls\n"
        "DESCRIPTION\n"
        "    Lists file names and sizes in the in-memory filesystem.\n"
        "SEE ALSO\n"
        "    create(1), read(1), cp(1), delete(1), exec(1), df(1)\n"
    },
    {
        "create",
        "NAME\n"
        "    create - create a new text file and open the editor\n"
        "SYNOPSIS\n"
        "    create <filename>\n"
        "DESCRIPTION\n"
        "    Creates an empty text file, then opens the full-screen\n"
        "    editor to enter content.\n"
        "SEE ALSO\n"
        "    edit(1), read(1), append(1), ls(1)\n"
    },
    {
        "read",
        "NAME\n"
        "    read - print a file's contents\n"
        "SYNOPSIS\n"
        "    read <filename>\n"
        "DESCRIPTION\n"
        "    Prints the contents of a text file.  Binary files may\n"
        "    contain embedded null bytes and are better run with exec.\n"
        "SEE ALSO\n"
        "    ls(1), create(1), append(1), info(1)\n"
    },
    {
        "append",
        "NAME\n"
        "    append - append text to a file\n"
        "SYNOPSIS\n"
        "    append <filename> <text...>\n"
        "DESCRIPTION\n"
        "    Appends the remaining arguments as text to an existing\n"
        "    text file.  Binary files cannot be appended.\n"
        "SEE ALSO\n"
        "    create(1), read(1), edit(1)\n"
    },
    {
        "delete",
        "NAME\n"
        "    delete - remove a file\n"
        "SYNOPSIS\n"
        "    delete <filename>\n"
        "DESCRIPTION\n"
        "    Deletes a file from the RAM filesystem.\n"
        "SEE ALSO\n"
        "    ls(1), create(1)\n"
    },
    {
        "cp",
        "NAME\n"
        "    cp - copy a file\n"
        "SYNOPSIS\n"
        "    cp <source> <dest>\n"
        "DESCRIPTION\n"
        "    Copies source to a new destination name.  Works for\n"
        "    text and binary files.\n"
        "SEE ALSO\n"
        "    ls(1), create(1)\n"
    },
    {
        "edit",
        "NAME\n"
        "    edit - open the full-screen editor\n"
        "SYNOPSIS\n"
        "    edit <filename>\n"
        "DESCRIPTION\n"
        "    Opens an existing file in the JupiterOS editor.\n"
        "    Saving writes the buffer back to the file.\n"
        "SEE ALSO\n"
        "    create(1), read(1), append(1)\n"
    },
    {
        "info",
        "NAME\n"
        "    info - show file metadata\n"
        "SYNOPSIS\n"
        "    info <filename>\n"
        "DESCRIPTION\n"
        "    Prints name, size, capacity, and data preview for a file.\n"
        "SEE ALSO\n"
        "    ls(1), df(1)\n"
    },
    {
        "df",
        "NAME\n"
        "    df - RAM filesystem disk usage\n"
        "SYNOPSIS\n"
        "    df\n"
        "DESCRIPTION\n"
        "    Shows file count and bytes used in the RAM filesystem.\n"
        "SEE ALSO\n"
        "    ls(1), info(1)\n"
    },
    {
        "ps",
        "NAME\n"
        "    ps - list processes\n"
        "SYNOPSIS\n"
        "    ps\n"
        "DESCRIPTION\n"
        "    Shows PID, parent PID, state, priority, and name for\n"
        "    each process in the process table.\n"
        "SEE ALSO\n"
        "    procinfo(1), run(1), exec(1), wait(1), yield(1)\n"
    },
    {
        "procinfo",
        "NAME\n"
        "    procinfo - show one process control block\n"
        "SYNOPSIS\n"
        "    procinfo <pid>\n"
        "DESCRIPTION\n"
        "    Prints PID, parent PID, state, priority, name, and\n"
        "    ring-0 or ring-3 mode for the given process.\n"
        "EXAMPLES\n"
        "    procinfo 0\n"
        "    procinfo 1\n"
        "SEE ALSO\n"
        "    ps(1), wait(1), exec(1)\n"
    },
    {
        "run",
        "NAME\n"
        "    run - start a kernel test process\n"
        "SYNOPSIS\n"
        "    run <name>\n"
        "DESCRIPTION\n"
        "    Creates a ring-0 test process that runs a short busy loop\n"
        "    and prints start/finish messages.\n"
        "SEE ALSO\n"
        "    ps(1), wait(1), yield(1)\n"
    },
    {
        "wait",
        "NAME\n"
        "    wait - wait for a child process to exit\n"
        "SYNOPSIS\n"
        "    wait <pid>\n"
        "DESCRIPTION\n"
        "    Blocks until the given child becomes a zombie, then reaps\n"
        "    it.  Only works for processes created by the shell.\n"
        "SEE ALSO\n"
        "    ps(1), exec(1), run(1)\n"
    },
    {
        "yield",
        "NAME\n"
        "    yield - voluntarily give up the CPU\n"
        "SYNOPSIS\n"
        "    yield\n"
        "DESCRIPTION\n"
        "    Switches to another ready process if one exists.\n"
        "SEE ALSO\n"
        "    ps(1), tickers(1)\n"
    },
    {
        "tickers",
        "NAME\n"
        "    tickers - start three ring-3 ticker processes\n"
        "SYNOPSIS\n"
        "    tickers [prefix]\n"
        "DESCRIPTION\n"
        "    Creates three ring-3 ticker processes named prefix1,\n"
        "    prefix2, and prefix3.  Default prefix is t.\n"
        "SEE ALSO\n"
        "    ticker(1), ps(1), yield(1)\n"
    },
    {
        "ticker",
        "NAME\n"
        "    ticker - start one ring-3 ticker process\n"
        "SYNOPSIS\n"
        "    ticker <name>\n"
        "DESCRIPTION\n"
        "    Creates a single ring-3 ticker process with the given\n"
        "    unique name.\n"
        "SEE ALSO\n"
        "    tickers(1), exec(1)\n"
    },
    {
        "ring3proc",
        "NAME\n"
        "    ring3proc - start the built-in hello ring-3 demo\n"
        "SYNOPSIS\n"
        "    ring3proc <name>\n"
        "DESCRIPTION\n"
        "    Creates a ring-3 process from the embedded hello binary\n"
        "    and yields to it.  Does not wait for completion.\n"
        "SEE ALSO\n"
        "    exec(1), ring3test(1), crash(1)\n"
    },
    {
        "ring3test",
        "NAME\n"
        "    ring3test - paging and CR3 switch debug test\n"
        "SYNOPSIS\n"
        "    ring3test\n"
        "DESCRIPTION\n"
        "    Runs a kernel paging test that creates a new address\n"
        "    space, switches CR3, and restores the original CR3.\n"
        "SEE ALSO\n"
        "    paging(1), ring3proc(1)\n"
    },
    {
        "crash",
        "NAME\n"
        "    crash - ring-3 fault injection test\n"
        "SYNOPSIS\n"
        "    crash <name>\n"
        "    crash <name> kernel\n"
        "    crash <name> priv\n"
        "DESCRIPTION\n"
        "    Starts a ring-3 process designed to fault.  Default mode\n"
        "    reads NULL.  kernel writes to low kernel memory.  priv\n"
        "    executes a privileged instruction.  Only the faulting\n"
        "    process is killed.\n"
        "SEE ALSO\n"
        "    ring3proc(1), ps(1)\n"
    },
    {
        "pmm",
        "NAME\n"
        "    pmm - physical memory manager statistics\n"
        "SYNOPSIS\n"
        "    pmm\n"
        "DESCRIPTION\n"
        "    Prints free and used physical pages.  After boot with no\n"
        "    user processes, used pages should match the baseline\n"
        "    (321 in the current kernel).\n"
        "SEE ALSO\n"
        "    meminfo(1), pmtest(1), paging(1)\n"
    },
    {
        "meminfo",
        "NAME\n"
        "    meminfo - kernel heap memory summary\n"
        "SYNOPSIS\n"
        "    meminfo\n"
        "DESCRIPTION\n"
        "    Shows total, used, and free bytes for the kernel heap.\n"
        "SEE ALSO\n"
        "    pmm(1), memmap(1), memtest(1)\n"
    },
    {
        "memmap",
        "NAME\n"
        "    memmap - print the memory map\n"
        "SYNOPSIS\n"
        "    memmap\n"
        "DESCRIPTION\n"
        "    Displays regions detected at boot (multiboot map).\n"
        "SEE ALSO\n"
        "    meminfo(1), pmm(1)\n"
    },
    {
        "paging",
        "NAME\n"
        "    paging - paging status and mapping test\n"
        "SYNOPSIS\n"
        "    paging\n"
        "DESCRIPTION\n"
        "    Reports whether paging is enabled and runs a dynamic\n"
        "    map/unmap self-test.\n"
        "SEE ALSO\n"
        "    pf(1), vmtest(1), pmm(1)\n"
    },
    {
        "vmtest",
        "NAME\n"
        "    vmtest - virtual memory allocator test\n"
        "SYNOPSIS\n"
        "    vmtest\n"
        "DESCRIPTION\n"
        "    Allocates three virtual pages, writes patterns, frees\n"
        "    them, and reports pass/fail.\n"
        "SEE ALSO\n"
        "    vmtest2(1), paging(1), kheaptest(1)\n"
    },
    {
        "vmtest2",
        "NAME\n"
        "    vmtest2 - multi-page virtual memory test\n"
        "SYNOPSIS\n"
        "    vmtest2\n"
        "DESCRIPTION\n"
        "    Tests contiguous multi-page allocation, reuse, and free.\n"
        "SEE ALSO\n"
        "    vmtest(1)\n"
    },
    {
        "pmtest",
        "NAME\n"
        "    pmtest - physical page allocate/free test\n"
        "SYNOPSIS\n"
        "    pmtest\n"
        "DESCRIPTION\n"
        "    Allocates two physical pages, prints addresses, frees\n"
        "    them, and shows free page counts.\n"
        "SEE ALSO\n"
        "    pmm(1)\n"
    },
    {
        "memtest",
        "NAME\n"
        "    memtest - kmalloc smoke test\n"
        "SYNOPSIS\n"
        "    memtest\n"
        "DESCRIPTION\n"
        "    Allocates several heap blocks and prints meminfo.\n"
        "SEE ALSO\n"
        "    meminfo(1), kheaptest(1)\n"
    },
    {
        "kheaptest",
        "NAME\n"
        "    kheaptest - kernel VMM heap test\n"
        "SYNOPSIS\n"
        "    kheaptest\n"
        "DESCRIPTION\n"
        "    Exercises kvmalloc/kvfree and page accounting.\n"
        "SEE ALSO\n"
        "    kheapstress(1), kmallocvmm(1)\n"
    },
    {
        "kheapstress",
        "NAME\n"
        "    kheapstress - kernel heap stress test\n"
        "SYNOPSIS\n"
        "    kheapstress\n"
        "DESCRIPTION\n"
        "    Variable-size allocations, reclamation, and hole reuse.\n"
        "SEE ALSO\n"
        "    kheaptest(1)\n"
    },
    {
        "kmallocvmm",
        "NAME\n"
        "    kmallocvmm - large kmalloc VMM fallback test\n"
        "SYNOPSIS\n"
        "    kmallocvmm\n"
        "DESCRIPTION\n"
        "    Allocates a large block via kmalloc VMM fallback path.\n"
        "SEE ALSO\n"
        "    kheaptest(1), vmtest(1)\n"
    },
    {
        "nulltest",
        "NAME\n"
        "    nulltest - null page fault recovery test\n"
        "SYNOPSIS\n"
        "    nulltest\n"
        "DESCRIPTION\n"
        "    Triggers a user/kernel null-page fault handler test.\n"
        "SEE ALSO\n"
        "    pf(1), rotest(1), guardtest(1)\n"
    },
    {
        "rotest",
        "NAME\n"
        "    rotest - read-only page protection test\n"
        "SYNOPSIS\n"
        "    rotest\n"
        "DESCRIPTION\n"
        "    Tests write fault on a read-only mapped page.\n"
        "SEE ALSO\n"
        "    nulltest(1), guardtest(1)\n"
    },
    {
        "guardtest",
        "NAME\n"
        "    guardtest - guard page fault test\n"
        "SYNOPSIS\n"
        "    guardtest\n"
        "DESCRIPTION\n"
        "    Tests access to an unmapped guard region.\n"
        "SEE ALSO\n"
        "    nulltest(1), pf(1)\n"
    },
    {
        "pf",
        "NAME\n"
        "    pf - deliberate page fault test\n"
        "SYNOPSIS\n"
        "    pf\n"
        "DESCRIPTION\n"
        "    Writes to a high unmapped address and checks recovery\n"
        "    behavior (kernel test harness).\n"
        "SEE ALSO\n"
        "    paging(1), nulltest(1)\n"
    },
    {
        "timer",
        "NAME\n"
        "    timer - show timer tick count\n"
        "SYNOPSIS\n"
        "    timer\n"
        "DESCRIPTION\n"
        "    Prints the raw timer tick counter (100 Hz).\n"
        "SEE ALSO\n"
        "    time(1), uptime(1), sleep(1)\n"
    },
    {
        "time",
        "NAME\n"
        "    time - uptime in milliseconds\n"
        "SYNOPSIS\n"
        "    time\n"
        "DESCRIPTION\n"
        "    Prints uptime derived from timer ticks.\n"
        "SEE ALSO\n"
        "    uptime(1), timer(1)\n"
    },
    {
        "uptime",
        "NAME\n"
        "    uptime - human-readable uptime\n"
        "SYNOPSIS\n"
        "    uptime\n"
        "DESCRIPTION\n"
        "    Prints hours, minutes, and seconds since boot.\n"
        "SEE ALSO\n"
        "    time(1), timer(1)\n"
    },
    {
        "sleep",
        "NAME\n"
        "    sleep - delay for milliseconds\n"
        "SYNOPSIS\n"
        "    sleep <ms>\n"
        "DESCRIPTION\n"
        "    Busy-waits for approximately the given milliseconds\n"
        "    using the timer (not a syscall).\n"
        "SEE ALSO\n"
        "    timer(1), uptime(1)\n"
    },
    {
        "clear",
        "NAME\n"
        "    clear - clear the screen\n"
        "SYNOPSIS\n"
        "    clear\n"
        "DESCRIPTION\n"
        "    Clears the text console.\n"
        "SEE ALSO\n"
        "    help(1)\n"
    },
    {
        "echo",
        "NAME\n"
        "    echo - print shell arguments\n"
        "SYNOPSIS\n"
        "    echo <words...>\n"
        "DESCRIPTION\n"
        "    Prints arguments to the console (shell builtin, not the\n"
        "    ring-3 echo program).  For the demo program use exec echo.\n"
        "SEE ALSO\n"
        "    exec(1)\n"
    },
    {
        "calc",
        "NAME\n"
        "    calc - simple calculator\n"
        "SYNOPSIS\n"
        "    calc <expr...>\n"
        "DESCRIPTION\n"
        "    Evaluates a basic arithmetic expression from arguments.\n"
        "SEE ALSO\n"
        "    help(1)\n"
    },
    {
        "cpuinfo",
        "NAME\n"
        "    cpuinfo - CPU identification\n"
        "SYNOPSIS\n"
        "    cpuinfo\n"
        "DESCRIPTION\n"
        "    Shows vendor, model, and feature flags via CPUID.\n"
        "SEE ALSO\n"
        "    osinfo(1), status(1)\n"
    },
    {
        "osinfo",
        "NAME\n"
        "    osinfo - JupiterOS version information\n"
        "SYNOPSIS\n"
        "    osinfo\n"
        "DESCRIPTION\n"
        "    Prints OS name and build details.\n"
        "SEE ALSO\n"
        "    cpuinfo(1), status(1)\n"
    },
    {
        "status",
        "NAME\n"
        "    status - system status summary\n"
        "SYNOPSIS\n"
        "    status\n"
        "DESCRIPTION\n"
        "    Shows a brief overall system status.\n"
        "SEE ALSO\n"
        "    osinfo(1), meminfo(1)\n"
    },
    {
        "date",
        "NAME\n"
        "    date - read RTC date and time\n"
        "SYNOPSIS\n"
        "    date\n"
        "DESCRIPTION\n"
        "    Reads the real-time clock and prints date/time.\n"
        "SEE ALSO\n"
        "    uptime(1)\n"
    },
    {
        "beep",
        "NAME\n"
        "    beep - PC speaker tone\n"
        "SYNOPSIS\n"
        "    beep [frequency_hz] [duration_ms]\n"
        "DESCRIPTION\n"
        "    Plays a tone on the PC speaker.  Defaults 1000 Hz, 200 ms.\n"
        "    Frequency must be 20-20000 Hz.\n"
        "SEE ALSO\n"
        "    melody(1)\n"
    },
    {
        "melody",
        "NAME\n"
        "    melody - play a built-in melody\n"
        "SYNOPSIS\n"
        "    melody\n"
        "DESCRIPTION\n"
        "    Plays a short tune on the PC speaker.\n"
        "SEE ALSO\n"
        "    beep(1)\n"
    },
    {
        "mouse",
        "NAME\n"
        "    mouse - PS/2 mouse status and paint mode\n"
        "SYNOPSIS\n"
        "    mouse\n"
        "    mouse paint on|off\n"
        "    mouse hide|show\n"
        "DESCRIPTION\n"
        "    Without options, prints position and button state.\n"
        "    paint toggles screen drawing; hide/show the cursor.\n"
        "SEE ALSO\n"
        "    clear(1)\n"
    },
    {
        "ping",
        "NAME\n"
        "    ping - ICMP echo to a host\n"
        "SYNOPSIS\n"
        "    ping <ip> [count]\n"
        "DESCRIPTION\n"
        "    Sends ICMP echo requests to the given IPv4 address.\n"
        "EXAMPLES\n"
        "    ping 10.0.2.2\n"
        "SEE ALSO\n"
        "    ifconfig(1), net(1), arp(1)\n"
    },
    {
        "ifconfig",
        "NAME\n"
        "    ifconfig - network interface addresses\n"
        "SYNOPSIS\n"
        "    ifconfig\n"
        "DESCRIPTION\n"
        "    Shows MAC, IP, gateway, and mask when the network stack\n"
        "    is initialized.\n"
        "SEE ALSO\n"
        "    net(1), ping(1), arp(1)\n"
    },
    {
        "arp",
        "NAME\n"
        "    arp - ARP table or operations\n"
        "SYNOPSIS\n"
        "    arp\n"
        "DESCRIPTION\n"
        "    Displays ARP-related network information.\n"
        "SEE ALSO\n"
        "    ping(1), ifconfig(1)\n"
    },
    {
        "net",
        "NAME\n"
        "    net - network status and tests\n"
        "SYNOPSIS\n"
        "    net\n"
        "    net test\n"
        "    net send <ip> <message>\n"
        "DESCRIPTION\n"
        "    net shows initialization status.  net test listens for\n"
        "    packets briefly.  net send transmits a UDP message.\n"
        "EXAMPLES\n"
        "    net test\n"
        "    net send 10.0.2.2 hello\n"
        "SEE ALSO\n"
        "    ping(1), ifconfig(1)\n"
    },
    {
        "whoami",
        "NAME\n"
        "    whoami - current logged-in user\n"
        "SYNOPSIS\n"
        "    whoami\n"
        "DESCRIPTION\n"
        "    Prints the active username or not logged in.\n"
        "SEE ALSO\n"
        "    logout(1)\n"
    },
    {
        "logout",
        "NAME\n"
        "    logout - end session and return to login\n"
        "SYNOPSIS\n"
        "    logout\n"
        "DESCRIPTION\n"
        "    Logs out the current user and shows the login screen.\n"
        "SEE ALSO\n"
        "    whoami(1)\n"
    },
    {
        "shutdown",
        "NAME\n"
        "    shutdown - halt the CPU\n"
        "SYNOPSIS\n"
        "    shutdown\n"
        "DESCRIPTION\n"
        "    Halts the system in an infinite hlt loop.\n"
        "SEE ALSO\n"
        "    reboot(1)\n"
    },
    {
        "reboot",
        "NAME\n"
        "    reboot - restart JupiterOS\n"
        "SYNOPSIS\n"
        "    reboot\n"
        "DESCRIPTION\n"
        "    Jumps back to kernel main after a short delay.\n"
        "SEE ALSO\n"
        "    shutdown(1)\n"
    },
};

#define MAN_PAGE_COUNT (sizeof(man_pages) / sizeof(man_pages[0]))

void shell_print_help(void)
{
    print("JupiterOS shell\n\n");
    print("Categories:\n");
    print("  files     ls create read cp delete edit append info df exec\n");
    print("  process   ps procinfo jobs fg bg exec wait yield tickers ticker crash\n");
    print("  memory    pmm meminfo memmap paging vmtest pmtest memtest\n");
    print("  system    clear timer uptime sleep help man cpuinfo osinfo\n");
    print("  network   net ping ifconfig arp\n");
    print("  user      whoami logout\n\n");
    print("Common: ls, exec, ps, pmm, clear, help\n\n");
    print("Use 'man <command>' for details.  Try 'man' or 'man 1'.\n");
}

void man_command(int argc, char **args)
{
    const char *topic;
    unsigned int i;

    if (argc < 2) {
        print("Usage: man <topic>\n");
        print("       man 1          list section 1 manual topics\n");
        print("Try: man exec, man ls, man ps\n");
        return;
    }

    topic = args[1];

    if (strcmp(topic, "1") == 0) {
        print("JupiterOS section 1 commands:\n\n");
        for (i = 0; i < MAN_PAGE_COUNT; i++) {
            print(man_pages[i].name);
            print("\n");
        }
        return;
    }

    for (i = 0; i < MAN_PAGE_COUNT; i++) {
        if (strcmp(man_pages[i].name, topic) == 0) {
            man_show_topic(topic, man_pages[i].body);
            return;
        }
    }

    print("No manual entry for ");
    print(topic);
    print(".\n");
}
