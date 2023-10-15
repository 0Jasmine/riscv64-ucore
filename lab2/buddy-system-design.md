# buddy system 设计实现

> 源码 [buddy_system_pmm.c](./kern/mm/buddy_system_pmm.c) ， 我可以当懒狗吗🤧， 写源码的时候给了很多 `@brief` 的说明（自认为真的很多）， 这里简单说一下实现思路。

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

### 分配内存

寻找大小合适的内存块（ 大于等于 所需大小 并且 `最接近 2 的幂` ，比如需要27，实际分配32）

1. 如果找到了，分配给应用程序。
2. 如果没找到，分出合适的内存块。
   1. 对半分离出高于所需大小的空闲内存块
   2. 如果分到最低限度，分配这个大小。
   3. 回溯到步骤1（寻找合适大小的块）
   4. 重复该步骤直到一个合适的块

### 释放内存

释放该内存块

1. 寻找相邻的块，看其是否释放了。
2. 如果相邻块也释放了，合并这两个块，重复上述步骤直到遇上未释放的相邻块，或者达到最高上限（即所有内存都释放了）。

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

    assert(nr_free == nr_free_store - 64 - 32 - 128);

    free_pages(p0, 45);
    free_pages(p1, 31);
    free_pages(p2, 125);

    assert(nr_free == nr_free_store);
}
```

因为测试结果比较长， 为了看得清晰我增加了一个 `__dump_list()` 用来打印设计的 buddy order，输出结果重定向在了[make test输出结果](./test.txt).