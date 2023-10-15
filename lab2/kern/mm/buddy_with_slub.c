#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <buddy_with_slub.h>
#include <stdio.h>


#define IS_POWER_OF_2(x) (!((x) & ((x)-1)))

typedef struct mem_object{
    uintptr_t pointer;
    int bytes_num;
    list_entry_t object_link; // 连接object
} mem_object_t;

/// @brief slab 定义
typedef struct mem_slab {
    struct Page* allocate_pages; // 对应分配的地址
    unsigned int byte_free;      // 对应可用字节数
    unsigned int num;      // 对应 object 数量
    mem_object_t object_list;  
} mem_slab_t;

/// @brief slab 在 mem_cache 中的链表表示
typedef struct mem_slab_list {
    mem_slab_t slab;
    list_entry_t slab_ref;    // 连接 slab
} mem_slab_list_t;

/// @brief 为每种范围大小的对象创建 cache
typedef struct mem_cache {
    unsigned int num;         // 对应 slab 总数量
    mem_slab_list_t fullslab; // slab 管理链表， 表示已满
    mem_slab_list_t usedslab; // slab 管理链表， 表示已用
    mem_slab_list_t freeslab; // slab 管理链表， 表示全空
} mem_cache_t;

mem_cache_t cache_area[MAX_ORDER];

static void
buddy_with_slab_init(void)
{
    buddy_system_pmm_manager.init();

    for(int i=0;i<MAX_ORDER;i++)
    {
        list_init(&(cache_area[i].freeslab.slab_ref));
        list_init(&(cache_area[i].usedslab.slab_ref));
        list_init(&(cache_area[i].fullslab.slab_ref));
    }
}

/// @brief 这里还根本不需要用， 所以先不创建 slab
/// @param base 基址
/// @param n 页数
static void
buddy_with_slab_init_memmap(struct Page *base, size_t n)
{
    buddy_system_pmm_manager.init_memmap(base,n);
}


static struct Page*
buddy_with_slab_alloc_bytes(size_t n)
{
    assert(n > 0);
    // 先对已有的 cache 进行查询
    unsigned int page_num = n & 0x3ff ? (n>>10)+1 : (n>>10);
    if(!IS_POWER_OF_2(page_num))
    {
        // fixsize(n);
    }
    return NULL;
}

static void
buddy_with_slab_free_bytes(struct Page *base, size_t n)
{
    
}

static size_t
buddy_with_slab_nr_free_pages(void)
{
    return buddy_system_pmm_manager.nr_free_pages();
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
    .alloc_pages = buddy_with_slab_alloc_bytes,
    .free_pages = buddy_with_slab_free_bytes,
    .nr_free_pages = buddy_with_slab_nr_free_pages,
    .check = buddy_with_slab_system_check,
};
