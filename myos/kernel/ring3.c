#include "ring3.h"
#include "paging.h"
#include "memory.h"
#include "screen.h"
#include <stdint.h>

#define USER_CODE_ADDRESS  0x00400000U
#define USER_STACK_ADDRESS 0x00800000U

extern unsigned char user_start[];
extern unsigned char user_end[];
extern void enter_ring3(unsigned int user_stack);

uint32_t ring3_kernel_esp;

static void copy_user_code(void *destination)
{
    unsigned char *dst = (unsigned char *)destination;
    unsigned char *src = user_start;
    unsigned int size = (unsigned int)(user_end - user_start);
    unsigned int i;

    for (i = 0; i < size; i++)
        dst[i] = src[i];
}

void ring3_test(void)
{
    void *code_page;
    void *stack_page;
    unsigned int user_stack;

    print("\nRing 3 Test\n");
    print("===========\n");

    code_page = pmm_alloc_page();

    if (!code_page) {
        print("User code page allocation: FAIL\n");
        return;
    }

    stack_page = pmm_alloc_page();

    if (!stack_page) {
        pmm_free_page(code_page);
        print("User stack page allocation: FAIL\n");
        return;
    }

    if (map_page(
            USER_CODE_ADDRESS,
            (unsigned int)code_page,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0) {

        pmm_free_page(code_page);
        pmm_free_page(stack_page);

        print("User code mapping: FAIL\n");
        return;
    }

    if (map_page(
            USER_STACK_ADDRESS,
            (unsigned int)stack_page,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0) {

        unmap_page(USER_CODE_ADDRESS);
        pmm_free_page(code_page);
        pmm_free_page(stack_page);

        print("User stack mapping: FAIL\n");
        return;
    }

    print("Copying user code...\n");

    copy_user_code((void *)USER_CODE_ADDRESS);

    print("User code copy: PASS\n");
    print("User code mapping: PASS\n");
    print("User stack mapping: PASS\n");
    print("Entering Ring 3...\n");

    user_stack = USER_STACK_ADDRESS + 0x1000U;

    enter_ring3(user_stack);

    print("Returned from Ring 3.\n");

    unmap_page(USER_CODE_ADDRESS);
    unmap_page(USER_STACK_ADDRESS);

    pmm_free_page(code_page);
    pmm_free_page(stack_page);

    print("Ring 3 test complete.\n");
}