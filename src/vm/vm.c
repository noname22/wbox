/*
 * WBOX Virtual Machine Manager
 * Sets up protected mode execution environment for PE binaries
 */
#include "vm.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../cpu/platform.h"
#include "../cpu/x86.h"
#include "../loader/loader.h"
#include "../thread/scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global VM context pointer for syscall handler access */
static vm_context_t *g_vm_context = NULL;

/* GDT entry structure */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/* GDT access byte values */
#define GDT_PRESENT     0x80
#define GDT_DPL_RING0   0x00
#define GDT_DPL_RING3   0x60
#define GDT_TYPE_CODE   0x1A  /* Executable, readable, accessed */
#define GDT_TYPE_DATA   0x12  /* Writable, accessed */

/* GDT flags (granularity byte, high nibble) */
#define GDT_FLAG_GRAN   0x80  /* 4KB granularity */
#define GDT_FLAG_32BIT  0x40  /* 32-bit segment */

/* IDT entry structure */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

/* IDT type attributes */
#define IDT_PRESENT     0x80
#define IDT_DPL_RING3   0x60
#define IDT_TYPE_INT32  0x0E  /* 32-bit interrupt gate */
#define IDT_TYPE_TRAP32 0x0F  /* 32-bit trap gate */

/* Physical addresses for system structures */
#define GDT_PHYS_ADDR   0x00001000
#define IDT_PHYS_ADDR   0x00002000
#define SYSENTER_STACK  0x00010000  /* Kernel stack for SYSENTER */

/* Helper: Create GDT entry */
static void make_gdt_entry(gdt_entry_t *entry, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags)
{
    entry->limit_low = limit & 0xFFFF;
    entry->base_low = base & 0xFFFF;
    entry->base_mid = (base >> 16) & 0xFF;
    entry->access = access;
    entry->limit_high_flags = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    entry->base_high = (base >> 24) & 0xFF;
}

/* Helper: Write GDT entry to physical memory */
static void write_gdt_entry(uint32_t phys, int index, const gdt_entry_t *entry)
{
    uint32_t addr = phys + index * 8;
    mem_writew_phys(addr + 0, entry->limit_low);
    mem_writew_phys(addr + 2, entry->base_low);
    mem_writeb_phys(addr + 4, entry->base_mid);
    mem_writeb_phys(addr + 5, entry->access);
    mem_writeb_phys(addr + 6, entry->limit_high_flags);
    mem_writeb_phys(addr + 7, entry->base_high);
}

int vm_init(vm_context_t *vm)
{
    memset(vm, 0, sizeof(*vm));

    /* Set global context for syscall handler */
    g_vm_context = vm;

    /* Initialize handle table with stdin/stdout/stderr */
    handles_init(&vm->handles);

    /* Initialize paging at 1MB physical */
    paging_init(&vm->paging, PAGING_PHYS_BASE, VM_PHYS_MEM_SIZE);

    /* Set up memory layout */
    vm->stack_top = VM_USER_STACK_TOP;
    vm->stack_base = VM_USER_STACK_TOP - VM_USER_STACK_SIZE;
    vm->teb_addr = VM_TEB_ADDR;
    vm->peb_addr = VM_PEB_ADDR;

    /* GDT/IDT addresses */
    vm->gdt_phys = GDT_PHYS_ADDR;
    vm->gdt_virt = GDT_PHYS_ADDR;  /* Identity mapped */
    vm->gdt_limit = VM_GDT_ENTRIES * 8 - 1;

    vm->idt_phys = IDT_PHYS_ADDR;
    vm->idt_virt = IDT_PHYS_ADDR;  /* Identity mapped */
    vm->idt_limit = 256 * 8 - 1;

    return 0;
}

int vm_load_pe(vm_context_t *vm, const char *path)
{
    pe_image_t pe;

    if (pe_load(path, &pe) != 0) {
        fprintf(stderr, "vm_load_pe: failed to load PE file: %s\n", path);
        return -1;
    }

    pe_dump_info(&pe);

    /* Determine load address */
    vm->image_base = pe.image_base;
    vm->entry_point = vm->image_base + pe.entry_point_rva;
    vm->size_of_image = pe.size_of_image;

    printf("Loading PE at 0x%08X, entry point 0x%08X\n",
           vm->image_base, vm->entry_point);

    /* Allocate physical memory for the image */
    uint32_t image_phys = paging_alloc_phys(&vm->paging, pe.size_of_image);
    if (image_phys == 0) {
        fprintf(stderr, "vm_load_pe: failed to allocate physical memory\n");
        pe_free(&pe);
        return -1;
    }

    /* Copy PE headers */
    for (uint32_t i = 0; i < pe.size_of_headers; i++) {
        mem_writeb_phys(image_phys + i, pe.file_data[i]);
    }

    /* Copy sections */
    for (int i = 0; i < pe.num_sections; i++) {
        const pe_section_t *sec = &pe.sections[i];
        uint32_t sec_phys = image_phys + sec->virtual_address;
        uint32_t copy_size = (sec->raw_size < sec->virtual_size)
                           ? sec->raw_size : sec->virtual_size;

        printf("  Section %s: VA=0x%08X size=0x%X -> phys=0x%08X\n",
               sec->name, sec->virtual_address, sec->virtual_size, sec_phys);

        /* Copy raw data */
        for (uint32_t j = 0; j < copy_size; j++) {
            mem_writeb_phys(sec_phys + j, pe.file_data[sec->raw_offset + j]);
        }

        /* Zero-fill remainder (BSS-like) */
        if (sec->virtual_size > copy_size) {
            for (uint32_t j = copy_size; j < sec->virtual_size; j++) {
                mem_writeb_phys(sec_phys + j, 0);
            }
        }
    }

    /* Apply base relocations if loaded at different address */
    if (pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].size > 0) {
        uint32_t reloc_rva = pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].virtual_address;
        uint32_t reloc_size = pe.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC].size;
        int64_t delta = (int64_t)vm->image_base - (int64_t)pe.image_base;

        if (delta != 0) {
            printf("Applying relocations (delta=%lld)\n", (long long)delta);

            uint32_t offset = 0;
            while (offset < reloc_size) {
                uint32_t block_rva = *(uint32_t *)(pe.file_data +
                                      pe_rva_to_file_offset(&pe, reloc_rva + offset));
                uint32_t block_size = *(uint32_t *)(pe.file_data +
                                       pe_rva_to_file_offset(&pe, reloc_rva + offset + 4));

                if (block_size == 0)
                    break;

                uint32_t entry_count = (block_size - 8) / 2;
                for (uint32_t i = 0; i < entry_count; i++) {
                    uint16_t entry = *(uint16_t *)(pe.file_data +
                                      pe_rva_to_file_offset(&pe, reloc_rva + offset + 8 + i * 2));
                    uint8_t type = entry >> 12;
                    uint16_t off = entry & 0xFFF;

                    if (type == IMAGE_REL_BASED_HIGHLOW) {
                        uint32_t addr_phys = image_phys + block_rva + off;
                        uint32_t val = mem_readl_phys(addr_phys);
                        mem_writel_phys(addr_phys, val + (uint32_t)delta);
                    }
                }
                offset += block_size;
            }
        }
    }

    /* Map the PE image into virtual address space */
    uint32_t map_flags = PTE_USER | PTE_WRITABLE;  /* Full access for now */
    if (paging_map_range(&vm->paging, vm->image_base, image_phys,
                         pe.size_of_image, map_flags) != 0) {
        fprintf(stderr, "vm_load_pe: failed to map PE image\n");
        pe_free(&pe);
        return -1;
    }

    /* Allocate and map user stack
     * We need to map the entire range from the page containing stack_base
     * to the page containing stack_top (inclusive).
     */
    uint32_t stack_base_page = vm->stack_base & PAGE_MASK;
    uint32_t stack_top_page = vm->stack_top & PAGE_MASK;
    uint32_t stack_map_size = (stack_top_page - stack_base_page) + PAGE_SIZE;
    uint32_t stack_phys = paging_alloc_phys(&vm->paging, stack_map_size);
    if (stack_phys == 0) {
        fprintf(stderr, "vm_load_pe: failed to allocate stack\n");
        pe_free(&pe);
        return -1;
    }
    if (paging_map_range(&vm->paging, stack_base_page, stack_phys,
                         stack_map_size, PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe: failed to map stack\n");
        pe_free(&pe);
        return -1;
    }
    printf("User stack: 0x%08X-0x%08X (phys 0x%08X, mapped 0x%X bytes)\n",
           vm->stack_base, vm->stack_top, stack_phys, stack_map_size);

    /* Allocate and map TEB */
    uint32_t teb_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (teb_phys == 0) {
        fprintf(stderr, "vm_load_pe: failed to allocate TEB\n");
        pe_free(&pe);
        return -1;
    }
    if (paging_map_page(&vm->paging, vm->teb_addr, teb_phys,
                        PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe: failed to map TEB\n");
        pe_free(&pe);
        return -1;
    }
    printf("TEB at 0x%08X (phys 0x%08X)\n", vm->teb_addr, teb_phys);

    /* Allocate and map PEB */
    uint32_t peb_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (peb_phys == 0) {
        fprintf(stderr, "vm_load_pe: failed to allocate PEB\n");
        pe_free(&pe);
        return -1;
    }
    if (paging_map_page(&vm->paging, vm->peb_addr, peb_phys,
                        PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe: failed to map PEB\n");
        pe_free(&pe);
        return -1;
    }
    printf("PEB at 0x%08X (phys 0x%08X)\n", vm->peb_addr, peb_phys);

    pe_free(&pe);
    return 0;
}

int vm_setup_gdt(vm_context_t *vm)
{
    gdt_entry_t entry;

    printf("Setting up GDT at 0x%08X\n", vm->gdt_phys);

    /* Entry 0: Null descriptor */
    memset(&entry, 0, sizeof(entry));
    write_gdt_entry(vm->gdt_phys, 0, &entry);

    /* Entry 1 (0x08): Ring 0 code segment - flat 4GB */
    make_gdt_entry(&entry, 0, 0xFFFFF,
                   GDT_PRESENT | GDT_DPL_RING0 | GDT_TYPE_CODE,
                   GDT_FLAG_GRAN | GDT_FLAG_32BIT);
    write_gdt_entry(vm->gdt_phys, 1, &entry);

    /* Entry 2 (0x10): Ring 0 data segment - flat 4GB */
    make_gdt_entry(&entry, 0, 0xFFFFF,
                   GDT_PRESENT | GDT_DPL_RING0 | GDT_TYPE_DATA,
                   GDT_FLAG_GRAN | GDT_FLAG_32BIT);
    write_gdt_entry(vm->gdt_phys, 2, &entry);

    /* Entry 3 (0x18): Ring 3 code segment - flat 4GB */
    make_gdt_entry(&entry, 0, 0xFFFFF,
                   GDT_PRESENT | GDT_DPL_RING3 | GDT_TYPE_CODE,
                   GDT_FLAG_GRAN | GDT_FLAG_32BIT);
    write_gdt_entry(vm->gdt_phys, 3, &entry);

    /* Entry 4 (0x20): Ring 3 data segment - flat 4GB */
    make_gdt_entry(&entry, 0, 0xFFFFF,
                   GDT_PRESENT | GDT_DPL_RING3 | GDT_TYPE_DATA,
                   GDT_FLAG_GRAN | GDT_FLAG_32BIT);
    write_gdt_entry(vm->gdt_phys, 4, &entry);

    /* Entry 5 (0x28): Reserved */
    memset(&entry, 0, sizeof(entry));
    write_gdt_entry(vm->gdt_phys, 5, &entry);

    /* Entry 6 (0x30): Reserved */
    write_gdt_entry(vm->gdt_phys, 6, &entry);

    /* Entry 7 (0x38): Ring 3 TEB segment (FS) - base=TEB, limit=4KB */
    make_gdt_entry(&entry, vm->teb_addr, 0xFFF,
                   GDT_PRESENT | GDT_DPL_RING3 | GDT_TYPE_DATA,
                   GDT_FLAG_32BIT);  /* No granularity - byte limit */
    write_gdt_entry(vm->gdt_phys, 7, &entry);

    /* Load GDT into CPU */
    gdt.base = vm->gdt_phys;
    gdt.limit = vm->gdt_limit;

    printf("GDT loaded: base=0x%08X limit=0x%04X\n", gdt.base, gdt.limit);

    return 0;
}

int vm_setup_idt(vm_context_t *vm)
{
    printf("Setting up IDT at 0x%08X\n", vm->idt_phys);

    /* Clear all IDT entries */
    for (int i = 0; i < 256; i++) {
        uint32_t addr = vm->idt_phys + i * 8;
        mem_writel_phys(addr + 0, 0);
        mem_writel_phys(addr + 4, 0);
    }

    /* Load IDT into CPU */
    idt.base = vm->idt_phys;
    idt.limit = vm->idt_limit;

    printf("IDT loaded: base=0x%08X limit=0x%04X\n", idt.base, idt.limit);

    return 0;
}

void vm_setup_paging(vm_context_t *vm)
{
    printf("Enabling paging, CR3=0x%08X\n", vm->paging.cr3);

    /* Identity map low memory for GDT/IDT access */
    paging_map_range(&vm->paging, 0, 0, 0x100000, PTE_WRITABLE);

    /* Set CR3 */
    cr3 = vm->paging.cr3;

    /* Enable paging (CR0.PG) and protection (CR0.PE) */
    cr0 = 0x80000001;

    /* Flush TLB */
    flushmmucache();
}

void vm_setup_sysenter(vm_context_t *vm)
{
    (void)vm;

    printf("Configuring SYSENTER MSRs\n");

    /* SYSENTER_CS: Ring 0 code segment selector */
    msr.sysenter_cs = VM_SEL_KERNEL_CODE;

    /* SYSENTER_ESP: Ring 0 stack pointer */
    msr.sysenter_esp = SYSENTER_STACK + PAGE_SIZE;

    /* SYSENTER_EIP: Ring 0 entry point (not used with callback) */
    msr.sysenter_eip = 0;

    printf("  SYSENTER_CS=0x%04X ESP=0x%08X EIP=0x%08X\n",
           msr.sysenter_cs, msr.sysenter_esp, msr.sysenter_eip);
}

void vm_setup_cpu_state(vm_context_t *vm)
{
    printf("Setting up CPU state for Ring 3 entry\n");

    /* Enable 32-bit operand/address mode */
    use32 = 0x300;

    /* Set CPU status flags for protected mode with flat 32-bit segments */
    cpu_cur_status = CPU_STATUS_USE32 | CPU_STATUS_STACK32 | CPU_STATUS_PMODE;
    /* NOTFLATDS and NOTFLATSS are cleared (flat segments) */

    /* Set stack32 global variable (separate from cpu_cur_status) */
    stack32 = 1;

    /* Set segment registers */
    cpu_state.seg_cs.seg = VM_SEL_USER_CODE;
    cpu_state.seg_cs.base = 0;
    cpu_state.seg_cs.limit = 0xFFFFFFFF;
    cpu_state.seg_cs.limit_low = 0;
    cpu_state.seg_cs.limit_high = 0xFFFFFFFF;
    cpu_state.seg_cs.access = 0xFB;  /* Present, DPL=3, Code, Readable */
    cpu_state.seg_cs.ar_high = 0xCF; /* 32-bit, 4KB granularity */

    cpu_state.seg_ds.seg = VM_SEL_USER_DATA;
    cpu_state.seg_ds.base = 0;
    cpu_state.seg_ds.limit = 0xFFFFFFFF;
    cpu_state.seg_ds.limit_low = 0;
    cpu_state.seg_ds.limit_high = 0xFFFFFFFF;
    cpu_state.seg_ds.access = 0xF3;  /* Present, DPL=3, Data, Writable */
    cpu_state.seg_ds.ar_high = 0xCF;

    cpu_state.seg_es = cpu_state.seg_ds;
    cpu_state.seg_ss = cpu_state.seg_ds;
    cpu_state.seg_gs = cpu_state.seg_ds;

    /* FS points to TEB */
    cpu_state.seg_fs.seg = VM_SEL_TEB;
    cpu_state.seg_fs.base = vm->teb_addr;
    cpu_state.seg_fs.limit = 0xFFF;
    cpu_state.seg_fs.limit_low = 0;
    cpu_state.seg_fs.limit_high = 0xFFF;
    cpu_state.seg_fs.access = 0xF3;
    cpu_state.seg_fs.ar_high = 0x40;  /* 32-bit, byte granularity */

    /* Set EIP to entry point */
    cpu_state.pc = vm->entry_point;

    /* Set ESP to top of stack */
    ESP = vm->stack_top;

    /* Set flags (IF=1) */
    cpu_state.flags = VM_INITIAL_EFLAGS & 0xFFFF;
    cpu_state.eflags = (VM_INITIAL_EFLAGS >> 16) & 0xFFFF;

    /* Clear general purpose registers */
    EAX = 0;
    EBX = 0;
    ECX = 0;
    EDX = 0;
    EBP = 0;
    ESI = 0;
    EDI = 0;

    printf("  CS=0x%04X DS=0x%04X SS=0x%04X FS=0x%04X\n",
           cpu_state.seg_cs.seg, cpu_state.seg_ds.seg,
           cpu_state.seg_ss.seg, cpu_state.seg_fs.seg);
    printf("  EIP=0x%08X ESP=0x%08X EFLAGS=0x%08X\n",
           cpu_state.pc, ESP,
           cpu_state.flags | (cpu_state.eflags << 16));
    printf("  FS.base=0x%08X FS.limit=0x%08X\n",
           cpu_state.seg_fs.base, cpu_state.seg_fs.limit);
    /* Verify TEB contents */
    uint32_t teb_self = readmemll(vm->teb_addr + 0x18);
    uint32_t teb_tid = readmemll(vm->teb_addr + 0x24);
    printf("  TEB[0x18] (Self)=0x%08X TEB[0x24] (ThreadId)=0x%08X\n",
           teb_self, teb_tid);
}

void vm_start(vm_context_t *vm)
{
    printf("\n=== Starting VM execution ===\n\n");

    /* Debug: Dump GetCurrentThreadId code if it exists */
    uint32_t gctid_addr = 0x7C50B920;  /* GetCurrentThreadId */
    printf("Code at GetCurrentThreadId (0x%08X):\n  ", gctid_addr);
    for (int i = 0; i < 16; i++) {
        printf("%02X ", readmembl(gctid_addr + i));
    }
    printf("\n");

    /* Debug: Dump the indirect call instruction that crashes */
    uint32_t crash_addr = 0x7C501240;  /* Location of crash */
    printf("Code at 0x7C501240 (crash location):\n  ");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", readmembl(crash_addr + i));
        if (i == 15) printf("\n  ");
    }
    printf("\n");

    /* Check what address the CALL [mem] at 0x7C4FF8E3 is calling through */
    /* FF 15 XX XX XX XX = CALL [XXXXXXXX] */
    uint32_t call_mem_addr = readmemll(0x7C4FF8E5);  /* Address after FF 15 */
    printf("CALL [0x%08X] - indirect call target address\n", call_mem_addr);
    uint32_t call_target = readmemll(call_mem_addr);
    printf("Value at 0x%08X = 0x%08X (the actual function pointer)\n", call_mem_addr, call_target);
    /* Calculate offset in kernel32.dll */
    uint32_t k32_base = 0x7C4F0000;
    printf("Offset in kernel32.dll IAT: 0x%08X\n", call_mem_addr - k32_base);

    /* Debug: Dump IDT entry for page fault (vector 0x0E) */
    uint32_t idt_pf = vm->idt_phys + 0x0E * 8;
    printf("IDT entry 0x0E (page fault) at phys 0x%08X:\n  ", idt_pf);
    for (int i = 0; i < 8; i++) {
        printf("%02X ", mem_readb_phys(idt_pf + i));
    }
    printf("\n");

    /* Debug: Dump IAT entry for GetCurrentThreadId (jmp [0x00410354]) */
    uint32_t iat_addr = 0x00410354;
    uint32_t iat_val = readmemll(iat_addr);
    printf("IAT[0x%08X] = 0x%08X (should be GetCurrentThreadId 0x7C50B920)\n\n", iat_addr, iat_val);

    /* Debug: Dump patched RtlAllocateHeap */
    uint32_t rtl_alloc_heap = 0x7C824120;
    printf("Code at RtlAllocateHeap (0x%08X):\n  ", rtl_alloc_heap);
    for (int i = 0; i < 16; i++) {
        printf("%02X ", readmembl(rtl_alloc_heap + i));
    }
    printf("\n");

    /* Debug: Dump first 16 bytes of code at entry point */
    printf("Code at entry point 0x%08X:\n  ", vm->entry_point);
    for (int i = 0; i < 16; i++) {
        printf("%02X ", readmembl(vm->entry_point + i));
    }
    printf("\n\n");
    fflush(stdout);

    vm->exit_requested = 0;
    cpu_exit_requested = 0;  /* Reset CPU exit flag */

    /* Initialize scheduler if not already done */
    wbox_scheduler_t *sched = vm->scheduler;
    if (!sched) {
        sched = calloc(1, sizeof(wbox_scheduler_t));
        if (sched && scheduler_init(sched, vm) == 0) {
            vm->scheduler = sched;
        } else {
            free(sched);
            sched = NULL;
            fprintf(stderr, "Warning: Failed to initialize scheduler, running without threading\n");
        }
    }

    /* Run until exit is requested */
    while (!vm->exit_requested) {
        /* Execute some CPU cycles if we have a running thread (not idle thread) */
        if (!sched || (sched->current_thread && !sched->current_thread->is_idle_thread)) {
            exec386(1000);

            /* Scheduler tick for preemption */
            if (sched) {
                sched->tick_count++;
                scheduler_tick(sched);
            }
        }

        /* Process display events and render if in GUI mode */
        if (vm->gui_mode && vm->display.initialized) {
            /* Process SDL events */
            if (display_poll_events(&vm->display)) {
                /* Quit requested via SDL (window close, ESC) */
                vm->exit_requested = 1;
                vm->exit_code = 0;
                break;
            }

            /* Present frame buffer to screen */
            display_present(&vm->display);
        }

        /* Check for timeout expiry on waiting threads */
        if (sched) {
            scheduler_check_timeouts(sched);
        }

        /* If idle thread is running, check if we can switch to a ready thread */
        if (sched && sched->current_thread && sched->current_thread->is_idle_thread) {
            if (sched->ready_head != NULL) {
                /* Threads became ready (e.g., from timeout), switch to them */
                scheduler_switch(sched);
            } else {
                /* No runnable threads, sleep briefly */
                usleep(1000);  /* 1ms idle sleep */
            }
        }
    }

    printf("VM execution stopped (exit code: 0x%08X)\n", vm->exit_code);
}

void vm_request_exit(vm_context_t *vm, uint32_t code)
{
    vm->exit_requested = 1;
    vm->exit_code = code;
}

int vm_call_dll_entry(vm_context_t *vm, uint32_t entry_point, uint32_t base_va, uint32_t reason)
{
    /* Save current CPU state */
    uint32_t saved_eip = cpu_state.pc;
    uint32_t saved_esp = ESP;
    uint32_t saved_eax = EAX;
    uint32_t saved_ebx = EBX;
    uint32_t saved_ecx = ECX;
    uint32_t saved_edx = EDX;
    uint32_t saved_esi = ESI;
    uint32_t saved_edi = EDI;
    uint32_t saved_ebp = EBP;

    /* Reset DLL init done flag */
    vm->dll_init_done = 0;
    cpu_exit_requested = 0;

    fprintf(stderr, "DLL_ENTRY: start ESP=0x%08X, saved_esp=0x%08X\n", ESP, saved_esp);

    /* Check if this is shell32 - enable tracing */
    static int trace_enabled = 0;
    if (entry_point == 0x7A47FBF0) {
        fprintf(stderr, ">>> TRACING shell32.dll entry point <<<\n");
        trace_enabled = 1;
    }

    /* After msvcrt.dll init, check lock table state */
    if (entry_point == 0x7C311000) {  /* msvcrt.dll entry */
        fprintf(stderr, ">>> msvcrt.dll DllMain starting <<<\n");
    }

    /* Push arguments for DllMain in stdcall order (right to left)
     * BOOL WINAPI DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
     */
    ESP -= 4;
    writememll(ESP, 0);         /* lpReserved = NULL */
    ESP -= 4;
    writememll(ESP, reason);    /* ul_reason_for_call */
    ESP -= 4;
    writememll(ESP, base_va);   /* hModule = DLL base address */

    /* Push return address (points to DLL init stub) */
    ESP -= 4;
    writememll(ESP, vm->dll_init_stub_addr);

    /* Set EIP to entry point */
    cpu_state.pc = entry_point;

    /* Run until DLL init done */
    int iter_count = 0;
    uint32_t last_esp = ESP;
    wbox_scheduler_t *sched = (wbox_scheduler_t *)vm->scheduler;
    while (!vm->dll_init_done && !vm->exit_requested) {
        /* Check if scheduler is idle (no runnable threads) */
        if (sched && sched->idle) {
            /* Check for timeout wakeups */
            scheduler_check_timeouts(sched);

            /* If still idle, try to fast-forward to the next timeout.
             * During DLL init, waiting on a mutex/event that won't fire should
             * timeout rather than deadlock. Find the next timeout and jump to it. */
            if (sched->idle) {
                uint64_t next_timeout = 0;
                bool has_timeout = false;
                for (wbox_thread_t *t = sched->all_threads; t; t = t->next) {
                    if (t->state == THREAD_STATE_WAITING && t->wait_timeout != 0) {
                        if (!has_timeout || t->wait_timeout < next_timeout) {
                            next_timeout = t->wait_timeout;
                            has_timeout = true;
                        }
                    }
                }

                if (has_timeout) {
                    /* Fast-forward the scheduler clock to the timeout */
                    uint64_t now = scheduler_get_time_100ns();
                    if (next_timeout > now) {
                        /* Actually advance time by setting the time offset */
                        uint64_t advance = next_timeout - now + 1;
                        scheduler_advance_time(sched, advance);
                    }
                    /* Now check timeouts again - this should wake the thread */
                    scheduler_check_timeouts(sched);
                }

                if (sched->idle) {
                    /* No threads with timeouts - truly deadlocked */
                    fprintf(stderr, "DLL_ENTRY: DEADLOCK - scheduler idle, no timeouts\n");
                    vm->exit_requested = 1;
                    vm->exit_code = 0xDEAD;
                    break;
                }
            }

            /* A thread woke up - schedule it */
            cpu_exit_requested = 0;
            scheduler_switch(sched);
            continue;
        }

        /* For shell32, run one instruction at a time and trace */
        if (trace_enabled && iter_count < 100) {
            fprintf(stderr, "TRACE[%d]: EIP=0x%08X ESP=0x%08X EAX=0x%08X\n",
                    iter_count, cpu_state.pc, ESP, EAX);
            exec386(1);
            fprintf(stderr, "TRACE[%d]: AFTER: EIP=0x%08X ESP=0x%08X\n",
                    iter_count, cpu_state.pc, ESP);
        } else {
            exec386(1000);
        }
        iter_count++;
        /* Log significant ESP changes */
        if ((ESP < last_esp - 0x10000) || (ESP > last_esp + 0x10000)) {
            fprintf(stderr, "DLL_ENTRY: ESP changed from 0x%08X to 0x%08X after %d iterations\n",
                    last_esp, ESP, iter_count);
            last_esp = ESP;
        }
        /* Log if stack is getting dangerously low */
        if (ESP < 0x06000000 && last_esp >= 0x06000000) {
            fprintf(stderr, "WARNING: Stack below 0x06000000! ESP=0x%08X at iter %d\n", ESP, iter_count);
        }
    }

    fprintf(stderr, "DLL_ENTRY: end ESP=0x%08X (before restore)\n", ESP);

    /* After msvcrt.dll init, verify lock table state */
    if (entry_point == 0x7C311000) {  /* msvcrt.dll entry */
        /* Lock 0x11 (lock table lock) should be at 0x7c35f4dc (initialized) / 0x7c35f4e0 (CS) */
        uint32_t lock_11_init_addr = 0x7c35f4dc;
        uint32_t lock_11_phys = paging_get_phys(&vm->paging, lock_11_init_addr);
        if (lock_11_phys) {
            uint32_t init_val = mem_readl_phys(lock_11_phys);
            fprintf(stderr, ">>> msvcrt.dll: Lock 0x11 initialized flag at 0x%08X = 0x%08X <<<\n",
                    lock_11_init_addr, init_val);
        } else {
            fprintf(stderr, ">>> msvcrt.dll: Lock 0x11 address 0x%08X not mapped! <<<\n",
                    lock_11_init_addr);
        }
    }

    /* Get return value from EAX (DllMain returns BOOL) */
    int result = (EAX != 0);

    /* Restore CPU state */
    cpu_state.pc = saved_eip;
    ESP = saved_esp;
    EAX = saved_eax;
    EBX = saved_ebx;
    ECX = saved_ecx;
    EDX = saved_edx;
    ESI = saved_esi;
    EDI = saved_edi;
    EBP = saved_ebp;

    /* Reset flags for normal execution */
    vm->dll_init_done = 0;
    vm->exit_requested = 0;
    cpu_exit_requested = 0;

    return result;
}

int vm_init_dlls(vm_context_t *vm)
{
    if (!vm->loader) {
        fprintf(stderr, "vm_init_dlls: No loader context\n");
        return -1;
    }

    loader_context_t *loader = (loader_context_t *)vm->loader;

    printf("\n=== Initializing DLLs ===\n");

    /* DLLs need to be initialized in correct dependency order.
     * The module list order reflects encounter order during import resolution.
     * We use reverse order (tail to head) because dependencies of a module
     * are typically encountered and added to the list after the module itself. */

    /* Count modules */
    int count = 0;
    loaded_module_t *mod = loader->modules.modules;
    while (mod) {
        count++;
        mod = mod->next;
    }

    if (count == 0) {
        printf("  No DLLs to initialize\n");
        return 0;
    }

    /* Allocate array */
    loaded_module_t **modules = malloc(count * sizeof(loaded_module_t *));
    if (!modules) {
        fprintf(stderr, "vm_init_dlls: Out of memory\n");
        return -1;
    }

    /* Fill array in list order (index 0 = head) */
    mod = loader->modules.modules;
    for (int i = 0; i < count; i++) {
        modules[i] = mod;
        mod = mod->next;
    }

    /* Track which modules have been initialized */
    bool *inited = calloc(count, sizeof(bool));
    if (!inited) {
        free(modules);
        return -1;
    }

    int initialized = 0;

    /* Helper function to init a DLL */
    #define INIT_DLL(m, idx) do { \
        if (!(m)->is_main_exe && !inited[idx] && \
            (m)->entry_point != 0 && (m)->entry_point != (m)->base_va) { \
            printf("  Initializing %s (entry=0x%08X, ESP=0x%08X)...", (m)->name, (m)->entry_point, ESP); \
            fflush(stdout); \
            int r = vm_call_dll_entry(vm, (m)->entry_point, (m)->base_va, 1); \
            printf(" [post ESP=0x%08X]", ESP); \
            if (r) { printf(" OK\n"); initialized++; } \
            else { printf(" FAILED\n"); } \
            inited[idx] = true; \
        } \
    } while(0)

    /* Initialize core DLLs first in dependency order:
     * 1. kernel32.dll (depends on ntdll)
     * 2. gdi32.dll (depends on kernel32, ntdll)
     * 3. user32.dll (depends on gdi32, kernel32, ntdll)
     * These must be initialized before DLLs that use USER functions. */
    const char *priority_dlls[] = {
        "kernel32.dll", "msvcrt.dll", "advapi32.dll",
        "gdi32.dll", "user32.dll", NULL
    };

    for (int p = 0; priority_dlls[p] != NULL; p++) {
        for (int i = 0; i < count; i++) {
            if (strcasecmp(modules[i]->name, priority_dlls[p]) == 0) {
                INIT_DLL(modules[i], i);
                break;
            }
        }
    }

    /* Initialize remaining DLLs */
    for (int i = count - 1; i >= 0; i--) {
        mod = modules[i];
        INIT_DLL(mod, i);
    }

    #undef INIT_DLL
    free(inited);

    free(modules);

    printf("  Initialized %d DLLs\n", initialized);

    return 0;
}

vm_context_t *vm_get_context(void)
{
    return g_vm_context;
}

void vm_dump_state(vm_context_t *vm)
{
    printf("\n=== VM State ===\n");
    printf("Image: base=0x%08X entry=0x%08X size=0x%X\n",
           vm->image_base, vm->entry_point, vm->size_of_image);
    printf("Stack: base=0x%08X top=0x%08X\n", vm->stack_base, vm->stack_top);
    printf("TEB=0x%08X PEB=0x%08X\n", vm->teb_addr, vm->peb_addr);
    printf("\nCPU State:\n");
    printf("  EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", EAX, EBX, ECX, EDX);
    printf("  ESP=%08X EBP=%08X ESI=%08X EDI=%08X\n", ESP, EBP, ESI, EDI);
    printf("  EIP=%08X\n", cpu_state.pc);
    printf("  CS=%04X DS=%04X ES=%04X SS=%04X FS=%04X GS=%04X\n",
           cpu_state.seg_cs.seg, cpu_state.seg_ds.seg, cpu_state.seg_es.seg,
           cpu_state.seg_ss.seg, cpu_state.seg_fs.seg, cpu_state.seg_gs.seg);
    printf("\nPaging:\n");
    paging_dump(&vm->paging);
}

uint32_t vm_va_to_phys(vm_context_t *vm, uint32_t va)
{
    return paging_get_phys(&vm->paging, va);
}

int vm_load_pe_with_dlls(vm_context_t *vm, const char *exe_path,
                         const char *ntdll_path)
{
    /* Allocate loader context */
    loader_context_t *loader = malloc(sizeof(loader_context_t));
    if (!loader) {
        fprintf(stderr, "vm_load_pe_with_dlls: Out of memory\n");
        return -1;
    }

    /* Initialize loader */
    if (loader_init(loader, vm) < 0) {
        free(loader);
        return -1;
    }

    /* Set ntdll path if provided */
    if (ntdll_path) {
        loader_set_ntdll_path(loader, ntdll_path);
    }

    /* Store loader in VM context */
    vm->loader = loader;

    /* Allocate and map PEB BEFORE loading, so loader can set PEB.Ldr */
    uint32_t peb_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (peb_phys == 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to allocate PEB\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    if (paging_map_page(&vm->paging, vm->peb_addr, peb_phys,
                        PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to map PEB\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    printf("PEB at 0x%08X (phys 0x%08X)\n", vm->peb_addr, peb_phys);

    /* Load executable with DLLs */
    if (loader_load_executable(loader, vm, exe_path) < 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to load executable\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }

    /* Update VM state from loader */
    vm->image_base = loader_get_image_base(loader);
    vm->entry_point = loader_get_entry_point(loader);
    if (loader->main_module) {
        vm->size_of_image = loader->main_module->size;
    }

    /* Allocate and map user stack */
    uint32_t stack_base_page = vm->stack_base & PAGE_MASK;
    uint32_t stack_top_page = vm->stack_top & PAGE_MASK;
    uint32_t stack_map_size = (stack_top_page - stack_base_page) + PAGE_SIZE;
    uint32_t stack_phys = paging_alloc_phys(&vm->paging, stack_map_size);
    if (stack_phys == 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to allocate stack\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    if (paging_map_range(&vm->paging, stack_base_page, stack_phys,
                         stack_map_size, PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to map stack\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    printf("User stack: 0x%08X-0x%08X (phys 0x%08X, mapped 0x%X bytes)\n",
           vm->stack_base, vm->stack_top, stack_phys, stack_map_size);

    /* Allocate and map TEB */
    uint32_t teb_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (teb_phys == 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to allocate TEB\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    if (paging_map_page(&vm->paging, vm->teb_addr, teb_phys,
                        PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to map TEB\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    printf("TEB at 0x%08X (phys 0x%08X)\n", vm->teb_addr, teb_phys);

    /* PEB was already allocated and mapped before loader_load_executable
     * so that the loader could set PEB.Ldr */

    /* Allocate and map KUSER_SHARED_DATA at 0x7FFE0000
     * This is used by ntdll for syscalls via the SystemCall pointer at offset 0x300 */
    uint32_t kusd_phys = paging_alloc_phys(&vm->paging, PAGE_SIZE);
    if (kusd_phys == 0) {
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to allocate KUSER_SHARED_DATA\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }
    if (paging_map_page(&vm->paging, VM_KUSD_ADDR, kusd_phys,
                        PTE_USER) != 0) {  /* Read-only for user mode */
        fprintf(stderr, "vm_load_pe_with_dlls: Failed to map KUSER_SHARED_DATA\n");
        loader_free(loader);
        free(loader);
        vm->loader = NULL;
        return -1;
    }

    /* Create syscall stub at offset 0x340 in KUSD page
     * This stub executes SYSENTER to enter kernel mode
     * Matches Windows KiFastSystemCall:
     * Code: 89 E2 (MOV EDX, ESP), 0F 34 (SYSENTER), C3 (RET)
     * MOV EDX, ESP is required because the kernel uses EDX to locate
     * the user stack and read syscall arguments. */
    uint32_t syscall_stub_va = VM_KUSD_ADDR + 0x340;
    mem_writeb_phys(kusd_phys + 0x340, 0x89);  /* MOV EDX, ESP opcode byte 1 */
    mem_writeb_phys(kusd_phys + 0x341, 0xE2);  /* MOV EDX, ESP opcode byte 2 */
    mem_writeb_phys(kusd_phys + 0x342, 0x0F);  /* SYSENTER opcode byte 1 */
    mem_writeb_phys(kusd_phys + 0x343, 0x34);  /* SYSENTER opcode byte 2 */
    mem_writeb_phys(kusd_phys + 0x344, 0xC3);  /* RET */

    /* Set SystemCall pointer at offset 0x300 to point to our stub */
    mem_writel_phys(kusd_phys + 0x300, syscall_stub_va);

    /* Create DLL init return stub at offset 0x350 in KUSD page
     * This stub is used as the return address for DllMain calls
     * Code: B8 FE FF 00 00 (MOV EAX, 0xFFFE), 0F 34 (SYSENTER), CC (INT3 - should never reach) */
    uint32_t dll_init_stub_va = VM_KUSD_ADDR + 0x350;
    mem_writeb_phys(kusd_phys + 0x350, 0xB8);  /* MOV EAX, imm32 */
    mem_writeb_phys(kusd_phys + 0x351, 0xFE);  /* 0x0000FFFE low byte */
    mem_writeb_phys(kusd_phys + 0x352, 0xFF);
    mem_writeb_phys(kusd_phys + 0x353, 0x00);
    mem_writeb_phys(kusd_phys + 0x354, 0x00);
    mem_writeb_phys(kusd_phys + 0x355, 0x0F);  /* SYSENTER */
    mem_writeb_phys(kusd_phys + 0x356, 0x34);
    mem_writeb_phys(kusd_phys + 0x357, 0xCC);  /* INT3 - should never reach */
    vm->dll_init_stub_addr = dll_init_stub_va;

    printf("KUSER_SHARED_DATA at 0x%08X (phys 0x%08X)\n", VM_KUSD_ADDR, kusd_phys);
    printf("  SystemCall stub at 0x%08X\n", syscall_stub_va);
    printf("  DLL init stub at 0x%08X\n", dll_init_stub_va);

    /* Verify the memory was written correctly */
    uint32_t verify_syscall_ptr = mem_readl_phys(kusd_phys + 0x300);
    uint8_t verify_mov0 = mem_readb_phys(kusd_phys + 0x340);
    uint8_t verify_mov1 = mem_readb_phys(kusd_phys + 0x341);
    uint8_t verify_sysenter0 = mem_readb_phys(kusd_phys + 0x342);
    uint8_t verify_sysenter1 = mem_readb_phys(kusd_phys + 0x343);
    printf("  [DEBUG] @0x7FFE0300 = 0x%08X (expect 0x%08X)\n", verify_syscall_ptr, syscall_stub_va);
    printf("  [DEBUG] @0x7FFE0340 = %02X %02X %02X %02X (expect 89 E2 0F 34)\n",
           verify_mov0, verify_mov1, verify_sysenter0, verify_sysenter1);

    return 0;
}
