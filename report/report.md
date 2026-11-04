# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab3：中断与异常处理 |
| **小组成员** | 2411822-刘国全、2411350-刘爽、2413423-王乐妍 |
| **完成日期** | 2026-11-04 |
| **当前状态** | 阶段报告：Trap 入口和中断处理已完成，异常处理部分待完成 |

### 小组分工

| 成员 | 负责的模块 |
|------|------------|
| 刘国全 | `trapentry.S`：`RESTORE_ALL`、`__alltraps`、上下文恢复 |
| 刘爽 | `trap.c`：`interrupt_handler()`、时钟中断和中断兜底 |
| 王乐妍 | `trap.c`：`exception_handler()`、异常指令长度与 EPC 更新；测试与报告整合 |

---

## 一、实验目的

本实验的主要目的是：

1. 理解 RISC-V 中 Trap（中断和异常）的进入、处理和返回流程；
2. 理解 Trap frame 的布局以及上下文保存、恢复机制；
3. 掌握 `stvec`、`sepc`、`scause`、`sstatus` 等控制状态寄存器的作用；
4. 实现时钟中断的持续触发和处理；
5. 掌握非法指令、断点异常的识别与 `sepc` 修正；
6. 了解中断开关、原子位操作与内核临界区保护之间的关系；
7. 通过 AI 辅助设计 Prompt、生成代码、调试并验证 Trap 机制。

---

## 二、实验环境

实验在 WSL2 的 Ubuntu 24.04.1 LTS 中完成，目标架构为 RISC-V 64 位。

主要工具：

| 工具 | 用途 |
|------|------|
| `riscv64-unknown-elf-gcc` | 编译 RISC-V 内核 |
| `riscv64-unknown-elf-ld` | 链接内核 |
| `riscv64-unknown-elf-gdb` | 调试 guest 内核 |
| `qemu-system-riscv64` | 运行内核 |
| GNU Make | 构建工程 |
| Git | 版本管理 |

AI 工具配置：

| 成员 | AI 编程工具 | 底层模型 |
|------|------------|---------|
| 刘国全 | Codex（VS Code/终端 Agent） | DeepSeek Flash |
| 刘爽 | 豆包（桌面端） | 具体模型版本未记录 |
| 王乐妍 | Claude Code | DeepSeek Flash |

---

## 三、实验整体逻辑分析

### 3.1 本章逻辑主线

Lab3 围绕“Trap 发生以后，内核如何保存现场、进入 C 语言处理函数、处理中断或异常，并恢复原执行流”展开。

完整链路：

```text
程序执行
  -> 中断或异常发生
  -> CPU 根据 stvec 跳转到 __alltraps
  -> SAVE_ALL 构造 trapframe
  -> __alltraps 把 trapframe 传给 trap(tf)
  -> trap_dispatch 区分中断和异常
  -> interrupt_handler / exception_handler
  -> 返回 __trapret
  -> RESTORE_ALL 恢复上下文
  -> sret 返回原执行流
```

### 3.2 功能实现顺序

1. **理解 Trap frame 布局。**
   `trapframe` 包含 32 个通用寄存器和 `sstatus`、`sepc`、`sbadaddr`、`scause`，共 36 个 `uintptr_t`。

2. **实现 Trap 入口和上下文恢复。**
   `SAVE_ALL` 保存现场，`__alltraps` 调用 C 语言 `trap()`，`RESTORE_ALL` 恢复现场。

3. **实现中断处理。**
   `interrupt_handler()` 对时钟中断、软件中断和未知中断进行分类处理。

4. **实现异常处理。**
   `exception_handler()` 处理非法指令、断点异常，并更新 `tf->epc`。

5. **运行测试。**
   测试覆盖上下文恢复、非法指令、断点、软件中断、未知中断和持续时钟中断。

---

## 四、实验内容与实现

### 4.1 Trap 入口与上下文恢复

**负责人：**2411822-刘国全  
**文件：**`code/kern/trap/trapentry.S`

#### Trap frame 布局

```c
struct pushregs {
    uintptr_t zero, ra, sp, gp, tp;
    uintptr_t t0, t1, t2;
    uintptr_t s0, s1;
    uintptr_t a0, a1, a2, a3, a4, a5, a6, a7;
    uintptr_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uintptr_t t3, t4, t5, t6;
};

struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

Trap frame 偏移：

```text
0  ~ 31  * 8：通用寄存器 x0 ~ x31
32 * 8：sstatus
33 * 8：sepc
34 * 8：sbadaddr
35 * 8：scause
```

#### `RESTORE_ALL`

实现逻辑：

1. 从 `2*REGBYTES(sp)` 恢复原 `sp` 到 `sscratch`；
2. 从 `32*REGBYTES(sp)` 恢复 `sstatus`；
3. 从 `33*REGBYTES(sp)` 恢复 `sepc`；
4. 从 `34*REGBYTES(sp)` 恢复 `sbadaddr`；
5. 从 `35*REGBYTES(sp)` 恢复 `scause`；
6. 恢复 x1、x3~x31；
7. 最后恢复 x2/sp，避免提前改变 Trap frame 的基址。

#### `__alltraps`

```asm
__alltraps:
    SAVE_ALL
    mv a0, sp
    jal trap
    j __trapret
```

- `SAVE_ALL` 在栈顶构造 Trap frame；
- `mv a0, sp` 将 Trap frame 地址作为 `trap(struct trapframe *tf)` 的第一个参数；
- `jal trap` 调用 C 语言处理函数；
- `j __trapret` 进入上下文恢复和 `sret` 返回路径。

#### Spec

```text
Pre-Condition:
  CPU 已根据 stvec 进入 __alltraps；
  sscratch 中保存进入 Trap 前的 sp；
  当前栈上有足够空间容纳 36 个 uintptr_t。

Post-Condition:
  RESTORE_ALL 恢复 sstatus、sepc、sbadaddr、scause 和全部通用寄存器；
  sscratch 恢复为进入 Trap 前的 sp；
  sp 最后恢复为进入 Trap 前的值；
  __trapret 执行 sret，返回原执行流。

Failure:
  如果先恢复 sp，后续将失去 Trap frame 地址；
  如果 CSR 恢复晚于通用寄存器恢复，临时寄存器会被覆盖；
  如果 trapframe 偏移不一致，寄存器和 CSR 会恢复错误。
```

#### 验证状态

`trapentry.S` 已单独编译成功：

```text
+ cc kern/trap/trapentry.S
```

反汇编确认：

```text
__alltraps:
  csrw sscratch, sp
  ...
  mv a0, sp
  jal trap
  j __trapret

__trapret:
  ...恢复 CSR 和通用寄存器...
  ld sp, 16(sp)
  sret
```

完整运行测试仍依赖 `exception_handler()` 和 Lab2 PMM 代码并入 Lab3。

---

### 4.2 中断处理与时钟中断

**负责人：**2411350-刘爽  
**文件：**`code/kern/trap/trap.c`

#### `interrupt_handler`

实现内容：

1. 从 `tf->cause` 中屏蔽最高位，取出中断原因编号；
2. `IRQ_S_TIMER`：
   - `ticks++`；
   - 每 `TICK_NUM` 次输出统计；
   - 调用 `clock_set_next_event()` 设置下一次时钟中断；
   - 达到 `SHUTDOWN_AFTER` 后调用 `sbi_shutdown()`；
3. `IRQ_S_SOFT`：输出软件中断提示；
4. 未知中断：输出原因并调用 `print_trapframe(tf)`。

关键代码逻辑：

```c
uintptr_t cause_code =
    tf->cause & (((uintptr_t)1 << 63) - 1);

switch (cause_code) {
case IRQ_S_TIMER:
    ticks++;
    if (ticks % TICK_NUM == 0)
        print_ticks();
    clock_set_next_event();
    if (ticks >= SHUTDOWN_AFTER)
        sbi_shutdown();
    break;
case IRQ_S_SOFT:
    cprintf("Supervisor software interrupt\n");
    break;
default:
    cprintf("Unknown interrupt: cause %d\n", (int)cause_code);
    print_trapframe(tf);
}
```

#### Spec

```text
Pre-Condition:
  trap_dispatch() 已判断 tf->cause 是中断；
  clock_init() 已设置第一个时钟事件；
  sstatus.SIE 已打开。

Post-Condition:
  每次时钟中断重新设置下一次事件；
  ticks 每次加一；
  ticks 为 TICK_NUM 整数倍时输出统计；
  ticks 达到 SHUTDOWN_AFTER 后关机；
  软件中断和未知中断有对应输出。
```

#### 当前验证状态

代码已提交，但 `trap.c` 当前无法单独编译，因为 `exception_handler()` 还未实现。完整 `make grade` 和 `100 ticks` 运行验证需等待异常处理部分完成。

---

### 4.3 异常处理

**负责人：**2413423-王乐妍  
**状态：待完成**

本模块需要实现：

```c
void exception_handler(struct trapframe *tf);
```

待完成内容：

- 非法指令异常识别；
- 断点异常识别；
- 根据 RISC-V 指令低两位判断 2 字节或 4 字节指令；
- 更新 `tf->epc`，避免重复执行异常指令；
- 未知异常调用 `print_trapframe(tf)` 兜底。

在完成之前，`trap.c` 中仍保留：

```text
//lab3 function2 YOUR CODE
```

因此当前实验尚不能完成完整测试。

---

## 五、测试与验证

### 5.1 已完成检查

```text
trapentry.S 单独编译：通过
trap.o 单独编译：未通过（缺少 exception_handler 声明/实现）
```

### 5.2 完整测试前置条件

需要：

1. 将 Lab2 已完成的 PMM 代码并入 Lab3；
2. 完成 `exception_handler()`；
3. 再运行：
   ```bash
   cd ~/OS2026/code
   make clean
   make
   make grade
   ```

预期测试：

```text
[Test 1] Trap entry and breakpoint return: PASS (35/35)
[Test 2] Illegal instruction return: PASS (25/25)
[Test 3] Sustained supervisor timer: PASS (40/40, ticks=200)
Total Score: 100/100
```

当前不能声称上述测试已经通过。

---

## 六、实验总结与收获

### 对操作系统的理解

| 本实验知识点 | 对应 OS 原理 | 含义、关系与差异 |
|--------------|--------------|------------------|
| Trap frame | 进程上下文和现场保存 | Trap frame 保存被打断执行流的寄存器现场；它是内核恢复执行的基础，但不等于完整进程控制块。 |
| `SAVE_ALL/RESTORE_ALL` | 上下文切换 | 二者完成一次 Trap 的保存和恢复；真正的进程上下文切换还会切换页表、栈和调度状态。 |
| `stvec`、`sepc`、`scause` | 异常/中断控制接口 | 这些 CSR 决定 Trap 入口、返回地址和原因；OS 原理中的中断控制器和异常表是更一般的抽象。 |
| `sret` | 特权级返回 | `sret` 根据 `sstatus.SPP` 等状态返回；本实验只做内核态 Trap，尚未完整实现用户态切换。 |
| 时钟中断 | 抢占式调度基础 | 时钟中断提供时间片和系统节拍；本实验只累计 ticks，尚未实现调度器。 |
| `local_intr_save/restore`、原子位操作 | 临界区与同步 | 通过关中断和原子指令避免共享状态被中断破坏；本实验是单核场景，尚未涉及多核锁。 |

### 本实验尚未覆盖的重要 OS 知识点

- 用户态与内核态的完整切换；
- 系统调用和参数检查；
- 进程创建、调度和上下文切换；
- 多核中断、IPI 和 TLB shootdown；
- 嵌套中断及中断优先级；
- 设备驱动和完整 I/O 中断；
- 按需分页和缺页异常处理；
- 中断并发下的锁和内存一致性。

### AI 协作开发的经验

1. Trap frame 的字段偏移必须与汇编严格一致，不能只根据自然语言描述生成代码；
2. AI 给出的寄存器恢复顺序需要人工检查，尤其是 `sp` 和 CSR 的恢复时机；
3. 中断处理必须同时考虑正常路径、未知路径和持续触发路径；
4. 生成代码后必须实际编译、反汇编或运行测试，不能只接受自然语言解释；
5. 当前异常处理尚未完成，报告和最终测试结果需在实现后更新，不能提前写成通过。

---

## 七、当前阶段结论

已完成：

- Trap frame 布局理解；
- `RESTORE_ALL`；
- `__alltraps`；
- Trap 入口到 `trap()` 再到 `__trapret` 的闭环；
- `interrupt_handler()`；
- 时钟中断重设、ticks 统计和关机逻辑。

待完成：

- `exception_handler()`；
- 非法指令和断点异常的 EPC 更新；
- Lab2 PMM 代码并入 Lab3；
- 完整 `make grade` 和最终截图。
