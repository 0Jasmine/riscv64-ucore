#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   找到合适的更大的块时通常被分割
   Please see Page 196~198, Section 8.2 of Yan Wei Min's chinese book "Data Structure -- C programming language"
*/
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 * 
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * 
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * 
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * 
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */

/* 在第一个适应算法中，分配器保留一个空闲块列表（称为空闲列表），并且，
    收到内存请求后，沿着列表扫描第一个足够大的块
    满足要求。 如果选择的块明显大于请求的块，那么它是
    通常会拆分，并将剩余部分作为另一个空闲块添加到列表中。
    找到合适的更大的块时通常被分割
    请参见严伟民中文著作《数据结构——C程序设计语言》第196~198页，第8.2节
*/
// 你应该重写函数：default_init、default_init_memmap、default_alloc_pages、default_free_pages。
/*
  * FFMA 详细信息
  * (1) 准备：为了实现First-Fit Mem Alloc (FFMA)，我们应该使用一些列表来管理空闲内存块。
  * struct free_area_t 用于管理空闲内存块。 首先你应该
  * 熟悉list.h中的struct list。 struct list 是一个简单的双向链表实现。
  * 你应该知道如何使用：list_init、list_add(list_add_after)、list_add_before、list_del、list_next、list_prev
  * 另一个棘手的方法是将通用列表结构体转换为特殊结构体（例如结构体 page）：
  * 你可以找到一些宏：le2page（在memlayout.h中），（在未来的实验中：le2vma（在vmm.h中），le2proc（在proc.h中）等）
  * 
  * (2) default_init: 您可以重用演示的default_init fun来初始化free_list并将nr_free设置为0。
  * free_list用于记录空闲mem块。 nr_free 是空闲内存块的总数。
  * 
  * (3) default_init_memmap: 调用图: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
  * 该函数用于初始化一个空闲块（带参数：addr_base、page_number）。
  * 首先你应该初始化这个空闲块中的每个页面（在memlayout.h中），包括：
  * p->flags 应该被设置为 PG_property 位（意味着这个页面是有效的。在 pmm_init fun 中（在 pmm.c 中），
  * PG_reserved 位在 p->flags 中设置）
  * 如果该页是空闲的并且不是空闲块的第一页，则 p->property 应设置为 0。
  * 如果此页是空闲的并且是空闲块的第一页，则 p->property 应设置为块的总数量。
  * p->ref 应该为 0，因为现在 p 是空闲的并且没有引用。
  * 我们可以使用 p->page_link 将此页面链接到 free_list，（如：list_add_before(&free_list, &(p->page_link)); ）
  * 最后，我们应该对空闲内存块的数量求和：nr_free+=n
  * 
  * (4) default_alloc_pages: 在空闲列表中搜索找到第一个空闲块（块大小 >=n）并调整空闲块大小，返回地址
  * 分配的块。
  * (4.1) 所以你应该像这样搜索空闲列表：
  * list_entry_t le = &free_list;
  * while((le=list_next(le)) != &free_list) {
  * ....
  * (4.1.1) 在 while 循环中，获取 struct page 并检查 p->property（记录空闲块的数量） >=n？
  * 
  * struct Page *p = le2page(le, page_link);
  * if(p->属性 >= n){ ...
  * (4.1.2) 如果我们找到这个p，那么就意味着我们找到了一个空闲块（块大小>=n），并且前n页可以被分配。
  * 该页的一些标志位需要设置：PG_reserved =1, PG_property =0
  * 取消页面与 free_list 的链接
  * (4.1.2.1) 如果(p->property >n)，我们应该重新计算这个空闲块的剩余部分的数量，
  * (如: le2page(le,page_link))->property = p->property - n;)
  * (4.1.3) 重新计算nr_free（剩余的所有空闲块的数量）
  * (4.1.4) 返回p
  * (4.2) 如果找不到空闲块（块大小 >=n），则返回 NULL
  * 
  * (5) default_free_pages: 将页面重新链接到空闲列表中，可能将小空闲块合并为大空闲块。
  * (5.1) 根据撤回块的基址，查找空闲链表，找到正确的位置
  *（从低地址到高地址），并插入页面。 （可以使用list_next、le2page、list_add_before）
  * (5.2) 重置页面的字段，如p->ref、p->flags(PageProperty)
  * (5.3) 尝试合并低地址或高地址块。 注意：应正确更改某些页面的 p->property。
*/

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    // 初始化可用块列表
    list_init(&free_list);
    nr_free = 0;
}

/// @brief 初始化一个空闲块
/// @param base 指定基址
/// @param n 指定大小
static void
default_init_memmap(struct Page *base, size_t n) {
    
    assert(n > 0);
    // 指向基址的 Page 位置
    struct Page *p = base;
    for (; p != base + n; p ++) {
        // 确认这些页尚未被分配
        assert(PageReserved(p));
        p->flags = p->property = 0;
        // 引用计数清0
        set_page_ref(p, 0);
    }
    // 设置块中页数量， 对标志位适用 原子操作置位
    base->property = n;
    SetPageProperty(base);
    // 增加页数
    nr_free += n;

    // 为空时直接在添加
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        // 不为空时
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            // 将页面链接到空闲链表
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}

/// @brief 从操作系统获取多个页
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        // 存在空闲块有足够大小
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            // 将块的剩余大小取下
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

/// @brief 将多页归还给操作系统
/// @param base 基址
/// @param n 页数
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        // 将块归还
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        // 合并前一部分
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    // 头部不包含空间， 仅作为指针
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
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

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

