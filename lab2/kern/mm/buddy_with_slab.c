#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <stdio.h>

/// @brief slab 定义
typedef struct mem_slab {
    struct Page* allocate_pages;
    unsigned int size;      // 对应总字节数
    unsigned int num;      // 对应 object 数量
    
} mem_slab_t;

/// @brief slab 在 mem_cache 中的链表表示
typedef struct mem_slab_list {
    mem_slab_t slab;
    list_entry_t slab_ref;    // 连接 slab
} mem_slab_list_t;

/// @brief 为每种范围大小的对象创建 cache
typedef struct mem_cache {
    uint8_t slab_order;    // 对应每个 slab 的 order
    unsigned int num;         // 对应 slab 总数量
    mem_slab_list_t fullslab; // slab 管理链表， 表示已满
    mem_slab_list_t usedslab; // slab 管理链表， 表示已用
    mem_slab_list_t freeslab; // slab 管理链表， 表示全空
    list_entry_t cache_ref;    // 连接所有 cache
} mem_cache_t;



static void
buddy_with_slab_init(void)
{

}

/// @brief 初始化整体可用内存， 这部分其实就是增加
// 由操作系统管理的内存块, 与 简单版本不同， 这里的 n 可能过大
// 那么就采用 分割的方式 分配到多个 order 层
/// @param base 基址
/// @param n 页数
static void
buddy_with_slab_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);

}


static struct Page *
buddy_with_slab_alloc_pages(size_t n)
{
    assert(n > 0);

}

static void
buddy_with_slab_free_pages(struct Page *base, size_t n)
{
    
}

static size_t
buddy_with_slab_nr_free_pages(void)
{
    return buddy_system_nr_free_pages();
}

static void
slab_basic_check(void)
{
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);


    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);


    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    
}

static void
buddy_with_slab_system_check(void)
{
    slab_basic_check();
}


const struct pmm_manager buddy_with_slab_pmm_manager = {
    .name = "buddy_with_slab_pmm_manager",
    .init = buddy_with_slab_init,
    .init_memmap = buddy_with_slab_init_memmap,

    .alloc_pages = buddy_with_slab_alloc_pages,
    .free_pages = buddy_with_slab_free_pages,
    .nr_free_pages = buddy_with_slab_nr_free_pages,
    .check = buddy_with_slab_system_check,
};
