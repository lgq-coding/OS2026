# Lab4 Prompt 汇总

> 本文件按 Lab0.5 的四段式规范，汇总 Lab4 中使用和迭代过的 Prompt。每组 Prompt 均包含 `[PROMPT]`、`[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]`。

## 一、Prompt 索引

| 编号 | 负责人 | 功能模块 | 结果 |
|------|--------|----------|------|
| P01 | 刘国全 | Lab2 Best-Fit PMM 与 `page_init()` 迁移 | 编译及 PMM 检查通过 |
| P02 | 王乐妍 | Lab3 Trap 恢复链迁移 | Trap、首次运行链正常 |
| P03 | 刘国全 | `page_insert()`、`page_remove()` | `[Test PMM] PASS` |
| P04 | 刘爽 | 进程创建与初始化 | `[Test Process Create] PASS` |
| P05 | 王乐妍 | `schedule()`、`proc_run()`、`switch_to()` | 初版完成，Schedule 未通过 |
| P06 | 刘国全 | 修复调度链表回绕和 idleproc 选择问题 | `[Test Schedule] PASS`，30/30 |
| P07 | 全员 | 测试、截图、报告与最终检查 | 报告及测试材料完成 |

---

## P01：迁移 Lab2 Best-Fit PMM 与 `page_init()`

**负责人：**2411822-刘国全

**使用目的：**把 Lab2 的 Best-Fit 物理页管理器和 `page_init()` 迁移到 Lab4 骨架，解决 Lab4 中缺失 PMM 实现的问题。

### Prompt

```text
[PROMPT]
请检查 Lab4 的 kern/mm/default_pmm.c 和 kern/mm/pmm.c，将 Lab2 的 Best-Fit
物理页管理器迁移到 Lab4。Lab4 骨架没有 best_fit_pmm.h，且 pmm.c 使用
default_pmm_manager，因此不要把实现放到不存在的头文件中，应把 Best-Fit
实现适配为 default_pmm_manager。同时补全 page_init()，根据 DTB 内存范围建立
Page 描述符数组并把可用连续物理页交给 PMM。保留现有 local_intr_save/restore
中断保护，不修改 basic_check()、best_fit_check()。

[RELY]
- kern/mm/default_pmm.c、default_pmm.h、pmm.c、pmm.h
- get_memory_base()、get_memory_size()
- struct Page、free_area_t、list_*、PageReserved、PageProperty
- DRAM_BASE、PGSIZE、PPN、PADDR、ROUNDUP、ROUNDDOWN
- init_memmap()、pa2page()、alloc_page()、free_page()
- Lab4 骨架已有 best_fit_check()，测试函数不可修改
- alloc_pages()/free_pages() 已使用 local_intr_save/restore

[GUARANTEE]
派生出：
- static free_area_t free_area
- free_list、nr_free 宏
- best_fit_init()
- best_fit_init_memmap()
- best_fit_alloc_pages()
- best_fit_free_pages()
- best_fit_nr_free_pages()
- const struct pmm_manager default_pmm_manager
- static void page_init(void)

[SPECIFICATION]
Pre-Condition：
DTB 已初始化，get_memory_base()/get_memory_size() 返回合法物理内存范围；
kernel 的 end 符号和 boot_page_table_sv39 可用；
每页描述符中的 reserved 位初始可写。

Post-Condition：
npage 覆盖 [DRAM_BASE, mem_end)；
pages 位于 kernel end 之后且不越过物理内存末尾；
Page 描述符数组占用区域被保留；
PMM 只接管描述符数组之后的完整物理页；
Best-Fit 分配选择满足请求且剩余空间最小的空闲块；
释放页面时正确合并左右相邻空闲块；
alloc_pages/free_pages/nr_free_pages 仍保持中断保护。

Failure：
若 DTB 范围非法、页描述符数组越界或没有可用物理内存，page_init() 必须 panic；
alloc_pages 在 nr_free 不足时返回 NULL；
不能修改 best_fit_check() 或通过修改测试规避失败。
```

### 结果

- 删除不存在的 `best_fit_pmm.h` 依赖。
- Best-Fit 实现放入 `default_pmm.c`，并导出为 `default_pmm_manager`。
- `page_init()` 成功建立物理页描述符和空闲页链表。
- `check_alloc_page()`、`check_pgdir()`、`check_boot_pgdir()` 均通过。

---

## P02：迁移 Lab3 Trap 恢复链

**负责人：**2413423-王乐妍

**使用目的：**恢复 Lab4 所需的 Lab3 Trap 处理能力，使 `forkret -> forkrets -> __trapret` 首次运行链可用。

### Prompt

```text
[PROMPT]
根据 Lab3 的实现补齐 Lab4 的 kern/trap/trapentry.S 和 kern/trap/trap.c：
实现 RESTORE_ALL、__alltraps、interrupt_handler() 和 exception_handler()。
要求 Trap frame 为 32 个通用寄存器加 sstatus、sepc、sbadaddr、scause。
__alltraps 保存现场后以 sp 作为 trap(struct trapframe *tf) 的参数。
RESTORE_ALL 先恢复 CSR，再恢复 x1、x3~x31，最后恢复 sp。
forkrets 必须能接收一个新进程的 Trap frame 并跳转到 __trapret。

[RELY]
- Lab3 已验证的 trapentry.S 和 trap.c
- struct trapframe 的 36 项布局
- STORE/LOAD、REGBYTES、sret
- IRQ_S_TIMER、IRQ_S_SOFT
- CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT
- tlb_invalidate、clock_set_next_event
- forkrets、__trapret、forkret

[GUARANTEE]
补充：
- RESTORE_ALL 宏
- __alltraps 入口
- forkrets 所需的 __trapret 恢复链
- interrupt_handler()
- exception_handler()
- trap_insn_len()

[SPECIFICATION]
Pre-Condition：
Trap 发生时硬件已写入 sepc、scause、sstatus；
sp 指向有效的内核栈；
sscratch 中保存进入 Trap 前的 sp。

Post-Condition：
Trap frame 完整保存 32 个通用寄存器和 4 个 CSR；
trap() 能根据 scause 最高位分发给中断或异常处理；
时钟中断每次重新设置下一次 timer；
非法指令和断点异常按真实指令长度推进 epc；
RESTORE_ALL 正确恢复 CSR 和通用寄存器，最后恢复 sp；
__trapret 能通过 sret 返回；
forkrets 能从一个新进程的 Trap frame 恢复首次运行现场。

Failure：
sp 不能提前恢复；
CSR 恢复必须在覆盖 s0~s4 前完成；
异常处理不能对压缩指令固定加 4；
forkret/forkrets 链中的栈和寄存器不能错位。
```

### 结果

- Lab3 Trap 代码成功迁移到 Lab4。
- 时钟中断持续触发。
- `forkret -> forkrets -> __trapret -> kernel_thread_entry` 链路正常。
- Schedule 测试最终能进入 `init_main`。

---

## P03：实现 `page_insert()` 和 `page_remove()`

**负责人：**2411822-刘国全

**使用目的：**完成 Lab4 功能模块一，维护页表项、引用计数和 TLB 的一致性。

### Prompt

```text
[PROMPT]
在 kern/mm/pmm.c 的 //LAB4 function1 位置实现 page_insert()、page_remove()
以及内部辅助 page_remove_pte()。要求正确处理新增映射、同页重复映射、替换旧页、
未映射地址、get_pte() 失败和 TLB 失效。

[RELY]
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
static void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep);

- get_pte()、pte_create()、pte2page()、page2ppn()
- page_ref_inc()、page_ref_dec()、free_page()
- tlb_invalidate()、flush_tlb()
- PTE_V、PTE_U、PTE_W、PTE_R
- boot_pgdir_va

[GUARANTEE]
必须实现：
- page_insert()
- page_remove_pte()
- page_remove()

[SPECIFICATION]
Pre-Condition：
pgdir 是有效页目录；
page 是有效物理页；
la 是需要操作的线性地址；
perm 是目标页权限。

Post-Condition：
page_insert 成功时 la 映射到 page，并带有 perm；
页表项有效位为 1；
page 的引用计数与映射次数一致；
替换旧页时旧页引用计数减一，必要时释放；
修改页表后对应 TLB 失效；
page_remove 对未映射地址返回而不崩溃；
page_remove 可重复调用。

Failure：
get_pte(..., 1) 失败时返回 -E_NO_MEM；
失败时不能留下半完成映射；
不能重复增加同一页面在同一地址上的引用；
不能清空其他地址的映射。
```

### 结果

- `page_insert()`、`page_remove_pte()`、`page_remove()` 编译通过。
- `check_pgdir()` 和 `check_boot_pgdir()` 通过。
- `[Test PMM] PASS`。

---

## P04：实现进程创建与初始化

**负责人：**2411350-刘爽

**使用目的：**完成 PCB 初始化、`idleproc`/`initproc` 创建、内核栈分配与首次运行上下文构造，并实现 `cpu_idle()`、`proc_run()` 两个调度接口；让新线程第一次被调度时能沿 `forkret -> forkrets -> __trapret -> sret -> kernel_thread_entry -> fn(arg)` 链路运行。

### Prompt

```text
[PROMPT]
在 code/kern/process/proc.c 中实现两组函数：
1) 创建与初始化：alloc_proc()、proc_init()、kernel_thread()、
   do_fork()、copy_thread()、copy_mm()；
2) 调度接口：cpu_idle()、proc_run()。
要求：创建 idleproc（pid=0，复用 bootstack）和 initproc（pid=1，父进程为
idleproc）；正确初始化 PCB 字段、内核栈、Trap frame 与 context；
do_fork 按 alloc_proc -> setup_kstack -> copy_mm -> copy_thread ->
get_pid/hash_proc/list_add/nr_process++ -> wakeup_proc 的顺序组合资源，
失败按逆序释放；kernel_thread 伪造 trapframe，使 epc=kernel_thread_entry、
s0=fn、s1=arg；copy_thread 将 tf 放在内核栈顶，context.ra=forkret、
context.sp=tf；proc_run 在关中断临界区内更新 current、切换 satp 并调用
switch_to；cpu_idle 在 need_resched 为真时调用 schedule。不要修改测试函数、
inject.py 或 grade.sh。

[RELY]
- struct proc_struct、struct context、struct trapframe
- proc_list、hash_list、current、idleproc、initproc、nr_process
- PROC_UNINIT / PROC_RUNNABLE、MAX_PROCESS、HASH_LIST_SIZE、KSTACKSIZE
- kmalloc、kfree、setup_kstack、put_kstack、alloc_pages、free_pages
- bootstack、boot_pgdir_pa、page2kva、kva2page
- get_pid、hash_proc、find_proc、set_proc_name、wakeup_proc
- kernel_thread_entry、forkret、forkrets、__trapret、switch_to、schedule
- read_csr(sstatus)、write_csr(satp)、SSTATUS_SPP、SSTATUS_SPIE、SSTATUS_SIE、
  RISCV_PGSHIFT
- local_intr_save、local_intr_restore
- list_init、list_add

[GUARANTEE]
必须实现并保持签名：
- static struct proc_struct *alloc_proc(void);
- void proc_init(void);
- int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
- int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
- static void copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf);
- static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
- void cpu_idle(void);
- void proc_run(struct proc_struct *proc);

[SPECIFICATION]
Pre-Condition：
current 指向创建线程的父执行流；
内核堆与物理页分配器已初始化；
proc_list 与 PID 哈希表尚未初始化或需要重新初始化；
switch_to、schedule 由其他模块提供。

Post-Condition：
alloc_proc 返回字段全部初始化的 PCB，context 与两个链表节点清零/自链；
idleproc 的 pid=0、state=PROC_RUNNABLE、kstack=bootstack、need_resched=1；
current 指向 idleproc；
initproc 的 pid=1、state=PROC_RUNNABLE、parent=idleproc；
initproc 已加入 proc_list 和 PID 哈希表；
initproc 的 tf 位于自己内核栈顶部；
initproc 的 context.ra 指向 forkret，context.sp 指向其 Trap frame；
kernel_thread 构造的 trapframe 中 epc=kernel_thread_entry、s0=fn、s1=arg、
a0=0，status 的 SPP=1、SPIE=1、SIE=0；
copy_mm 对内核线程置 mm=NULL、pgdir=boot_pgdir_pa；
proc_run 在同一进程时直接返回；切换时 current、runs、satp 与上下文一致；
cpu_idle 循环检查 need_resched，为真时调用 schedule()。

Failure：
PCB 分配失败返回 NULL；
内核栈分配失败释放 PCB；
copy_mm 失败释放内核栈与 PCB；
不能把 pid=-1、未挂接或无 Trap frame 的进程置为 RUNNABLE；
PID 分配、入链表与入哈希表必须在关中断临界区内完成；
proc_run 不能在切换中途让 current 与 satp 不一致；
同一进程重复进入 proc_run 必须直接返回；
不得通过修改测试或评分脚本来制造通过结果。
```

### 结果

- `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()`、`copy_mm()`、`cpu_idle()`、`proc_run()` 编译通过。
- `[Test Process Create] PASS`，`initproc` 能通过首次运行链进入 `init_main`。
- `proc_run()` 与 `switch_to()` 配合完成上下文与页表切换；`schedule()` 的环形扫描修复见 P06。

---

## P05：实现调度与上下文切换

**负责人：**2413423-王乐妍

**使用目的：**实现 `cpu_idle()`、`schedule()`、`proc_run()` 和 `switch_to()`，让已创建的线程真正运行。

### Prompt

```text
[PROMPT]
实现 Lab4 的进程调度模块：
- cpu_idle() 持续检查 current->need_resched 并在需要时调用 schedule()；
- schedule() 从 proc_list 中选择下一个 PROC_RUNNABLE 线程，找不到时回退 idleproc；
- proc_run() 在临界区内更新 current、切换 satp 并调用 switch_to()；
- switch_to() 只保存和恢复 callee-saved 寄存器。

[RELY]
- struct proc_struct、struct context
- proc_list、current、idleproc
- PROC_RUNNABLE
- need_resched、runs
- local_intr_save/local_intr_restore
- satp、RISCV_PGSHIFT
- forkret、forkrets、__trapret
- struct context 包含 ra、sp、s0~s11

[GUARANTEE]
派生：
- void cpu_idle(void)
- void schedule(void)
- void proc_run(struct proc_struct *proc)
- void switch_to(struct context *from, struct context *to)

[SPECIFICATION]
Pre-Condition：
proc_list 已完成初始化；
idleproc、current 和 initproc 均已存在；
调度只在关闭中断的临界区中修改共享状态。

Post-Condition：
cpu_idle 在 need_resched 为 1 时调用 schedule；
schedule 选择一个非 idle 的 PROC_RUNNABLE 线程；
如果只有 idleproc 可运行，则回退到 idleproc；
proc_run 更新 current 和 runs，切换 satp，并调用 switch_to；
switch_to 保存 from 的 ra/sp/s0~s11，恢复 to 的 ra/sp/s0~s11；
新线程首次恢复时从 forkret 进入首次运行链。

Failure：
不能无条件重新开启中断；
不能在切换过程中让 current、satp 和实际执行的上下文不一致；
switch_to 不能破坏 caller-saved 寄存器；
不能在链表遍历时遗漏可运行线程。
```

### 结果

- `switch_to()` 编译并完成上下文切换。
- 初版 `schedule()` 能编译并进入调度，但 Schedule 测试仍失败。

---

## P06：修复 Schedule 测试中的调度链表问题

**负责人：**2411822-刘国全

**使用目的：**定位 Schedule 测试 20/30 的原因并修复链表回绕与 idleproc 选择问题。

### Prompt

```text
[PROMPT]
make grade 的 PMM 和 Process Create 已通过，但 Schedule 失败，QEMU 只反复打印
100 ticks。请基于 proc_list、current、idleproc、test_lab4_schedule 和
schedule() 分析为什么 initproc 创建的测试线程没有被选中，并修复调度算法。
不要修改测试代码或 grade.sh。

[RELY]
- kern/schedule/sched.c
- kern/process/proc.c
- proc_list 的插入顺序
- idleproc/current/need_resched
- test_lab4_schedule
- GDB 断点 schedule、proc_run、forkret、kernel_thread_entry、init_main

[GUARANTEE]
修改 schedule()，保持：
void schedule(void);

[SPECIFICATION]
Pre-Condition：
current 指向当前运行线程；
proc_list 是环形双向链表；
idleproc 是空闲线程；
线程状态通过 PROC_RUNNABLE 表示可运行。

Post-Condition：
从 current 的下一个线程开始查找；
到达链表尾后回绕到链表头；
正常调度时跳过 idleproc；
不重复选择 current；
存在其他可运行线程时，不能提前切回 idleproc；
没有其他可运行线程时，才选择 idleproc 作为保底。

Failure：
不能通过修改测试函数、注入脚本或评分脚本来规避问题；
不能在链表中无限循环；
不能在临界区内遗漏中断状态恢复。
```

### 结果

将调度查找改为环形遍历，并跳过 `idleproc` 和当前线程。测试结果变为：

```text
-check PMM:                                OK
-check Process Create:                     OK
-check Schedule:                           OK
Total Score: 30/30
```

---

## P07：测试、截图与报告整理

**负责人：**全员，王乐妍统稿

**使用目的：**将实现、测试、Bug 记录和 Prompt 整理成可提交的 Markdown 报告。

### Prompt

```text
[PROMPT]
根据 Lab4 的实际源码、Git 提交记录和 make grade 输出，完成实验报告、
Prompt 汇总和测试截图。不得虚构测试结果；报告中的分工、函数名、
代码路径和得分必须与当前工程一致。最终报告以文本说明为主。

[RELY]
- code/kern/mm/pmm.c
- code/kern/mm/default_pmm.c
- code/kern/process/proc.c
- code/kern/process/switch.S
- code/kern/schedule/sched.c
- code/kern/trap/trap.c
- code/kern/trap/trapentry.S
- report/images/01_pmm.png
- report/images/02_process_create.png
- report/images/03_schedule.png
- make grade 输出：Total Score: 30/30

[GUARANTEE]
- report/report.md
- report/prompt.md
- report/images/01_pmm.png
- report/images/02_process_create.png
- report/images/03_schedule.png

[SPECIFICATION]
Pre-Condition：
Lab4 代码已合并；
make grade 已得到 30/30；
三张测试截图已存在。

Post-Condition：
报告包含实验整体逻辑、每个核心模块、Spec、Prompt、AI Bug、测试结果、
OS 知识点和未覆盖知识点；
Prompt 汇总为四段式；
所有测试截图路径有效；
报告中不出现模板占位文本；
报告中的数字、函数名和提交结果与实际情况一致。

Failure：
不得把其他成员的代码归属写错；
不得声称未验证的功能通过；
不得修改评分脚本或测试代码来制造通过结果。
```

### 结果

报告、Prompt 和测试截图已整理完成，最终测试为 30/30。
