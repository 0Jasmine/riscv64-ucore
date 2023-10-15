# lab2 实验报告

## 练习1：理解first-fit 连续物理内存分配算法

> 源码在(default_pmm.c)[./kern/mm/default_pmm.c]， 考虑报告篇幅， 除必要解释外不再插入整段代码

### default_init

函数用于初始化 free_list 并将 nr_free 设置为0。其中 free_list 用于记录空闲 mem 块。 nr_free 是空闲内存块中 page 的总数。

### default_init_memmap

调用图如：

```mermaid
graph LR
A[kern_init] --> B[pmm_init]
B --> C[page_init]
C --> D[init_memmap]
D --> E[pmm_manager]
E --> F[init_memmap]
```

该函数用于初始化一个空闲块（带参数： `addr_base` 、 `page_number` ）。初始化时需要以 Page 的粒度初始化这个空闲块， flags 应该被设置为 `PG_property` 位（意味着这个页面是有效的）。

- 如果该页是空闲的并且不是空闲块的第一页，则 p->property 应设置为 0。
- 如果此页是空闲的并且是空闲块的第一页，则 p->property 应设置为块的总数量。

p->ref 应该为 0，因为现在 p 是空闲的并且没有引用。使用 p->page_link 将此页面链接到 free_list。 最终对空闲内存块的数量求和：`nr_free+=n`。

### default_alloc_pages

函数用于 在空闲列表中搜索找到第一个空闲块（块大小 >=n）并调整空闲块大小，返回地址即分配的块。实现方法在 `while` 循环中，获取 `struct page` 并检查 `p->property`（记录空闲块的数量） >=n , 如果我们找到这个p，那么就意味着我们找到了一个空闲块（块大小>=n），并且前n页可以被分配。此时该页的一些标志位需要设置：PG_reserved =1, PG_property =0, 取消页面与 free_list 的链接， 且如果 `p->property >n`，我们应该重新计算这个空闲块的剩余部分的数量; 如果找不到空闲块（块大小 >=n），则返回 NULL。

### default_free_pages

将页面重新链接到空闲列表中，可能将小空闲块合并为大空闲块。根据撤回块的基址，查找空闲链表，找到正确的位置, 因为它插入 `free_list` 是按照地址顺序排序的， 这样就只需要查看头尾是否连续就可以确定是否需要合并。

### 改进空间

我认为改进的空间有：
  * 分配内存的时候按照页数来分，排序的时候按照地址来排序，导致每一次分配需要遍历链表，这样的缺点很明显：
    1. 时间复杂度高
    2. 容易造成外部碎片
  * 再有它 nr_free 这个操作记录的是整条链表上剩余的 page 数目， 但是在分配时显然都是针对于整块， 就需要连续内存， 这样可能需要块的移动， 但内存的移动消耗是很大的， 朴素地从 `如何减少移动` 思考， 其实或许是 `如何尽量高效地分配相邻空间` 的问题。

## 练习2：实现 Best-Fit 连续物理内存分配算法

`best-fit` 实现思路很简单， 代码与 `default_pmm` 相差也不大， 一点点区别在于 `best-fit` 找到一个可以用的块后不立即进行分割， 而是找到一个所需要大小最接近的块后再进行分割。

测试结果：

![passed](./image/2-make-grade.png)

### 改进空间

与默认的内存管理类似，best-fit 我认为对前面我提出的问题没有多大改进，稍微优秀一点可能是一些情况下外部碎片更小，但同样这也是扫描整个链表换来的， 所以我认为改进的空间有：
  * 分配内存的时候按照页数来分，排序的时候按照地址来排序，导致每一次分配需要遍历链表，这样的缺点很明显：
    1. 时间复杂度高
    2. 容易造成外部碎片
  * 更深入地从 `如何减少移动` 思考， 其实或许是 `如何尽量高效地分配相邻空间` 的问题。

不过， 这段代码真的没问题吗？

```cpp
        list_entry_t *le = &(free_area[high_order].free_list);
        while ((le = list_next(le)) != &(free_area[high_order].free_list))
        {
            struct Page *page = le2page(le, page_link);

            if (split_page < page)
            {
                list_add_before(le, &(split_page->page_link));
                break;
            }
            else if (list_next(le) == &(free_area[high_order].free_list))
            {
                list_add(le, &(split_page->page_link));
                // here
                // 意思是感觉这个会不停在末尾 add
                // 所以我源码里增加了 break
            }
        }
```

## challenge1

[design report](./buddy-system-design.md)
[src code](./kern/mm/buddy_system_pmm.c)

## challenge2

[design report](./slub-design.md)
[src code](./kern/mm/buddy_with_slub.c)

## challenge3

我觉得方法可以使用汇编语言来测试， 编写汇编语言程序：使用汇编语言编写一个简单的程序，用于测试物理内存的不同范围。具体得，定义了一个内存起始地址`memory_start`和一个内存结束地址`memory_end`，用于指定要测试的内存范围。然后，程序使用循环结构来遍历每个内存地址，将地址加载到寄存器`eax`中，并尝试修改这些内容，以测试该地址是否可用。如果该地址是可用的，程序将在该地址处写入一个值，然后继续测试下一个地址。如果所有地址都测试完毕，程序结束并进入暂停状态。由于程序直接访问物理内存地址，确保计算机处于实模式或保护模式。这个方法我觉得有点不好控制的是， 一般不是会将外设模拟成内存， 那如果写的区域有问题会不会影响到硬件导致故障🤔， 对此我觉得是否可能通过设置 trap 处理， 使用异或操作去写入，第一次异或测试是否写入成功，第二次异或还原，在发生 trap 的时候，处理函数先异或还原， 然后确定不能写的范围后退出完成测试。