#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

uint64
sys_sigalarm(void)
{
  int ticks = 0;
  uint64 phandler = 0;
  struct proc *p = myproc();

  if (argint(0, &ticks) < 0) {
    return -1;
  }
  if(argaddr(1, &phandler) < 0) {
    return -1;
  }

  if (ticks != 0) {
    p->alarmpastticks = 0;
    p->alarminterval = ticks;
    p->palarmhandler = phandler;
  }

  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();

  memmove(p->trapframe, p->alarmtrapframe, PGSIZE);
  p->alarmwaitret = 0;

  return 0;
}
