/*
 * WBOX Virtual Machine Manager
 * Sets up protected mode execution environment for PE binaries
 */
#include "vm.h"
#include "../cpu/cpu.h"
#include "../cpu/mem.h"
#include "../cpu/platform.h"
#include "../cpu/x86.h"

#include <stdio.h>
#include <string.h>

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

    /* Allocate and map user stack */
    uint32_t stack_phys = paging_alloc_phys(&vm->paging, VM_USER_STACK_SIZE);
    if (stack_phys == 0) {
        fprintf(stderr, "vm_load_pe: failed to allocate stack\n");
        pe_free(&pe);
        return -1;
    }
    if (paging_map_range(&vm->paging, vm->stack_base, stack_phys,
                         VM_USER_STACK_SIZE, PTE_USER | PTE_WRITABLE) != 0) {
        fprintf(stderr, "vm_load_pe: failed to map stack\n");
        pe_free(&pe);
        return -1;
    }
    printf("User stack: 0x%08X-0x%08X (phys 0x%08X)\n",
           vm->stack_base, vm->stack_top, stack_phys);

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
}

void vm_start(vm_context_t *vm)
{
    printf("\n=== Starting VM execution ===\n\n");

    /* Debug: Dump first 16 bytes of code at entry point */
    printf("Code at entry point 0x%08X:\n  ", vm->entry_point);
    for (int i = 0; i < 16; i++) {
        printf("%02X ", readmembl(vm->entry_point + i));
    }
    printf("\n\n");
    fflush(stdout);

    vm->exit_requested = 0;
    cpu_exit_requested = 0;  /* Reset CPU exit flag */

    /* Run until exit is requested */
    while (!vm->exit_requested) {
        exec386(100);
    }

    printf("VM execution stopped (exit code: 0x%08X)\n", vm->exit_code);
}

void vm_request_exit(vm_context_t *vm, uint32_t code)
{
    vm->exit_requested = 1;
    vm->exit_code = code;
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
