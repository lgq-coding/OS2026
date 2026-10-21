# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab2：物理内存和页表 |
| **小组成员** | 2411822-刘国全、2411350-刘爽、2413423-王乐妍 |
| **完成日期** | 2026-10-21 |

### 小组分工

#### 练习分工：

| 成员 | 负责的练习/模块 |
|------|----------------|
| 刘国全 | 物理内存探测、`page_init()`、PMM 初始化框架与代码集成 |
| 刘爽 | Best-Fit 分配器、分配器切换、GDB/QEMU 调试 |
| 王乐妍 | First-Fit 分析、Buddy System、Prompt 与实验报告整理 |

#### 报告分工：

| 报告部分 | 负责人 | 协作人 |
|----------|--------|--------|
| 实验目的、实验环境 | 王乐妍 | 全员 |
| 物理内存初始化 | 刘国全 | 刘爽 |
| First-Fit 分析 | 王乐妍 | 刘国全 |
| Best-Fit 实现与测试 | 刘爽 | 王乐妍 |
| Buddy System 与测试 | 王乐妍 | 刘爽 |
| QEMU/GDB 调试 | 刘爽 | 刘国全 |
| Prompt、原理总结与报告校对 | 王乐妍 | 全员 |

---

## 一、实验目的

本实验的主要目的是：

1. 理解操作系统从设备树发现物理内存，并通过 `Page` 描述符组织物理页的基本方法。
2. 掌握 First-Fit 和 Best-Fit 连续物理页分配器的空闲链表维护、页块拆分与释放合并流程。
3. 通过实现多 order 空闲链表、2 的幂次块拆分和伙伴合并，理解 Buddy System 的设计思想及外部碎片问题。
4. 学会以任务、上下文、接口、约束和验证方式组织 Prompt，并对 AI 生成的底层代码进行审查与测试。

---

## 二、实验环境

实验在 WSL2 的 Ubuntu 24.04.1 LTS 环境中完成，目标架构为 RISC-V 64 位。

主要工具包括 GNU Make、Python 3、`riscv64-unknown-elf-` 交叉编译工具链、`qemu-system-riscv64` 和 GDB。内核以 Sv39 分页方式运行，物理内存范围和页描述符布局由启动阶段及 DTB 初始化结果确定。


#### AI 工具配置

| 成员 | AI 编程工具 | 底层模型 | 配置 |
|------|------------|---------|-------------|
| 刘国全 | 待确认 | 待确认 | 待确认 |
| 刘爽 | 待确认 | 待确认 | 待确认 |
| 王乐妍 | Claude Code | DeepSeek flash | 推理强度 high |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本实验围绕“内核如何发现、组织和分配物理内存”展开。内核启动后先建立高地址虚拟映射，接着从设备树获取物理内存范围；`page_init()` 根据内核结束位置建立 `Page` 描述符数组，保留 OpenSBI、内核镜像、启动页表和描述符数组占用的页面，再把剩余连续空闲页交给当前 `pmm_manager`。之后的页面申请和释放统一通过 `alloc_pages()`、`free_pages()` 调用具体分配器。

实验在已有 First-Fit 的基础上实现 Best-Fit，并进一步使用 2 的幂次块、伙伴索引和逐级合并实现 Buddy System。完整主链路如下：

```text
entry.S
  -> kern_init()
  -> dtb_init()
  -> pmm_init()
  -> init_pmm_manager()
  -> page_init()
  -> init_memmap()
  -> check_alloc_page()
  -> alloc_pages() / free_pages()
```

其中，`page_init()` 解决“哪些物理页可以管理”，PMM 管理器解决“如何把可用页分配给调用者”，Best-Fit 和 Buddy System 则是对分配策略的不同实现。启动页表同时建立了内核虚拟地址和物理地址之间的固定偏移关系，为后续页表调试和更高地址运行提供基础。

### 3.2 功能的逐步实现

1. **建立启动地址环境。** 启动汇编建立 Sv39 页表并设置 `satp`，使内核从高虚拟地址继续执行，同时保持内核虚拟地址与物理地址之间可计算的固定偏移。
2. **探测物理内存。** `dtb_init()` 从设备树获取内存基地址和大小，确定当前 QEMU 环境中可管理的物理内存范围。
3. **建立页描述符。** 在 `end` 之后放置 `struct Page` 数组，为每个可管理物理页保存引用计数、保留状态和空闲块信息。
4. **注册初始空闲区。** 排除固件、内核和描述符数组所占区域后，把页对齐的完整空闲页交给 `init_memmap()`。
5. **分析并实现分配策略。** 先通过 First-Fit 理解地址有序链表、页块拆分和左右合并，再实现 Best-Fit 并切换 `pmm_manager`。
6. **扩展 Buddy System。** 使用多个 order 的空闲链表维护 2 的幂次块，完成按 order 向上查找、逐层拆分、伙伴计算和释放合并。
7. **验证管理链路。** 通过 `check_alloc_page()`、课程注入测试和 Buddy 自检验证初始化、分配、释放及计数是否正确。

---

## 四、实验内容与实现

### 4.1 物理内存初始化功能

#### 4.1.1实现物理内存探测与页描述符初始化功能

---

### 4.2 物理内存分配功能

#### 4.2.1 First-Fit 连续物理内存分配

`kern/mm/default_pmm.c` 中使用地址有序的空闲页块链表管理物理页。主要函数如下：

```c
static void default_init(void);
static void default_init_memmap(struct Page *base, size_t n);
static struct Page *default_alloc_pages(size_t n);
static void default_free_pages(struct Page *base, size_t n);
static size_t default_nr_free_pages(void);
```

`default_init()` 初始化链表头并将空闲页计数清零。`default_init_memmap()` 清除一段连续页的描述符状态，把该连续空闲区的首描述符设为 `PageProperty`，用 `property` 记录页块大小，再按照物理地址顺序插入空闲链表。

申请页面时，`default_alloc_pages()` 从链表头开始扫描，选择第一个可用页数不少于请求页数的页块。若页块恰好等于请求大小，就将其整体移出；若页块更大，则保留前 `n` 页作为分配结果，把剩余部分建立为新的空闲块。释放页面时，`default_free_pages()` 将页块重新插入地址有序链表，并分别检查左右相邻块；只要相邻页块的尾地址与当前块首地址连续，就合并为一个更大的空闲块。这样能够减少空闲区数量并降低后续大块分配失败的概率。

First-Fit 的优点是实现简单，优先利用低地址区域，且地址有序链表便于合并。它的局限是需要线性扫描链表，并且可能不断切分较大的空闲块，留下大量较小碎片。

优化思路可以为：维护按大小分组的空闲链表、从最小可满足大小开始查找，或者增加大小索引以避免无效扫描；也可以采用 Best-Fit 选择剩余空间最小的页块来降低单次分配产生的碎片。

---

#### 4.2.2 实现 Best-Fit 连续物理内存分配算法


---

#### 4.2.3 Buddy System 连续物理内存分配

##### 模块功能描述

需要实现和接入的函数如下：

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
extern const struct pmm_manager buddy_pmm_manager;
```

Buddy System 将空闲内存划分为页数始终为 `2^k` 的块，并要求 order 为 `k` 的块首地址按 `2^k` 页对齐。实现使用 `buddy_free_area[0..20]` 分别维护各个 order 的空闲块链表；页块头设置 `PageProperty`，`property` 表示块大小，额外标志位区分“已分配页”和“分配块头页”。因此，分配块可以在块头保存实际大小，释放时不必只依赖调用者传入的页数。

`buddy_init_memmap()` 首先把描述符转为初始状态，再将一段可能未按大块对齐的空闲区分割成若干个最大的、地址符合要求的 2 的幂次块。分配时，请求页数先向上取整为 2 的幂；若当前 order 无空闲块，就向更高 order 查找并逐层对半拆分，保留低半块继续拆分，把高半块放回相应链表。释放时通过页块索引异或块大小定位伙伴，并检查伙伴是否同大小、同 order、未被保留且位于空闲链表；条件满足时反复合并并提升 order。

Buddy 模块已经按 `struct pmm_manager` 接口与 PMM 框架对接，Buddy System 通过切换管理器和调用其自检函数完成独立验证，具体过程见 5.3 节。

##### 提示词

###### 提示词一：实现 Buddy System

[PROMPT]

**任务**：在 `code/kern/mm/` 下新建 `buddy_pmm.c` 和 `buddy_pmm.h`，实现 Buddy System 分配模块并按 `struct pmm_manager` 接口接入内核。本次只实现核心分配逻辑，`buddy_check()` 留空占位，不写任何测试。

**操作要求**：必须在项目中修改实际文件，而非只展示代码。不得修改 `buddy_pmm.c/.h` 之外的任何文件，不得改动课程测试、`basic_check()`、`best_fit_check()`、`page_init()` 或其他分配器。可临时在 `pmm.c` 的 `init_pmm_manager()` 切换到 `buddy_pmm_manager` 检查编译，最终必须恢复为 Best-Fit。

**输出要求**：以 `[RELY]` 中的真实定义为准实现 `[GUARANTEE]` 列出的接口；与 `[SPECIFICATION]` 冲突时以 `[SPECIFICATION]` 为准。不引入动态内存、文件系统等无关功能。

[RELY]

```c
// pmm.h （管理器接口）
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

// memlayout.h （页描述符与状态）
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;   // 空闲块/分配块的页数
    list_entry_t page_link;
};
#define PG_reserved 0
#define PG_property 1
#define SetPageReserved(page)   ((page)->flags |=  (1UL << PG_reserved))
#define ClearPageReserved(page) ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)      (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)   ((page)->flags |=  (1UL << PG_property))
#define ClearPageProperty(page) ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)      (((page)->flags >> PG_property) & 1)
typedef struct { list_entry_t free_list; unsigned int nr_free; } free_area_t;
#define le2page(le, member) to_struct((le), struct Page, member)

// pmm.h （全局变量与地址转换）
extern struct Page *pages;      // 描述符 pages[PPN(pa) - nbase]
extern size_t npage;
extern const size_t nbase;
static inline ppn_t page2ppn(struct Page *p) { return p - pages + nbase; }
static inline uintptr_t page2pa(struct Page *p) { return page2ppn(p) << PGSHIFT; }
static inline struct Page *pa2page(uintptr_t pa) { return &pages[PPN(pa) - nbase]; }

// mmu.h
#define PGSIZE 4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)
```

```text
list.h 提供 list_init / list_add / list_add_after / list_del / list_del_init /
list_empty / list_next / list_prev。
初始化链路：pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap(base, n) -> check()。
page_init() 只把完整空闲页交给 init_memmap()；进入时这些页均为 PageReserved、ref=0、property=0。
assert/panic 可用；kern/mm/ 下新增 .c 由 Makefile 自动扫描编译。
```

[GUARANTEE]

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);          // 本阶段空占位
extern const struct pmm_manager buddy_pmm_manager;
```

模块内部状态须满足以下约定（后续自检依赖）：
- 按 order 维护一组空闲链表，order k 只存 `2^k` 页的块，order 0 为单页块；
- 空闲块仅块首入链，块首 `PageProperty` 置位、`property` 记块页数；
- 已分配块用额外标志位区分“已分配页 / 分配块首”，块内每页 `property` 都记实际块页数；
- 模块维护当前空闲页总数与初始登记总页数。

[SPECIFICATION]

- **buddy_init**：清空各 order 链表与计数，进入可接收 `init_memmap` 的干净状态，不改任何页描述符。
- **buddy_init_memmap**：把 `[base, base+n)` 完整拆成若干合法 Buddy 块（2 的幂、按块大小对齐）。起点未必按大块对齐，须向下取对齐逐块切分，不得遗漏、重叠或越界；非法参数或页状态不符时终止，不留中间状态。
- **buddy_alloc_pages**：n 向上取整到最小 2 的幂，本 order 不足时向高 order 查找并逐层对半拆分（保留低半、回挂高半）；成功返回对齐块首，块内每页记录实际块大小供释放时自证；n=0、溢出或空间不足返回 `NULL` 且状态不变；必须返回连续页，不得拼凑小块。
- **buddy_free_pages**：验证 base 为分配块首且页数匹配后整块释放，再与空闲伙伴逐级合并（同 order、同对齐、未保留、在空闲链表）；非 2 的幂请求（如 `alloc_pages(5)` 后 `free_pages(base, 5)`）须能释放向上取整后的完整块；非法参数终止，不得部分释放。
- **buddy_nr_free_pages**：返回当前空闲页总数，每次分配/释放后保持准确。
- **buddy_check**：本阶段空占位，不执行测试。

###### 提示词二：为 Buddy System 设计自检

[PROMPT]

**任务**：补全 `buddy_pmm.c` 中的 `buddy_check()`，覆盖初始状态、分配与释放、块拆分与伙伴合并、空间不足、碎片化、空闲页计数及测试后状态恢复。

**操作要求**：只修改 `buddy_pmm.c`，不得改动分配器主体、框架接口或课程测试。先阅读当前实现与 `default_pmm.c` 的 `basic_check()` 写法，按实际结构选择测试手段。测试失败用 `assert` 报告；运行自检时可临时切换 `pmm_manager` 为 `buddy_pmm_manager`，完成后恢复 Best-Fit。

**输出要求**：直接更新实际文件；测试结束后释放其创建的全部页面，恢复可用状态；不依赖动态内存或额外测试模块。

[RELY]

```c
// pmm.h （对外入口）
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
#define alloc_page()  alloc_pages(1)
#define free_page(p)  free_pages(p, 1)
```

待测模块内部状态遵循提示词一 [GUARANTEE] 的约定。运行时机：pmm_init() 在 init_memmap() 之后调用 check()，进入 buddy_check() 时全部空闲页已按 Buddy 结构登记完毕。参考：default_pmm.c 的 basic_check()。

[GUARANTEE]

```c
static void buddy_check(void);
```

可添加 `static` 测试辅助函数，但不得改变 `buddy_pmm_manager` 的接口绑定或新增公开测试接口。

[SPECIFICATION]

- **初始状态**：空闲结构完整覆盖已登记页，块均为 2 的幂且对齐，空闲页总数与各块实际页数一致。
- **请求边界**：0 页与超量请求返回 `NULL`，失败后空闲页数与状态不变。
- **拆分与释放**：非 2 的幂请求被向上取整，确认块大小与对齐正确、空闲页按实际块大小扣减；用原请求页数释放，确认整块恢复。
- **碎片化**：构造两个物理相邻但不互为伙伴的单页块，确认页数足够仍无法满足 2 页请求；释放后复用必须返回刚释放的页。
- **伙伴合并**：构造可逐级合并的相邻伙伴，验证释放后按实际伙伴关系逐级合并，且不误合并已分配或非伙伴块。
- **状态恢复**：释放全部测试分配，空闲页总数恢复基线，并复查各 order 计数、块大小与对齐、无重复登记、无仍可合并的同级伙伴；计数统一经公共 `nr_free_pages()` 与内部计数交叉验证。

##### 实现迭代与问题记录

提示词一的迭代：

使用提示词一生成 Buddy 模块后，人工审查发现实现有三个欠缺：

1. `buddy_init_memmap` 未约定起点未按大块对齐的处理，实现按“整段可被 2 的幂整除”假设登记，对 `page_init()` 实际传入的空闲区产生遗漏和越界；
2. `buddy_free_pages` 未约定非 2 的幂请求的释放，`alloc_pages(5)` 实际占用 8 页块后，`free_pages(base, 5)` 无法释放完整块；
3. `buddy_alloc_pages` 未要求分配结果自带块大小信息，释放路径无法自证实际块大小。

随后直接基于源代码向 AI 提出修改要求，补齐 `buddy_init_memmap` 的未对齐起点切分、`buddy_free_pages` 的非 2 的幂释放、`buddy_alloc_pages` 的块大小自证。修正后编译通过，手工验证分配、释放与合并正常。

提示词二的迭代：

使用提示词二生成自检后，经与其他 AI 共同审查发现几处遗漏：

1. 未真正验证伙伴合并——释放后只查空闲总数与块自身状态，即使 `free_pages()` 完全不合并也可能通过；
2. 分段释放后的复用未断言同一页面——只断言分配成功，未检查返回的是刚释放的块；
3. 未覆盖“总空闲页足够，但无合适大小或对齐的连续块时应返回 `NULL`”；
4. 未检查非伙伴块不会被错误合并；
5. 未通过公共 `nr_free_pages()` 交叉验证，只直接读了内部计数；
6. 空闲链表检查只验证每个块自身，未检查是否存在本应合并却仍分开的伙伴块。

随后直接基于源代码补强 `buddy_check`：构造相邻非伙伴单页断言“页数够仍分配失败、复用返回刚释放页”（覆盖 2、3、4），各用例空闲计数统一经 `nr_free_pages()` 交叉验证（覆盖 5），状态恢复时复查“不存在仍可合并的同级伙伴”（覆盖 1、6）。

##### 测试覆盖说明

| 环节 | 覆盖点 |
|------|--------|
| 初始状态 | 空闲区被完整覆盖，块均为 2 的幂且对齐，各 order 计数与空闲页总数一致 |
| 请求边界 | 0 页与超量请求返回 `NULL`，失败后管理状态不变 |
| 拆分与释放 | 5 页请求取整为 8 页对齐块，可按原请求页数释放整块 |
| 碎片化 | 相邻但不互为伙伴的两个单页块，页数足够仍无法满足 2 页请求 |
| 伙伴合并 | 连续单页逐级合并，无遗漏可合并的伙伴、无误合并非伙伴块 |
| 状态恢复 | 全量释放后空闲计数与链表结构恢复为进入自检时的基线 |

为确保这些测试的有效性，各环节校验返回值与内部状态（链表归属、`property`、分配标记），任何拆分或合并的记账错误都在结构不变量上暴露；测试以进入自检时的空闲页数为基线，适应非对齐的初始空闲区，不依赖固定布局；碎片化用例构造“页数够但结构不满足”的场景，正是 Buddy 区别于 First-Fit/Best-Fit 的本质特征。非法释放参数会触发内核断言，自检不主动调用，该路径由实现侧“先验证后修改”的断言策略保证。

---

## 五、测试与验证

### 5.1 编译与 QEMU 启动

在 `code/` 目录中清理并重新构建：

```bash
cd ~/OS2026/code
make clean
make
ls -l bin
```

构建阶段应成功生成 `bin/kernel` 和 `bin/ucore.img`。

![Lab2 编译成功](./images/01_make.png)

随后启动 QEMU：

```bash
make qemu
```

内核输出物理内存范围、当前物理内存管理器以及页面分配自检结果。

![Lab2 内核在 QEMU 中启动](./images/02_qemu.png)

### 5.2 课程检查

首次执行 `make grade` 时，课程脚本在 WSL 中报告 `Syntax error: ")" unexpected (expecting "then")`，并伴随 `: not found.sh: 2` 等错位信息。检查发现，工作区启用了 Git 的 `core.autocrlf=true`，因此仓库中以 LF 保存的 `tools/grade.sh` 在 Windows 工作区中被检出为 CRLF。WSL 中的 `/bin/sh` 会把行尾的 `\r` 当作脚本内容，导致 `if ... then` 等语法无法正确解析。

在 WSL 中转换脚本换行符并检查语法后重新执行：

```bash
cd "OS2026/code"
sed -i 's/\r$//' tools/grade.sh
bash -n tools/grade.sh
make grade
```

`bash -n` 无输出说明语法检查通过，`make grade` 输出显示：

```text
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

![课程检查结果](./images/03_make_grade.png)

这说明物理内存初始化和 Best-Fit 管理器切换均已通过课程检查。该换行符问题属于 Windows/WSL 工作区的文件换行符差异，不是课程脚本或内核代码逻辑错误。

### 5.3 Buddy System 自检

将当前物理内存管理器临时切换为 `buddy_pmm_manager`，重新执行 `make clean && make qemu`。`pmm_init()` 调用 `check_alloc_page()` 后，`buddy_check()` 会依次完成空闲链表检查、非 2 的幂请求测试、碎片化测试和多级伙伴合并测试。自检过程中没有触发断言并继续输出 `check_alloc_page() succeeded!`，即表示上述状态检查均通过。

![Buddy System 自检输出](./images/04_buddy_check.png)

该测试只用于独立验证 Buddy 模块，截图完成后恢复 Best-Fit 配置，不影响 5.2 节的课程检查结果。

---

## 六、实验总结与收获

### 对操作系统的理解

| 本实验中的知识点 | 对应 OS 原理 | 含义、关系与差异 |
|------------------|--------------|------------------|
| DTB 物理内存探测 | 硬件资源发现与内存管理初始化 | DTB 是启动阶段获取内存范围的具体机制；通用操作系统还会处理多段内存、保留区和不可用区。本实验确认了“先发现资源、再建立管理结构”的基本顺序。 |
| `struct Page` 描述符数组 | 物理页帧管理 | 本实验的 `Page` 记录引用数、保留、空闲块头和块大小等状态，是物理页元数据；它不是进程页表中的页表项，也不负责虚拟地址到物理地址的直接翻译。 |
| `alloc_pages()` / `free_pages()` | 连续物理页分配 | 实验实现的是内核内部的物理页级分配；一般 OS 还会提供 slab、伙伴系统分层、按 CPU 缓存和 NUMA 策略等更复杂的分配路径。 |
| First-Fit、Best-Fit、Buddy | 动态分区与碎片管理 | 三者都通过拆分空闲块满足请求。First-Fit 选地址上的第一个可用块，Best-Fit 选剩余空间最小的块，Buddy 则利用对齐和伙伴关系限制可合并块，侧重点从扫描策略转为结构性合并。 |
| 内核虚拟地址与 `va_pa_offset` | 地址空间与地址转换 | 内核通过高地址虚拟地址运行，启动页表建立固定偏移映射 |

### 本实验没有覆盖的重要 OS 知识点

本实验没有涉及：

- 进程与线程调度
- 进程地址空间隔离
- 按需分页与页面置换
- 写时复制 
- 同步与死锁
- SMP 下的并发分配和 TLB shootdown
- 文件系统和设备驱动
- 系统调用安全检查和内存热插拔等

这些机制通常建立在物理页管理之上，但本实验只覆盖了内存管理链条中的资源发现、页级分配和部分地址映射基础。

### AI 协作开发的经验

本次实验中，AI 主要用于：

- 梳理 First-Fit 的空闲链表维护、页块拆分和相邻块合并流程；
- 分析 Buddy System 的 order 划分、分裂与伙伴合并条件；
- 根据 uCore 的 `pmm_manager` 接口设计 Buddy 模块；
- 设计边界测试、定位状态断言失败的原因；
- 整理实验报告和 Prompt 记录。

实际使用中的主要经验如下：

1. Prompt 应说明框架接口、数据结构、状态不变量和失败场景，仅提出“实现一个伙伴系统”容易得到与现有代码不兼容的结果。
2. AI 生成的代码需要通过人工审查和断言验证。未对齐的初始空闲区、非 2 的幂请求的释放和“相邻但非伙伴”的页块，都是不能仅凭代码表面判断正确性的地方。
3. 先让 AI 解释方案和边界条件，再检查它引用的字段与函数，最后结合 QEMU 输出确认实际行为，比直接接受生成结果更可靠。
4. 测试失败时应定位到具体状态不变量，例如块对齐、链表计数或分配块头标记，而不是只要求 AI 将代码修改到能够运行。
5. Prompt、问题原因和修正结果应及时记录，便于复盘实现过程，也能保证报告描述与最终代码一致。

---

