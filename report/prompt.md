# Lab3 Prompt 汇总

> 本文件按 Lab0.5 的四段式规范记录 Lab3 已有部分的 Prompt。
>
> 当前只包含已完成的 Trap 入口和中断处理模块；异常处理部分待王乐妍完成后补充。

## 一、Prompt 索引

| 编号 | 负责人 | 模块 | 结果 |
|------|--------|------|------|
| P01 | 刘国全 | RESTORE_ALL | 汇编目标文件编译通过 |
| P02 | 刘国全 | __alltraps | 汇编目标文件编译通过 |
| P03 | 刘国全 | Trap 上下文 Review | 反汇编检查完成 |
| P04 | 刘爽 | interrupt_handler | 代码已提交，待完整联调 |
| P05 | 刘爽 | 时钟中断验证 | 待 exception_handler 和 Lab2 代码并入后运行 |
| P06 | 全员 | 阶段报告整理 | 已完成阶段版报告 |

---

## P01：实现 RESTORE_ALL

**负责人：**刘国全

```text
[PROMPT]
在 kern/trap/trapentry.S 中实现 RESTORE_ALL，与已有的 SAVE_ALL 严格对应，
恢复 Trap 发生前的 CSR 和全部通用寄存器，最后恢复 sp，并保证后续 __trapret 可以执行 sret。

[RELY]
- SAVE_ALL 的 Trap frame 布局：x0~x31 后依次保存 sstatus、sepc、sbadaddr、scause
- REGBYTES、LOAD 宏
- sscratch 保存 Trap 发生前的 sp
- struct trapframe 的 36 个 uintptr_t 布局

[GUARANTEE]
RESTORE_ALL 汇编宏。

[SPECIFICATION]
Pre-Condition：
  sp 指向有效的 36 项 Trap frame。
Post-Condition：
  恢复 sscratch、sstatus、sepc、sbadaddr、scause；
  恢复 x1、x3~x31；
  最后恢复 x2/sp；
  不改变 SAVE_ALL 和 trapframe 布局。
Failure：
  sp 必须最后恢复，否则后续无法访问 Trap frame；
  CSR 恢复必须在覆盖 s0-s4 之前完成；
  偏移必须与 SAVE_ALL 一致。
```

**结果：**`trapentry.S` 成功编译为 RISC-V 目标文件。

## P02：实现 __alltraps

**负责人：**刘国全

```text
[PROMPT]
实现 __alltraps 的汇编入口，使 Trap 保存的 Trap frame 按 RISC-V 调用约定传给
void trap(struct trapframe *tf)，trap() 返回后跳转到 __trapret。

[RELY]
- SAVE_ALL
- __trapret
- RISC-V a0 参数寄存器和 jal 调用约定
- trap(struct trapframe *tf) 签名

[GUARANTEE]
__alltraps 汇编入口。

[SPECIFICATION]
Pre-Condition：
  进入 __alltraps 时 sp 指向可用的内核栈。
Post-Condition：
  SAVE_ALL 完成；
  a0 = 当前 Trap frame 地址；
  调用 trap；
  trap 返回后进入 __trapret；
  不修改 trapframe 布局和 trap 签名。
Failure：
  不能在调用前后破坏 sp；
  不能直接跳转到 C 函数而遗漏参数传递。
```

**结果：**`__alltraps` 和 `__trapret` 的符号及跳转关系已在反汇编中确认。

## P03：Review Trap 入口与恢复顺序

**负责人：**刘国全

```text
[PROMPT]
审查 trapentry.S 的 SAVE_ALL、RESTORE_ALL 和 __alltraps，检查 Trap frame 偏移、
sscratch/sp 特殊处理、CSR 恢复顺序、jal trap 参数和 sret 返回路径。

[RELY]
SAVE_ALL、RESTORE_ALL、struct trapframe、riscv.h、trap.c、test_lab3_context.S。

[GUARANTEE]
输出审查结论和需要修正的汇编。

[SPECIFICATION]
确认：
  x2/sp 与普通寄存器处理不同；
  sp 最后恢复；
  jal trap 前 a0=sp；
  __trapret 后执行 sret。
```

**结果：**汇编对象编译通过；反汇编顺序正确。

## P04：实现 interrupt_handler

**负责人：**刘爽

```text
[PROMPT]
在 kern/trap/trap.c 中实现 interrupt_handler，处理 RISC-V 中断：时钟中断需要
每次 ticks++、重新设置下一次时钟事件、周期性打印 ticks，并在达到关机条件后关机；
软件中断和未知中断需要输出规定信息。

[RELY]
struct trapframe、ticks、TICK_NUM、SHUTDOWN_AFTER、IRQ_S_TIMER、IRQ_S_SOFT、
clock_set_next_event()、print_ticks()、print_trapframe()、sbi_shutdown()。

[GUARANTEE]
void interrupt_handler(struct trapframe *tf);

[SPECIFICATION]
- tf->cause 最高位表示异步中断，低 63 位是中断原因；
- IRQ_S_TIMER 每次重新设置下一次时钟事件；
- ticks 每次加一；
- ticks 为 TICK_NUM 倍数时输出 100 ticks；
- ticks 达到 SHUTDOWN_AFTER 后关机；
- 软件中断和未知中断有对应输出。
```

**结果：**代码已提交，尚需在异常处理和 Lab2 PMM 代码完成后做完整联调。

## P05：验证时钟中断

**负责人：**刘爽

```text
[PROMPT]
为 interrupt_handler 设计验证：确认时钟中断能持续重复触发，ticks 按次增长，
每 100 ticks 输出一次统计，达到条件后关机；同时验证软件中断和未知中断兜底。

[RELY]
interrupt_handler、clock_set_next_event、ticks、TICK_NUM、SHUTDOWN_AFTER、
sbi_shutdown、test_lab3.c。

[GUARANTEE]
输出测试步骤和预期结果。

[SPECIFICATION]
预期包含：
  100 ticks
  ticks=200
  Supervisor software interrupt
  Unknown interrupt: cause 15
```

**结果：**测试计划已准备；完整运行等待其他模块完成。

## P06：整理阶段报告和 Prompt

**负责人：**全员

```text
[PROMPT]
根据当前已完成的 trapentry 和 interrupt_handler 代码，整理 Lab3 阶段报告和 Prompt，
明确标出 exception_handler 和完整测试尚未完成，不虚构测试通过结果。

[RELY]
code/kern/trap/trapentry.S、code/kern/trap/trap.c、report/report.md、
report/prompt.md、tools/test_lab3.c、tools/grade.sh。

[GUARANTEE]
形成阶段报告和 Prompt 记录，列出完成项、阻塞项和后续测试命令。

[SPECIFICATION]
报告不得把未实现模块写成已完成；
所有已完成模块需有 Spec、Prompt、实现说明和验证状态；
最终测试结果必须在代码完整后重新运行确认。
```

**结果：**已形成本阶段报告和 Prompt 汇总。
