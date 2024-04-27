#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];

void init_proc() {
  // Lab2-1, set status and pgdir
  curr->status = RUNNING;
  curr->pgdir = vm_curr();
  pcb[0].kstack = (void*)(KER_MEM - PGSIZE);
  // Lab2-4, init zombie_sem
  sem_init(&pcb[0].zombie_sem, 0);
  // Lab3-2, set cwd
}

proc_t *proc_alloc() {
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  // TODO();
  proc_t *free_pcb = NULL;
  for (int i = 1; i < PROC_NUM; i++) {
    if (pcb[i].status == UNUSED) {
      free_pcb = &pcb[i];
      break;
    }
  }
  if (free_pcb == NULL) {
    return NULL;
  }
  free_pcb->pid = next_pid;
  next_pid++;
  free_pcb->status = UNINIT;
  free_pcb->pgdir = vm_alloc();
  free_pcb->kstack = kalloc();
  free_pcb->brk = 0;
  free_pcb->ctx = &(free_pcb->kstack->ctx);
  free_pcb->child_num = 0;
  free_pcb->parent = NULL;
  sem_init(&free_pcb->zombie_sem, 0);

  for (int i = 0; i < MAX_USEM; i++) {
    free_pcb->usems[i] = NULL;
  }

  for (int i = 0; i < MAX_UFILE; i++) {
    free_pcb->files[i] = NULL;
  }
  return free_pcb;
  // init ALL attributes of the pcb
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
  // TODO();
  if (proc->status == RUNNING) {
    return;
  }
  proc->status = UNUSED;
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  curr->status = READY;
  INT(0x81);
}

void proc_copycurr(proc_t *proc) {
  // Lab2-2: copy curr proc
  // TODO();

  vm_copycurr(proc->pgdir);
  proc->brk = curr->brk;
  *(proc->ctx) = curr->kstack->ctx;
  proc->ctx->eax = 0;
  proc->parent = curr;
  curr->child_num++;
  // Lab2-5: dup opened usems
  for (int i = 0; i < MAX_USEM; i++) {
    if (curr->usems[i] == NULL) {
      proc->usems[i] = NULL;
    } else {
      proc->usems[i] = usem_dup(curr->usems[i]);
    }
  }
  // Lab3-1: dup opened files
  for (int i = 0; i < MAX_UFILE; i++) {
    if (curr->files[i] == NULL) {
      proc->files[i] = NULL;
    } else {
      proc->files[i] = fdup(curr->files[i]);
    }
  }
  
  // Lab3-2: dup cwd
}

void proc_makezombie(proc_t *proc, int exitcode) {
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  
  //TODO();
  proc->status = ZOMBIE;
  proc->exit_code = exitcode;

  for (int i = 0; i < PROC_NUM; i++) {
    if (pcb[i].parent == proc) {
      pcb[i].parent = NULL;
    }
  }

  if(proc->parent != NULL) {
    sem_v(&proc->parent->zombie_sem);
  }
  
  // Lab2-5: close opened usem
  for (int i = 0; i < MAX_USEM; i++) {
    if (proc->usems[i] != NULL) {
      usem_close(proc->usems[i]);
    }
  }
  // Lab3-1: close opened files
  for(int i = 0; i < MAX_UFILE; i++)
  {
    if(proc->files[i] != NULL)
      fclose(proc->files[i]);
  }
  // Lab3-2: close cwd

}

proc_t *proc_findzombie(proc_t *proc) {
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
  // TODO();
  for (int i = 0; i < PROC_NUM; i++) {
    if (pcb[i].parent == proc && pcb[i].status == ZOMBIE) {
      return &pcb[i];
    }
  }
  return NULL;

}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
  // TODO();
  for (int i = 0; i < MAX_USEM; i++) {
    if (proc->usems[i] == NULL) {
      return i;
    }
  }
  return -1;
}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  // TODO();
  if (sem_id >= MAX_USEM || sem_id < 0) {
    return NULL;
  }
  return proc->usems[sem_id];
}

int proc_allocfile(proc_t *proc) {
  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  // TODO();
  for (int i = 0; i < MAX_UFILE; i++) {
    if (proc->files[i] == NULL) {
      return i;
    }
  }
  return -1;
}

file_t *proc_getfile(proc_t *proc, int fd) {
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  // TODO();
  if (fd >= MAX_UFILE || fd < 0) {
    return NULL;
  }
  return proc->files[fd];
}

void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  //TODO();
  curr->ctx = ctx;
  int temp = curr - pcb;
  for (int i = 1; i <= PROC_NUM; i++) {
    if (pcb[(temp + i) % PROC_NUM].status == READY) {
      proc_run(&pcb[(temp + i) % PROC_NUM]);
    }
  }

}
