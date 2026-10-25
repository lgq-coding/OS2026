# Lab2 Prompt 汇总

> 本文件汇总 Lab2 期间使用的主要 Prompt，并按照 Lab0.5 的要求进行整理。
>
> 核心规范来自 Lab0.5「提示词工程」和「本实验的标准提示词结构」：
>
> 1. Prompt 描述目标和行为规格，不只给函数名或实现伪代码；
> 2. 代码生成 Prompt 使用四段式结构：
>    - `[PROMPT]`：任务、操作要求、输出要求；
>    - `[RELY]`：可以依赖的真实宏、类型、函数和全局变量；
>    - `[GUARANTEE]`：必须实现的函数签名和接口；
>    - `[SPECIFICATION]`：Pre-Condition、Post-Condition、Case、Requirements；
> 3. `[RELY]` 必须以真实工程代码为准，不能凭记忆编造；
> 4. 提示词必须明确边界、失败路径和可验证条件；
> 5. 产生代码后必须编译、运行测试并记录 Bug 与修正过程；
> 6. 修改代码或发现需求遗漏后，应同步更新 Prompt。

## 一、Prompt 索引

| 编号 | 负责人 | 模块 | Prompt 目标 | 结果 |
|------|--------|------|-------------|------|
| P01 | 刘国全 | 工程环境 | 检查 Lab2 编译与 QEMU 环境 | 确认工具链可用 |
| P02 | 刘国全 | 仓库结构 | 建立 `code/`、`report/` 交付结构 | 已推送 `lab2` 分支 |
| P03 | 王乐妍 | 总体理解 | 梳理 Lab2 任务、代码与分工 | 明确模块边界 |
| P04 | 刘国全 | `page_init` | 实现物理内存探测和 Page 布局 | `[Test 1] PASS` |
| P05 | 刘国全 | `page_init` | 审查地址转换、保留区和边界 | 通过课程测试 |
| P06 | 王乐妍 | First-Fit | 分析分配、释放、合并和改进空间 | 报告分析完成 |
| P07 | 刘爽 | Best-Fit | 实现并接入 Best-Fit | `[Test 2] PASS` |
| P08 | 刘爽 | Best-Fit | 审查最佳块选择与合并逻辑 | 20/20 |
| P09 | 王乐妍 | Buddy | 理解 Buddy 与 `pmm_manager` 关系 | 接口确认 |
| P10 | 王乐妍 | Buddy | 从零实现 Buddy System | 自检通过 |
| P11 | 王乐妍 | Buddy | 设计 Buddy 自检 | 自检通过 |
| P12 | 刘爽 | QEMU MMU | 调试 QEMU 页表翻译路径 | 源码级 GDB 验证完成 |
| P13 | 刘国全 | SLUB | 实现页级分配之上的对象分配 | `[SLUB] PASS` |
| P14 | 刘国全 | SLUB | 设计对象分配测试 | 回归 20/20 |
| P15 | 全员 | 报告 | 汇总 Spec、Prompt、Bug 和结论 | 报告完成 |
| P16 | 刘国全 | 工具链 | 配置并验证 RISC-V 工具链和 QEMU | 环境可用 |
| P17 | 刘国全 | 代码盘点 | 对照指导书识别 Lab2 文件与 TODO | 任务清单明确 |
| P18 | 刘国全 | 集成 | 合并 Best-Fit、Buddy 和 SLUB | 代码集成完成 |
| P19 | 刘国全 | 回归测试 | 验证编译、QEMU、课程测试和 SLUB | 20/20，SLUB PASS |
| P20 | 刘国全 | 完成度审计 | 对照指导书确认遗漏项 | Lab2 完成度确认 |

---

## 二、基础与环境类 Prompt

### P01：检查 Lab2 构建环境

**负责人：**刘国全

**使用目的：**确认 RISC-V 交叉编译、QEMU、GDB 和 make 能满足 Lab2 要求。

```text
[PROMPT]
检查当前 WSL 中 Lab2 的构建和调试环境。实际编译 code/、运行 QEMU，并确认
OpenSBI、DTB 物理内存探测和内核输出正常；若失败，给出具体错误和修复方式。

[RELY]
操作系统为 WSL2 Ubuntu；工程入口为 code/Makefile；工具包括
riscv64-unknown-elf-gcc/gdb、qemu-system-riscv64、make、Python 和 Git。

[GUARANTEE]
不修改实验代码；输出环境版本、构建结果、QEMU 启动结果和缺失依赖清单。

[SPECIFICATION]
Pre-Condition：代码和 Makefile 已存在。
Post-Condition：make 成功生成 bin/kernel、bin/ucore.img；QEMU 能输出 DTB 和内核信息。
Failure：记录命令未找到、编译错误或 QEMU 启动失败的具体原因和修复方式。
```

**结果：**构建成功，QEMU 输出 DTB、物理内存范围、PMM 检查和 SLUB 自检结果。

### P02：整理 Lab2 提交结构

**负责人：**刘国全

**使用目的：**按课程提交要求建立独立分支和目录。

```text
[PROMPT]
从课程网站取得 Lab2 官方代码，创建独立 lab2 分支并将代码放入 code/，建立
report/report.md、report/prompt.md 和 report/images/，不得覆盖 lab1。

[RELY]
Git 远程仓库、已有 lab1 分支、课程网站 lab2.zip 及组内提交规范。

[GUARANTEE]
lab2 分支根目录只保留 code/ 和 report/ 等必要文件；report/images/ 保留 .gitkeep。

[SPECIFICATION]
Pre-Condition：lab2 分支不存在或可从 main 创建。
Post-Condition：lab2 分支包含官方代码、报告模板、Prompt 汇总和图片目录，并已推送远程。
Failure：若压缩包目录层级不符，必须剥离顶层目录后再放入 code/；不得混入 lab1 代码。
```

**结果：**`lab2` 分支建立，目录结构符合组内提交约定。

### P03：梳理项目与分工边界

**负责人：**王乐妍

**使用目的：**快速理解 Lab2 全貌，确认各自负责的模块。

```text
[PROMPT]
阅读 Lab2 指导书和代码，梳理物理内存初始化、First-Fit、Best-Fit、Buddy System
之间的关系，说明各文件职责，并给三人小组划分互不冲突的实现边界。

[RELY]
Lab2 指导书、code/kern/mm、code/kern/driver、code/kern/init、code/tools。

[GUARANTEE]
输出模块关系图、接口边界、成员分工和依赖顺序，不修改代码。

[SPECIFICATION]
Pre-Condition：Lab2 官方代码已放入 code/。
Post-Condition：每个模块都有唯一负责人，公共接口和阻塞关系明确。
Failure：不得把同一文件同时分给多人修改；无法确定依赖时必须显式标注。
```

**结果：**确认 `page_init`、Best-Fit、Buddy、报告和 QEMU 调试的负责人及依赖顺序。

---

## 三、物理内存初始化

### P04：实现 `page_init()`

**负责人：**刘国全
**对应文件：**`code/kern/mm/pmm.c`

#### 最终 Prompt

```text
[PROMPT]

任务：
实现 Lab2 的物理内存初始化功能。根据设备树报告的物理内存范围建立 Page 描述符布局，
并将 OpenSBI、内核镜像、启动页表和 Page 描述符数组之外的完整空闲页交给当前
pmm_manager 管理。

操作要求：
必须在 code/kern/mm/pmm.c 中修改实际文件，保留现有初始化链路，只在
//lab2 function1 标记处补全，不修改 pmm.h 的公共接口或其他分配器。

输出要求：
使用下面 RELY、GUARANTEE 和 SPECIFICATION 中的真实定义和约束；完成后保证
make 编译通过，并保留课程测试可注入的接口。

[RELY]

- PHYSICAL_MEMORY_OFFSET、DRAM_BASE、PGSIZE、PPN、PADDR、ROUNDUP、ROUNDDOWN
- struct Page、pages、npage、nbase、va_pa_offset
- get_memory_base()、get_memory_size()
- init_memmap()、pa2page()
- 链接符号 end
- PageReserved、SetPageReserved、PageProperty 等页面状态宏

[GUARANTEE]

static void page_init(void);

[SPECIFICATION]

Pre-Condition：
1. dtb_init() 已完成，get_memory_base()/get_memory_size() 返回有效物理内存范围；
2. pmm_manager 已完成 init()，尚未接收任何空闲页面；
3. 内核运行在高地址虚拟地址，启动页表已建立固定虚实映射。

Post-Condition：
1. va_pa_offset 等于 PHYSICAL_MEMORY_OFFSET；
2. npage 等于 PPN(mem_base + mem_size)，且 npage > nbase；
3. pages 位于 ROUNDUP(end, PGSIZE)，并覆盖 [nbase, npage) 的每个物理页；
4. OpenSBI、内核镜像和 Page 描述符数组所在页面均保持 reserved；
5. 从描述符数组末尾到 DTB 内存末尾之间，只有按页对齐的完整页面被交给 init_memmap()；
6. 输出物理内存基地址、大小和结束地址。

Failure：
1. DTB 范围无效或 mem_end <= mem_base 时 panic；
2. npage <= nbase 时 panic；
3. Page 描述符数组超过物理内存末尾时 panic；
4. 没有完整空闲页时不得调用 init_memmap()。
```

#### 迭代与问题记录

- 第一版只考虑 `pages = end`，会导致描述符数组未按页对齐，课程测试会失败；
- 最终版本改为 `ROUNDUP(end, PGSIZE)`，先用 `PADDR()` 求描述符数组物理结束地址，再分别向上/向下对齐空闲区；
- 当前镜像中所有描述符先标记保留，再由 `init_memmap()` 清除实际空闲页的保留位。

#### 验证结果

```text
physical memory map:
memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
[Test 1] Page Init: PASS
check_alloc_page() succeeded!
```

### P05：审查 `page_init()` 的地址转换和边界

**负责人：**刘国全

```text
[PROMPT]
审查 page_init 是否严格区分 VA、PA、PPN 和 Page 描述符地址，检查 nbase、npage、
pages、va_pa_offset 的初始化顺序；验证 OpenSBI、内核和描述符数组不会进入空闲区；
检查 mem_base/mem_size 非法、描述符数组超界、尾部不足一页等失败路径。
[RELY]
pmm.c、pmm.h、memlayout.h、mmu.h、dtb.c、kernel.ld 以及 test_lab2.c。
[GUARANTEE]
不新增公共接口，只允许修复 page_init 内部逻辑。
[SPECIFICATION]
满足 test_lab2.c 的八项断言；不得改变 pmm_manager 的公共语义。
```

**结果：**修正 `PADDR()` 使用顺序和页对齐后，`[Test 1] Page Init: PASS`。

---

## 四、物理内存分配

### P06：分析 First-Fit

**负责人：**王乐妍
**对应文件：**`code/kern/mm/default_pmm.c`

```text
[PROMPT]
阅读 default_pmm.c，分析 First-Fit 如何初始化空闲页、分配连续页、回收页面并合并
相邻块，并回答该算法是否有进一步改进空间。

[RELY]
default_init、default_init_memmap、default_alloc_pages、default_free_pages、
default_nr_free_pages、free_area_t、PageProperty、le2page、nr_free。

[GUARANTEE]
输出 First-Fit 各函数职责、数据流、优缺点和改进方向；不修改代码。

[SPECIFICATION]
Pre-Condition：理解 init_memmap 注册的页块结构和地址有序空闲链表。
Post-Condition：报告准确说明分配、拆分、释放、左右合并和 O(N) 扫描特性。
Failure：不得把 Best-Fit 的选择规则混入 First-Fit；不得声称链表无序。
```

**结论：**First-Fit 从链表头线性扫描，选中第一个足够大的块；释放时通过地址有序链表合并左右连续块。优化方向包括按大小分桶、大小索引和 Best-Fit。

### P07：实现并接入 Best-Fit

**负责人：**刘爽
**对应文件：**`code/kern/mm/best_fit_pmm.c`、`code/kern/mm/pmm.c`

```text
[PROMPT]

任务：
参考 default_pmm.c，在 best_fit_pmm.c 中实现完整 Best-Fit 管理器，并在 pmm.c 的
init_pmm_manager() 中切换为 best_fit_pmm_manager。

操作要求：
只修改 best_fit_pmm.c、best_fit_pmm.h 和 pmm.c 的必要位置；不得修改
basic_check()、best_fit_check() 或测试调用方式；保留 lab2 标记。

输出要求：
实现后能够编译、通过课程 Best-Fit 测试，并保持页面链表和计数一致。

[RELY]

- struct pmm_manager、free_area_t
- list_init/list_add/list_add_before/list_add_after/list_del/list_empty
- le2page、PageProperty、SetPageProperty、ClearPageProperty
- page_ref、set_page_ref、npages、nbase、pages
- alloc_pages/free_pages/nr_free_pages
- 课程提供的 basic_check 和 best_fit_check

[GUARANTEE]

static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void best_fit_check(void);
extern const struct pmm_manager best_fit_pmm_manager;

[SPECIFICATION]

Pre-Condition：
best_fit_pmm_manager 已完成 init，init_memmap() 已建立初始空闲页链表。

Post-Condition：
1. alloc_pages(n) 在所有 property >= n 的块中选择 property 最小的块；
2. 若存在恰好等于 n 的块，可直接结束扫描；
3. 分配后剩余部分必须成为新的空闲块，并保持地址有序；
4. free_pages(base, n) 必须将页块插回链表，并合并左右连续空闲块；
5. nr_free_pages() 等于所有空闲块页数之和；
6. basic_check() 和 best_fit_check() 不得被修改。

Failure：
n==0、n 超过 nr_free 或无合适块时返回 NULL；非法释放、重复释放和页状态不一致由断言阻止。
```

**迭代与问题记录：**

- 第一版需要同时提供 `free_area`、`free_list` 和 `nr_free`，否则课程测试无法编译；
- 选择最小可用块时要维护“当前最小剩余量”，不能只保存第一个满足条件的块；
- 拆分后新块必须设置 property 和 PageProperty，并插回地址有序链表。

**结果：**`[Test 2] Best-Fit: PASS`，`make grade` 总分 20/20。

### P08：审查 Best-Fit 选择与合并

**负责人：**刘爽

```text
[PROMPT]
审查 best_fit_alloc_pages 和 best_fit_free_pages：确认全表扫描、最小剩余块选择、
精确匹配提前结束、剩余块拆分、左右合并和 nr_free 更新正确；检查空链表、超量请求、
重复释放和边界页块。
[RELY]
best_fit_pmm.c、pmm.h、memlayout.h、list.h、test_lab2.c。
[GUARANTEE]
不改变课程测试和 pmm_manager 接口。
[SPECIFICATION]
所有测试用例不得触发断言，链表、property 和 nr_free 必须保持一致。
```

**结果：**Best-Fit 测试通过，后续 Buddy 和 SLUB 接入未破坏该实现。

---

## 五、Buddy System

### P09：理解 Buddy 与 PMM 的关系

**负责人：**王乐妍

```text
[PROMPT]
说明 Lab2 从 kern_init 到 pmm_init、init_pmm_manager、page_init、init_memmap、
alloc_pages/free_pages 的完整调用链，并解释 Buddy System 在其中的接入位置。

[RELY]
pmm.c、pmm.h、memlayout.h、list.h、page_init() 和 struct pmm_manager。

[GUARANTEE]
输出 Buddy 与 PMM 的接口关系、初始空闲区传递方式和对外分配/释放调用路径。

[SPECIFICATION]
Pre-Condition：page_init 已建立 Page 描述符和初始空闲区。
Post-Condition：明确 Buddy 通过 init_memmap 接收连续空闲页，通过六个函数指针接入框架。
Failure：不得绕过 struct pmm_manager 直接修改 page_init 的公共语义。
```

**结果：**确认 Buddy 只通过 `struct pmm_manager` 接入，不修改 `page_init()` 的公共逻辑。

### P10：生成 Buddy 实现

**负责人：**王乐妍
**对应文件：**`code/kern/mm/buddy_pmm.c`、`buddy_pmm.h`

```text
[PROMPT]

任务：
在现有 ucore Lab2 PMM 框架下从零实现 Buddy System 物理页分配器，并接入
struct pmm_manager。

操作要求：
只新增/修改 buddy_pmm.c 和 buddy_pmm.h；不得修改课程测试、page_init() 和
Best-Fit；允许在临时验证时切换 pmm_manager，最终必须恢复 Best-Fit。

输出要求：
实现可编译的 Buddy 模块，并提供自检函数。

[RELY]

- struct Page、free_area_t、PageProperty/SetPageProperty/ClearPageProperty
- PageReserved、page2ppn、page2pa、pa2page、npage、nbase、pages
- list_init/list_add/list_del/list_empty/le2page
- struct pmm_manager
- alloc_pages/free_pages/nr_free_pages

[GUARANTEE]

static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
extern const struct pmm_manager buddy_pmm_manager;

[SPECIFICATION]

Pre-Condition：
pmm_manager 已初始化为 buddy_pmm_manager；初始空闲区由 page_init() 通过
init_memmap() 提供。

Post-Condition：
1. 每个 order 的块大小必须是 2^k 页，且首地址按 2^k 页对齐；
2. 分配时向上取整到 2 的幂，并从当前或更高 order 找到块；
3. 高 order 块拆分时，保留低半块，高半块放回对应 order；
4. 释放时通过页索引异或块大小定位伙伴；
5. 只有当伙伴同 order、空闲且状态一致时才合并；
6. nr_free_pages() 等于所有 order 空闲链表页数之和；
7. 自检结束后空闲页链表和计数恢复到初始基线。

Failure：
请求为 0、超过总空闲页或无可用 order 时返回 NULL；非法释放、重复释放、块不对齐和
伙伴状态不一致由断言阻止。
```

### P11：生成 Buddy 自检

**负责人：**王乐妍

```text
[PROMPT]
为 buddy_pmm.c 设计并实现 buddy_check()，覆盖初始空闲链表、0 页和超量请求、
非 2 的幂请求、跨 order 分配、精确拆分、释放合并、非伙伴块不可合并、碎片化场景
和状态恢复。测试只通过公共 alloc_pages/free_pages/nr_free_pages 检查行为。
[RELY]
buddy_pmm.c 的全部内部结构和 pmm.h 的公共接口。
[GUARANTEE]
void buddy_check(void);
[SPECIFICATION]
成功时不触发断言；失败时由 assert 暴露；测试结束后的空闲页总数恢复到起点。
```

**迭代与 Bug：**

1. 初版未处理未对齐的初始空闲区，已改为按最大可用对齐块分割；
2. 初版释放时只按请求页数处理，已改为利用分配块头保存实际 2 的幂块大小；
3. 初版未检查相邻但非伙伴块，已加入“页数足够但无 order 块时返回 NULL”的碎片化用例；
4. 初版未验证状态恢复，已增加 `nr_free_pages()` 基线检查和空闲链表不变量检查。

**结果：**Buddy 自检通过，未影响 Best-Fit 的正式评分。

---

## 六、QEMU MMU 分支任务

### P12：调试 QEMU 页表翻译

**负责人：**刘爽

```text
[PROMPT]

任务：
分析 QEMU 4.1.1 RISC-V Sv39 地址翻译过程，并通过宿主机 GDB 观察 guest 的一次
高地址取指如何从虚拟地址翻译成物理地址。

操作要求：
编译带调试符号的 QEMU，不安装覆盖正式 QEMU；在宿主机 GDB 中设置
riscv_cpu_tlb_fill、get_physical_address 和 tlb_set_page 断点；只观察高地址内核
访问，避免被低位 MROM 访问干扰。

输出要求：
记录 TLB miss 调用栈、satp、VPN2/VPN1/VPN0、页内偏移、物理地址、prot 和
TLB 安装参数。

[RELY]

- QEMU 4.1.1 源码
- target/riscv/cpu_helper.c: get_physical_address、riscv_cpu_tlb_fill
- accel/tcg/cputlb.c: tlb_fill、get_page_addr_code、tlb_set_page
- CPURISCVState 的 satp、priv 字段
- guest ucore.img

[GUARANTEE]

输出关键调用路径和 GDB 观察结果；说明 QEMU 软件 TLB 与真实硬件 TLB 的差异。

[SPECIFICATION]

Pre-Condition：
QEMU 使用 --enable-debug 构建，guest 加载到 0x80200000。

Post-Condition：
观察到一次虚拟地址 0xffffffffc02000d8 的 TLB miss，并确认其页级映射
0xffffffffc0200000 -> 0x80200000。

Failure：
若断点先命中低位地址，应增加 address >= 0xffffffffc0000000 条件。
```

**实际结果：**

```text
TLB miss address = 0xffffffffc02000d8
satp            = 0x8000000000080205
physical        = 0x80200000
prot            = 0x7
tlb_set_page    : vaddr=0xffffffffc0200000 paddr=0x80200000
```

**迭代与 Bug：**

- 第一次直接用 `--ex` 传多行 GDB `commands`，GDB 把 `silent` 当成顶层命令；
- 改为写入 `.gdb` 命令文件后正常；
- 第一次无条件断点命中 `0x1000`，改为只匹配高地址内核空间；
- 最终成功得到 `satp` 和物理地址，而不是只观察 `info mem`。

---

## 七、SLUB Challenge

### P13：生成 SLUB 实现

**负责人：**刘国全
**对应文件：**`code/kern/mm/slub.c`、`slub.h`

```text
[PROMPT]

任务：
在 Lab2 的物理页分配器之上实现简化的 SLUB 对象分配器。

操作要求：
新增 slub.c/slub.h，并在 pmm_init() 的 check_alloc_page() 之后执行
slub_init() 和 slub_check()；不能使用宿主机 malloc/free；不能修改课程测试。

输出要求：
实现对象分配、对象释放、空 slab 页面回收和超大对象多页分配，并通过自检。

[RELY]

- PGSIZE、struct Page、page2pa、pa2page、va_pa_offset
- alloc_pages/free_pages
- nr_free_pages
- assert、cprintf、size_t/uintptr_t

[GUARANTEE]

void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);

[SPECIFICATION]

Pre-Condition：
pmm_manager 已完成初始化并能分配物理页。

Post-Condition：
1. 小对象按 8 到 2048 字节分类；
2. 每个 slab 占用一个物理页，页头保存空闲对象链表和使用计数；
3. 分配对象地址至少 16 字节对齐，size==0 返回 NULL；
4. 空 slab 释放回 PMM；
5. 大于 2048 字节的请求分配连续多页并保存页数；
6. slub_free(NULL) 无副作用，非法释放通过断言阻止；
7. 自检结束后的 nr_free_pages() 恢复基线。

Failure：
物理页不足或请求大小溢出时返回 NULL；重复释放和 magic 不匹配由断言阻止。
```

### P14：生成 SLUB 测试

**负责人：**刘国全

```text
[PROMPT]
为 slub.c 设计 slub_check()，覆盖 0 字节、96 个 64 字节跨 slab 分配、地址唯一和
对齐、100 字节数据完整性、2048 字节边界、5000 字节多页对象、释放后复用和空闲页
计数恢复。测试只通过 public SLUB API 和 nr_free_pages() 验证。
[RELY]
slub.c、slub.h、pmm.h、assert.h、stdio.h。
[GUARANTEE]
void slub_check(void);
[SPECIFICATION]
所有测试成功时输出 [SLUB] self-check: PASS；失败时 assert 中止。
```

**迭代与 Bug：**

- 第一版需要明确大对象释放路径，最终通过页首 large header 保存页数和 payload offset；
- 需要保证空 slab 从 cache 链表移除后再归还页面；
- 用 96 个 64 字节对象验证一个 slab 不够时会创建多个 slab；
- 所有对象释放后验证 `nr_free_pages()` 回到基线。

**结果：**`[SLUB] self-check: PASS`，随后 `make grade` 仍为 20/20。

---

## 八、报告与复盘类 Prompt

### P15：整理最终报告

**负责人：**全体成员

```text
[PROMPT]
根据最终代码、课程测试输出、GDB 日志和上述 Prompt，整理 Lab2 实验报告。

[RELY]
report/report.md、report/prompt.md、report/images/、最终代码、make/qemu/grade 输出、
QEMU MMU 调试日志。

[GUARANTEE]
报告包含整体逻辑、核心模块、每个练习的 Spec/Prompt、Bug 与修复、OS 原理对比、
未覆盖知识点和测试截图。

[SPECIFICATION]
Pre-Condition：所有代码模块和测试已完成，截图路径已确定。
Post-Condition：报告无空章节、无占位符，所有图片引用存在，测试结果可复现。
Failure：发现空章节、错误路径或未记录 Bug 时必须补齐，不得用推测内容代替测试结果。
```

**结果：**形成完整的 `report/report.md`，并保留本文作为 Prompt 追溯记录。

---

## 九、刘国全：环境、集成与质量检查

### P16：配置并验证 RISC-V 工具链和 QEMU

**负责人：**刘国全

```text
[PROMPT]
配置 WSL 中的 Lab2 开发环境，安装或定位 riscv64-unknown-elf 工具链、
qemu-system-riscv64 和 GDB，配置 PATH/RISCV 环境变量，并验证可以编译和启动 ucore。

[RELY]
WSL2 Ubuntu、bash、~/.bashrc、SiFive RISC-V 工具链、QEMU 预编译包、
OpenSBI 固件、make、git。

[GUARANTEE]
不修改实验源码；交付可用的环境变量、工具路径、版本信息和验证命令。

[SPECIFICATION]
Pre-Condition：WSL 可以访问网络，用户目录可写。
Post-Condition：riscv64-unknown-elf-gcc/gdb、riscv64-unknown-elf-objcopy 和
qemu-system-riscv64 均可从 PATH 找到；QEMU 能加载 OpenSBI。
Failure：如果 QEMU 找不到 OpenSBI 固件，使用 -L 指向随包 share/qemu 目录；
如果工具链未生效，检查 RISCV 和 PATH 后重新 source ~/.bashrc。
```

**结果：**交叉编译、GDB 和 QEMU 均可用，后续 Lab2 构建和调试正常。

### P17：对照指导书盘点 Lab2 代码与待实现位置

**负责人：**刘国全

```text
[PROMPT]
对照 Lab2 指导书检查官方代码，列出内存初始化、First-Fit、Best-Fit、Buddy System
涉及的文件、核心函数、已有实现和 YOUR CODE 占位位置，并标出哪些任务会阻塞其他人。

[RELY]
code/Makefile、kern/init、kern/driver/dtb.c、kern/mm/pmm.c、
default_pmm.c、best_fit_pmm.c、pmm.h、test_lab2.c。

[GUARANTEE]
输出文件职责表、待实现函数表、模块依赖关系和责任人；不修改代码。

[SPECIFICATION]
Pre-Condition：lab2 官方代码已导入 code/。
Post-Condition：所有 YOUR CODE 和未提供框架的扩展任务都被识别，阻塞关系明确。
Failure：不得把已有功能误报为待实现；无法确定的接口必须回到源码核对。
```

**结果：**确认 `page_init()` 和 Best-Fit 是主要代码入口，Buddy/SLUB 为扩展模块。

### P18：集成 Best-Fit、Buddy 和 SLUB

**负责人：**刘国全

```text
[PROMPT]
把组员完成的 Best-Fit、Buddy System 和 SLUB 代码集成到 lab2 分支，保持
page_init()、课程测试和 pmm_manager 接口不变；处理重复定义、头文件引用和管理器切换，
最终只启用 Best-Fit 作为正式 pmm_manager。

[RELY]
pmm.c、pmm.h、best_fit_pmm.c/.h、buddy_pmm.c/.h、slub.c/.h、Makefile 自动源文件扫描。

[GUARANTEE]
保留课程测试函数；修复必要的 include 和链接关系；不改变分配器公共语义；
提交的代码中不得留下 YOUR CODE。

[SPECIFICATION]
Pre-Condition：三个模块已能在各自临时环境中编译。
Post-Condition：完整 lab2 分支一次 make 成功，正式 pmm_manager 为 best_fit_pmm_manager，
Buddy 可独立自检，SLUB 能使用当前页分配器。
Failure：出现重复符号时定位共同全局变量；Buddy 未启用时不得改变 Best-Fit 的评分结果。
```

**结果：**Best-Fit 保持正式路径，Buddy 自检独立运行，SLUB 通过页分配接口接入。

### P19：执行完整回归测试

**负责人：**刘国全

```text
[PROMPT]
执行 Lab2 的完整回归测试：make clean、make、make qemu、make grade 和 SLUB 自检。
记录每一步的真实输出；若某一步失败，先定位编译/链接/断言错误再修改，不修改课程测试来规避失败。

[RELY]
code/Makefile、tools/grade.sh、tools/test_lab2.c、QEMU、RISC-V GDB、模拟器输出。

[GUARANTEE]
输出可复现的测试结果和失败日志；最终代码保持可编译、可启动、可评分。

[SPECIFICATION]
Pre-Condition：所有模块已集成，工作区干净。
Post-Condition：生成 bin/kernel 和 bin/ucore.img；QEMU 输出 DTB 与 PMM 信息；
make grade 显示 page init 和 Best-Fit 均通过；SLUB 自检输出 PASS。
Failure：若测试失败，记录具体断言、复现命令和修复后的重新测试结果。
```

**实际结果：**

```text
make: success
make qemu: success
-check page init: OK
-check best fit:  OK
Total Score: 20/20
[SLUB] self-check: PASS
```

### P20：对照指导书做完成度审计

**负责人：**刘国全

```text
[PROMPT]
逐条对照 Lab2 指导书，检查必做练习、Buddy System、SLUB Challenge、物理内存探测
Challenge、QEMU MMU 分支任务和报告要求是否完成。输出状态表，并区分已完成、未完成、
选做和助教负责的内容。

[RELY]
Lab2 指导书目录、实验内容、练习页、报告要求页、仓库 lab2 分支、report/report.md、
report/prompt.md、代码和测试输出。

[GUARANTEE]
输出逐项完成度表；对未完成项说明原因和后续动作；不得把关注项误写成已完成。

[SPECIFICATION]
Pre-Condition：代码与报告处于最终候选版本。
Post-Condition：每个指导书要求都有状态、证据和对应文件/命令。
Failure：发现空章节、占位符、缺失截图或未执行测试时，必须先补齐再标记完成。
```

**结果：**完成必做任务、两个 Challenge、QEMU MMU 分支任务、报告和 Prompt 的最终审计。

---

## 十、复盘

### 效果较好的提示方式

1. 先给出真实框架和接口，再描述行为；
2. 明确地址边界、页对齐、资源保留和失败路径；
3. 要求实现真实文件，并明确禁止修改测试；
4. 用可验证的断言和 QEMU 输出检验结果；
5. 把 AI 错误和修正过程记录下来，而不是只保留最终成功版本。

### 主要问题与修正

- 只给函数名会导致实现遗漏地址转换和边界条件；
- 不提供真实宏和结构体会导致编译错误；
- 不说明非法输入和空空闲区会导致测试失败；
- 不记录 Prompt 迭代会导致报告无法解释代码来源；
- 只观察 QEMU monitor 不足以完成源码级 MMU 调试，最终通过带符号 QEMU 和宿主机 GDB 完成验证。
