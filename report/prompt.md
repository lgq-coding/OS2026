# Lab5 Prompt 汇总

> 按 Lab0.5 的四段式规范记录本实验使用的全部 Prompt。

## 一、Prompt 索引

| 编号 | 负责人 | 功能模块 | 结果 |
|------|--------|----------|------|
| P01 | 刘国全 | Lab2/Lab3/Lab4 依赖迁移 | 待验证 |
| P02 | 王乐妍 | 系统调用入口与 `sys_*` 分发 | 待验证 |
| P03 | **刘爽** | 第一个用户进程与 ELF 装载 | 代码完成，语法检查通过，待合并运行 |
| P04 | 刘国全 | 用户进程复制与地址空间复制 | 待验证 |
| P05 | **刘爽** | 进程退出 / 等待 / 让出 / 终止 | 代码完成，语法检查通过，待合并运行 |
| P06 | 王乐妍 | U/S Trap 返回与 GDB 调试 | 待验证 |
| P07 | 全员 | 报告、截图与最终验证 | 待统稿 |

---

## P03：第一个用户进程创建与 ELF 装载

**负责人：**2411350-刘爽

**使用目的：**把 Lab4 的内核线程模型扩展为“内核线程 → 用户进程”，完成 `alloc_proc` 字段扩展、`kernel_thread` / `copy_thread` / `proc_init` / `init_main` 的进程链搭建，以及 `user_main` / `kernel_execve` / `do_execve` / `load_icode` 的用户态首次进入。

### Prompt

```text
[PROMPT]
请基于 Lab5 骨架中的 code/kern/process/proc.c（当前只保留 set_proc_name、
get_proc_name、set_links、remove_links、get_pid、forkret、hash_proc、
unhash_proc、find_proc、setup_kstack、put_kstack、setup_pgdir、put_pgdir
等公共函数，其余为 YOUR CODE 标记），实现刘爽负责的部分：
alloc_proc、kernel_thread、copy_thread、proc_init、init_main、user_main、
kernel_execve、do_execve、load_icode。注意 copy_mm 与 do_fork 属于刘国全，
不要实现，保留原始标记。要求：
1) alloc_proc 除 Lab4 的 14 个字段外，还要初始化 Lab5 新增的 exit_code、
   wait_state、cptr、yptr、optr，pgdir 取 boot_pgdir_pa。
2) kernel_thread 用 s0 存函数指针、s1 存参数，epc 指向 kernel_thread_entry，
   status 置 SPP|SPIE 并清 SIE。
3) copy_thread 把 trapframe 放在内核栈顶（kstack + KSTACKSIZE - sizeof(tf)），
   a0 置 0，context.ra = forkret，context.sp = tf，esp 非 0 时 gpr.sp 用 esp。
4) proc_init 先建 idleproc(pid 0，复用 bootstack，need_resched=1)，再
   kernel_thread(init_main) 建 initproc(pid 1)；idleproc 的 parent 为 NULL，
   不能调 set_links。
5) init_main 建 user_main 后循环 do_wait(0, NULL) 直到返回非 0，最后
   panic("init check memory pass.")。
6) kernel_execve 调 do_execve 后不能 return，要把 sp 指向 current->tf 并跳
   __trapret，由 sret 回到 U 态。
7) do_execve 顺序：释放旧 mm（set_mm_count=0 → exit_mmap → put_pgdir →
   mm_destroy）→ mm_create + setup_pgdir → load_icode →
   current->mm/pgdir(PADDR)/name 生效；失败路径要清理已建对象。
8) load_icode 校验 ELF_MAGIC 与 e_phoff/e_phnum；逐 PT_LOAD 段用
   mm_map(mm, p_va, p_memsz, vm_flags) 建 VMA，再逐页 alloc_page、清零、
   拷 filesz 部分、page_insert；随后建 USTACKTOP 向下 USTACKPAGE 页用户栈；
   最后把 tf 置为 epc=e_entry、gpr.sp=USTACKTOP、
   status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE。
9) 保留 Lab4/Lab5 原有注释，YOUR CODE 处标注 2411350，不改 proc.h 接口。

[RELY]
- kern/process/proc.c 骨架与 proc.h 中的 struct proc_struct、PF_EXITING、
  WT_CHILD、MAX_PROCESS/MAX_PID
- alloc_proc/kernel_thread/copy_thread 的 Lab4 语义；kernel_thread_entry
  （entry.S: move a0, s1; jalr s0; jal do_exit）；forkret/forkrets
- trapframe（36 个 8 字节槽：32 个 gpr + status/epc/badvaddr/cause）
- KSTACKSIZE、bootstack、boot_pgdir_pa、PADDR、page2kva、alloc_page、
  page_insert、free_page
- mm_create、setup_pgdir、put_pgdir、mm_destroy、exit_mmap、set_mm_count、
  mm_map、lock_mm/unlock_mm
- elf.h：elfhdr、proghdr、ELF_MAGIC、ELF_PT_LOAD、ELF_PF_X/W/R
- mmu.h：PGSIZE、PTE_U/PTE_V/PTE_R/PTE_W/PTE_X；riscv.h：SSTATUS_SPP/SPIE
- memlayout.h：USTACKTOP、USTACKPAGE、USTACKSIZE
- Makefile 的 --format=binary 与 _binary_obj___user_<name>_out_start/size

[GUARANTEE]
派生出：
- static struct proc_struct *alloc_proc(void)
- int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
- static void copy_thread(struct proc_struct *, uintptr_t, struct trapframe *)
- void proc_init(void)
- static int init_main(void *arg)
- static int user_main(void *arg)
- int kernel_execve(const char *name, unsigned char *binary, size_t size)
- int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
- static int load_icode(struct mm_struct *, unsigned char *, size_t)

[SPECIFICATION]
Pre-Condition：
pmm_init 已建立 boot_pgdir_pa 与 bootstack；proc_list 与 hash_list 未初始化；
binary 是内核镜像内一段合法 ELF（由 --format=binary 嵌入）。

Post-Condition：
idleproc: pid=0、PROC_RUNNABLE、kstack=bootstack、mm=NULL、tf=NULL、parent=NULL；
initproc: pid=1、PROC_RUNNABLE、parent=idleproc、context.ra 与 context.sp 非 0；
current=idleproc 且 need_resched=1；
user_main 经 kernel_execve → do_execve → load_icode 后，
current->mm 为新 mm、current->pgdir=PADDR(mm->pgdir)、name 为程序名，
current->tf: epc=e_entry、gpr.sp=USTACKTOP、status.SPP=0、status.SPIE=1；
kernel_execve 不返回，CPU 由 __trapret + sret 进入 U 态并从 e_entry 开始执行。

Failure：
e_magic != ELF_MAGIC、e_phnum==0、p_filesz > p_memsz -> -E_INVAL_ELF；
mm_create/setup_pgdir/alloc_page/page_insert 失败 -> -E_NO_MEM 并清理已建 mm；
alloc_proc 失败 -> panic；kernel_thread 建 init_main/user_main 失败 -> panic；
user_main 中 execve 失败 -> panic("user_main execve failed")。
```

### 结果

- `proc.c` 由 314 行骨架扩到 872 行，`YOUR CODE` 标记替换为 `2411350`。
- `copy_mm` / `do_fork` 区域保持原标记未动，避免与刘国全冲突。
- `gcc -fsyntax-only`（`-D__riscv -D__riscv_xlen=64`，同 Makefile 的 include 路径）零 error 零 warning。
- 运行验证依赖王乐妍的 `RESTORE_ALL/__alltraps`、`exception_handler`、`syscall` 和刘国全的 `copy_mm/do_fork/dup_mmap`，合并后跑 `make grade`。

---

## P05：进程退出、等待、让出与终止

**负责人：**2411350-刘爽

**使用目的：**实现进程生命周期的后半段：僵尸态与父进程回收分离、孤儿过继给 initproc、主动让出与被动终止。

### Prompt

```text
[PROMPT]
请继续在 code/kern/process/proc.c 中实现刘爽负责的 do_wait、do_exit、
do_yield、do_kill，遵循 ucore 的“退出与回收分离”模型：
1) do_exit：禁止 idleproc/initproc 调用；释放 mm（set_mm_count=0 后
   exit_mmap/put_pgdir/mm_destroy）；置 PF_EXITING 与 exit_code；把自己所有
   孩子过继给 initproc（已是僵尸的孩子要唤醒 initproc）；父进程若睡在
   WT_CHILD 上则清标志并唤醒；最后置 PROC_ZOMBIE 并 schedule()。
2) do_wait：code_store 非空时先用 user_mem_check 校验用户可写地址；
   在 proc_list 中筛 parent==current 且 pid 匹配的孩子；找到 PROC_ZOMBIE 时
   按 unhash_proc → remove_links → put_kstack → kfree 顺序回收，退出码用
   copy_to_user 写回；只能找到活孩子则 state=PROC_SLEEPING、
   wait_state=WT_CHILD、schedule() 后重试；一个孩子都没有返回 -E_BAD_PROC。
3) do_yield：current->need_resched = 1; schedule(); return 0。
4) do_kill：find_proc 校验，置 PF_EXITING，目标在 SLEEPING 时唤醒，
   非法 pid 或已在退出中返回 -E_INVAL。
5) 关键顺序：kfree 之前必须先把 pid 和 exit_code 存到栈上，否则 use-after-free；
   copy_to_user 必须放在中断恢复之后；判断指针是否为空不能用已释放的 PCB。
6) 保留原有注释，标记 2411350，不修改 proc.h 与调度接口。

[RELY]
- struct proc_struct 的 state/exit_code/wait_state/cptr/yptr/optr/flags
- PROC_ZOMBIE、PROC_SLEEPING、PROC_RUNNABLE、PF_EXITING、WT_CHILD、E_KILLED
- unhash_proc、remove_links、put_kstack、kfree、find_proc、proc_list
- schedule()、wakeup_proc()、local_intr_save/restore
- exit_mmap、put_pgdir、mm_destroy、set_mm_count
- user_mem_check、copy_to_user、lock_mm/unlock_mm
- 测试约束：do_wait(-1, NULL) == -E_BAD_PROC，do_kill(-1) == -E_INVAL

[GUARANTEE]
派生出：
- int do_wait(int pid, int *code_store)
- int do_exit(int error_code)
- int do_yield(void)
- int do_kill(int pid)

[SPECIFICATION]
Pre-Condition：
do_exit 的 current 不是 idleproc/initproc；do_wait 的 current 可能有子进程。

Post-Condition：
do_exit：mm==NULL、pgdir==boot_pgdir_pa、flags 含 PF_EXITING、state==PROC_ZOMBIE、
         孩子 parent 全指向 initproc、父进程被唤醒、函数不返回。
do_wait：僵尸孩子从 hash_list/proc_list/内核栈/PCB 彻底释放，*code_store 得到
         退出码，返回该孩子 pid。
do_yield：need_resched==1 且 schedule() 返回 0。
do_kill：目标 flags 含 PF_EXITING，返回 0。

Failure：
do_wait 无子进程 -> -E_BAD_PROC；code_store 非法 -> -E_INVAL；
do_wait 睡醒后发现 PF_EXITING -> do_exit(-E_KILLED)；
do_kill pid 非法或目标已在退出 -> -E_INVAL；
对 idleproc/initproc 调 do_exit -> panic。
```

### 结果

- 四个函数实现完毕，`do_wait` 采用“在 `proc_list` 中按 `parent` 筛选”而非手工遍历 `cptr` 兄弟链，降低链表维护出错风险。
- `gcc -fsyntax-only` 零 error 零 warning。
- 预期测试：`[Test Process Control] PASS`、`exit pass.`、`spin may pass.`、`forktest pass.`（需合并后运行）。
