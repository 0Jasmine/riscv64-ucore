#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lruk.h>
#include <list.h>

const int lru_k = 2;

list_entry_t pra_list_head;
list_entry_t lru_list_head;

static int
_lruk_init_mm(struct mm_struct *mm)
{     
     list_init(&pra_list_head);
     list_init(&lru_list_head);
     mm->sm_priv = &pra_list_head;
     //cprintf(" mm->sm_priv %x in lruk_init_mm\n",mm->sm_priv);
     return 0;
}

static int
_lruk_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    list_add(head, entry);
    page->visited = 1;
    return 0;
}


static int
_lruk_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
    list_entry_t* entry = list_prev(head);
    if (entry != head) {
        list_del(entry);
        *ptr_page = le2page(entry, pra_page_link);
    } else {
        head = &lru_list_head;
        entry = list_prev(head);
        if (entry != head) {
            list_del(entry);
            *ptr_page = le2page(entry, pra_page_link);
        }
        else
        {
            ptr_page = NULL;
        }
    }
    cprintf("lru_ptr %lx\n", entry);
    return 0;
}

static int
_lruk_check_swap(void) {
    cprintf("write Virt Page c in lruk_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==4);
    cprintf("write Virt Page a in lruk_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==4);
    cprintf("write Virt Page d in lruk_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==4);
    cprintf("write Virt Page b in lruk_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==4);
    cprintf("write Virt Page e in lruk_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    cprintf("write Virt Page b in lruk_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==5);
    cprintf("write Virt Page a in lruk_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==6);
    cprintf("write Virt Page b in lruk_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==7);
    cprintf("write Virt Page c in lruk_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==8);
    cprintf("write Virt Page d in lruk_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==9);
    cprintf("write Virt Page e in lruk_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==10);
    cprintf("write Virt Page a in lruk_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==11);
    return 0;
}


static int
_lruk_init(void)
{
    return 0;
}

static int
_lruk_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lruk_tick_event(struct mm_struct *mm)
{ 
    return 0; 
}

static int
_lruk_access_inc(struct mm_struct *mm, uintptr_t addr)
{
    pte_t* ptep = get_pte(mm->pgdir, addr, 0);
    if(ptep == NULL || *ptep == NULL)
    {
        return -1;
    }

    struct Page * target = pte2page(*ptep);
    list_entry_t * entry = &target->pra_page_link;

    // 大于等于 k 在 lru list
    if(target->visited >= lru_k)
    {
        list_del(entry);
        list_add(&lru_list_head, entry);
    }
    // 被 sm 管理
    else if(target->visited > 0)
    {
        list_del(entry);
        target->visited++;
        if(target->visited>=lru_k)
        {
            list_add(&lru_list_head, entry);
        }
        else{
            list_add(&pra_list_head, entry);
        }
    }
    return 0;
}

struct swap_manager swap_manager_lruk =
{
     .name            = "lruk swap manager",
     .init            = &_lruk_init,
     .init_mm         = &_lruk_init_mm,
     .tick_event      = &_lruk_tick_event,
     .map_swappable   = &_lruk_map_swappable,
     .set_unswappable = &_lruk_set_unswappable,
     .swap_out_victim = &_lruk_swap_out_victim,
     .check_swap      = &_lruk_check_swap,
};
