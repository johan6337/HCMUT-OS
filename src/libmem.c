/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt) {
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    if (rg_node != NULL)
        rg_elmt->rg_next = rg_node;

    mm->mmap->vm_freerg_list = rg_elmt;
    return 0;
}

struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid) {
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;
    return &mm->symrgtbl[rgid];
}

int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
   struct vm_rg_struct rgnode;
 
   // Better error checking with more descriptive messages
   if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
     printf("-------Process %d alloc error: Invalid region ID %d (max: %d)-------\n", 
            caller->pid, rgid, PAGING_MAX_SYMTBL_SZ);
     return -1;
   }
 
   if (caller->mm->symrgtbl[rgid].rg_start < caller->mm->symrgtbl[rgid].rg_end) {
     printf("-------Process %d alloc error: Region %d already allocated-------\n", 
            caller->pid, rgid);
     return -1;
   }
 
   // Add mutex lock for thread safety
   pthread_mutex_lock(&mmvm_lock);
   
   // Try to find free region first
   int result = 0;
   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
   { 
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end; 
     *alloc_addr = rgnode.rg_start;

     // Define page size as a constant for better readability
     
     // Handle page mapping
     for (int i = rgnode.rg_start / PAGING_PAGESZ; i <= rgnode.rg_end / PAGING_PAGESZ; i++) {
      uint32_t current_pte = caller->mm->pgd[i];
      
      // Skip the last page if the region ends exactly at a page boundary
      if (rgnode.rg_end % PAGING_PAGESZ == 0 && i == rgnode.rg_end / PAGING_PAGESZ && rgnode.rg_end > rgnode.rg_start) {
          break;
      }

      if (!PAGING_PAGE_PRESENT(current_pte)) {
          printf("Process %d: Loading page %d from swap\n", caller->pid, i);
          
          int targetfpn = GETVAL(current_pte, GENMASK(20, 0), 5);
          uint32_t *vicpte = &caller->mm->pgd[find_victim_page(caller->mm, &targetfpn)];
          int vicfpn = GETVAL(*vicpte, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
          int swpfpn;

          if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0) {
              printf("Process %d: Out of swap space\n", caller->pid);
              result = -3000;
              goto cleanup;
          }

          __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
          pte_set_swap(vicpte, 0, swpfpn);
          __swap_cp_page(caller->active_mswp, targetfpn, caller->mram, vicfpn);
          pte_set_fpn(&caller->mm->pgd[i], vicfpn);
          MEMPHY_put_freefp(caller->active_mswp, targetfpn);
      }
     }
   }
   else {
     // Handle heap expansion
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     if (!cur_vma) {
         printf("Process %d: Invalid VMA ID %d\n", caller->pid, vmaid);
         result = -1;
         goto cleanup;
     }
     
     int inc_sz = PAGING_PAGE_ALIGNSZ(size);
     int old_sbrk = cur_vma->sbrk;
 
     if (inc_vma_limit(caller, vmaid, inc_sz) != 0) {
         printf("Process %d: Failed to increase VMA limit\n", caller->pid);
         result = -1;
         goto cleanup;
     }
 
     caller->mm->symrgtbl[rgid].rg_start = old_sbrk; 
     caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size; 
     *alloc_addr = old_sbrk;
 
     // Only create free region if there's actual space left
     if (old_sbrk + size < cur_vma->sbrk) {
         struct vm_rg_struct *rgnode_temp = malloc(sizeof(struct vm_rg_struct));
         if (rgnode_temp) {
             rgnode_temp->rg_start = old_sbrk + size;
             rgnode_temp->rg_end = cur_vma->sbrk;
             enlist_vm_freerg_list(caller->mm, rgnode_temp);
         } else {
             printf("Process %d: Warning - couldn't track free region\n", caller->pid);
             // Continue anyway, this is not a fatal error
         }
     }
   }

cleanup:
   pthread_mutex_unlock(&mmvm_lock);
   return result;
}

int __free(struct pcb_t *caller, int vmaid, int rgid) {
    struct vm_rg_struct *rgnode;

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    rgnode = get_symrg_byid(caller->mm, rgid);
    if (rgnode->rg_start == rgnode->rg_end) {
        printf("Process %d FREE Error: Region wasn't alloc or was freed before\n", caller->pid);
        return -1;
    }

    struct vm_rg_struct *rgnode_temp = malloc(sizeof(struct vm_rg_struct));
    rgnode_temp->rg_start = rgnode->rg_start;
    rgnode_temp->rg_end = rgnode->rg_end;
    rgnode->rg_start = rgnode->rg_end = 0;

    enlist_vm_freerg_list(caller->mm, rgnode_temp);
    return 0;
}

int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index) {
    int addr;
    return __alloc(proc, 0, reg_index, size, &addr);
}

int libfree(struct pcb_t *proc, uint32_t reg_index) {
    return __free(proc, 0, reg_index);
}


int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {
  // Validate input parameters
  if (pgn < 0 || pgn >= PAGING_MAX_PGN || !mm || !fpn || !caller) {
      return -1;
  }
  
  uint32_t pte = mm->pgd[pgn];

  // Page fault handling - page not in physical memory
  if (!PAGING_PAGE_PRESENT(pte)) {
      // Log page fault
      printf("Page fault: Process %d accessing page %d\n", caller->pid, pgn);
      
      int fpn_temp = -1;
      
      // Case 1: We have a free frame available
      if (MEMPHY_get_freefp(caller->mram, &fpn_temp) == 0) {
          // Get frame number from swap
          int tgtfpn = GETVAL(pte, GENMASK(20, 0), 5);
          
          // Copy page from swap to RAM
          if (__swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, fpn_temp) != 0) {
              printf("Error copying page from swap\n");
              MEMPHY_put_freefp(caller->mram, fpn_temp); // Return the frame
              return -1;
          }
          
          // Update page table
          pte_set_fpn(&mm->pgd[pgn], fpn_temp);
          
          // Update FIFO queue for page replacement
          struct pgn_t *new_pg = malloc(sizeof(struct pgn_t));
          if (new_pg) {
              new_pg->pgn = pgn;
              new_pg->pg_next = NULL;
              
              // Add to end of FIFO queue
              if (!mm->fifo_pgn) {
                  mm->fifo_pgn = new_pg;
              } else {
                  struct pgn_t *pg = mm->fifo_pgn;
                  while (pg->pg_next) pg = pg->pg_next;
                  pg->pg_next = new_pg;
              }
          }
      } else {
          // Case 2: Need to swap out a page
          int swpfpn;
          int tgtfpn = GETVAL(pte, GENMASK(20, 0), 5);
          
          // Find victim page to swap out
          int victim_pgn;
          if (find_victim_page(mm, &victim_pgn) < 0) {
              printf("Error finding victim page\n");
              return -1;
          }
          
          uint32_t *vicpte = &mm->pgd[victim_pgn];
          int vicfpn = GETVAL(*vicpte, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

          // Get free frame in swap
          if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0) {
              printf("Error: Swap space full\n");
              return -3000;
          }

          // Execute the swap
          if (__swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn) != 0 ||
              __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn) != 0) {
              printf("Error during page swapping\n");
              MEMPHY_put_freefp(caller->active_mswp, swpfpn);
              return -1;
          }
          
          // Update page tables
          pte_set_swap(vicpte, 0, swpfpn);
          pte_set_fpn(&mm->pgd[pgn], vicfpn);
          MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
          
          // Update FIFO queue
          struct pgn_t *new_pg = malloc(sizeof(struct pgn_t));
          if (new_pg) {
              new_pg->pgn = pgn;
              new_pg->pg_next = NULL;
              
              // Add to end of FIFO queue
              if (!mm->fifo_pgn) {
                  mm->fifo_pgn = new_pg;
              } else {
                  struct pgn_t *pg = mm->fifo_pgn;
                  while (pg->pg_next) pg = pg->pg_next;
                  pg->pg_next = new_pg;
              }
          }
      }
  }
  
  // Get the physical frame number
  *fpn = GETVAL(mm->pgd[pgn], PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
  return 0;
}

int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) {
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
    MEMPHY_write(caller->mram, phyaddr, value);
    return 0;
}
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);
  //struct sc_regs regs;
  //regs.a1 = ...
  //regs.a2 = ...
  //regs.a3 = ...

  /* SYSCALL 17 sys_memmap */
  // syscall(caller, 17, &regs);
  // Update data
  // *data = (BYTE)regs.a3;

  return 0;
}

int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data) {
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL)
        return -1;

    return pg_getval(caller->mm, currg->rg_start + offset, data, caller); // Fix: Call defined function
}

int libread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t *destination) {
    BYTE data;
    int val = __read(proc, 0, source, offset, &data);
    *destination = (uint32_t)data;
#ifdef IODUMP
    printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
    MEMPHY_dump(proc->mram);
#endif
    return val;
}

int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value) {
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL)
        return -1;

    return pg_setval(caller->mm, currg->rg_start + offset, value, caller);
}

int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset) {
#ifdef IODUMP
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
    MEMPHY_dump(proc->mram);
#endif
    return __write(proc, 0, destination, offset, data);
}

int free_pcb_memph(struct pcb_t *caller) {
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++) {
        pte = caller->mm->pgd[pagenum];

        if (!PAGING_PAGE_PRESENT(pte)) {
            fpn = PAGING_PTE_FPN(pte);
            MEMPHY_put_freefp(caller->mram, fpn);
        } else {
            fpn = PAGING_PTE_SWP(pte);
            MEMPHY_put_freefp(caller->active_mswp, fpn);
        }
    }
    return 0;
}

int find_victim_page(struct mm_struct *mm, int *retpgn) {
    struct pgn_t *pg = mm->fifo_pgn;

    // Placeholder: Should implement a proper victim selection mechanism
    if (pg != NULL) {
        *retpgn = pg->pgn;
        mm->fifo_pgn = pg->pg_next;
        free(pg);
        return *retpgn; // Return page number for now
    }
    return 0; // Default case, should be improved
}

int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

    if (rgit == NULL)
        return -1;

    newrg->rg_start = newrg->rg_end = -1;

    while (rgit != NULL) {
        if (rgit->rg_start + size <= rgit->rg_end) {
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            if (rgit->rg_start + size < rgit->rg_end) {
                rgit->rg_start = rgit->rg_start + size;
            } else {
                struct vm_rg_struct *nextrg = rgit->rg_next;
                if (nextrg != NULL) {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;
                    rgit->rg_next = nextrg->rg_next;
                    free(nextrg);
                } else {
                    rgit->rg_start = rgit->rg_end;
                    rgit->rg_next = NULL;
                }
            }
            break;
        }
        rgit = rgit->rg_next;
    }
    if (newrg->rg_start == -1)
        return -1;

    return 0;
}