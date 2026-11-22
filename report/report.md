# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab4：进程管理 |
| **小组成员** | 2411822-刘国全、2411350-刘爽、2413423-王乐妍 |
| **完成日期** | 2026-11-18 |

### 小组分工

| 成员 | 负责的练习/模块 |
|------|----------------|
| 刘国全 | Lab2 PMM 依赖迁移、`page_init()`、`page_insert()`、`page_remove()`、VMM Challenge、`schedule()` 调度问题修复 |
| 刘爽 | `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()`、`copy_mm()`、`cpu_idle()`、`proc_run()` |
| 王乐妍 | Lab3 Trap 依赖迁移、`switch_to()`、`schedule()` 初版、测试与最终报告整合 |

### 报告分工

| 报告部分 | 负责人 | 协作人 |
|----------|--------|--------|
| 4.1 虚拟内存页操作 | 刘国全 | 刘爽 |
| 4.2 进程创建与初始化 | 刘爽 | 王乐妍 |
| 4.3 进程调度与上下文切换 | 王乐妍 | 刘国全 |
| 4.4 Lab2/Lab3 依赖迁移 | 刘国全、王乐妍 | 全员 |
| Challenge 与知识点 | 刘国全、王乐妍 | 刘爽 |
| 测试矩阵、截图与格式检查 | 王乐妍 | 全员 |

---

## 一、实验目的

本实验的主要目的是：

1. 理解虚拟内存中多级页表、页表项、TLB 和物理页引用计数之间的关系。
2. 实现 `page_insert()` 和 `page_remove()`，完成线性地址到物理页的映射维护。
3. 理解进程控制块 `struct proc_struct`、内核栈、上下文 `struct context` 和中断帧 `struct trapframe` 的作用。
4. 掌握内核线程的创建、初始化、首次运行和上下文切换过程。
5. 理解调度器如何从进程链表中选取下一个可运行线程，并通过 `proc_run()` 和 `switch_to()` 完成切换。
6. 使用 Prompt 驱动 AI 生成底层代码，并通过编译、测试和 GDB 调试验证实现。

---

## 二、实验环境

| 项目 | 内容 |
|------|------|
| 操作系统 | WSL2 Ubuntu |
| 目标架构 | RISC-V 64 |
| 内核地址空间 | Sv39，三级页表 |
| 交叉编译器 | `riscv64-unknown-elf-gcc` |
| 交叉调试器 | `riscv64-unknown-elf-gdb` |
| 模拟器 | `qemu-system-riscv64`；调试时使用 QEMU 4.1.1 调试版 |
| 构建工具 | GNU Make、Python 3 |
| 版本控制 | Git |

### AI 工具配置

| 成员 | AI 编程工具 | 底层模型 | 配置 |
|------|------------|---------|------|
| 刘国全 | Codex（VS Code/终端 Agent） | DeepSeek Flash（`deepseek-flash`） | 推理强度 high |
| 刘爽 | 豆包（桌面端） | 具体模型版本未记录 | 独立 WSL 环境补充验证 |
| 王乐妍 | Claude Code | DeepSeek Flash | 推理强度 high |

---

## 三、实验整体逻辑分析

### 3.1 本章逻辑主线

Lab4 从已经具备物理内存管理和 Trap 处理能力的内核出发，进一步建立虚拟内存映射、进程控制块、内核线程、上下文切换和进程调度能力。

完整执行链如下：

```text
kern_init
  -> pmm_init
  -> vmm_init
  -> proc_init
  -> cpu_idle
  -> schedule
  -> proc_run
  -> switch_to
  -> forkret -> forkrets -> __trapret -> kernel_thread_entry
```

三个功能模块的职责分别是：

1. **虚拟内存页操作**：维护页表项、物理页引用计数和 TLB 状态。
2. **进程创建与初始化**：构造 PCB、内核栈和初始 Trap frame，创建 `idleproc` 与 `initproc`。
3. **进程调度与上下文切换**：选择可运行线程，保存旧上下文并恢复新上下文，使线程能够真正运行。

### 3.2 功能的逐步实现

1. **迁移 Lab2/Lab3 依赖。**
   - 在 `best_fit_pmm.c` 中恢复 Best-Fit 管理器。
   - 在 `pmm.c` 中实现 `page_init()`。
   - 在 `trap.c` 和 `trapentry.S` 中恢复中断、异常和 Trap 返回链。
2. **实现页表映射维护。**
   - `page_insert()` 建立或更新映射。
   - `page_remove_pte()` 删除页表项并维护引用计数。
   - `page_remove()` 提供幂等的解除映射接口。
3. **实现进程创建。**
   - `alloc_proc()` 初始化 PCB。
   - `proc_init()` 创建 `idleproc` 和 `initproc`。
   - `kernel_thread()` 构造内核线程首次运行所需的 Trap frame。
   - `do_fork()` 组合 PCB、内核栈、PID、链表和状态。
   - `copy_thread()` 构造新线程的首次运行上下文。
4. **实现调度。**
   - `cpu_idle()` 在需要调度时让出 CPU。
   - `schedule()` 按环形顺序选择可运行线程。
   - `proc_run()` 更新 `current`、切换页表并调用 `switch_to()`。
   - `switch_to()` 保存和恢复 callee-saved 寄存器。
5. **测试与调试。**
   - 依次验证 PMM、Process Create 和 Schedule。
   - 修复调度链表未回绕以及 `idleproc` 被错误优先选中的问题。

---

## 四、实验内容与实现

### 4.1 功能模块：虚拟内存页操作

**负责人：**2411822-刘国全

**涉及文件：**

- `code/kern/mm/pmm.c`
- `code/kern/mm/best_fit_pmm.c`

#### 4.1.1 功能说明

`page_insert()` 负责把指定物理页映射到线性地址 `la`，并根据 `perm` 设置权限。`page_remove()` 负责删除已有映射。实现过程中必须同时维护：

```text
页表项内容
物理页引用计数
TLB 中的地址翻译缓存
```

#### 4.1.2 核心实现

```c
static void
page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (ptep != NULL && (*ptep & PTE_V)) {
        struct Page *page = pte2page(*ptep);
        if (page_ref_dec(page) == 0)
            free_page(page);
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

int
page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
        return -E_NO_MEM;

    page_ref_inc(page);
    if (*ptep & PTE_V) {
        struct Page *old_page = pte2page(*ptep);
        if (old_page == page)
            page_ref_dec(page);
        else
            page_remove_pte(pgdir, la, ptep);
    }

    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);
    return 0;
}
```

实现要点：

1. `get_pte(pgdir, la, 1)` 可能分配中间层页表页，失败时返回 `NULL`，因此必须返回 `-E_NO_MEM`。
2. 新物理页先增加引用计数。
3. 如果原地址已经映射到同一个物理页，先增加再减少引用计数，最终引用数不变。
4. 如果原地址映射到其他物理页，调用 `page_remove_pte()` 清除旧映射并减少旧页引用计数。
5. 最后写入新页表项并刷新 TLB。

`page_remove()` 的设计要求是幂等：如果目标地址没有有效映射，函数应安静返回，不能把正常的“未映射”情况当成异常。

#### 4.1.3 Spec

```text
Pre-Condition:
  pgdir 指向有效的页目录；
  page 指向有效的物理页描述符；
  la 为需要建立或删除映射的线性地址。

Post-Condition:
  page_insert 成功时，la 稳定映射到 page，权限等于 perm；
  page 的引用计数与映射次数一致；
  被替换的旧页引用计数正确减少；
  页表修改后对应 TLB 项失效。

Failure:
  get_pte 分配中间页表失败时返回 -E_NO_MEM；
  失败路径不能留下半完成映射；
  page_remove 在地址未映射时应直接返回。
```

#### 4.1.4 使用的 Prompt

对应 `report/prompt.md` 中的 **P03：实现 `page_insert()` 和 `page_remove()`**。

#### 4.1.5 验证状态

`check_pgdir()` 和 `check_boot_pgdir()` 均通过，并输出：

```text
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
[Test PMM] PASS
```

---

### 4.2 功能模块：进程创建与初始化

**负责人：**2411350-刘爽

**涉及文件：**`code/kern/process/proc.c`

#### 4.2.1 功能说明

本模块的函数分为两组：

- **进程创建与初始化**：`alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()`、`copy_mm()`；
- **调度接口**：`cpu_idle()`、`proc_run()`。真正“选谁运行”由王乐妍实现的 `schedule()` 完成；这两个函数只负责“空闲时让出 CPU”和“真正完成一次上下文与页表切换”，调度算法与 `switch_to()` 见 4.3 节。

函数原型：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
void cpu_idle(void);
void proc_run(struct proc_struct *proc);
```

整体职责链：

```text
proc_init
  -> alloc_proc(idleproc)
  -> kernel_thread(init_main, "Hello world!!", 0)
       -> do_fork
            -> alloc_proc -> setup_kstack -> copy_mm -> copy_thread
            -> 关中断: get_pid -> hash_proc -> list_add -> nr_process++
            -> wakeup_proc
```

#### 4.2.2 PCB 字段说明

`struct proc_struct` 是内核管理一个线程/进程的唯一实体，各字段含义与本模块的初值来源如下：

| 字段 | 含义 | 初值 / 来源 |
|------|------|-------------|
| `state` | 进程状态：`PROC_UNINIT` / `PROC_SLEEPING` / `PROC_RUNNABLE` / `PROC_ZOMBIE` | `PROC_UNINIT`，`wakeup_proc()` 后改为 `PROC_RUNNABLE` |
| `pid` | 进程号 | `alloc_proc` 置 `-1`，`get_pid()` 分配；`idleproc` 固定为 0 |
| `runs` | 被调度运行的次数 | 0，`proc_run()` 中 `runs++` |
| `kstack` | 内核栈物理地址 | `setup_kstack()` 分配；`idleproc` 复用 `bootstack` |
| `need_resched` | 是否需要被抢占调度 | 0；`idleproc` 置 1，进入 `cpu_idle` 后立即让出 |
| `parent` | 父进程指针 | `do_fork()` 中记为 `current` |
| `mm` | 用户地址空间管理结构 | 内核线程为 `NULL` |
| `context` | 上下文切换时保存的最小寄存器现场（`ra`、`sp`、`s0~s11`） | `memset` 清零 |
| `tf` | 中断帧 `struct trapframe *` | `copy_thread()` 指向内核栈顶的 trapframe |
| `pgdir` | 页目录物理地址 | 默认沿用 `boot_pgdir_pa` |
| `flags` | 进程标志 | 0 |
| `name` | 进程名（`PROC_NAME_LEN + 1`） | `memset` 清空，`set_proc_name()` 命名 |
| `list_link` | 就绪/进程双向链表节点 | `list_init` 自成空链 |
| `hash_link` | PID 哈希表节点 | `list_init` 自成空链 |

#### 4.2.3 核心实现

##### （1）`alloc_proc()`：PCB 显式初始化

PCB 本身用 `kmalloc` 从内核堆上分配；此时还没有内核栈、没有 PID，所有字段都必须显式赋初值，不能依赖 `kmalloc` 返回的内存内容：

```c
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = (struct proc_struct *)kmalloc(sizeof(struct proc_struct));
    if (proc == NULL)
    {
        return NULL;
    }

    proc->state = PROC_UNINIT;                              // 尚未初始化
    proc->pid = -1;                                         // 尚未分配 pid
    proc->runs = 0;                                         // 还未被调度过
    proc->kstack = 0;                                       // 内核栈稍后由 setup_kstack 分配
    proc->need_resched = 0;                                 // 不需要被抢占
    proc->parent = NULL;                                    // 父进程稍后设置
    proc->mm = NULL;                                        // 内核线程没有用户地址空间
    memset(&(proc->context), 0, sizeof(struct context));    // 上下文清零（含 ra）
    proc->tf = NULL;                                        // 中断帧稍后由 copy_thread 设置
    proc->pgdir = boot_pgdir_pa;                            // 默认沿用内核页目录（物理地址）
    proc->flags = 0;                                        // 无特殊标志
    memset(proc->name, 0, sizeof(proc->name));              // 名字清空
    list_init(&(proc->list_link));                          // 链表节点自成一个空链
    list_init(&(proc->hash_link));                          // 哈希节点自成空链

    return proc;
}
```

要点：`context` 用 `memset` 整块清零，保证 `context.ra` 初始为 0；两个链表节点必须 `list_init`，否则后续 `list_add` / `list_next` 会遍历到野指针。

##### （2）`copy_mm()`：内核线程无独立地址空间

Lab4 创建的全部是内核线程（`idleproc` 与 `initproc`），`current->mm == NULL`，因此不需要复制用户地址空间：

```c
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    (void)clone_flags;

    if (current->mm == NULL)
    {
        proc->mm = NULL;
        proc->pgdir = boot_pgdir_pa;
        return 0;
    }

    /* 用户进程的地址空间复制属于后续实验内容，Lab4 不涉及。 */
    proc->mm = NULL;
    proc->pgdir = boot_pgdir_pa;
    return 0;
}
```

接口按完整 fork 流程保留，便于后续实验扩展用户进程地址空间复制；`pgdir` 沿用内核页目录物理地址。

##### （3）`copy_thread()`：构造新线程的首次运行现场

在内核栈顶端留出一个 `struct trapframe`，把模板（`kernel_thread` 伪造的中断帧）复制进去，并设置切换上下文：

```c
static void
copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    (void)stack;

    // 内核栈顶减去一个 trapframe，得到本进程 trapframe 的起始地址
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;

    // 复制父进程/模板的中断帧
    *(proc->tf) = *tf;

    // 子进程的返回值：a0 = 0
    proc->tf->gpr.a0 = 0;

    // 新线程第一次被 sret 返回时使用的栈指针（栈向下增长，
    // 该地址之上就是 trapframe，函数栈帧不会破坏 tf）
    proc->tf->gpr.sp = (uintptr_t)(proc->tf);

    // 首次被 switch_to 换入时，从 forkret 开始执行
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

内核栈与 trapframe 的布局（栈向下增长）：

```text
高地址  kstack + KSTACKSIZE
        +----------------------+
        |  struct trapframe     |  <- proc->tf
        +----------------------+
        |  后续函数栈帧向下生长  |
低地址  kstack
```

字段设置的含义：

- `tf->gpr.a0 = 0`：子进程从 fork 返回 0（父进程返回子 PID）；
- `tf->gpr.sp = tf`：`sret` 后使用的栈指针指向自己的 trapframe；
- `context.ra = forkret`：`switch_to` 恢复后先跳到 `forkret`；
- `context.sp = tf`：切换时 `sp` 指向该 trapframe。

##### （4）`do_fork()`：资源申请顺序与逆序释放

`do_fork()` 按固定顺序组合资源，任何一步失败都按“已获得资源的逆序”释放：

```text
alloc_proc -> 记录 parent -> setup_kstack -> copy_mm -> copy_thread
  -> 关中断: get_pid -> hash_proc -> list_add -> nr_process++
  -> wakeup_proc
```

失败回退标签：

| 失败点 | 回退动作 |
|--------|----------|
| `alloc_proc` 失败 | 直接 `fork_out`，返回 `-E_NO_MEM` |
| `setup_kstack` 失败 | `goto bad_fork_cleanup_proc`：`kfree(proc)` |
| `copy_mm` 失败 | `goto bad_fork_cleanup_kstack`：`put_kstack(proc)` → `kfree(proc)` |

PID 分配、入哈希表、入进程链表和 `nr_process++` 放在 `local_intr_save` 临界区内，保证这组对共享链表/哈希表的修改是原子的；最后 `wakeup_proc(proc)` 把状态置为 `PROC_RUNNABLE`，等待调度器选中。

##### （5）`kernel_thread()`：伪造首次运行的 trapframe

RISC-V 没有 x86 那样用 `iret` 造栈的技巧，这里直接伪造一个 `trapframe`，让 `sret` 之后跳到 `kernel_thread_entry`，由它执行 `a0 = s1(arg)`、`jalr s0(fn)`：

```c
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;

    memset(&tf, 0, sizeof(struct trapframe));

    tf.gpr.s0 = (uintptr_t)fn;                        // kernel_thread_entry: jalr s0
    tf.gpr.s1 = (uintptr_t)arg;                       // kernel_thread_entry: a0 = s1
    tf.epc = (uintptr_t)kernel_thread_entry;          // sret 的落点
    // 从 S 态返回 S 态(SPP=1)，返回后开中断(SPIE=1)，当前关中断(SIE=0)
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;

    return do_fork(clone_flags, 0, &tf);
}
```

`tf.status` 三个状态位的作用：

- `SSTATUS_SPP = 1`：`sret` 返回后仍处于内核态（S 态）；
- `SSTATUS_SPIE = 1`：返回后按进入前的中断使能状态恢复；
- `SSTATUS_SIE = 0`：进入线程时先关中断，避免创建过程中被打断。

函数地址放 `s0`、参数放 `s1`，是与汇编入口 `kernel_thread_entry` 的约定：它先 `move a0, s1` 取参数，再 `jalr s0` 调函数。

##### （6）`proc_init()`：建立 0 号、1 号进程

1. `list_init(&proc_list)`，并初始化 `HASH_LIST_SIZE` 个 PID 哈希桶；
2. `alloc_proc()` 创建 `idleproc`：`pid = 0`、状态 `PROC_RUNNABLE`、`kstack = bootstack`、`need_resched = 1`，命名 `"idle"`，入进程链表和哈希表，`nr_process++`；
3. `current = idleproc`；
4. `kernel_thread(init_main, "Hello world!!", 0)` 创建 `initproc`；
5. `find_proc(pid)` 取回 `initproc`，命名 `"init"`。

`idleproc->need_resched = 1` 的作用：0 号进程进入 `cpu_idle` 循环后立即触发一次调度，把 CPU 让给刚创建的 `initproc`。

##### （7）`cpu_idle()` 与 `proc_run()`

`cpu_idle()` 是 0 号进程的主循环：没有其他事情可做时，只要 `current->need_resched` 为真就调用 `schedule()`：

```c
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}
```

`proc_run()` 在关中断的临界区内完成“改 `current`、切页表、切上下文”三步：

```c
void proc_run(struct proc_struct *proc)
{
    if (proc == current)
    {
        return;
    }

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        struct proc_struct *prev = current;

        current = proc;                                 // 更新当前进程
        proc->runs++;                                   // 运行次数 +1

        // 切换页表：satp = SV39 模式 | (页目录物理地址 >> 12)
        // 与 pmm.c 建立内核页表时的写法保持一致
        write_csr(satp, 0x8000000000000000ULL | ((uintptr_t)proc->pgdir >> RISCV_PGSHIFT));

        // 保存 prev 的上下文到 prev->context，并从 proc->context 恢复
        switch_to(&(prev->context), &(proc->context));
    }
    local_intr_restore(intr_flag);
}
```

要点：

- `proc == current` 时直接返回，避免自己切自己；
- 临界区保证 `current`、`satp`、上下文三者一致，不会在切换中途被中断打断；
- `satp` 写法与 `pmm.c` 建立内核页表时保持一致：模式位 `0x8000000000000000`（Sv39），页目录物理地址右移 `RISCV_PGSHIFT`；`proc_run()` 中不使用 `lsatp()`，因为现有 `libs/riscv.h` 中 `lsatp()` 的模式位对 64 位 Sv39 不正确；
- `switch_to()` 只保存/恢复 callee-saved 寄存器（`ra`、`sp`、`s0~s11`），其实现见 4.3 节。

新线程第一次被调度时的执行链：

```text
switch_to
  -> forkret
  -> forkrets
  -> __trapret
  -> sret
  -> kernel_thread_entry
  -> fn(arg)
```

`forkret` 由 `context.ra` 进入；`forkrets` 把新线程的 trapframe 作为栈指针；`__trapret` 恢复 CSR 和通用寄存器后执行 `sret`；最后 `kernel_thread_entry` 用 `s1` 作参数、`s0` 作函数地址调用 `fn`。

#### 4.2.4 Spec

```text
Pre-Condition:
  current 指向当前执行线程；
  kernel_thread 提供合法的函数入口和参数；
  alloc_pages、kmalloc 和进程链表初始化已完成；
  proc_list 与 PID 哈希表尚未初始化。

Post-Condition:
  idleproc 的 PID 为 0，状态为 PROC_RUNNABLE，kstack 为 bootstack；
  current 指向 idleproc；
  initproc 的 PID 为 1，状态为 PROC_RUNNABLE，parent 为 idleproc；
  initproc 已挂入 proc_list 和 PID 哈希表；
  initproc 的 tf 位于自己的内核栈顶部；
  initproc 的 context.ra 指向 forkret；
  initproc 的 context.sp 与 tf 指向其内核栈中的 Trap frame；
  kernel_thread 构造的 Trap frame 中 epc=kernel_thread_entry，s0=fn，s1=arg；
  proc_run 切换后 current、satp 与恢复的上下文三者一致；
  cpu_idle 在 need_resched 为 1 时调用 schedule()。

Failure:
  PCB 或内核栈分配失败时返回负错误码；
  失败路径必须按逆序释放已获得的内核栈与 PCB；
  不能把 pid=-1、未挂接或无 Trap frame 的进程设置为可运行；
  PID 分配、入链表和入哈希表必须在关中断的临界区内完成；
  proc_run 不能在切换中途让 current 与 satp 不一致；
  同一进程重复调用 proc_run 应直接返回。
```

#### 4.2.5 使用的 Prompt

对应 `report/prompt.md` 中的 **P04：实现进程创建与初始化**。

#### 4.2.6 验证状态

Process Create 测试由 `tools/inject.py` 注入到 `proc_init()` 末尾，发生在 `intr_enable()` 之前，因此不依赖调度器。测试断言包括：

- `idleproc`：`current == idleproc`、`pid == 0`、`state == PROC_RUNNABLE`、`kstack == bootstack`、`parent == NULL`、`mm == NULL`、`tf == NULL`、`runs == 0`、`flags == 0`、`context.ra == 0`；
- `initproc`：`pid == 1`、`state == PROC_RUNNABLE`、`parent == idleproc`、`mm == NULL`、`context.ra != 0`、`context.sp != 0`。

通过输出：

```text
[Test Process Create] PASS
check Process Create: OK
```

---

### 4.3 功能模块：进程调度与上下文切换

**负责人：**2413423-王乐妍（`schedule()` 初版与 `switch_to()` 实现）；刘国全协助完成调度链表回绕修复

**涉及文件：**

- `code/kern/schedule/sched.c`
- `code/kern/process/switch.S`
- `code/kern/process/proc.c`（`cpu_idle()`、`proc_run()` 由刘爽负责，实现见 4.2 节）

#### 4.3.1 功能说明

调度模块负责把已经创建出来的内核线程真正交给 CPU 执行，覆盖 `cpu_idle -> schedule -> proc_run -> switch_to` 完整链路。各层职责严格分层：

- `schedule()` 负责"选谁"：从 `proc_list` 中选出下一个 `PROC_RUNNABLE` 的线程；
- `proc_run()` 负责"怎么切过去"：关中断、更新 `current`、切换页表（写 `satp`）、调用 `switch_to()`（实现见 4.2 节）；
- `switch_to()` 负责"最底层的寄存器现场保存与恢复"，只做上下文搬运，不参与任何策略判断；
- `cpu_idle()` 是 idle 线程的执行主体，检测到 `need_resched` 时调用 `schedule()`。

新线程第一次运行之所以能走到正确入口，是因为 4.2 节已经把它的 `context.ra` 和 trapframe 预先组织成了 `forkret -> forkrets -> __trapret` 恢复链路（Trap 侧实现见 4.4.2 节）。

#### 4.3.2 核心实现

##### switch_to()（kern/process/switch.S）

```asm
switch_to:
    # a0 = from, a1 = to（RISC-V 调用约定）
    sd  ra, 0*REGBYTES(a0)     # 保存当前现场到 *from
    sd  sp, 1*REGBYTES(a0)
    sd  s0~s11, 2~13*REGBYTES(a0)
    ld  ra, 0*REGBYTES(a1)     # 从 *to 恢复目标现场
    ld  s0~s11, 2~13*REGBYTES(a1)
    ld  sp, 1*REGBYTES(a1)     # sp 最后恢复
    ret                        # 返回地址即 to->ra
```

实现要点：

1. **只保存 callee-saved 寄存器**：`ra`、`sp`、`s0~s11` 共 14 个，与 `struct context` 字段一一对应（`REGBYTES = 8`）。不保存 caller-saved 寄存器（`t0~t6`、`a0~a7`）——按 RISC-V 调用约定，普通函数调用边界上 C 编译器已经负责保存这些寄存器，`switch_to` 对 C 调用方而言就是一个普通函数。
2. **sp 必须最后恢复**：恢复过程中对 `*from` / `*to` 的所有访存都依赖 a0/a1 与当前 sp 寻址，一旦 sp 被替换，旧指针即失效。
3. **`ret` 的返回地址是刚载入的 `to->ra`**：线程被切回时从上次切出点继续执行；线程首次运行时 `ra` 指向 `forkret`（由 `copy_thread()` 预先设置），由此进入首次运行恢复链。
4. `switch_to` 不选择线程、不修改 `current`、不关开中断——这些由 `schedule()` 和 `proc_run()` 完成。

##### schedule()（kern/schedule/sched.c）

`schedule()` 的最终逻辑描述如下（完整代码见 `code/kern/schedule/sched.c`）：

1. 进入 `local_intr_save` 临界区后清除 `current->need_resched`，避免同一次调度请求被重复处理；
2. **起点规则**：`current != idleproc` 时从 `current->list_link` 的下一个链表项开始查找，`current == idleproc` 时从 `proc_list` 链表头开始；
3. **环形回绕**：起点恰好是链表头（即 current 是最后一个节点）时改从第一个节点开始；扫描过程中遇到链表头也绕回继续，直到回到起点，形成完整的环形遍历，保证任何位置的可运行线程都不会被跳过；
4. **选择条件**：`p != idleproc && p != current && p->state == PROC_RUNNABLE`——idleproc 只作为兜底，不允许它抢占真正的可运行线程，也不能重复选中当前线程；
5. **兜底规则**：整个环形遍历没有找到可运行线程时 `next` 保持 `idleproc`，不 panic、不解引用 NULL；
6. `next == current`（链表上只有当前线程可运行）时直接返回，避免无意义的自切换；
7. 找到目标后调用 `proc_run(next)`，选取与切换全程处于 `local_intr_save/restore` 临界区内（机制详见 Challenge 6.1）。

#### 4.3.3 实现迭代过程

本模块经历 2 次迭代：

##### 第一次迭代：schedule() 内重复计数 runs（跨模块职责冲突）

**遇到的问题：**AI 首次生成的 `schedule()` 在调用 `proc_run()` 之前自行执行了 `next->runs++`。单独审查 `schedule()` 的局部逻辑（起点规则、回退 idleproc、临界区保护、`next == current` 直接返回）全部正确，但对照分工文档发现 `proc->runs++` 已明确划给 `proc_run()`（分工 B5 第 4 步）。两处同时存在会使每次调度 `runs` 被加 2，若测试检查 `runs` 值将直接出错。

**问题解决策略：**在提示词的 `[SPECIFICATION]` 中显式声明"`runs++` 属于 `proc_run()` 的职责，`schedule()` 不得重复计数"，删除该行后重新生成提交（c74d392）。这是一次典型的**跨模块职责边界类 AI Bug**：单函数视角下完全正确的代码，只有对照模块间接口约定才能发现冲突。

##### 第二次迭代：调度链表不回绕、idleproc 被提前选中（负责人：刘国全）

**遇到的问题：**初版 `schedule()` 从当前线程的下一个节点开始查找，但扫描到链表尾后没有回到链表头——当 current 是链表最后一个节点时，起点直接落到链表头，循环一次都不执行，测试线程永远选不上；同时选择条件只判断 `PROC_RUNNABLE`，而 idleproc 本身在 `proc_list` 中且状态为 `PROC_RUNNABLE`，会被当成"真实可运行线程"提前选中。表现为 Schedule 测试失败，QEMU 只重复输出 `100 ticks`。

**问题解决策略：**将查找循环改为 `do { ... } while (le != start)` 的环形遍历，起点为链表头时绕回第一个节点；选择条件增加 `p != idleproc && p != current`；`need_resched` 清零移入临界区。修复集中在 `schedule()` 内部，未修改测试和评分脚本。

#### 4.3.4 Spec

```text
## switch_to
Pre-Condition:
- a0 = from，a1 = to，各指向一个合法的 struct context；
- to 指向的 context 中 ra 与 sp 已初始化：要么是目标线程上次被切出时保存的现场，
  要么是首次运行前设置的入口（ra 指向 forkret，sp 指向其内核栈）。

Post-Condition:
- from 所指内存已保存切换时刻的 ra、sp、s0~s11 共 14 个寄存器值，与 struct context 一一对应；
- CPU 的 ra、sp、s0~s11 已被 to 所指内存中的值替换；
- 通过 ret 返回，返回地址为 to->ra；
- 不修改其余通用寄存器，不访问全局变量，不关开中断（由调用方负责）。

Failure:
- 无（纯寄存器搬运，无失败路径）。

## schedule
Pre-Condition:
- current 指向当前正在运行的线程，不为 NULL；
- proc_list 中至少存在一个 PROC_RUNNABLE 的线程，或可回退到 idleproc。

Post-Condition:
- 在临界区内清除 current->need_resched；
- next 选取：从 current（current 为 idleproc 时从链表头）的下一项开始环形遍历，
  找第一个非 idle、非 current 且 PROC_RUNNABLE 的线程；查完一整圈无可运行线程时
  next = idleproc；
- next != current 时已调用 proc_run(next)；next == current 时直接返回；
- 选取与切换处于 local_intr_save/restore 之间，返回前中断状态已恢复；
- runs++ 由 proc_run() 负责，schedule() 不重复计数。

Failure:
- proc_list 为空或全部线程不可运行时，不 panic、不解引用 NULL，回退到 next = idleproc。
```

#### 4.3.5 使用的 Prompt

- **P05：实现调度与上下文切换**（初版，含 runs++ 职责修正）
- **P06：修复调度链表回绕与 idleproc 选择问题**（第二次迭代）
- **P07：调度测试与 GDB 验证**

#### 4.3.6 验证状态

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

Schedule 相关检查通过，`make grade` 最终结果 30/30（截图见 5.3 节 `04_make_grade_result.png`）。

---

### 4.4 Lab2/Lab3 依赖代码迁移

#### 4.4.1 Lab2 PMM 迁移

**负责人：**2411822-刘国全

Lab4 的初始骨架只有 `default_pmm.c`，没有独立的 Best-Fit 实现文件，且 `pmm.c` 原先引用 `default_pmm_manager`。为避免把 Best-Fit 算法和默认文件混在一起，新增独立的 `best_fit_pmm.c` / `best_fit_pmm.h`，并将 `pmm.c` 切换到 `best_fit_pmm_manager`。Best-Fit 管理器仍复用骨架提供的 `best_fit_check()`：

```c
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};
```

`page_init()` 根据 DTB 获取物理内存范围，在 kernel `end` 后建立 `Page` 描述符数组，将固件、内核和描述符区域标记为保留，再把剩余完整物理页交给 PMM。

#### 4.4.2 Lab3 Trap 迁移

**负责人：**2413423-王乐妍

**涉及文件：**

- `code/kern/trap/trapentry.S`
- `code/kern/trap/trap.c`

Lab4 的进程首次运行与调度触发都建立在 Lab3 的 Trap 框架之上：

1. `kernel_thread()` 构造的临时 trapframe，要经过 `forkrets -> __trapret -> sret` 才能落到 `kernel_thread_entry` 开始执行——没有这条恢复链，新线程永远无法"第一次跑起来"；
2. 时钟中断链路必须完好，调度器才有可靠的周期性触发源。

按练习要求，本模块不重新设计提示词，而是把 Lab3 已实现并通过测试的代码回填到 Lab4 框架中标有 `lab3` 注释的位置（迁移用提示词见 `prompt.md` P02）。代码沿用 Lab3 各成员的实现（trapentry.S 汇编部分在 Lab3 由刘国全完成、`interrupt_handler()` 由刘爽完成、`exception_handler()` 由王乐妍完成），Lab4 中的统一回填与验证由王乐妍负责。

##### SAVE_ALL / RESTORE_ALL 宏（trapentry.S）

- **SAVE_ALL（框架已有）**：先把中断前的 sp 存入 `sscratch`，腾出 `36 * REGBYTES` 的 trapframe；保存 `x1`、`x3~x31`（`x0` 恒为 0 不存，槽 2 留给原 sp）；再用 `s0~s4` 作中转，把 `sscratch`（原 sp）、`sstatus`、`sepc`、`sbadaddr`（CSR 0x143）、`scause` 五个特殊值写入帧内固定槽位（槽 2、32~35）。
- **RESTORE_ALL（本次回填）**：顺序与 SAVE_ALL 相反——先恢复 CSR（`sscratch/sstatus/sepc/sbadaddr/scause`），再恢复通用寄存器，sp（x2）最后恢复。**必须先 CSR 后 GPR**：恢复通用寄存器要用 `s0~s4` 做中转，若先恢复它们会破坏中转值；**sp 必须最后恢复**：帧内所有槽位都通过它寻址。

##### __alltraps / __trapret / forkrets

```asm
__alltraps:
    SAVE_ALL
    mv  a0, sp        # trapframe 指针作为参数
    jal trap
    j   __trapret
__trapret:
    RESTORE_ALL
    sret
forkrets:
    move sp, a0       # 栈切到新进程内核栈顶部的 trapframe
    j   __trapret
```

`forkrets` 把栈切换到新进程的 trapframe 后跳入 `__trapret`，这正是新线程第一次运行链 `forkret -> forkrets -> __trapret -> kernel_thread_entry` 的后半段——`sret` 时 `sepc` 已被 `kernel_thread()` 设为 `kernel_thread_entry`，于是新线程从自己的入口函数开始执行。

##### interrupt_handler()（trap.c）

- `IRQ_S_TIMER`：`ticks++`；每 `TICK_NUM`（=100）次打印一行统计；调用 `clock_set_next_event()` **重新武装下一次时钟中断**——若遗漏此步，只会收到一次中断，调度将失去周期性触发源；
- `IRQ_S_SOFT`：打印提示；
- 未知中断：打印 trapframe 兜底。

##### exception_handler()（trap.c）

- 非法指令与断点异常：打印提示，并用 `trap_insn_len()` 依据指令编码低两位判断真实长度（低两位为 0x3 → 32 位指令 4 字节；否则为 RVC 压缩指令 2 字节）前进 `tf->epc`，保证 `sret` 返回后不会重复执行同一条故障指令；
- 其余异常：`print_trapframe()` 兜底，不改变执行位置。

##### Spec

```text
Pre-Condition:
- idt_init() 已将 stvec 指向 __alltraps，sscratch 置 0（表示来自内核态）。

Post-Condition:
- 内核能正常接收时钟中断，且每次处理后下一次中断已被重新武装；
- forkrets 能以新进程 trapframe 为栈完成恢复，__trapret 正确执行 sret；
- 异常处理只前进 epc、不破坏已保存上下文。

Failure:
- 未知 cause 一律走 print_trapframe() 兜底，不 panic。
```

#### 4.4.3 验证状态

依赖代码并入后，内核能够正常启动，PMM、VMM、进程创建和调度测试均通过。

---

## 五、测试与验证

### 5.1 测试环境说明

测试在 WSL2 的 `~/OS2026/code` 目录下执行：

```bash
make clean
make
make grade
```

当前最终结果：

```text
-check PMM:                                OK
-check Process Create:                     OK
-check Schedule:                           OK
Total Score: 30/30
```

### 5.2 测试矩阵

| 测试 | 验证内容 | 结果 |
|------|----------|------|
| PMM | `page_insert()`、`page_remove()`、引用计数、页表项和 TLB | PASS |
| Process Create | `idleproc`、`initproc`、PCB 字段、内核栈和上下文 | PASS |
| Schedule | `schedule()`、`proc_run()`、`switch_to()` 和多线程运行 | PASS |

### 5.3 测试截图

以下截图按实际执行顺序保留了 `make grade` 的构建、编译警告、链接输出和最终评分。编译警告来自测试框架的参数类型和 `kmalloc.c` 中原有代码，不影响最终测试结果；链接器输出是 `--gc-sections` 删除未使用 section 的正常信息。

![执行 make grade 并开始构建](./images/01_make_grade_start.png)

![编译阶段 warning 详情](./images/02_compile_warning.png)

![链接器输出](./images/03_linker_output.png)

![最终测试结果：30/30](./images/04_make_grade_result.png)

### 5.4 测试结果分析

1. PMM 测试通过，说明物理页管理、页表项更新、引用计数和 TLB 失效逻辑正确。
2. Process Create 测试通过，说明 `idleproc` 和 `initproc` 的 PCB 字段、内核栈和首次运行上下文设置正确。
3. Schedule 测试通过，说明调度器能够找到可运行线程，并且 `proc_run()` 和 `switch_to()` 能完成上下文切换。
4. 调度修复前总分为 20/30；补全环形扫描逻辑后达到 30/30。

---

## 六、Challenge

### 6.1 `local_intr_save/restore` 如何实现开关中断

**负责人：**2413423-王乐妍

#### 1. 开关中断的实现机制

```c
// kern/sync/sync.h
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();          // clear_csr(sstatus, SSTATUS_SIE)
        return 1;
    }
    return 0;
}
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();           // set_csr(sstatus, SSTATUS_SIE)
    }
}
#define local_intr_save(x)    do { x = __intr_save(); } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

`local_intr_save(x)` 读取 `sstatus` 的 `SIE` 位并保存"快照"到 `x`：中断原本开启，则用 `clear_csr` 关闭中断并记 `x = 1`；原本就是关闭的，则只记 `x = 0`，完全不碰硬件。`local_intr_restore(x)` 按快照恢复：`x = 1` 才用 `set_csr` 重新开中断，`x = 0` 时什么都不做。

所以这对宏的本质**不是"关中断/开中断"，而是"保存中断状态快照 + 关中断 / 按快照恢复"**，保证临界区结束后中断状态与进入前完全一致。

#### 2. `do { } while (0)` 的作用

- 把多条语句包装成一个语法上的"单语句"，使宏在**未加花括号的 if/else 分支**中展开也安全；否则展开出的多条语句会让 `else` 悬挂到错误位置，导致编译错误或逻辑错误。
- `while (0)` 的循环体只执行一次，不产生任何运行时开销，编译器会直接优化掉。
- 让宏调用处能像函数调用一样以分号结尾（`local_intr_save(flag);`），避免展开后多余或缺失分号带来的语法问题。

#### 3. 为什么不能无条件 `intr_enable()`

- **临界区可能嵌套**：外层已关中断时进入内层临界区，`__intr_save` 记录 `x = 0`；若内层退出时无条件开中断，外层临界区剩余执行期间就暴露在中断之下，互斥保护被破坏。
- **上下文可能本来就关中断**：比如异常处理早期中断尚未开启，退出临界区时不应擅自改变全局中断状态。
- 因此正确的语义是"**恢复到进入前的状态**"而不是"打开中断"——这正是 `__intr_save()` 返回值存在的意义。

### 6.2 `sv32`、`sv39`、`sv48` 的异同

三者都是分页模式，虚拟地址和物理地址被划分为若干级页表索引加页内偏移：

| 模式 | 虚拟地址宽度 | 页表级数 | 每级索引位数 | 页大小 |
|------|--------------|----------|--------------|--------|
| Sv32 | 32 位 | 2 | 10 + 10 | 4 KiB |
| Sv39 | 39 位 | 3 | 9 + 9 + 9 | 4 KiB |
| Sv48 | 48 位 | 4 | 9 + 9 + 9 + 9 | 4 KiB |

Sv32 的 PTE 为 4 字节；Sv39/Sv48 的 PTE 通常为 8 字节。三者的中间层查找逻辑相似，因此 `get_pte()` 中多次出现形式相近的代码。

### 6.3 `get_pte()` 是否应该拆分

当前 `get_pte()` 同时承担“查找页表项”和“按需分配中间页表”的职责。

优点：

- 调用者只需一次调用就能获得可写的页表项；
- 简化 `page_insert()` 等上层接口。

缺点：

- 查找和分配耦合，调用者无法只查询而不产生分配；
- 失败路径和资源管理更复杂；
- 调试时较难区分“页表不存在”和“页表项不存在”。

可以拆分为：

```c
pte_t *walk_pte(pde_t *pgdir, uintptr_t la);
int ensure_pte(pde_t *pgdir, uintptr_t la, pte_t **ptep);
```

前者只负责查找，后者负责在需要时分配中间页表，职责更清晰。

---

## 七、实验总结与收获

### 7.1 对操作系统的理解

| 本实验知识点 | 对应 OS 原理 |
|--------------|--------------|
| 多级页表和 PTE | 虚拟地址到物理地址的翻译 |
| `page_insert/page_remove` | 映射建立、撤销和引用计数 |
| TLB 刷新 | 页表更新后的地址翻译一致性 |
| PCB 和进程状态 | 进程/线程的管理实体 |
| 内核栈 | 内核执行流和 Trap frame 的保存位置 |
| `struct context` | 进程切换时保存的最小 CPU 上下文 |
| `switch_to` | callee-saved 寄存器的保存与恢复 |
| 进程链表和调度 | 就绪队列与 CPU 分配 |
| `local_intr_save/restore` | 单核临界区保护 |

### 7.2 AI 协作开发经验

1. **先约定数据结构，再生成代码。** `struct proc_struct`、`struct context` 和 `struct trapframe` 决定字段偏移和调用链，必须与已有汇编严格一致。
2. **Prompt 要写出失败路径。** `page_insert()` 中 `get_pte()` 失败、替换旧页、重复映射等边界情况必须在 Spec 中提前说明。
3. **测试失败要结合调度路径分析。** 本次 Schedule 失败并非上下文切换本身错误，而是调度链表没有回绕，导致 `initproc` 创建出的测试线程一直没有被选中。
4. **编译通过不等于功能正确。** `switch_to` 和 `schedule` 都能编译，但只有实际运行测试才能发现调度顺序问题。
5. **不能修改测试函数来迎合评分。** 修复应集中在 `schedule()`，保持测试和评分脚本不变。

### 7.3 AI 生成 Bug 与解决过程

#### Bug 1：Best-Fit 实现与默认 PMM 文件混在一起

- 现象：初始骨架只有 `default_pmm.c`，Best-Fit 管理器与默认文件职责混用。
- 原因：骨架没有独立的 `best_fit_pmm.c` / `best_fit_pmm.h`，而 `pmm.c` 仍引用 `default_pmm_manager`。
- 解决：新增独立的 `best_fit_pmm.c` / `best_fit_pmm.h`，在新的文件中导出 `best_fit_pmm_manager`，并把 `pmm.c` 的初始化切换到该管理器。

#### Bug 2：调度器不回到链表头

- 现象：PMM 和 Process Create 通过，Schedule 失败，QEMU 只重复输出 `100 ticks`。
- 原因：`schedule()` 扫描到链表尾后停止，且过早选择 `idleproc`，导致测试线程没有机会执行。
- 解决：实现环形链表扫描，跳过 idleproc 和 current，找不到其他线程时才回退到 idleproc。

#### Bug 3：PCB 未初始化字段风险

- 现象：进程创建对字段要求严格，未初始化字段可能让调度器或测试读到随机值。
- 解决：`alloc_proc()` 对状态、PID、父进程、上下文、Trap frame、页目录、名称和链表节点全部显式初始化。

#### Bug 4：首次运行链路难以直接调试

- 现象：新线程创建成功，但不能直接确认其首次执行路径。
- 解决：通过 GDB 逐步验证：

```text
schedule -> proc_run -> switch_to -> forkret -> forkrets
         -> __trapret -> kernel_thread_entry -> init_main
```

最终确认 `initproc` 能正常输出：

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

### 7.4 本实验未覆盖的重要 OS 知识点

- 用户进程独立地址空间的复制与隔离；
- 缺页异常和按需分页；
- 文件描述符和进程资源管理；
- 完整的进程退出、僵尸进程回收和父进程等待；
- 多核调度、锁、内存序和跨核 TLB shootdown；
- 抢占式调度和时间片轮转；
- 用户态与内核态之间的系统调用参数检查。

---

## 八、提交检查

- [x] Lab2 依赖代码已迁移
- [x] Lab3 依赖代码已迁移
- [x] `page_insert()` 和 `page_remove()` 已实现
- [x] 进程创建相关函数已实现
- [x] 调度与上下文切换已实现
- [x] `make grade` 通过，得分 30/30
- [x] 报告和 Prompt 完成
- [x] 测试截图已放入 `report/images/`
- [x] 代码中的 `YOUR CODE` 已替换为学号
