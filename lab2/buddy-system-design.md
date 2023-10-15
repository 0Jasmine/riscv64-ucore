# buddy system 设计实现

> 源码 [buddy_system_pmm.c](./kern/mm/buddy_system_pmm.c) ， 我可以当懒狗吗🤧， 写源码的时候给了很多 `@brief` 的说明（自认为真的很多）， 这里简单说一下实现思路顺带放了对应的代码。

## intro

buddy system 是一种经典的内存分配算法, 实质就是一种特殊的“分离适配”，即将内存按 `2` 的幂进行划分，相当于分离出若干个块大小一致的空闲链表， 搜索该链表并给出同需求最佳匹配的大小。

两个大小相等且邻接的内存块被称作 `buddy` 。

如果两个伙伴都是空闲的，会将其合并成一个更大的内存块，作为下一层次上某个内存块的伙伴。

page分为两类：

- 属于Buddy系统（PG_buddy，待分配）
  - page->order记录order（page所属的free_area也表示了其order），用于合并时的检测

- 不属于Buddy系统（已分配）
  1. 单页：page->order记录order
  2. 组合页：首个（PG_head）page记录order，其余（PG_tail）指向首页。order用于释放时的解组合

其优点是 **快速搜索合并（O(logN)时间复杂度）** 以及 `低外部碎片（最佳适配best-fit）`；

其缺点是==内部碎片==，因为按2的幂划分块，如果碰上66单位大小，那么必须划分128单位大小的块。但若需求本身就按2的幂分配，就会很有优势。

## design

其实实现比较简单， 一开始设计参考[实验指导书上的文章](http://coolshell.cn/articles/10427.html)， 但是它的结构我觉得不适用于 ucore, 因为如果专门为 `buddy` 写一个结构体， 那么管理的数组显然只能确定分配空间的大小， 但不能确定分配空间的位置， 如果再专门增加一个指向 page 的指针那太冗余了， 所以设计修改了 

```cpp
// 原本的结构
free_area_t free_area;
```

变为新的

```cpp
// 修改的结构
free_area_t free_area[MAX_ORDER];
```

即从不同 order 层取空闲块， 注意以下对其进行基于 `块` 粒度的分割、合并，以及基于 `页` 粒度的标志位修改即可。`"MAX_ORDER"` 的值默认为11，一次分配可以请求的 page frame 数目为1, 2, 4……最大为1024。

> 如果要说很值得注意的地方， 就是把标志位， 比如说记得 PG_reserved, PG_property 置位、 清除什么的。

### 初始化

递归地（当然这里用 `while` 实现）， 根据最大的 order 层可以用到的内存数将 n 分割。

```cpp
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
```

### 增加分配 mmap

> `@brief` 是我的设计思路

```cpp

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
```

### 分配内存

寻找大小合适的内存块（ 大于等于 所需大小 并且 `最接近 2 的幂` ，比如需要27，实际分配32）

1. 如果找到了，分配给应用程序。
2. 如果没找到，分出合适的内存块。
   1. 对半分离出高于所需大小的空闲内存块
   2. 如果分到最低限度，分配这个大小。
   3. 回溯到步骤1（寻找合适大小的块）
   4. 重复该步骤直到一个合适的块

```cpp
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

```

`expand(page, order, current_order)` 方法用于向下扩展不需要分配的空间

```cpp
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
        area--;
        high_order--;
        size >>= 1;
        // 折半
        struct Page *split_page = &page[size];

        page->property = size;

        split_page->property = size;
        SetPageProperty(split_page);
        split_page->flags = 0;

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
    __dump_list(); // 测试用的
}
```

### 释放内存

释放该内存块

1. 寻找相邻的块，看其是否释放了。
2. 如果相邻块也释放了，合并这两个块，重复上述步骤直到遇上未释放的相邻块，或者达到最高上限（即所有内存都释放了）。

代码比较长， 但其实逻辑很简单

```cpp
static void
buddy_system_free_pages(struct Page *base, size_t n)
{
    assert(base->property > 0);

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
    uint8_t order = getlog2(n);
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
        if (le != free_list)
        {
            p = le2page(le, page_link);
            if (p + p->property == base)
            {
                p->property <<= 1;
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

        le = list_next(&(base->page_link));
        if (le != free_list)
        {
            p = le2page(le, page_link);
            if (p + p->property == base)
            {
                base->property <<= 1;
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

        // 如果当前块没有合并则退出
        break;
    }
}
```

## test

设计测试样例：

```cpp
static void
basic_check(void)
{
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);
    __dump_list();
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

    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);

    cprintf("now nr_free is: %d", nr_free);

    nr_free += nr_free_store;
    nr_free_store = nr_free;

    assert((p0 = alloc_pages(45)) != NULL);
    assert((p1 = alloc_pages(31)) != NULL);
    assert((p2 = alloc_pages(125)) != NULL);

  // 这里测试分配多个页， 可以确认是按照2的整数次幂分配的
    assert(nr_free == nr_free_store - 64 - 32 - 128);

    free_pages(p0, 45);
    free_pages(p1, 31);
    free_pages(p2, 125);

    assert(nr_free == nr_free_store);
}
```

因为测试结果比较很长， 为了看得清晰我增加了一个 `__dump_list()` 用来打印设计的 buddy order，输出结果重定向在了[make test输出结果](./test.txt).