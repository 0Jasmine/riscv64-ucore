#ifndef __KERN_MM_BUDDY_SYSTEM_PMM_H__
#define  __KERN_MM_BUDDY_SYSTEM_PMM_H__

#define MAX_ORDER 11

#include <pmm.h>


static size_t buddy_system_nr_free_pages(void);
static unsigned fixsize(unsigned size);
static void buddy_with_slab_free_pages(struct Page *base, size_t n);
static uint8_t getlog2(unsigned number);
static void buddy_system_init(void);
static void buddy_system_init_memmap(struct Page *base, size_t n)
static struct Page * buddy_system_alloc_pages(size_t n);
static void buddy_system_free_pages(struct Page *base, size_t n);

extern const struct pmm_manager buddy_system_pmm_manager;

#endif /* ! __KERN_MM_BUDDY_SYSTEM_PMM_H__ */