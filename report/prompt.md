# Lab4 Prompt 汇总

> 本文件按 Lab0.5 的四段式规范，汇总 Lab4 中使用和迭代过的 Prompt。每组 Prompt 均包含 `[PROMPT]`、`[RELY]`、`[GUARANTEE]` 和 `[SPECIFICATION]`。

## 一、Prompt 索引

| 编号 | 负责人 | 功能模块 | 结果 |
|------|--------|----------|------|
| P01 | 刘国全 | Lab2 Best-Fit PMM 与 `page_init()` 迁移 | 编译及 PMM 检查通过 |
| P02 | 王乐妍 | Lab3 Trap 恢复链迁移（复用 Lab3 提示词回填） | 已回填（d6af9fe），首次运行链联调通过 |
| P03 | 刘国全 | `page_insert()`、`page_remove()` | `[Test PMM] PASS` |
| P04 | 刘爽 | 进程创建与初始化 | `[Test Process Create] PASS` |
| P05 | 王乐妍 | `switch_to()`、`schedule()` 初版（含 runs++ 职责修正） | 实现（c74d392），Schedule 测试未通过 |
| P06 | 刘国全 | 修复调度链表回绕和 idleproc 选择问题 | `[Test Schedule] PASS`，30/30 |
| P07 | 王乐妍 | 调度测试与 GDB 验证 | make grade 30/30，首次运行链路确认 |

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

> 按练习要求，依赖迁移不重新设计提示词，复用 Lab3 已迭代定稿的提示词，将生成结果回填到 Lab4 框架标有 `lab3` 注释的位置。此处记录迁移用的汇总性提示词。

### Prompt

```text
[PROMPT]
Lab4 框架中 kern/trap/trapentry.S 与 kern/trap/trap.c 已保留 Lab3 的框架代码并留有
"#lab3:填写你在lab3中实现的代码" 与 "//lab3 请补充你在lab3中的代码" 标记。请将 Lab3
分支中已通过全部测试的 Trap 实现原样回填到对应标记位置，包括：trapentry.S 的 RESTORE_ALL
宏、__alltraps 入口、__trapret 恢复链；trap.c 的 interrupt_handler() 与
exception_handler()。只做代码迁移，不重新设计，不修改任何函数签名与框架既有代码。

[RELY]
Lab3 分支已验证的 trapentry.S / trap.c 实现；Lab4 框架保留的 SAVE_ALL 宏、trap() 分发
框架、print_trapframe() / print_regs()；clock.h 的 ticks 与 clock_set_next_event()；
riscv.h 的 SSTATUS_SIE、IRQ_S_TIMER、CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT 宏；
mmu.h / trap.h 中 trapframe 的槽位布局。

[GUARANTEE]
必须实现的接口：
RESTORE_ALL   宏：与 SAVE_ALL 对称的恢复序列
__alltraps:   SAVE_ALL + trap() 分发 + __trapret
__trapret:    RESTORE_ALL + sret
forkrets:     move sp, a0 后跳入 __trapret（框架已有，不可改动）
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
不得改动 idt_init()、trap() 及其他既有代码。

[SPECIFICATION]
Pre-Condition：
idt_init() 已将 stvec 指向 __alltraps，sscratch 置 0 表示来自内核态。

Post-Condition：
时钟中断可周期性到达并被处理，clock_set_next_event() 被重新调用；
forkrets 能以传入 trapframe 为栈经 __trapret 完成恢复并 sret；
非法指令与断点异常按指令真实长度（RVC 2 字节 / 普通 4 字节）前进 tf->epc。

Failure：
未知 cause 一律 print_trapframe() 兜底，不 panic、不推进 epc。
```

### 结果

已回填并提交（d6af9fe）。联调验证：时钟中断持续触发，`forkret -> forkrets -> __trapret -> kernel_thread_entry` 首次运行链正常，Schedule 测试最终能进入 `init_main`。

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

**使用目的：**实现 `switch_to()` 与 `schedule()`，让已创建的线程真正运行。分两步：先以完整文件覆盖式提示词生成 `switch_to`（P05-a），再以标记插入式提示词生成 `schedule()`（P05-b）。

### Prompt

#### P05-a：switch_to（完整文件覆盖式提示词）

```text
[PROMPT]
**任务**：编写文件 code/kern/process/switch.S，实现 RISC-V 汇编函数
switch_to(struct context *from, struct context *to)，完成内核线程间的上下文保存与恢复。
当前文件只有一行 "//LAB4 function3 YOUR CODE" 标记，请以完整文件内容覆盖。

**操作要求**：你必须在项目中修改实际文件，而不是仅展示代码。只定义 switch_to 一个符号，
不得修改工程中任何其他文件。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的
信息，以工程真实代码为准，不要引入无关功能。

[RELY]
// struct context（kern/process/proc.h）
struct context
{
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

// 函数原型（kern/process/proc.h）
void switch_to(struct context *from, struct context *to);

// 调用点（kern/process/proc.c 的 proc_run()）
switch_to(&prev->context, &next->context);

[GUARANTEE]
必须实现的主要接口：
.globl switch_to
switch_to:  # void switch_to(struct context *from, struct context *to);
你可以根据需要添加：
- 汇编注释与偏移量定义（REGBYTES = 8）。

[SPECIFICATION]
## switch_to
Pre-Condition:
- a0 = from，a1 = to，各指向一个合法的 struct context；
- to 指向的 context 中，ra 与 sp 已被初始化：要么是目标线程上次被切出时保存的现场，
  要么是首次运行前设置的入口（ra 指向 forkret，sp 指向其内核栈）。

Post-Condition:
- from 所指内存中已保存切换时刻的 ra、sp、s0~s11 共 14 个寄存器值，与 struct context
  字段一一对应；
- CPU 的 ra、sp、s0~s11 已被 to 所指内存中的值替换；
- 函数通过 ret 返回，返回地址为 to->ra；
- sp 在所有访存完成后最后恢复；不修改其余通用寄存器，不访问全局变量，不关开中断
  （由调用方 proc_run()/schedule() 负责）。

Requirements:
- 工程经 riscv64 交叉工具链汇编，不得使用未知伪指令或非 RISC-V 指令；
- 保存与恢复的寄存器集合、顺序必须与 struct context 字段一一对应，禁止遗漏 s0~s11
  中任何一个。
```

#### P05-b：schedule（标记插入式提示词，含迭代修正）

```text
[PROMPT]
**任务**：修改文件 code/kern/schedule/sched.c，在 "//LAB4 function3 YOUR CODE" 标记处
实现调度函数 schedule()：清除当前线程的 need_resched，从 proc_list 中选出下一个 state
为 PROC_RUNNABLE 的线程，必要时通过 proc_run() 完成切换。保留文件中所有现有代码，只在
标记位置插入新实现，不得修改任何既有函数的函数体或签名。

**操作要求**：你必须在项目中修改实际文件，而不是仅展示代码。若需辅助函数必须声明为 static。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的
信息，以工程真实代码为准，不要引入无关功能。

[RELY]
// 进程状态与进程控制块（kern/process/proc.h）
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

struct proc_struct
{
    enum proc_state state;        // Process state
    int pid;                      // Process ID
    int runs;                     // the running times of Proces
    uintptr_t kstack;             // Process kernel stack
    volatile bool need_resched;   // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;   // the parent process
    struct mm_struct *mm;         // Process's memory management field
    struct context context;       // Switch here to run process
    struct trapframe *tf;         // Trap frame for current interrupt
    uintptr_t pgdir;              // the base addr of Page Directroy Table(PDT)
    uint32_t flags;               // Process flag
    char name[PROC_NAME_LEN + 1]; // Process name
    list_entry_t list_link;       // Process link list
    list_entry_t hash_link;       // PID hash list
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;

// 切换接口（kern/process/proc.h）
void proc_run(struct proc_struct *proc);

// 链表操作（libs/list.h）
list_entry_t *list_next(list_entry_t *le);

// 临界区保护（kern/sync/sync.h，sched.c 已 #include <sync.h>）
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

// 既有声明（kern/schedule/sched.h）
void schedule(void);

[GUARANTEE]
必须实现的主要接口：
void schedule(void);
你可以根据需要添加：
- static 辅助函数或宏（如有必要）。

[SPECIFICATION]
## schedule
Pre-Condition:
- current 指向当前正在运行的线程，不为 NULL；
- proc_list 中至少存在一个 state 为 PROC_RUNNABLE 的线程，或可回退到 idleproc。

Post-Condition:
- current->need_resched 已被清 0；
- next 的选取规则：current != idleproc 时，从 current->list_link 的下一个链表项开始
  循环查找第一个 state == PROC_RUNNABLE 的线程；current == idleproc 时，从 proc_list
  链表头开始查找；整个链表查完无可运行线程时，next = idleproc；
- next != current 时已调用 proc_run(next)；next == current 时不调用 proc_run，直接返回；
- 选取与切换过程处于 local_intr_save/local_intr_restore 之间，返回前中断状态已恢复；
- proc->runs++ 属于 proc_run() 的职责（分工 B5 第 4 步），schedule() 不得重复计数。

Failure:
- proc_list 为空或全部线程不可运行时，不得 panic、不得解引用 NULL，必须回退到
  next = idleproc。

Requirements:
- 不得修改 wakeup_proc() 及文件中其他既有代码；不得改动 proc.h/sched.h/sync.h 等头文件；
- 工程以 GNU C99 且 -Wall -Werror 编译，不得引入隐式声明等告警。
```

### 结果

一次迭代后实现完成（提交 c74d392）。迭代原因见报告 4.3.3 第一次迭代：首次生成中 AI 在 `schedule()` 内自行加了 `next->runs++`，与 `proc_run()` 的职责重复会导致 `runs` 每次调度 +2；修正方式为在 `[SPECIFICATION]` 中显式声明 `runs++` 归 `proc_run()`，删除该行后重新生成。`switch_to()` 编译并完成上下文切换；初版 `schedule()` 能编译并进入调度，但 Schedule 测试仍未通过（环形回绕问题，由 P06 修复）。

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

## P07：调度测试与 GDB 验证

**负责人：**2413423-王乐妍

**使用目的：**proc.c 集成后验证调度模块并输出验证证据（make grade 结果 + GDB 首次运行链观察）。

### Prompt

```text
[PROMPT]
proc.c 集成完成后，协助验证调度模块并输出验证证据：
1) 运行 make grade，确认输出 [Test Schedule] PASS，以及
   "this initproc, pid = 1, name = \"init\""、
   "To U: \"Hello world!!\"."、
   "To U: \"en.., Bye, Bye. :)\"" 等预期输出；
2) 给出用 GDB 验证上下文切换的操作步骤：在 switch_to 处设置断点，检查 a0/a1 指向的
   struct context 内容，单步观察 ra/sp/s0~s11 的保存与恢复，确认首次运行的线程经
   to->ra 进入 forkret，再经 forkrets -> __trapret 落到 kernel_thread_entry。

[RELY]
tools/test_lab4.c 的 test_lab4_schedule()（由 tools/inject.py 注入）；
kern/process/switch.S 的 switch_to；kern/process/proc.c 的 forkret/forkrets；
tools/gdbinit 与 make debug/make gdb 调试入口。

[GUARANTEE]
只做验证与调试观察，不修改任何内核源码。

[SPECIFICATION]
Pre-Condition：
proc.c 集成完成，make 可生成 bin/kernel。

Post-Condition：
make grade 输出 [Test Schedule] PASS；GDB 断点处能观察到 switch_to 前后 sp/ra 的
保存与恢复、以及新线程首次进入 forkret 的完整链路。

Failure：
若测试失败，定位并报告首个断言失败的检查点（需指出对应源码位置与现场寄存器值），
不猜测性修改代码。
```

### 结果

`make grade` 通过：PMM、Process Create、Schedule 检查全部 OK，Total Score 30/30；initproc 预期输出确认（见报告 4.3.6 与 5.3 节截图）。首次运行链路经 GDB 断点逐步观察确认（记录见报告 7.3 节 Bug 4）。
