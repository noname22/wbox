/*
 * wbox - Memory interface stub
 * Stub header replacing 86box/mem.h
 */
#ifndef WBOX_MEM_H
#define WBOX_MEM_H

#include <stdint.h>

/* Memory granularity defines */
#define MEM_GRANULARITY_BITS   12
#define MEM_GRANULARITY_SIZE   (1 << MEM_GRANULARITY_BITS)
#define MEM_GRANULARITY_HBOUND (MEM_GRANULARITY_SIZE - 2)
#define MEM_GRANULARITY_QBOUND (MEM_GRANULARITY_SIZE - 4)
#define MEM_GRANULARITY_MASK   (MEM_GRANULARITY_SIZE - 1)
#define MEM_GRANULARITY_HMASK  ((1 << (MEM_GRANULARITY_BITS - 1)) - 1)
#define MEM_GRANULARITY_QMASK  ((1 << (MEM_GRANULARITY_BITS - 2)) - 1)
#define MEM_GRANULARITY_PMASK  ((1 << (MEM_GRANULARITY_BITS - 3)) - 1)
#define MEM_MAPPINGS_NO        ((0x100000 >> MEM_GRANULARITY_BITS) << 12)
#define MEM_GRANULARITY_PAGE   (MEM_GRANULARITY_MASK & ~0xfff)
#define MEM_GRANULARITY_BASE   (~MEM_GRANULARITY_MASK)

/* Memory state flags */
#define MEM_READ_DISABLED  0x4010
#define MEM_READ_INTERNAL  0x1001
#define MEM_READ_EXTERNAL  0
#define MEM_WRITE_DISABLED 0x0200
#define MEM_WRITE_INTERNAL 0x0020
#define MEM_WRITE_EXTERNAL 0

/* Mapping flags */
#define MEM_MAPPING_EXTERNAL 1
#define MEM_MAPPING_INTERNAL 2
#define MEM_MAPPING_ROM_WS   4
#define MEM_MAPPING_IS_ROM   8
#define MEM_MAPPING_ROM      (MEM_MAPPING_ROM_WS | MEM_MAPPING_IS_ROM)
#define MEM_MAPPING_ROMCS    16
#define MEM_MAPPING_SMRAM    32
#define MEM_MAPPING_CACHE    64

/* Page masks for dynarec */
#define PAGE_MASK_SHIFT 6
#define PAGE_MASK_MASK  63

#define PAGE_BYTE_MASK_SHIFT       6
#define PAGE_BYTE_MASK_OFFSET_MASK 63
#define PAGE_BYTE_MASK_MASK        63

#define EVICT_NOT_IN_LIST ((uint32_t) -1)

/* Forward declarations */
struct _mem_mapping_;

/* Page structure for dynarec */
typedef struct page_t {
    void (*write_b)(uint32_t addr, uint8_t val, struct page_t *page);
    void (*write_w)(uint32_t addr, uint16_t val, struct page_t *page);
    void (*write_l)(uint32_t addr, uint32_t val, struct page_t *page);

    uint8_t *mem;

    uint16_t block, block_2;
    uint16_t head;

    uint64_t code_present_mask;
    uint64_t dirty_mask;

    uint32_t evict_prev;
    uint32_t evict_next;

    uint64_t *byte_dirty_mask;
    uint64_t *byte_code_present_mask;
} page_t;

/* Memory mapping structure */
typedef struct _mem_mapping_ {
    struct _mem_mapping_ *prev;
    struct _mem_mapping_ *next;

    int enable;

    uint32_t base;
    uint32_t size;

    uint32_t base_ignore;
    uint32_t mask;

    uint8_t (*read_b)(uint32_t addr, void *priv);
    uint16_t (*read_w)(uint32_t addr, void *priv);
    uint32_t (*read_l)(uint32_t addr, void *priv);
    void (*write_b)(uint32_t addr, uint8_t val, void *priv);
    void (*write_w)(uint32_t addr, uint16_t val, void *priv);
    void (*write_l)(uint32_t addr, uint32_t val, void *priv);

    uint8_t *exec;

    uint32_t flags;

    void *priv;
} mem_mapping_t;

/* RAM and ROM */
extern uint8_t *ram;
extern uint8_t *ram2;
extern uint32_t rammask;
extern uint8_t *rom;
extern uint32_t biosmask;

/* Lookup tables */
extern int        readlookup[256];
extern uintptr_t  old_rl2;
extern uint8_t    uncached;
extern int        readlnext;
extern int        writelookup[256];
extern int        writelnext;

extern page_t    *pages;
extern page_t    *page_lookup[1048576];
extern uintptr_t  readlookup2[1048576];
extern uintptr_t  writelookup2[1048576];

extern uint32_t   get_phys_virt;
extern uint32_t   get_phys_phys;

extern int        mem_a20_state;
extern int        mem_a20_alt;
extern int        mem_a20_key;

extern uint32_t   mem_logical_addr;

extern uint64_t  *byte_dirty_mask;
extern uint64_t  *byte_code_present_mask;

extern uint32_t   purgable_page_list_head;

extern uint8_t   *_mem_exec[MEM_MAPPINGS_NO];

extern int        read_type;
extern uint8_t    high_page;
extern uint8_t    page_ff[4096];

extern int        shadowbios;
extern int        shadowbios_write;

/* Physical address helper functions */
extern uint32_t get_phys(uint32_t addr);
extern uint32_t get_phys_noabrt(uint32_t addr);

/* Memory read/write functions */
extern uint8_t  readmembl(uint32_t addr);
extern void     writemembl(uint32_t addr, uint8_t val);
extern uint16_t readmemwl(uint32_t addr);
extern void     writememwl(uint32_t addr, uint16_t val);
extern uint32_t readmemll(uint32_t addr);
extern void     writememll(uint32_t addr, uint32_t val);
extern uint64_t readmemql(uint32_t addr);
extern void     writememql(uint32_t addr, uint64_t val);

extern uint8_t  readmembl_no_mmut(uint32_t addr, uint32_t a64);
extern void     writemembl_no_mmut(uint32_t addr, uint32_t a64, uint8_t val);
extern uint16_t readmemwl_no_mmut(uint32_t addr, uint32_t *a64);
extern void     writememwl_no_mmut(uint32_t addr, uint32_t *a64, uint16_t val);
extern uint32_t readmemll_no_mmut(uint32_t addr, uint32_t *a64);
extern void     writememll_no_mmut(uint32_t addr, uint32_t *a64, uint32_t val);

extern void     do_mmutranslate(uint32_t addr, uint32_t *a64, int num, int write);

/* 286/386 specific memory functions */
extern uint8_t  readmembl_2386(uint32_t addr);
extern void     writemembl_2386(uint32_t addr, uint8_t val);
extern uint16_t readmemwl_2386(uint32_t addr);
extern void     writememwl_2386(uint32_t addr, uint16_t val);
extern uint32_t readmemll_2386(uint32_t addr);
extern void     writememll_2386(uint32_t addr, uint32_t val);
extern uint64_t readmemql_2386(uint32_t addr);
extern void     writememql_2386(uint32_t addr, uint64_t val);

extern uint8_t  readmembl_no_mmut_2386(uint32_t addr, uint32_t a64);
extern void     writemembl_no_mmut_2386(uint32_t addr, uint32_t a64, uint8_t val);
extern uint16_t readmemwl_no_mmut_2386(uint32_t addr, uint32_t *a64);
extern void     writememwl_no_mmut_2386(uint32_t addr, uint32_t *a64, uint16_t val);
extern uint32_t readmemll_no_mmut_2386(uint32_t addr, uint32_t *a64);
extern void     writememll_no_mmut_2386(uint32_t addr, uint32_t *a64, uint32_t val);

extern void     do_mmutranslate_2386(uint32_t addr, uint32_t *a64, int num, int write);

/* Physical memory access */
extern uint8_t  mem_readb_phys(uint32_t addr);
extern uint16_t mem_readw_phys(uint32_t addr);
extern uint32_t mem_readl_phys(uint32_t addr);
extern void     mem_writeb_phys(uint32_t addr, uint8_t val);
extern void     mem_writew_phys(uint32_t addr, uint16_t val);
extern void     mem_writel_phys(uint32_t addr, uint32_t val);

/* Page write functions for dynarec */
extern void     mem_write_ramb_page(uint32_t addr, uint8_t val, page_t *page);
extern void     mem_write_ramw_page(uint32_t addr, uint16_t val, page_t *page);
extern void     mem_write_raml_page(uint32_t addr, uint32_t val, page_t *page);
extern void     mem_flush_write_page(uint32_t addr, uint32_t virt);

/* Cache functions */
extern uint8_t *getpccache(uint32_t a);
extern uint64_t mmutranslatereal(uint32_t addr, int rw);
extern uint32_t mmutranslatereal32(uint32_t addr, int rw);
extern uint64_t mmutranslate_noabrt(uint32_t addr, int rw);
extern void     addreadlookup(uint32_t virt, uint32_t phys);
extern void     addwritelookup(uint32_t virt, uint32_t phys);

/* MMU cache flush */
extern void     flushmmucache(void);
extern void     flushmmucache_write(void);
extern void     flushmmucache_pc(void);
extern void     flushmmucache_nopc(void);

extern void     mem_invalidate_range(uint32_t start_addr, uint32_t end_addr);
extern void     mem_reset_page_blocks(void);

extern int      mem_addr_is_ram(uint32_t addr);

/* Memory mapping functions */
extern void mem_mapping_add(mem_mapping_t *,
                            uint32_t base, uint32_t size,
                            uint8_t (*read_b)(uint32_t addr, void *priv),
                            uint16_t (*read_w)(uint32_t addr, void *priv),
                            uint32_t (*read_l)(uint32_t addr, void *priv),
                            void (*write_b)(uint32_t addr, uint8_t val, void *priv),
                            void (*write_w)(uint32_t addr, uint16_t val, void *priv),
                            void (*write_l)(uint32_t addr, uint32_t val, void *priv),
                            uint8_t *exec, uint32_t flags, void *priv);
extern void mem_mapping_disable(mem_mapping_t *);
extern void mem_mapping_enable(mem_mapping_t *);

/* A20 gate */
extern void mem_a20_init(void);
extern void mem_a20_recalc(void);

/* Memory init/reset */
extern void mem_init(void);
extern void mem_close(void);
extern void mem_reset(void);

/* Page eviction list helpers */
static inline int page_in_evict_list(page_t *page)
{
    return (page->evict_prev != EVICT_NOT_IN_LIST);
}
extern void page_remove_from_evict_list(page_t *page);
extern void page_add_to_evict_list(page_t *page);

#endif /* WBOX_MEM_H */
