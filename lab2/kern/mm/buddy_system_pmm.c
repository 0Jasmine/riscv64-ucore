#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_system_pmm.h>
#include <stdio.h>

#define IS_POWER_OF_2(x) (!((x) & ((x)-1)))
static void __dump_list();

free_area_t free_area[MAX_ORDER];
unsigned nr_free = 0;

static unsigned fixsize(unsigned size)
{
    // 不断清除右边的 0
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
}

static uint8_t getlog2(unsigned number)
{
    uint8_t res = 0;
    while (number > 0)
    {
        number >>= 1;
        res += 1;
    }
    return res;
}

/// @brief 将组合页进行分裂，以获得所需大小的页, 这里需要和前面对应的地方是，
/// 我已经将 最大的页 从free_area[high_order].free_list中删除了， 所以这里不用重复
/// 记录一下怕忘，，，感觉还需要写注释就说明代码不够规范，大输特输 55555
/// @param page
/// @param low_order
/// @param high_order
static void expand(struct Page *page,
                   unsigned long low_order, unsigned long high_order)
{
    
    cprintf("\nafter expand : %d, %d  \n", low_order, high_order);
    free_area_t *area = &free_area[high_order];
    unsigned long size = (1U << high_order);
    while (high_order > low_order)
    {
        high_order--;
        area = &free_area[high_order];
        size >>= 1;
        // 折半
        struct Page *split_page = &page[size];
        cprintf("\n page = %lx, size = %ld, split = %lx\n", &page->page_link, size, &split_page->page_link);
        page->property = size;

        split_page->property = size;
        SetPageProperty(split_page);
        page->flags = 0;

        area->nr_free++;

        // 按照地址顺序插入
        if (list_empty(&(area->free_list)))
        {
            list_add(&(area->free_list), &(split_page->page_link));
        }
        else
        {
            list_entry_t *le = &(area->free_list);
            while ((le = list_next(le)) != &(area->free_list))
            {
                struct Page *page = le2page(le, page_link);

                if (split_page < page)
                {
                    list_add_before(le, &(split_page->page_link));
                    break;
                }
                else if (list_next(le) == &(area->free_list))
                {
                    list_add(le, &(split_page->page_link));
                    break;
                }
            }
        }
    }
    __dump_list();
}

static void
buddy_system_init(void)
{
    // 在这里初始化内存管理的链表
    // 根据可以分配的 MAX_ORDER
    for (int i = 0; i < MAX_ORDER; i++)
    {
        list_init(&(free_area[i].free_list));
        free_area[i].nr_free = 0;
    }
}

/// @brief 初始化整体可用内存， 这部分其实就是增加
// 由操作系统管理的内存块, 与 简单版本不同， 这里的 n 可能过大
// 那么就采用 分割的方式 分配到多个 order 层
/// @param base 基址
/// @param n 页数
static void
buddy_system_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);
    // 修正大小, 这里注意是往小修正
    // 且还可能超出分配的范围， 故需要调整
    // 第 0 层对应 1， 第二层对应 2 , 同理 4,8,...
    // 这里 为 1024
    unsigned max_n = 1 << (MAX_ORDER - 1);
    struct Page *p = NULL;
    // 将大于 1 << (MAX_ORDER - 1) 全部添加到 MAX_ORDER 层
    while (n > max_n)
    {
        n -= max_n;
        for (p = base; p != base + max_n; p++)
        {
            assert(PageReserved(p));
            // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
            p->flags = p->property = 0;
            // 引用计数清0
            set_page_ref(p, 0);
        }
        base->property = max_n;
        SetPageProperty(base);

        nr_free += max_n;
        free_area[MAX_ORDER - 1].nr_free++;
        list_entry_t *free_list = &(free_area[MAX_ORDER - 1].free_list);
        if (list_empty(free_list))
        {
            list_add(free_list, &(base->page_link));
        }
        else
        {
            list_entry_t *le = free_list;
            while ((le = list_next(le)) != free_list)
            {
                struct Page *page = le2page(le, page_link);
                if (base < page)
                {
                    list_add_before(le, &(base->page_link));
                    break;
                }
                else if (list_next(le) == free_list)
                {
                    list_add(le, &(base->page_link));
                    break;
                }
            }
        }
        base = p;
    }
    // 剩下的逐一添加
    while (n > 0)
    {
        // 此时要取其最高位对应的 order, 所以需要 -1
        uint8_t fix_order = getlog2(n) - 1;
        max_n = 1U << fix_order;
        n -= max_n;
        for (p = base; p != base + max_n; p++)
        {
            assert(PageReserved(p));
            // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
            p->flags = p->property = 0;
            // 引用计数清0
            set_page_ref(p, 0);
        }
        base->flags = 0;
        base->property = max_n;

        SetPageProperty(base);

        nr_free += max_n;
        free_area[fix_order].nr_free++;

        list_entry_t *free_list = &(free_area[fix_order].free_list);
        if (list_empty(free_list))
        {
            list_add(free_list, &(base->page_link));
        }
        else
        {
            list_entry_t *le = free_list;
            while ((le = list_next(le)) != free_list)
            {
                struct Page *page = le2page(le, page_link);
                if (base < page)
                {
                    list_add_before(le, &(base->page_link));
                    break;
                }
                else if (list_next(le) == free_list)
                {
                    list_add(le, &(base->page_link));
                    break;
                }
            }
        }
        base = p;
    }
}

/// @brief 确认 order 后从 buddy system 中获取， 若该 order 找不到则向上分割
/// 分割的同时， 多余的部分要添加到后面的链表中
static struct Page *
buddy_system_alloc_pages(size_t n)
{
    assert(n > 0);
    // 全局可分配空间小于时 return NULL
    if (nr_free < n)
    {
        return NULL;
    }

    // 如果不是2的次幂， 则修正大小
    if (!IS_POWER_OF_2(n))
    {
        n = fixsize(n);
    }

    struct Page *page = NULL;
    free_area_t *area = NULL;

    uint8_t current_order = 0;
    // 2的次幂对应的 order 都需要 -1
    uint8_t order = getlog2(n) - 1;

    for (current_order = order;
         current_order < MAX_ORDER; current_order++)
    {
        area = &free_area[current_order];
        if (list_empty(&(area->free_list)))
        {
            continue;
        }
        // 删除最近的符合的块
        page = le2page(list_next(&(area->free_list)), page_link);
        list_del(&(page->page_link));
        ClearPageProperty(page);

        nr_free -= n;
        area->nr_free--;

        // 多页尝试向下扩展
        if (current_order > 0 && current_order != order)
        {
            expand(page, order, current_order);
        }
        return page;
    }
    // 不存在可分配内存
    return NULL;
}

static void
buddy_system_free_pages(struct Page *base, size_t n)
{
    assert(base->property > 0);

   cprintf("after %s %d p = %lx res = %d, pro = %d, pro = %ld\n", __FILE__, __LINE__, &base->page_link, PageReserved(base), PageProperty(base),base->property);

    // 这里设计简单一点直接回收整个块， 小于块的操作留给 slab
    // NOTICE 这里的设计需要和 slab 对应
    n = base->property;

    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }

    SetPageProperty(base);
    uint8_t order = getlog2(n) - 1;
    nr_free += n;

    free_area_t *area = &(free_area[order]);
    area->nr_free++;

    list_entry_t *free_list = &(area->free_list);
    if (list_empty(free_list))
    {
        list_add(free_list, &(base->page_link));
    }
    else
    {
        list_entry_t *le = free_list;
        while ((le = list_next(le)) != free_list)
        {
            struct Page *page = le2page(le, page_link);
            if (base < page)
            {
                list_add_before(le, &(base->page_link));
                break;
            }
            else if (list_next(le) == free_list)
            {
                list_add(le, &(base->page_link));
                break;
            }
        }
    }

    // 合并
    while (order < MAX_ORDER - 1)
    {
        // 设置为当前层的 head
        free_list = &(free_area[order].free_list);

        list_entry_t *le = list_prev(&(base->page_link));
        order++;
if (le != free_list && le > &base->page_link)
        {
            p = le2page(le, page_link);
            if (p - p->property == base)
            {
                base->property <<= 1;
                p->property = 0;
                ClearPageProperty(p);
                list_del(&(base->page_link));
                list_del(&(p->page_link));

                area->nr_free -= 2;
                // 插入合并块, 此时free_list为下一层的 head
                area = &(free_area[order]);
                area->nr_free += 1;
                free_list = &(area->free_list);

                if (list_empty(free_list))
                {
                    list_add(free_list, &(base->page_link));
                }
                else
                {
                    le = free_list;
                    while ((le = list_next(le)) != free_list)
                    {
                        struct Page *page = le2page(le, page_link);
                        if (base < page)
                        {
                            list_add_before(le, &(base->page_link));
                            break;
                        }
                        else if (list_next(le) == free_list)
                        {
                            list_add(le, &(base->page_link));
                            break;
                        }
                    }
                }
                // 合并成功进入下一层
                continue;
            }
        }
        else if (le != free_list && le < &base->page_link)
        {
            p = le2page(le, page_link);
            if (p + p->property == base)
            {
                
                p->property <<= 1;
                base->property = 0;
                ClearPageProperty(base);
                list_del(&(base->page_link));
                list_del(&(p->page_link));
                base = p;

                area->nr_free -= 2;
                // 插入合并块, 此时free_list为下一层的 head
                area = &(free_area[order]);
                area->nr_free += 1;
                free_list = &(area->free_list);

                if (list_empty(free_list))
                {
                    list_add(free_list, &(base->page_link));
                }
                else
                {
                    le = free_list;
                    while ((le = list_next(le)) != free_list)
                    {
                        struct Page *page = le2page(le, page_link);
                        if (base < page)
                        {
                            list_add_before(le, &(base->page_link));
                            break;
                        }
                        else if (list_next(le) == free_list)
                        {
                            list_add(le, &(base->page_link));
                            break;
                        }
                    }
                }
                // 合并成功进入下一层
                continue;
            }
        }

        // 如果当前块没有合并则退出
        break;
    }
}

static size_t
buddy_system_nr_free_pages(void)
{
    return nr_free;
}

static void
basic_check(void)
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

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);
    free_page(p0);
    free_page(p1);
    free_page(p2);
    __dump_list();
    assert(nr_free == 3);
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p2);
    free_page(p1);
    free_page(p0);

    cprintf("now nr_free is: %d", nr_free);

    nr_free += nr_free_store;
    nr_free_store = nr_free;

    assert((p0 = alloc_pages(45)) != NULL);
    assert((p1 = alloc_pages(31)) != NULL);
    assert((p2 = alloc_pages(125)) != NULL);

    assert(nr_free == nr_free_store - 64 - 32 - 128);

    free_pages(p0, 45);
    free_pages(p1, 31);
    free_pages(p2, 125);

    assert(nr_free == nr_free_store);
}

static void
buddy_system_check(void)
{
    basic_check();
}

/// @brief 测试函数， 调试的过程用来打印 free_area 的
static void
__dump_list()
{
    for (uint8_t i = 0; i < MAX_ORDER; i++)
    {
        cprintf("order %d : %d ----- ", i, free_area[i].nr_free);
        list_entry_t *free_list = &(free_area[i].free_list);
        list_entry_t *le = free_list;
        while ((le = list_next(le)) != free_list)
        {
            cprintf("%lx --> ", le);
        }
        cprintf("\n");
    }
}

const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_pmm_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = buddy_system_check,
};
