#include "ring3.h"
#include "paging.h"
#include "screen.h"
#include <stdint.h>

uint32_t ring3_kernel_esp;

void ring3_test(void)
{
    uint32_t new_cr3;
    uint32_t old_cr3;

    print("\nCR3 SWITCH DEBUG\n");
    print("================\n");

    old_cr3 = paging_get_current_cr3();

    print("Old CR3: ");
    print_hex(old_cr3);
    print("\n");

    new_cr3 = paging_create_address_space();

    if (!new_cr3) {
        print("CREATE FAILED\n");
        return;
    }

    print("New CR3: ");
    print_hex(new_cr3);
    print("\n");

    print("Switching now...\n");

    /*
     * NOTE: we do NOT cli/hlt forever here. This was the bug:
     * the old code switched CR3 and then parked the CPU in an
     * infinite hlt loop, never returning to the caller (the shell).
     * Only interrupts (timer/keyboard) could still run, and their
     * side effects (e.g. redrawing the prompt) made it *look* like
     * the system had "looped back to login", when really the CPU
     * was permanently stuck here.
     */
    asm volatile(
        "cli\n"
        "mov %0, %%cr3\n"
        :
        : "r"(new_cr3)
        : "memory"
    );

    print("CR3 SWITCH PASSED\n");

    print("Current CR3: ");
    print_hex(paging_get_current_cr3());
    print("\n");

    /*
     * Restore the original address space before handing control
     * back, so the shell / rest of the kernel keeps running against
     * the mappings it expects.
     */
    asm volatile(
        "mov %0, %%cr3\n"
        :
        : "r"(old_cr3)
        : "memory"
    );

    asm volatile("sti");

    print("Restored original CR3: ");
    print_hex(paging_get_current_cr3());
    print("\n");
    print("ring3test complete, returning to shell.\n");

    /*
     * Falls through and returns normally now — no infinite hlt loop.
     *
     * NOTE: this still doesn't perform a *real* ring-3 transition.
     * enter_ring3 (in ring3_enter.asm) is currently never called from
     * anywhere in the kernel. To actually run code in ring 3 you'd
     * need to:
     *   1. map a code page for the user routine at some user-space
     *      virtual address (e.g. 0x00400000) inside new_cr3's
     *      directory, with PAGE_USER set
     *   2. switch to new_cr3
     *   3. call enter_ring3(user_stack_top) so it IRETs into that
     *      mapped code at CPL=3
     *   4. have that user code return via a syscall (int 0x80) that
     *      restores kernel CR3/state, rather than switching CR3 back
     *      directly from ring 0 like this debug test does
     */
}