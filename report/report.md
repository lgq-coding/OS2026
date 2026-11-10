# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab4：进程管理 |
| **小组成员** | 2411822-刘国全、2411350-刘爽、2413423-王乐妍 |
| **完成日期** | 2026-11-10 |

### 小组分工

| 成员 | 负责的练习/模块 |
|------|----------------|
| 刘国全 | 待划分 |
| 刘爽 | 待划分 |
| 王乐妍 | 待划分 |

> 本文件为 Lab4 初始报告模板。完成代码和测试后，请补充分工、实现过程、Prompt、缺陷分析与测试截图。

---

## 一、实验目的

1. 理解虚拟内存中页表项维护、物理页映射和引用计数的关系。
2. 掌握 `page_insert()`、`page_remove()` 等页表操作函数的实现。
3. 理解进程控制块、内核栈、Trap frame 和 CPU 上下文之间的关系。
4. 掌握内核线程的创建、初始化、首次运行和上下文切换流程。
5. 理解调度器如何选择下一个可运行线程，以及 `proc_run()` 与 `switch_to()` 的协作。
6. 使用 AI 辅助完成代码设计、审查、调试和验证，并记录完整 Prompt。

---

## 二、实验环境

| 项目 | 内容 |
|------|------|
| 操作系统 | WSL2 Ubuntu |
| 目标架构 | RISC-V 64 |
| 交叉编译器 | `riscv64-unknown-elf-gcc` |
| 模拟器 | QEMU |
| 构建工具 | GNU Make、Python 3 |
| 调试工具 | `riscv64-unknown-elf-gdb` |
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

本实验从已经具备物理内存管理和 Trap 处理能力的内核出发，进一步建立虚拟内存映射、进程控制块、内核线程和进程调度能力。

```text
kern_init
  -> pmm_init
  -> pic_init / idt_init
  -> vmm_init
  -> proc_init
  -> cpu_idle
  -> schedule
  -> proc_run
  -> switch_to
```

需要说明：

1. 地址空间如何从物理页管理扩展到多级页表映射；
2. 进程控制块如何描述一个线程；
3. `idleproc` 和 `initproc` 如何创建；
4. 新线程第一次运行时如何通过 `forkret -> forkrets -> __trapret` 进入用户函数；
5. 调度器如何选择线程并完成上下文切换。

### 3.2 功能的逐步实现

1. **完成 Lab2/Lab3 依赖代码迁移。** 将 `page_init()`、物理页分配器和 Trap 处理代码填入标有 `lab2`/`lab3` 注释的位置。
2. **实现虚拟内存页操作。** 完成 `page_insert()`、`page_remove()`，保证页表项、引用计数和 TLB 状态一致。
3. **实现进程创建。** 完成 `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()`、`copy_mm()`。
4. **实现进程调度。** 完成 `cpu_idle()`、`schedule()`、`proc_run()` 和 `switch_to()`。
5. **运行测试。** 验证 PMM、Process Create、Schedule 三个测试项。

---

## 四、实验内容与实现

### 4.1 功能模块：虚拟内存页操作

**负责人：**待填写

**涉及文件：**`code/kern/mm/pmm.c`

**涉及函数：**

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

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

### 4.2 功能模块：进程创建与初始化

**负责人：**待填写

**涉及文件：**`code/kern/process/proc.c`

**涉及函数：**

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

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

### 4.3 功能模块：进程调度

**负责人：**待填写

**涉及文件：**

- `code/kern/process/proc.c`
- `code/kern/schedule/sched.c`
- `code/kern/process/switch.S`

**涉及函数：**

```c
void cpu_idle(void);
void schedule(void);
void proc_run(struct proc_struct *proc);
void switch_to(struct context *from, struct context *to);
```

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

### 4.4 Lab2/Lab3 依赖代码迁移

#### 功能说明

Lab4 依赖 Lab2 的物理内存管理和 Lab3 的 Trap 处理代码。需要根据此前实验的 Prompt 重新生成或迁移相应实现。

#### 涉及位置

```text
code/kern/mm/pmm.c        //lab2 请补充你在lab2中的代码
code/kern/mm/default_pmm.c //lab2 请补充你在lab2中的代码
code/kern/trap/trap.c     //lab3 请补充你在lab3中的代码
```

#### 验证状态

待编译验证。

---

## 五、测试与验证

### 5.1 测试环境说明

待填写。

### 5.2 测试矩阵

| 测试 | 验证内容 | 结果 |
|------|----------|------|
| PMM | `page_insert()`、`page_remove()`、引用计数和页表项 | 待测试 |
| Process Create | `idleproc`、`initproc`、PCB 字段和内核栈 | 待测试 |
| Schedule | `schedule()`、`proc_run()`、`switch_to()` 和多线程切换 | 待测试 |

### 5.3 测试截图

```markdown
![PMM 测试](./images/01_pmm.png)
![进程创建测试](./images/02_process_create.png)
![调度测试](./images/03_schedule.png)
```

待补充截图文件。

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

- [ ] Lab2 依赖代码已迁移
- [ ] Lab3 依赖代码已迁移
- [ ] `page_insert()` 和 `page_remove()` 已实现
- [ ] 进程创建相关函数已实现
- [ ] 调度与上下文切换已实现
- [ ] `make grade` 通过
- [ ] 报告和 Prompt 完成
- [ ] 测试截图已放入 `report/images/`
- [ ] 代码中的 `YOUR CODE` 已替换为学号
