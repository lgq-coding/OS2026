# Lab2 Prompt 汇总

本文件用于按时间顺序汇总 Lab2 期间使用的所有 Prompt。

<!-- 后续在此按“Prompt 内容 + 使用目的 + 结果说明”的格式补充。 -->

## 王乐妍：

### Prompt 1：梳理项目与分工边界

**内容：** 帮我梳理 Lab2 项目，简述作业要求、各文件作用、以及小组其他成员负责的模块，简洁一点

**使用目的：** 快速建立实验全貌与分工边界，明确自己负责 First-Fit 分析与 Buddy System 两部分。

**结果：** 确认 PMM 初始化链路与各分配器文件归属，厘清与刘国全、刘爽的模块边界。

---

### Prompt 2：分析 First-Fit 并回答改进空间

**内容：** 阅读 kern/mm/default_pmm.c，说明各个主要函数的职责，并据此分析 First-Fit 如何初始化空闲页、分配连续页、回收并合并相邻块，分析其优劣

**使用目的：** 完成 First-Fit 思考题，同时作为实现 Buddy 前对参考实现的对照。

**结果：** 形成 First-Fit 分配/回收/合并的完整分析，并给出"按大小分组、最小可满足查找"等改进方向。

---

### Prompt 3：理解 Buddy 在内核中的位置与调用关系

**内容：** 阐述内核从初始化到页的分配和释放的整体调用流程，并说明 Buddy System 作为 pmm_manager 实例在其中的位置和调用关系。

**使用目的：** 弄清 Buddy 与 PMM 框架的对接方式，为按 pmm_manager 接口实现做准备。

**结果：** 确认 Buddy 需注册为 pmm_manager 实例，经 init_memmap 接收初始空闲区、经 alloc/free 对外服务。

---

### Prompt 4：理解 Buddy 算法逻辑

**内容：** 系统阐述 Buddy System 的算法逻辑

**使用目的：** 掌握分裂与合并的核心规则，明确实现要点。

**结果：** 明确 order 划分、伙伴索引与合并条件，了解 Buddy System 按 order 维护 2 的幂空闲块、分配时的向上取整与逐级分裂、释放时的伙伴定位与逐级合并逻辑。

---

### Prompt 5：核对要实现的接口清单

**内容：** 我需要准备实现 buddy system，它是一个 pmm_manager 新实例，我整理了它需要实现的接口如下，这些概括准确完整吗：“初始化内存分配器（init）、接收空闲页基地址并把相关结构初始化（init_memmap）、接收需求大小并按 buddy 相关算法分配相应大小的块（alloc_pages）、按 buddy 算法释放特定的块（free_pages）、获取空闲页数（nr_free_pages）、进行自检（check）”

**使用目的：** 确保项目理解正确，确认接口清单与 pmm_manager 完全对应。

**结果：** 确认六个接口即 pmm_manager 全部成员的职责和作用。

---

### Prompt 6：生成 Buddy 实现

**内容：** （见 report.md 4.2.3 提示词一）。

**使用目的：** 让 AI 生成 buddy_pmm.c / buddy_pmm.h 并按 pmm_manager 接入，check 留空占位。

**结果：** 生成可编译的 Buddy 模块（第一版）。

---

### Prompt 7：审查实现逻辑

**内容：** 审查当前 buddy_pmm.c，重点检查 init_memmap 对未按大块对齐空闲区的切分、alloc 的向上取整与逐级分裂、free 的伙伴合并条件，指出遗漏或隐患。

**使用目的：** 验证生成代码的正确性，重点核对边界处理。

**结果：** 发现 3 处欠缺：init_memmap 未处理未对齐起点、free 无法释放非 2 的幂请求的完整块、alloc 结果未自带块大小信息。

---

### Prompt 8：修复实现的三处欠缺

**内容：** 目前的 buddy 存在问题： init_memmap 未对齐起点切分、free_pages 非 2 的幂释放、alloc_pages 块大小自证，按这三点修改 buddy_pmm.c

**使用目的：** 修正第一版实现的边界问题。

**结果：** 修正后编译通过，手工验证分配、释放与合并正常。

---

### Prompt 9：生成 Buddy 自检

**内容：** （见 report.md 4.2.3 提示词二）。

**使用目的：** 让 AI 补全 buddy_check()，覆盖初始状态、分配释放、拆分合并、空间不足、碎片化、计数与状态恢复。

**结果：** 生成 Buddy 自检（第一版）。

---

### Prompt 10：审查测试覆盖

**内容：** 检查 buddy_check 是否完整覆盖边界情况：初始状态、超量请求、非 2 的幂分配与释放、伙伴合并逻辑、状态恢复等，确认异常路径是否通过返回 NULL 或 assert 处理

**使用目的：** 验证自检是否真正能检验 Buddy 实现。

**结果：** 发现 6 处遗漏：未真正验证伙伴合并、复用未断言同一页、未覆盖"空间足够但无合适块返回 NULL"、未查非伙伴误合并、未走公共 nr_free_pages、未查本应合并却分开的伙伴。

---

### Prompt 11：修复自检的六处遗漏

**内容：** 针对上述 6 点完善 buddy_check

**使用目的：** 让自检真正覆盖伙伴合并与碎片化等核心行为。

**结果：** 6 点全部覆盖，自检通过，测试后空闲页数恢复基线。

---

### Prompt 12：整理 Buddy 报告

**内容：** 根据 Buddy 模块的最终代码、两份提示词与迭代记录，整理报告 4.2.3（模块功能描述、最终提示词、实现迭代、测试覆盖说明），并汇总本实验的 Prompt。

**使用目的：** 完成报告与 Prompt 交付物。

**结果：** 形成 4.2.3 与 prompt.md 的完整内容。

---

---

## 刘国全：SLUB Challenge

### Prompt 13：生成 SLUB 对象分配模块

**内容：**

> 在 ucore Lab2 的 `kern/mm` 下从零实现简化 SLUB：小对象按 8 到 2048 字节分类、使用单页 slab 和对象空闲链表；大对象按连续多页分配；释放空 slab 归还页面；提供 `slub_init`、`slub_alloc`、`slub_free`、`slub_check`，并接入 `pmm_init`。

**使用目的：**完成 SLUB Challenge 的第一层和第二层分配结构。

**结果：**实现 `slub.c`、`slub.h`，使用现有 Best-Fit 页分配接口，编译和 QEMU 自检通过。

### Prompt 14：生成 SLUB 测试

**内容：**

> 为 `slub.c` 设计 `slub_check()`，覆盖零大小、不同大小对象、96 个对象跨 slab 分配、地址唯一和对齐、数据完整性、2048 字节边界、5000 字节多页对象、对象复用以及释放后空闲页计数恢复。

**使用目的：**验证 SLUB 的对象分配、释放和页面回收是否正确。

**结果：**QEMU 输出 `[SLUB] self-check: PASS`，之后 `make grade` 仍保持 20/20。

---

## 刘国全：`page_init()`

### Prompt 15：实现物理内存探测与 Page 描述符布局

**内容：**

> 在 `kern/mm/pmm.c` 中实现 `page_init()`：读取 DTB 的物理内存范围，设置 `va_pa_offset` 和 `npage`，在 `end` 后建立 Page 描述符数组，保留固件、内核和描述符页面，并把页对齐的完整空闲区交给 `init_memmap()`。

**使用目的：**完成 Lab2 物理内存初始化功能。

**结果：**课程 `test_page_init()` 输出 `[Test 1] Page Init: PASS`，默认 PMM 链路可正常分配和释放页面。

## 刘爽：Best-Fit

### Prompt 16：实现并接入 Best-Fit

**内容：**

> 参考 `default_pmm.c` 在 `best_fit_pmm.c` 中实现 Best-Fit 管理器，扫描完整空闲链表选择剩余空间最小且满足请求的页块，完成拆分、释放和相邻块合并，并在 `init_pmm_manager()` 中切换到 `best_fit_pmm_manager`，不修改课程测试。

**使用目的：**完成 Best-Fit 连续物理内存分配功能。

**结果：**课程检查输出 `[Test 2] Best-Fit: PASS`，总分为 20/20。
