# Lab3 Prompt 汇总

> 本文件按 Lab0.5 的四段式规范，按使用时间顺序汇总 Lab3 期间使用的所有 Prompt。

## 一、Prompt 索引

| 编号 | 负责人 | 模块 | 结果 |
|------|--------|------|------|
| P01 | 刘国全 | RESTORE_ALL | 编译通过，Test 1 PASS (35/35) |
| P02 | 刘国全 | __alltraps | 反汇编确认，Test 1 PASS (35/35) |
| P03 | 刘国全 | Trap 上下文 Review | 反汇编检查完成 |
| P04 | 刘爽 | interrupt_handler | Test 3 PASS (40/40) |
| P05 | 刘爽 | 时钟中断验证 | Test 3 PASS (40/40, ticks=200) |
| P06 | 全员 | 阶段报告整理 | 阶段报告完成，后整合为最终报告 |
| P07 | 王乐妍 | exception_handler 与指令长度判断 | 一次通过编译，Test 1/2 PASS |
| P08 | 王乐妍 | 复用 Lab2 提示词生成依赖代码 | 填入后编译通过，make grade 100/100 |
| P09 | 王乐妍 | 生成实验报告章节 | 章节生成并整合进最终报告 |

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

**结果：**`trapentry.S` 成功编译为 RISC-V 目标文件；Lab3 全部模块合并后 Test 1 PASS (35/35)。

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

**结果：**`__alltraps` 和 `__trapret` 的符号及跳转关系已在反汇编中确认；Lab3 全部模块合并后 Test 1 PASS (35/35)。

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

**结果：**Lab3 全部模块合并后 Test 3 PASS (40/40)，Total Score: 100/100。

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

**结果：**Test 3 PASS (40/40, ticks=200)，软件中断与未知中断兜底输出均在 Test 2 中验证通过。

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

**结果：**已形成阶段报告和 Prompt 汇总；后续与异常处理模块、最终测试结果整合为最终版报告。

---

## P07：实现 exception_handler 与指令长度判断

**负责人：**王乐妍

**使用目的**：在 `code/kern/trap/trap.c` 的 `//lab3 function3` 标记处实现 `exception_handler()`，识别非法指令与断点异常并按规定格式输出、按指令真实长度推进 `tf->epc`、其余异常调用 `print_trapframe()` 兜底；配套补充静态辅助函数 `trap_insn_len()` 与 `trap.h` 中的函数声明。

**Prompt 内容**：

````markdown
[PROMPT]
**任务**：修改文件 code/kern/trap/trap.c，在 `//lab3 function3` 标记处实现
exception_handler() 函数，完成异常处理功能：识别非法指令异常与断点异常并按规定
格式输出、根据指令实际长度更新 tf->epc、其余异常调用 print_trapframe() 兜底。

**操作要求**：你必须在项目中修改实际文件，而不是仅展示代码。保留文件中所有现有
代码，只在标记位置插入新实现，不得修改任何既有函数的函数体或签名。若
exception_handler 缺少声明，可在 kern/trap/trap.h 中补充其函数声明。

**输出要求**：直接进行文件操作。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION]
部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
// 数据结构（kern/trap/trap.h）
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

// 异常编码宏（libs/riscv.h）
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT          0x3

// 可用函数（分别位于kern/trap/trap.c和libs/stdio.h）
void print_trapframe(struct trapframe *tf);
int  cprintf(const char *fmt, ...);

// 调用点（trap.c，不要改动）
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0)
        interrupt_handler(tf);    // 中断分支（队友已实现）
    else
        exception_handler(tf);    // 异常分支（本任务要实现）
}

// 领域事实：RISC-V 指令最低 2 位不等于 0b11 时为 2 字节压缩指令，否则为 4 字节
// 领域事实：tools/grade.sh 的输出契约要求包含 "Illegal instruction at"、
//           "Breakpoint at" 等子串
// 领域事实：测试会用 memset 清零的合成 trapframe 直接调用本函数，此时 epc 可能为 0

[GUARANTEE]
必须实现的主要接口：
```c
void exception_handler(struct trapframe *tf);
```
你可以根据需要添加：
- 辅助函数 static size_t trap_insn_len(uintptr_t epc)
- 内部使用的宏定义（如有必要）

[SPECIFICATION]
## exception_handler
**Pre-Condition**:
- tf 指向一个合法的 trapframe，cause 最高位为 0。

**Post-Condition**:
- 对实验明确要求处理的两种异常：已识别异常类型、已输出符合 RELY 评分契约的
  记录，并使帧处于"返回后不会再次执行同一条异常指令"的状态；
- 对其余异常：已交由框架既有的 Trap frame 输出兜底，且未改变执行位置；
- 除执行位置（epc）外，未修改其它已保存上下文，也未改动任何全局状态。

  **Case 1（非法指令）**:
  - 触发条件：cause 为非法指令异常码；
  - 行为与最终状态：输出可被评分识别为非法指令、并携带当前异常地址的记录；
    帧中的执行位置前进到触发异常指令的下一条指令，前进量等于该指令的真实长度。

  **Case 2（断点）**:
  - 触发条件：cause 为断点异常码；
  - 行为与最终状态：与 Case 1 类似，但输出需可被评分识别为断点；此分支同时被
    Test 1 的 ebreak 触发，输出须与评分契约一致。

  **Case 3（其他异常）**:
  - 触发条件：任何实验未要求专门处理的异常码（含测试直接注入的未知 cause）；
  - 行为与最终状态：保持执行位置不变，将整个帧交给框架既有的输出函数显示。

**Requirements**:
- 执行位置的前进量必须由异常指令的真实长度决定，禁止对所有异常假定固定偏移；
- 仅 Case 1 / Case 2 允许访问 epc 处内存，Case 3 禁止任何对 epc 的内存读取；
- 函数签名与可修改函数范围以 GUARANTEE 为准；工程以 GNU C99 且 -Wall -Werror
  编译，不得引入隐式声明等告警。

## 指令长度辅助函数（可选）
**Pre-Condition**: epc 指向内核地址空间中一条有效指令的起始地址。
**Post-Condition**: 得到该指令所占的字节数；不修改任何状态。
````

**结果说明**：生成的代码经人工审查后一次通过编译与相关测试：三种 cause 分支齐全、输出格式与评分契约一致、EPC 前进量由指令真实长度动态决定、兜底分支不读 `epc` 内存（合成帧 `epc = 0` 安全）。Test 1（断点 EPC 推进，35/35）与 Test 2（非法指令 EPC 推进与返回，25/25）均 PASS，本 Prompt 无返工。

---

## P08：复用 Lab2 提示词生成 Lab2 依赖代码

**负责人：**王乐妍

**使用目的**：首次 `make grade` 无法编译，根因是 Lab3 骨架中标有 `//lab2 请补充` 的占位位置未填写——实验依赖的 Lab2 必做代码（Best-Fit 分配器实现、`page_init()`）缺失。按实验指导书"本实验依赖实验2。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有 lab2 注释的对应位置，确保代码能够正确编译"的要求，取出 Lab2 报告中记录的两份提示词重新生成相应代码并填入对应位置。

**Prompt 内容**：

提示词一（生成 `page_init()`，来自 Lab2 报告 4.1.1 节）：

````markdown
[PROMPT]
在 ucore Lab2 的 kern/mm/pmm.c 中实现 page_init()，建立 Page 描述符数组并把完整空闲
物理页注册给当前 pmm_manager。

[RELY]
get_memory_base()/get_memory_size()、DRAM_BASE、PGSIZE、PPN、PADDR、ROUNDUP、
ROUNDDOWN、struct Page、pages、npage、nbase、va_pa_offset、init_memmap()、pa2page()。

[GUARANTEE]
static void page_init(void);

[SPECIFICATION]
区分 VA、PA、PPN 和 Page 描述符地址；描述符覆盖 [nbase, npage)；数组从 ROUNDUP(end,
PGSIZE) 开始；所有描述符先标记 reserved；排除固件、内核和描述符数组后，只把页对齐的
完整空闲页交给 init_memmap()；非法 DTB 或无描述符空间时 panic。
````

提示词二（生成 Best-Fit 分配器，来自 Lab2 报告 4.2.2 节）：

````markdown
[PROMPT]
参考 default_pmm.c，在 kern/mm/best_fit_pmm.c 中实现 Best-Fit 管理器，并在 pmm.c 的
init_pmm_manager() 中切换到 best_fit_pmm_manager。不得修改 basic_check() 和
best_fit_check()。

[RELY]
struct pmm_manager、free_area_t、PageProperty、le2page、list_*、nr_free、alloc_pages、
free_pages。

[GUARANTEE]
best_fit_init、best_fit_init_memmap、best_fit_alloc_pages、best_fit_free_pages、
best_fit_nr_free_pages、best_fit_check、best_fit_pmm_manager。

[SPECIFICATION]
alloc_pages 必须选择满足请求且剩余空间最小的块；相等时保持链表遍历结果；剩余部分必须
拆分；free_pages 必须合并左右连续空闲块；分配、释放后链表、property、PageProperty 和
nr_free 必须保持一致。
````

**结果说明**：重新生成的代码填入 `best_fit_pmm.c` 与 `pmm.c` 标有 lab2 注释的占位位置后通过编译；同时将 `init_pmm_manager()` 挂载恢复为 `best_fit_pmm_manager`，并保留 Lab3 框架新增的 `local_intr_save` 中断保护包装，随后 `make grade` 三项测试全部通过，Total Score: 100/100。

---

## P09：生成实验报告章节

**负责人：**王乐妍

**使用目的：** 让 AI 基于工程里的 真实代码和测试事实 ，产出个人负责的"异常处理与指令长度判断"模块的报告章节（含 Spec、最终提示词、迭代记录、测试验证）

**Prompt 内容**：

```Markdown
[PROMPT]
**任务**：为 Lab3 实验报告（report/report.md）撰写我负责的章节——"4.1 功能模块：
异常处理与指令长度判断"（负责人 2413423-王乐妍），并补充第五章中与 Test 2 相关的
测试说明。以工程真实代码为唯一事实来源撰写，不得虚构实现细节。

**操作要求**：先阅读 [RELY] 中列出的全部代码与材料，再动笔。输出为 Markdown 片段，
章节结构和标题层级与 Lab2 报告（2/report/report.md）的 4.x 节保持一致；只写我负责的
模块，不撰写其他成员的章节。

**输出要求**：直接给出可粘贴进 report.md 的 Markdown 文本。所有代码引用、函数名、
宏定义、测试输出必须与工程实际内容一字不差。

[RELY]
一、我负责的代码：
code/kern/trap/trap.c 的 trap_dispatch()：cause 最高位为 0 时进入异常分支

参考阅读的代码：
code/kern/trap/trap.c、code/kern/trap/trap.h、libs/riscv.h

二、测试与评分事实：
- tools/grade.sh：Test 2 匹配子串含 "Illegal instruction at"；Test 1 的 ebreak
  断点输出 "Breakpoint at" 由我的函数产生
- tools/test_lab3.c：测试会用 memset 清零的合成 trapframe 直接调用
  exception_handler（此时 epc=0），并直接注入 cause=20 走兜底分支
- 实测结果（不得改动数字）：Test 1 PASS (35/35)、Test 2 PASS (25/25)、
  Test 3 PASS (40/40)、Total Score: 100/100；截图为
  ./images/01_test1.png、02_test2.png、03_test3.png


[GUARANTEE]
必须产出的内容块（参考 Lab2 报告）：
1. 模块功能描述：涉及函数签名（exception_handler、trap_insn_len）+ 功能说明，
   必须讲清"异常与中断在 sepc 语义上的差异→为什么必须推进 epc→为什么推进量要
   按指令真实长度动态判断"这条因果链
2. 核心实现：引用工程实际代码，附 3-4 条实现要点
3. Spec：Pre-Condition / Post-Condition / Failure（行为契约式描述，不得写成伪代码；
   须覆盖"兜底分支禁止读取 epc 处内存"的边界约束）
4. 最终提示词：完整引用 prompt.md 的 Prompt 1
5. 实现迭代过程：两次迭代，第一次（本模块一次通过+原因分析）、第二次（按上述
   叙事写依赖代码问题与解决策略）
6. 与 Test 1 的联合验证说明：断点分支同时被 Test 1 的 ebreak 触发

[SPECIFICATION]
**Pre-Condition**:
- 工程 make grade 已达到 100/100，报告引用的代码为最终合并版本；
- prompt.md 与三张测试截图已存在。

**Post-Condition**:
- 章节中每一处技术断言都能在 RELY 列出的代码/脚本/截图中找到对应依据；
- 数字（cause 编码、指令长度规则、测试得分）与工程事实完全一致；
- 不出现其他成员模块的实现细节；不出现"迁移/拷贝 lab2 分支"类措辞；
- 中文撰写，Markdown 可直接粘贴，图片引用路径为 ./images/0X_testX.png。
```

**结果说明**：生成的章节经人工核对后整合进最终报告（章节编号由 4.1 调整为 4.3，以与分工顺序保持一致）。
