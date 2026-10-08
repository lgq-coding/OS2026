# 操作系统实验报告

## 实验基本信息

| 项目       | 内容                         |
| -------- | -------------------------- |
| **实验名称** | Lab 1: RISC-V 启动流程与 GDB 调试 |
| **小组成员** | 2411350-刘爽、__学号2-姓名2__、__学号3-姓名3__ |
| **完成日期** | 2026-10-08                 |

### 小组分工

| 成员         | 负责的练习/模块                      |
| ---------- | ----------------------------- |
| 2411350-刘爽 | 环境搭建、编译运行、GDB 单步追踪上电流程、撰写实验报告 |
| __学号2-姓名2__ | __[待填写：负责的模块]__ |
| __学号3-姓名3__ | __[待填写：负责的模块]__ |

---

## 一、实验目的

本实验无需编写内核代码，核心目的是：

1. 理解 RISC-V 计算机从上电到操作系统内核第一条 C 语言代码执行的完整启动流程；
2. 掌握使用 GDB 单步调试 QEMU 模拟器的方法，能够观测 CPU 内部寄存器和 PC 的变化；
3. 理解 OpenSBI 固件在 RISC-V 启动中的作用，以及内核如何通过 `ecall` 陷入 SBI 完成串口输出；
4. 熟悉交叉编译工具链（riscv64-unknown-elf-gcc）与 QEMU RISC-V 模拟器的配合使用。

---

## 二、实验环境

| 项目      | 版本/说明                              |
| ------- | ---------------------------------- |
| 操作系统    | Windows + WSL2 Ubuntu              |
| 交叉编译器   | riscv64-unknown-elf-gcc (14.2.0)   |
| 模拟器     | QEMU 10.2.1 (qemu-system-riscv64)  |
| 内置固件    | OpenSBI v1.8                       |
| 调试器     | riscv64-unknown-elf-gdb (GDB 17.1) |
| 编辑器     | VS Code                            |

**AI 编程工具：**

| 成员            | AI 编程工具 | 底层模型 | 备注 |
| ------------- | --------- | ------ | -- |
| 2411350-刘爽    | 豆包（桌面端）   | 豆包大模型  | 环境问题定位、GDB 调试指导 |
| __学号2-姓名2__ | __[待填写]__  | __[待填写]__ | __[待填写]__ |
| __学号3-姓名3__ | __[待填写]__  | __[待填写]__ | __[待填写]__ |

---

## 三、实验整体逻辑分析

### 3.1 本章实验的核心主线

本次实验围绕"**计算机刚通电时到底发生了什么**"这一问题展开。我们没有写任何内核功能代码，而是把一个已经编译好的极简内核（只做了清零 BSS、打印一行字符串、然后死循环）放到 QEMU 里跑，并用 GDB 把 CPU 从复位那一刻开始的每一条指令都观察一遍。

解决的核心问题是：**操作系统内核是怎么被"启动"起来的？** 答案是：CPU 自己并不知道内核在哪，它上电后只知道一个固定地址；真正负责把内核拉起来的是固化在硬件里的一小段固件（OpenSBI），我们写的内核只是"接力赛的第三棒"。

### 3.2 启动流程的四个阶段

整个启动过程按顺序分为四段：

1. **Reset stub（0x1000）** → CPU 上电后硬件复位向量指向这里，是 QEMU virt 机器固化的一小段代码，负责读 CPU 号并跳转到 OpenSBI。
2. **OpenSBI 固件（0x80000000）** → 运行在最高特权级 M-mode，做内存保护、中断委托、时钟串口等硬件初始化，最后切到 S-mode 跳进我们的内核。
3. **kern_entry（0x80200000）** → 我们内核的第一条指令，用汇编把栈指针 `sp` 设置成内核栈顶 `bootstacktop`，然后 `tail` 跳到 C 入口。
4. **kern_init（0x8020000a）** → C 语言入口，清零 BSS 段、调用 `cprintf` 打印加载信息，最后进入死循环。

这个顺序不能乱：必须先有固件初始化硬件，内核才能碰串口；必须先设好栈，C 代码才能跑函数调用。

---

## 四、实验内容与实现

### 模块一：环境搭建与踩坑记录

**负责人：** 2411350-刘爽

#### 遇到的问题与解决

**问题 1：Windows 上没有 RISC-V 工具链**

Windows 原生无法直接编译和运行 RISC-V 程序。我使用 WSL2 Ubuntu 作为开发环境，通过 `sudo apt install` 安装了交叉编译器、QEMU 和 GDB。

**问题 2：装了 `qemu-system-misc` 后仍然找不到 `qemu-system-riscv64`**

执行 `sudo apt install qemu-system-misc` 后，`qemu-system-riscv64` 命令依然报 `not found`。原因是 Ubuntu 24.04 把 QEMU 按架构拆包了，`qemu-system-misc` 只包含 alpha、avr、loongarch 等冷门架构，RISC-V 单独在 `qemu-system-riscv` 包里。

解决：

```bash
sudo apt install -y qemu-system-riscv
sudo ln -sf /usr/bin/gdb-multiarch /usr/bin/riscv64-unknown-elf-gdb
```

**问题 3：`make qemu` 只打印了 OpenSBI banner，没有打印内核的 "(THU.CST) os is loading ..."**

用 GDB 配合 OpenSBI 启动日志发现关键一行：

```
Domain0 Next Address : 0x0000000000000000
```

OpenSBI 准备跳转的目标地址是 0！原因是课程给的 Makefile 用 `-device loader,file=ucore.img,addr=0x80200000` 这种方式加载内核，在旧版 QEMU/OpenSBI 下可行，但新版 OpenSBI v1.8 不知道 loader 设备塞进去的镜像，于是 Next Address 保持为 0。

解决：把 Makefile 中 `qemu` 和 `debug` 两个 target 里的

```
-device loader,file=$(UCOREIMG),addr=0x80200000
```

改成

```
-kernel $(UCOREIMG)
```

QEMU 会自动把内核加载到 0x80200000 并通过标准方式告知 OpenSBI。修改后 OpenSBI 日志变为：

```
Domain0 Next Address : 0x0000000080200000
Domain0 Next Mode    : S-mode
```

内核成功被跳转执行。

---

### 模块二：编译与直接运行

**负责人：** 2411350-刘爽

在 WSL 中进入 `~/lab1` 目录执行 `make`，编译输出：

```
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
+ cc kern/driver/console.c
+ cc libs/printfmt.c
+ cc libs/readline.c
+ cc libs/sbi.c
+ cc libs/string.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
```

生成 `bin/kernel`（带符号表的 ELF，供 GDB 使用）和 `bin/ucore.img`（纯二进制镜像，供 QEMU 加载）。

用 `nm` 查看关键符号地址：

```
0x80200000  T kern_entry
0x8020000a  T kern_init
0x80201000  D bootstack
0x80203000  D bootstacktop
0x80203008  D edata
0x80203008  D end
```

执行 `make qemu`，OpenSBI 打印 banner 后，内核串口输出：

```
(THU.CST) os is loading ...
```

随后进入 `while(1)` 死循环。

---

### 模块三：GDB 单步追踪上电流程（核心）

**负责人：** 2411350-刘爽

启动调试：一个终端执行 `make debug`（QEMU 加 `-s -S`，监听 1234 端口并启动即暂停），另一个终端执行 `make gdb` 连接。以下是单步观测到的真实数据。

#### 第 1 步：刚连上 GDB 时，CPU 停在复位向量

```
初始 PC = 0x1000
sp = 0x0

=> 0x1000:  auipc  t0,0x0
   0x1004:  addi   a2,t0,40
   0x1008:  csrr   a0,mhartid      # 读当前 HART ID
   0x100c:  ld     a1,32(t0)
   0x1010:  ld     t0,24(t0)
   0x1014:  jr     t0             # 跳转到 OpenSBI
```

**观察**：CPU 刚上电时 PC 并不是 0x80000000，而是 `0x1000`。这是 QEMU virt 机器的硬件复位向量。这段 stub 做两件事：读 `mhartid` 寄存器知道自己是几号 CPU，然后从旁边数据区读出 OpenSBI 的入口地址跳过去。此时 `sp=0`，栈还没设。

#### 第 2 步：在 `kern_entry` (0x80200000) 打断点并 continue

GDB 执行 `break *0x80200000` 后 continue，OpenSBI 完整跑完自己的初始化流程并打印 banner，最终：

```
Domain0 Next Address : 0x0000000080200000
Domain0 Next Mode    : S-mode
```

CPU 从 M-mode 切到 S-mode，跳进我们的内核。此时 GDB 报告：

```
>>> 到达 kern_entry! PC = 0x80200000
此时 sp = 0x80045e30     # 这是 OpenSBI 留下的临时栈
```

#### 第 3 步：单步执行 kern_entry 的两条指令

```
=> 0x80200000 <kern_entry>:  auipc  sp,0x3
   0x80200004 <kern_entry+4>: mv     sp,sp
   0x80200008 <kern_entry+8>: j      0x8020000a <kern_init>
```

单步执行后实测：

```
PC=0x80200004  sp=0x80203000   # 执行 la sp, bootstacktop 后
PC=0x80200008  sp=0x80203000   # sp 固定为我们的内核栈顶
PC=0x8020000a                  # tail kern_init 跳走
```

**观察**：`la sp, bootstacktop` 这条伪指令被编译成 `auipc sp,0x3; mv sp,sp` 两条，把 `sp` 从 OpenSBI 的临时栈 `0x80045e30` 切换成我们自己的 `0x80203000`。这就是内核接管硬件后做的第一件事——建立自己的栈。

#### 第 4 步：单步进入 kern_init

```
>>> 到达 kern_init! PC = 0x8020000a
=> 0x8020000a <kern_init>:   auipc  a0,0x3
   0x8020000e <kern_init+4>: addi   a0,a0,-2    # a0 = edata = 0x80203008
   0x80200012 <kern_init+8>: auipc  a2,0x3
   0x80200016 <kern_init+c>: addi   a2,a2,-10   # a2 = end   = 0x80203008
   0x8020001e <kern_init+14>: sub    a2,a2,a0
   0x80200022 <kern_init+18>: jal    memset
   0x80200036 <kern_init+2c>: jal    cprintf
   0x8020003a <kern_init+30>: j      0x8020003a   # while(1)
```

**观察**：

- `memset(edata, 0, end - edata)`：本次实验 `edata == end == 0x80203008`，长度为 0，所以这一步实际什么都没清（因为我们没定义任何全局初始化数据）。
- `jal cprintf` 执行完，串口就打印出了 `(THU.CST) os is loading ...`。
- 最后 `j .` 跳到自己，进入死循环，内核"启动完成"。

#### 第 5 步：cprintf 如何输出到串口

追进 `cprintf` → `printfmt` → `cons_putc` → `sbi_console_putchar`，最终在 `libs/sbi.c` 里看到：

```asm
mv  x17, sbi_type     # x17=SBI_CONSOLE_PUTCHAR=1
mv  x10, arg0         # x10=要打印的字符
ecall                 # 陷入 SBI，交还给 OpenSBI
mv  ret_val, x10
```

**观察**：内核自己不会写串口硬件，它通过 `ecall` 指令陷入 S-mode 之下的 M-mode OpenSBI，由 OpenSBI 代劳写 uart8250 寄存器。这就是 RISC-V 上"系统调用"的雏形。

---

### 模块四：__[学号2-姓名2]__ 负责部分（待补充）

**负责人：** __学号2-姓名2__

> 请在此处补充你负责的实验内容，格式参考模块一~三：
> - 你做了什么操作 / 遇到了什么问题 / 怎么解决
> - 关键 GDB 观察或命令
> - 相关截图

[待填写]

---

### 模块五：__[学号3-姓名3]__ 负责部分（待补充）

**负责人：** __学号3-姓名3__

> 请在此处补充你负责的实验内容。

[待填写]

---

## 五、测试与验证

### 5.1 直接运行 `make qemu`

OpenSBI v1.8 banner 之后，内核成功输出：

```
(THU.CST) os is loading ...
```

### 5.2 GDB 调试断点命中记录

| 断点地址       | 符号             | 命中时 PC     | 命中时 sp                |
| ---------- | -------------- | ---------- | --------------------- |
| 0x1000     | reset stub（初始） | 0x1000     | 0x0                   |
| 0x80200000 | kern_entry     | 0x80200000 | 0x80045e30（OpenSBI 栈） |
| 0x80200004 | kern_entry+4   | 0x80200004 | 0x80203000（已切到内核栈）    |
| 0x8020000a | kern_init      | 0x8020000a | 0x80203000            |

关键地址一览：

| 地址         | 含义                  |
| ---------- | ------------------- |
| 0x1000     | QEMU virt 硬件复位向量    |
| 0x80000000 | OpenSBI 固件基地址       |
| 0x80200000 | 内核加载地址 / kern_entry |
| 0x8020000a | kern_init（C 入口）     |
| 0x80203000 | bootstacktop（内核栈顶）  |
| 0x80203008 | edata / end（BSS 边界） |

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

1. **"启动悖论"的解决**：CPU 上电后不可能直接运行 OS，因为运行 OS 需要的驱动、文件系统都还没初始化。RISC-V 的做法是：硬件复位向量 → 一小段固化的 reset stub → OpenSBI 固件（M-mode）→ 我们的内核（S-mode）。OS 是被固件"拉起来"的，不是自己跑起来的。

2. **特权级的接力**：M-mode（OpenSBI）做完硬件初始化后，主动降级到 S-mode（内核），并把 PC 设为 0x80200000。这种"固件让出 CPU"的设计让 OS 不用碰最脏的 M-mode 细节。

3. **栈是 C 代码运行的前提**：`kern_entry` 第一条 C 代码之前必须先 `la sp, bootstacktop`。GDB 里能直接看到 sp 从 OpenSBI 的临时值变成 0x80203000——不做这一步，任何函数调用都会崩。

4. **BSS 清零**：`memset(edata, 0, end-edata)` 是 C 语言规范要求的（全局未初始化变量必须为 0）。本实验因为没有这类变量，长度为 0，但后续实验这个步骤会变得重要。

5. **ecall 是系统调用的雏形**：内核打印字符不是直接写串口寄存器，而是 `ecall` 陷入 SBI。这种"用户态陷入内核态、内核态陷入更底层固件"的嵌套关系，就是操作系统特权级设计的缩影。

### 6.2 AI 协作开发的经验

1. **AI 帮助跳过环境坑**：本次最大的两个坑（`qemu-system-misc` 不含 riscv、`-device loader` 在新版 OpenSBI 下不跳转）都是靠 AI 提示后，再用 `which`、`dpkg -l`、GDB 看 `Next Address` 这种实证方式定位的，而不是瞎猜。
2. **AI 不替代单步**：课程强调"避免在调试时陷入 AI 抽盲盒"是对的。GDB 里每一步 PC、sp 的真实数值都是自己亲眼看到、自己 `si` 走一遍。

