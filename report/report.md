# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab5：用户程序 |
| **小组成员** | 2411822-刘国全、2411350-刘爽、2413423-王乐妍 |
| **完成日期** | 2026-11-17 |

### 小组分工

| 成员 | 负责的练习/模块 |
|------|----------------|
| 刘国全 | Lab2 PMM 与 Lab4 页表迁移、`copy_range()`、`dup_mmap()`、`copy_mm()`、`do_fork()` |
| 刘爽 | `alloc_proc`、`kernel_thread`、`init_main`/`user_main`、`kernel_execve`、`do_execve`、`load_icode`、`do_wait`、`do_exit`、`do_yield`、`do_kill` |
| 王乐妍 | 系统调用分发与 `sys_*`、U/S 双向 Trap 返回、Lab3/Lab4 Trap 与调度迁移、GDB 调试 |

> 本文件为 Lab5 初始报告模板，代码和测试完成后请补充分工、实现过程、Prompt、Bug 记录和测试截图。

---

## 一、实验目的

结合 Lab5 指导书填写本实验的目标、涉及的核心机制和需要验证的行为。

---

## 二、实验环境

| 项目 | 内容 |
|------|------|
| 操作系统 | WSL2 Ubuntu |
| 目标架构 | RISC-V 64 |
| 交叉编译器 | `riscv64-unknown-elf-gcc` |
| 模拟器 | QEMU |
| 构建工具 | GNU Make、Python 3 |
| 调试工具 | GDB |
| 版本控制 | Git |

### AI 工具配置

| 成员 | AI 编程工具 | 底层模型 | 配置 |
|------|------------|---------|------|
| 刘国全 | 待填写 | 待填写 | 待填写 |
| 刘爽 | 待填写 | 待填写 | 待填写 |
| 王乐妍 | 待填写 | 待填写 | 待填写 |

---

## 三、实验整体逻辑分析

### 3.1 本章逻辑主线

待填写。需要说明 Lab5 的功能是如何按顺序实现的，以及各模块之间的调用关系。

### 3.2 功能的逐步实现

待填写。

---

## 四、实验内容与实现

### 4.1 功能模块：用户进程创建与初始化

**负责人：**2411350-刘爽

#### 功能说明

本模块负责把 Lab4 的“内核线程”扩展成“内核线程 + 用户进程”的双层模型，并建立上电后的第一条进程链：

```text
proc_init
  -> alloc_proc + kernel_thread        : 造出 idleproc(pid 0)
  -> kernel_thread(init_main)          : 造出 initproc(pid 1)
init_main
  -> kernel_thread(user_main)          : 造出即将 execve 的内核线程
  -> do_wait(0, NULL) 循环             : 等所有用户进程退出
```

`alloc_proc` 在 Lab4 的 14 个字段基础上补齐 Lab5 新增的 `exit_code`、`wait_state`、`cptr/yptr/optr`，否则 `do_wait` 遍历子进程链与 `do_exit` 过继子进程时会读到未初始化指针。`kernel_thread` 只负责把 `fn/arg` 塞进 `s0/s1`、把 `epc` 指向 `kernel_thread_entry`；真正的“首次运行现场”由 `copy_thread` 构造；`proc_init` 按“先 idle 后 init”的顺序建立 0/1 号进程。

#### 核心实现

| 函数 | 关键动作 | 易错点 |
|---|---|---|
| `alloc_proc` | `kmalloc` 一个 PCB，逐字段置初值，`pgdir = boot_pgdir_pa` | `tf = NULL`、`mm = NULL` 是内核线程的合法初始状态，`idleproc` 必须保持 `tf == NULL` |
| `kernel_thread` | 构造临时 `trapframe`：`s0 = fn`、`s1 = arg`、`epc = kernel_thread_entry`、`status = SPP|SPIE` 且清 `SIE` | 不能把 `fn` 直接写进 `ra`，内核线程入口是汇编 `kernel_thread_entry` |
| `copy_thread` | `tf` 放在 `kstack + KSTACKSIZE - sizeof(trapframe)`；`tf->gpr.a0 = 0`；`context.ra = forkret`、`context.sp = tf` | `sp` 仅在 `esp == 0` 时用 `tf`，否则用调用方给的 `esp`（用户进程 execve 后要用 USTACKTOP） |
| `proc_init` | 初始化 `proc_list` 与 1024 个哈希桶；`idleproc` 复用 `bootstack`；`need_resched = 1` | `idleproc` 的 `parent == NULL`，**不能**调 `set_links`（内部会解引用 `parent->cptr`），只能 `list_add + nr_process++` |
| `init_main` | 打印 initproc 信息；`kernel_thread(user_main)`；`while (do_wait(0,NULL) == 0) schedule();` 后 `panic("init check memory pass.")` | `do_wait` 是“回收一个就返回 0”，必须循环，否则只收一个孩子就 panic |

内核栈与 trapframe 的布局（`copy_thread` 的直接结果）：

```text
kstack + KSTACKSIZE  ┌──────────────────────────┐
                     │ trapframe (36 * 8 = 288B)│  <- proc->tf
                     │  epc = kernel_thread_entry│
                     │  gpr.s0 = fn, gpr.s1 = arg│
                     │  gpr.sp = tf (或 esp)     │
kstack + KSTACKSIZE-288 └───────────────────────┘
                     │  ... 向下生长的内核栈 ... │
kstack               └──────────────────────────┘
```

#### Spec

```text
Pre-Condition:
  proc_list 与 hash_list 未初始化；boot_pgdir_pa / bootstack 已由 pmm_init 准备好。

Post-Condition:
  idleproc: pid = 0, state = PROC_RUNNABLE, kstack = bootstack, mm = NULL, tf = NULL,
            parent = NULL, runs = 0, flags = 0, context.ra = 0
  initproc: pid = 1, state = PROC_RUNNABLE, parent = idleproc,
            context.ra (= forkret) 与 context.sp (= tf) 均非 0
  current = idleproc，且 idleproc->need_resched = 1，可被 schedule 让出 CPU

Failure:
  alloc_proc 失败        -> panic("cannot alloc idleproc")
  kernel_thread 返回 <=0 -> panic("create init_main failed")
```

#### 使用的 Prompt

P03（见 `report/prompt.md`），覆盖 `alloc_proc` / `kernel_thread` / `copy_thread` / `proc_init` / `init_main`。

#### 验证状态

`tools/inject.py` 会在 `proc_init` 末尾注入 `test_lab5_process_create()`，待三个模块合并后由 `make grade` 验证：

```text
[Test Process Create] PASS
```

### 4.2 功能模块：系统调用实现

**负责人：**待填写

#### 功能说明

待填写。

#### 核心实现

待填写。

#### Spec

```text
Pre-Condition:
待填写

Post-Condition:
待填写

Failure:
待填写
```

#### 使用的 Prompt

待填写，最终内容同步到 `report/prompt.md`。

#### 验证状态

待测试。
### 4.3 功能模块：第一次进入用户态

**负责人：**2411350-刘爽

#### 功能说明

本模块回答一个核心问题：一个**内核线程**如何在不“返回”的情况下变成**用户进程**。链路为：

```text
user_main (内核线程, S 态)
  -> KERNEL_EXECVE(hello)
  -> kernel_execve
  -> do_execve      : 拆旧 mm -> 建新 mm -> load_icode -> 更新 mm/pgdir/name
  -> load_icode     : 建 VMA / 分配物理页 / 拷 ELF 段 / 清 bss / 建用户栈 / 改 tf
  -> 跳 __trapret   : sp 指向 current->tf
  -> sret           : SPP = 0 -> 回到 U 态，epc = e_entry
```

关键点在于 `kernel_execve` **不能写成普通函数返回**：它做完 `do_execve` 后，把 `sp` 指到已经改造成“用户态现场”的 `current->tf`，然后 `j __trapret`，复用 Lab3/Lab4 的中断返回路径完成特权级切换。这样“首次进入用户态”和“每次系统调用返回用户态”走的是同一段汇编，不需要再写一份特殊的切换代码。

`load_icode` 负责把 ELF 段变成真正的用户地址空间：每个 `PT_LOAD` 段先 `mm_map` 建 VMA（长度用 `p_memsz`，把 bss 一并圈进去），再逐页 `alloc_page + page_insert`，`filesz` 区间从 ELF 拷贝、`filesz~memsz` 区间保持清零；最后在 `USTACKTOP - USTACKSIZE` 建 256 页用户栈，并重置 trapframe。

用户程序是如何被“预先加载”的（思考题）：Makefile 在链接内核时用 `--format=binary` 把 `obj/user/*.out` 直接当成目标文件塞进内核镜像，符号名形如 `_binary_obj___user_hello_out_start`。于是用户二进制在**编译期**就与内核同处一个镜像，QEMU 加载内核时它随内核一起进内存；`load_icode` 并不“读文件”，只是把这段已在内核镜像里的字节复制到新 `mm` 的物理页。常规 OS 则是在 exec 时按路径查文件系统、从磁盘/页缓存读 ELF 再映射。实验这样做的原因是：Lab5 之前还没有文件系统和块设备驱动，若要求 exec 读磁盘，第一个用户进程将无法启动；代价是所有用户程序体积都被计入内核镜像，且无法动态替换，属于实验环境的简化。

#### 核心实现

| 函数 | 关键动作 | 易错点 |
|---|---|---|
| `user_main` | `#ifdef TEST` 走 `KERNEL_EXECVE2`，否则 `KERNEL_EXECVE(hello)` | 宏展开后若 execve 失败必须 `panic`，不能 return，否则会掉回 `kernel_thread_entry` 再 `do_exit` |
| `kernel_execve` | `do_execve(name, strlen(name), binary, size)`；成功后 `mv sp, tf; j __trapret` | 不能 `return 0`；`tf` 必须指向内核栈顶那个 `current->tf` |
| `do_execve` | ①切回内核页表后释放旧 mm（`lsatp(boot_pgdir_pa)` → `set_mm_count=0` → `exit_mmap` → `put_pgdir` → `mm_destroy`）②`mm_create + setup_pgdir` ③`load_icode` ④`current->mm = new_mm`、`current->pgdir = PADDR(new_mm->pgdir)`、`lsatp + sfence.vma` 切到新页表、`set_proc_name` | `exit_mmap` 内有 `assert(mm_count == 0)`，必须先清零；PCB 存的是**物理**页目录地址，`mm->pgdir` 是内核虚拟地址，必须 `PADDR`；换页目录后 satp 不会自动更新，不 `lsatp` 就会用内核页表去跑用户地址 |
| `load_icode` | 校验 `ELF_MAGIC`/`e_phoff`；逐 `PT_LOAD` 段 `mm_map` + 逐页装入；清 bss；建 256 页用户栈；重置 `tf` | `tf->status = (sstatus & ~SPP) | SPIE`：SPP=0 回 U 态，SPIE=1 返回后开中断，SIE 保持 0 |

`load_icode` 的循环骨架（只列关键分支）：

```c
for (每个 proghdr) {
    if (p_type != ELF_PT_LOAD) continue;
    mm_map(mm, p_va, p_memsz, vm_flags, NULL);           // VMA 覆盖 bss
    for (va = p_va; va < p_va + p_memsz; va += PGSIZE) { // 一页一页装
        page = alloc_page(); memset(page2kva(page), 0, PGSIZE);
        if (off < file_end) memcpy(...);                 // 文件部分
        page_insert(mm->pgdir, page, va, perm);          // perm 来自 p_flags
    }
}
mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, VM_READ|VM_WRITE|VM_EXEC, NULL);
for (i = 0; i < USTACKPAGE; i++) { ... USTACKTOP - (i+1)*PGSIZE ... }
tf->epc = elf->e_entry; tf->gpr.sp = USTACKTOP;
```

#### Spec

```text
Pre-Condition:
  current 是一个内核线程（mm == NULL，tf 指向内核栈上的 trapframe）；
  binary 是内核镜像内一段合法 ELF，size 为其字节长度。

Post-Condition:
  current->mm 指向新 mm，current->pgdir = PADDR(mm->pgdir)；
  current->name 为程序名；
  current->tf->epc = e_entry、gpr.sp = USTACKTOP、status.SPP = 0、status.SPIE = 1；
  kernel_execve 不返回，CPU 经 __trapret + sret 进入 U 态执行用户代码。

Failure:
  e_magic != ELF_MAGIC / e_phnum == 0 / p_filesz > p_memsz -> -E_INVAL_ELF
  mm_create 或 setup_pgdir 失败                             -> -E_NO_MEM（并清理已建对象）
  alloc_page 或 page_insert 失败                            -> -E_NO_MEM（旧 mm 已释放，进程由调用方决定退出）
```

#### 使用的 Prompt

P03（见 `report/prompt.md`），覆盖 `user_main` / `kernel_execve` / `do_execve` / `load_icode`。

#### 验证状态

待三模块合并后运行 `make grade`，预期：

```text
kernel_execve: pid = 2, name = "hello".
Hello world!!.
I am process 2.
hello pass.
```

### 4.4 功能模块：进程退出处理

**负责人：**2411350-刘爽

#### 功能说明

本模块完成进程生命周期的后半段：`do_exit` 负责“自己退场”，`do_wait` 负责“父进程收尸”，`do_yield` 负责主动让出，`do_kill` 负责打上退出标记。

这套设计的关键是**退出与回收分离**：进程退出时不能顺手把自己的 PCB 也释放掉，因为父进程可能还想读它的退出码；于是 `do_exit` 只把自己变成 `PROC_ZOMBIE`，真正的 `kfree` 必须由父进程在 `do_wait` 里完成。同时为避免父进程先死导致孤儿进程无人回收，`do_exit` 会把自己所有孩子过继给 `initproc`（1 号进程常驻，负责兜底）。

```text
子进程 do_exit
  -> 释放 mm / pgdir
  -> flags |= PF_EXITING, exit_code = error_code
  -> 孩子过继给 initproc（若孩子已是僵尸则顺便唤醒 initproc）
  -> 父进程若睡在 WT_CHILD 上则唤醒它
  -> state = PROC_ZOMBIE; schedule();   // 永不返回

父进程 do_wait(pid, &code)
  -> 有僵尸孩子 : unhash -> remove_links -> put_kstack -> kfree，写回退出码
  -> 有活孩子   : state = PROC_SLEEPING, wait_state = WT_CHILD, schedule()
  -> 没孩子     : -E_BAD_PROC
```

#### 核心实现

| 函数 | 关键动作 | 易错点 |
|---|---|---|
| `do_exit` | 拒绝对 `idleproc/initproc` 调用；切回内核页表后释放 mm（`lsatp(boot_pgdir_pa)` → `set_mm_count=0` → `exit_mmap` → `put_pgdir` → `mm_destroy`）；`PF_EXITING`；过继孩子；唤醒父进程；置 `PROC_ZOMBIE`；`schedule()` | 必须先存 `exit_code` 再变僵尸；`parent` 可能为 `NULL`（idleproc 的孩子），判空；`wakeup_proc` 只能唤醒休眠进程；释放页目录前必须先把 satp 切回内核页表 |
| `do_wait` | 先校验 `code_store` 是用户可写地址；在 `proc_list` 里筛 `parent == current` 的孩子；僵尸则四步回收；有活孩子则睡 `WT_CHILD` 后 `goto repeat` | 回收时 `kfree` 之后再读 `pid/exit_code` 就是 use-after-free，必须先存到栈上；`copy_to_user` 要放在中断恢复之后 |
| `do_yield` | `current->need_resched = 1;` 然后 `schedule();` | Lab4 的 `trap.c` 定时器不置 `need_resched`，主动让出必须自己置位，否则 `schedule` 立刻返回 |
| `do_kill` | `find_proc(pid)` 校验；置 `PF_EXITING`；若目标是 `PROC_SLEEPING` 则唤醒 | 对已 `PF_EXITING` 或非法 pid 返回 `-E_INVAL`；测试要求 `do_kill(-1) == -E_INVAL` |

`do_wait` 的回收四步（顺序固定）：

```c
pid_ret = zombie->pid; exit_code = zombie->exit_code;  // 先存关键值
unhash_proc(zombie);      // 1. 从 pid 哈希表摘除，find_proc 立刻查不到
remove_links(zombie);     // 2. 从 proc_list 与兄弟链摘除，nr_process--
put_kstack(zombie);       // 3. 归还 2 页内核栈
kfree(zombie);            // 4. 归还 PCB 本身
```

#### Spec

```text
Pre-Condition:
  do_exit: current != idleproc 且 current != initproc。
  do_wait: current 至少可能拥有子进程；code_store 若非空必须是用户可写地址。

Post-Condition:
  do_exit: current->state == PROC_ZOMBIE，mm == NULL，pgdir == boot_pgdir_pa，
           exit_code 已写入；孩子 parent 均指向 initproc；父进程被唤醒；不再返回。
  do_wait: 目标僵尸孩子已从哈希表/进程链表/内核栈/PCB 中彻底释放，
           *code_store 得到退出码，返回值为该孩子 pid。
  do_yield: current->need_resched == 1，随后 schedule()。
  do_kill: 目标 proc->flags 含 PF_EXITING，返回 0。

Failure:
  do_wait 无子进程                      -> -E_BAD_PROC
  do_wait code_store 非法               -> -E_INVAL
  do_kill pid 非法或目标已在退出中      -> -E_INVAL
  do_exit 被唤醒后发现 PF_EXITING       -> do_exit(-E_KILLED)
```

#### 使用的 Prompt

P05（见 `report/prompt.md`），覆盖 `do_wait` / `do_exit` / `do_yield` / `do_kill`。

#### 验证状态

待三模块合并后运行 `make grade`，预期：

```text
[Test Process Control] PASS
exit pass.
spin may pass.
forktest pass.
```

### 4.5 功能模块：GDB 调试系统调用与返回

**负责人：**待填写

#### 功能说明

待填写。

#### 核心实现

待填写。

#### Spec

```text
Pre-Condition:
待填写

Post-Condition:
待填写

Failure:
待填写
```

#### 使用的 Prompt

待填写，最终内容同步到 `report/prompt.md`。

#### 验证状态

待测试。

---

## 五、测试与验证

### 5.1 测试环境说明

待填写。

### 5.2 测试矩阵

| 测试 | 验证内容 | 结果 |
|------|----------|------|
| 待填写 | 待填写 | 待测试 |

### 5.3 测试截图

待补充截图到 `report/images/`。

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

待填写。

### 6.2 AI 协作开发经验

待填写。

### 6.3 本实验涉及的重要知识点

待填写。

### 6.4 本实验未覆盖的重要 OS 知识点

待填写。

---

## 七、提交检查

- [ ] 代码中的 `YOUR CODE` 已替换为学号
- [ ] 所有实验要求函数已实现
- [ ] `make grade` 通过
- [ ] `report/report.md` 完成
- [ ] `report/prompt.md` 完成
- [ ] 测试截图已放入 `report/images/`
