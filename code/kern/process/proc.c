#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}


// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}


// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2411350
// ---------------------------------------------------------------------------
// 进程创建与初始化（刘爽 2411350）
//   alloc_proc / copy_mm / copy_thread / do_fork / kernel_thread / proc_init
// ---------------------------------------------------------------------------

// alloc_proc - 分配并初始化一个进程控制块(PCB)
//   PCB 本身用 kmalloc 从内核堆上分配；此时还没有内核栈、没有 pid，
//   所以所有字段都必须显式赋初值，绝不能留下 kmalloc 返回的随机内容。
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
    list_init(&(proc->hash_link));                          // 哈希节点自成一个空链

    return proc;
}

// copy_mm - 复制/共享进程的内存管理信息
//   Lab4 创建的全部是内核线程（idleproc 与 initproc），current->mm == NULL，
//   因此这里不需要真正复制用户地址空间，只要把新进程的 mm 置空、
//   页目录沿用内核页目录即可；接口保留完整，便于后续实验扩展。
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

// copy_thread - 构造新进程的“首次运行现场”
//   1) 在新进程内核栈的顶端留出一个 struct trapframe 的空间；
//   2) 把调用者（kernel_thread 造好的模板 / 父亲的中断帧）复制进去；
//   3) 子进程 tf->gpr.a0 = 0，保证子进程从 fork 返回 0；
//   4) context.ra = forkret，context.sp = tf，
//      于是 switch_to 之后 CPU 会从 forkret 开始执行，
//      forkret -> forkrets -> __trapret -> sret -> kernel_thread_entry
//   这条链把“上下文切换”平滑地接到“中断返回”，最终进入 fn(arg)。
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

// do_fork - 创建一个新进程（内核线程）
//   顺序：alloc_proc -> setup_kstack -> copy_mm -> copy_thread
//         -> 分配 pid -> hash_proc -> 挂 proc_list -> nr_process++ -> wakeup_proc
//   任何一步失败都要按“已获得资源的逆序”释放，避免内存泄漏。
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;

    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    // 1. 分配 PCB
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    // 2. 记录父子关系
    proc->parent = current;

    // 3. 分配内核栈（KSTACKPAGE 个物理页）
    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }

    // 4. 复制内存管理信息（内核线程为空实现）
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }

    // 5. 构造 trapframe 与切换上下文
    copy_thread(proc, stack, tf);

    // 6. pid 分配 + 入链表/哈希表，必须关中断保证原子性
    {
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            proc->pid = get_pid();
            hash_proc(proc);
            list_add(&proc_list, &(proc->list_link));
            nr_process++;
        }
        local_intr_restore(intr_flag);
    }

    // 7. 置为就绪态，等待被 schedule 选中
    wakeup_proc(proc);

    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// kernel_thread - 创建一个内核线程，入口是 fn(arg)
//   RISC-V 没有 x86 那样的“iret 造栈”技巧，我们直接伪造一个 trapframe：
//   让 sret 之后跳到 kernel_thread_entry，由它执行 a0 = s1(arg)、jalr s0(fn)。
//   因此 s0 存函数地址、s1 存参数。
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

// proc_init - 初始化进程管理子系统，创建 0 号进程 idleproc 和 1 号进程 initproc
void proc_init(void)
{
    int i, pid;

    // 1. 初始化进程链表与 pid 哈希表
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    // 2. 创建 idleproc（0 号进程，复用 bootstack 作为内核栈）
    if ((idleproc = alloc_proc()) == NULL)
    {
        cprintf("cannot alloc idleproc.\n");
        return;
    }
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;                        // 进入 cpu_idle 后立即让出 CPU
    set_proc_name(idleproc, "idle");
    nr_process++;
    list_add(&proc_list, &(idleproc->list_link));
    hash_proc(idleproc);

    // 3. 从此刻起，当前进程就是 idleproc
    current = idleproc;

    // 4. 创建 initproc：它会在第一次被调度时执行 init_main
    pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        cprintf("create init_main failed.\n");
        return;
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");
}


//LAB4 function3 2411350
// ---------------------------------------------------------------------------
// 进程调度接口（刘爽 2411350）：cpu_idle / proc_run
//   真正的“选谁”由王乐妍实现的 schedule() 负责，这里只负责
//   “空闲时让出 CPU”和“真正完成一次上下文+页表切换”。
// ---------------------------------------------------------------------------

// cpu_idle - 0 号进程的主循环：没有其他事情可做时，只要有人需要被调度就让出 CPU
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

// proc_run - 切换到 proc 进程运行
//   1) 同一个进程直接返回；
//   2) 关中断，保证“改 current / 改 satp / 切换上下文”是原子的；
//   3) 更新 current 与运行次数；
//   4) 切换页表（satp 写入新进程的页目录物理地址）；
//   5) switch_to 保存旧上下文、恢复新上下文；
//   6) 恢复中断状态。
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
