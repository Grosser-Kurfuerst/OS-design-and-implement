#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt()
{
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_USER);
  gdt[SEG_TSS] = SEG16(STS_T32A, &tss, sizeof(tss) - 1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0)
{
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));

typedef union free_page
{
  union free_page *next;
  char buf[PGSIZE];
} page_t;

page_t *free_page_list;

void init_page()
{
  extern char end;
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  for (int i = 0; i < PHY_MEM / PT_SIZE; i++)
  {
    kpd.pde[i].val = MAKE_PDE((uint32_t)(&kpt[i]), 3); // Assuming kernel space is accessible in supervisor mode
    for (int j = 0; j < NR_PTE; j++)
    {
      kpt[i].pte[j].val = MAKE_PTE((i << DIR_SHIFT) | (j << TBL_SHIFT), 3);
    }
  }
  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  free_page_list = (void *)KER_MEM;
  page_t *p = free_page_list;
  for (int i = 0; i < (PHY_MEM - KER_MEM) / PGSIZE - 1; i++) {
    page_t *page = p + 1;
    p->next = page;
    p = p->next;
  }
  p->next = NULL;
  // TODO();
}

void *kalloc()
{
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
  // TODO();
  if (free_page_list->next == NULL)
  {
    // TODO: Decide what to do when there is no free memory
    // assert(0); // or return NULL
    return NULL; // For now, returning NULL when there is no free memory
  }

  // Allocate a page from the kernel heap
  page_t *allocated_page = free_page_list;
  void *temp = free_page_list->next;
  free_page_list->next = NULL;
  free_page_list = temp;

  return allocated_page;
}

void kfree(void *ptr)
{
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  // TODO();
  if (ptr == NULL)
  {
    return;
  }

  // Free a page to the kernel heap
  page_t *page = (page_t *)ptr;
  page->next = free_page_list;
  free_page_list = page;
}

PD *vm_alloc()
{
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
  // TODO();
  PD *pd = (PD *)kalloc();
  for (int i = 0; i < NR_PDE; i++)
  {
    if (i < 32)
    {
      pd->pde[i].val = MAKE_PDE((uint32_t)(&kpt[i]), 3);
    }
    else
    {
      pd->pde[i].val = 0;
    }
  }
  return pd;
}

void vm_teardown(PD *pgdir)
{
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  // TODO();
  if (pgdir == NULL)
  {
    return;
  }

  for (int i = 0; i < NR_PDE; i++)
  {
    if (i < 32)
    {
      // Skip the first 32 PDEs (corresponding to kpt)
      continue;
    }

    PDE *pde = &(pgdir->pde[i]);

    if (pde->present)
    {
      // If PDE is present, free the corresponding page table and its mapped pages
      PT *pt = PDE2PT(*pde);

      for (int j = 0; j < NR_PTE; j++)
      {
        PTE *pte = &(pt->pte[j]);

        if (pte->present)
        {
          // If PTE is present, free the mapped page
          void *page = PTE2PG(*pte);
          kfree(page);
        }
      }

      // Free the page table
      kfree(pt);
    }
  }

  // Free the page directory
  kfree(pgdir);
}

PD *vm_curr()
{
  return (PD *)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot)
{
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  assert((prot & ~7) == 0);
  // TODO();

  if (pgdir == NULL) {
    return NULL;
  }

  int pd_index = ADDR2DIR(va);
  PDE *pde = &(pgdir->pde[pd_index]);

  if (!pde->present) {
    // PDE not present
    if (prot == 0) {
      // If prot is 0, return NULL
      return NULL;
    }

    // Allocate a new page table
    PT *new_pt = (PT *)kalloc();
    if (new_pt == NULL) {
      // Allocation failed
      return NULL;
    }

    // Clear the page table
    memset(new_pt, 0, PGSIZE);

    // Set up the new PDE
    pde->val = MAKE_PDE((uint32_t)new_pt, prot | PTE_P);

    // Return the PTE pointer (not setting up the PTE)
    return &(new_pt->pte[ADDR2TBL(va)]);
  }
  pde->val |= prot;

  // PDE present, return the PTE pointer
  PT *pt = PDE2PT(*pde);
  return &(pt->pte[ADDR2TBL(va)]);
}

void *vm_walk(PD *pgdir, size_t va, int prot)
{
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  PTE* pte = vm_walkpte(pgdir, va, prot);
  void *page = PTE2PG(*pte); // 根据PTE找物理页的地址
  void *pa = (void*)((uint32_t)page | ADDR2OFF(va)); // 补上页内偏移量
  return pa;
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot)
{
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
  // TODO();

  for (size_t current_va = start; current_va < end; current_va += PGSIZE) {
    PTE *pte = vm_walkpte(pgdir, current_va, prot);

    if (pte != NULL) {
      PT* pt=kalloc();
      // Set up the PTE
      pte->val = MAKE_PTE(pt, prot | PTE_P);
    }
    // If pte is NULL, it means the page table allocation failed or prot is 0, and we can ignore it.
  }
}

void vm_unmap(PD *pgdir, size_t va, size_t len)
{
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  // assert(ADDR2OFF(va) == 0);
  // assert(ADDR2OFF(len) == 0);
  // TODO();
}

void vm_copycurr(PD *pgdir)
{
  // Lab2-2: copy memory mapped in curr pd to pgdir
  // TODO();
  for (size_t i = PHY_MEM; i < USR_MEM; i += PGSIZE) {
    PTE *pte = vm_walkpte(proc_curr()->pgdir, i, 0);
    if (pte != NULL && pte->present) {
      vm_map(pgdir, i, PGSIZE,7);
      void* pa = vm_walk(pgdir, i, 0);
      memcpy(pa, (void*)(i), PGSIZE);
    }
  }
}

void vm_pgfault(size_t va, int errcode)
{
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
