#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  file_t* file = proc_getfile(proc_curr(), fd);
  if(file == NULL) {
    return -1;
  }
  //assert(0);
  return fwrite(file, buf, count);
}

int sys_read(int fd, void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  file_t* file = proc_getfile(proc_curr(), fd);
  if(file == NULL) {
    return -1;
  }
  //assert(0);
  return fread(file, buf, count);
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
  size_t brk = proc_curr()->brk; // use brk of proc instead of this in Lab2-1
  size_t new_brk = PAGE_UP(addr);
  if (brk == 0) {
    proc_curr()->brk = new_brk;
  } else if (new_brk > brk) {
    PD *cur = vm_curr();
    vm_map(cur, brk, new_brk-brk, 7);
    proc_curr()->brk = new_brk;
  } else if (new_brk < brk) {
    // can just do nothing
  }
  return 0;
}

void sys_sleep(int ticks) {
  // TODO(); // Lab1-7
  uint32_t now = get_tick();
  while (get_tick() < now + ticks) {
    // sti(); 
    // hlt(); 
    // cli();
    proc_yield();
  }
  
}

int sys_exec(const char *path, char *const argv[]) {
  //TODO(); // Lab1-8, Lab2-1
  PD *pd = vm_alloc();
  Context ctx;
  int ret = load_user(pd, &ctx, path, argv);
  if (ret != 0) {
    kfree(pd);
    return -1;
  }
  PD *now_pd = vm_curr();
  set_cr3(pd);
  proc_curr()->pgdir=pd;
  kfree(now_pd);
  irq_iret(&ctx);
  return 0;
}

int sys_getpid() {
  return proc_curr()->pid; // Lab2-1
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
  //TODO(); // Lab2-2
  proc_t *pcb = proc_alloc();
  if (pcb == NULL) {
    return -1;
  }
  proc_copycurr(pcb);
  proc_addready(pcb);
  return pcb->pid;
}

void sys_exit(int status) {
  //TODO(); // Lab2-3
  proc_makezombie(proc_curr(), status);
  INT(0x81);
  assert(0);
}

int sys_wait(int *status) {
  // TODO(); // Lab2-3, Lab2-4
  if(!proc_curr()->child_num){
    return -1;
  }

  sem_p(&proc_curr()->zombie_sem);
  proc_t* zombie = proc_findzombie(proc_curr());

  if(status != NULL){
    *status = zombie->exit_code;
  }
  int pid = zombie->pid;
  proc_free(zombie);
  proc_curr()->child_num--;
  return pid;

}

int sys_sem_open(int value) {
  // TODO(); // Lab2-5
  int index = proc_allocusem(proc_curr());
  if (index == -1) {
    return -1;
  }
  usem_t *sem = usem_alloc(value);
  if (sem == NULL) {
    return -1;
  }
  proc_curr()->usems[index] = sem;
  return index;
  
}

int sys_sem_p(int sem_id) {
  // TODO(); // Lab2-5
  usem_t *sem = proc_getusem(proc_curr(), sem_id);
  if (sem == NULL) {
    return -1;
  }
  sem_p(&sem->sem);
  return 0;
}

int sys_sem_v(int sem_id) {
  // TODO(); // Lab2-5
  usem_t *sem = proc_getusem(proc_curr(), sem_id);
  if (sem == NULL) {
    return -1;
  }
  sem_v(&sem->sem);
  return 0;
}

int sys_sem_close(int sem_id) {
  // TODO(); // Lab2-5
  usem_t* sem = proc_getusem(proc_curr(),sem_id);
  if (sem == NULL) {
    return -1;
  }
  usem_close(sem);
  proc_curr()->usems[sem_id] = NULL;
  return 0;
}

int sys_open(const char *path, int mode) {
  // TODO(); // Lab3-1
  int fd = proc_allocfile(proc_curr());
  if(fd == -1){
    return -1;
  }

  file_t* file = fopen(path, mode);
  if(file == NULL){
    return -1;
  }

  proc_curr()->files[fd] = file;
  return fd;
}

int sys_close(int fd) {
  // TODO(); // Lab3-1
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }

  fclose(file);
  proc_curr()->files[fd] = NULL;
  return 0;
}

int sys_dup(int fd) {
  // TODO(); // Lab3-1
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }

  int new_fd = proc_allocfile(proc_curr());
  if(new_fd == -1){
    return -1;
  }

  proc_curr()->files[new_fd] = fdup(file);
  return new_fd;
}

uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  // TODO(); // Lab3-1
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }

  int res = fseek(file, off, whence);
  return res;
}

int sys_fstat(int fd, struct stat *st) {
  // TODO(); // Lab3-1
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }

  if(file->type == TYPE_FILE){
    st->type = itype(file->inode);
    st->node = ino(file->inode);
    st->size = isize(file->inode);
  } else if(file->type == TYPE_DEV){
    st->type = TYPE_DEV;
    st->node = 0;
    st->size = 0;
  }
  return 0;
}

int sys_chdir(const char *path) {
  TODO(); // Lab3-2
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
