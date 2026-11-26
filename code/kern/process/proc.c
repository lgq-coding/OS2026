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
#include <unistd.h>

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
// proc.h 未声明 kernel_execve（不修改冻结接口），在文件内补原型
int kernel_execve(const char *name, unsigned char *binary, size_t size);

//lab4 请补充你在lab4中的代码

//lab5 function2 2411350 update alloc_proc

// alloc_proc - alloc_proc() against the proc structure
//             (1) allocate one proc_struct
//             (2) init all fields of proc_struct
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->pgdir = boot_pgdir_pa;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);
        list_init(&(proc->list_link));
        list_init(&(proc->hash_link));
        // lab5 2411350: PCB 新增的进程树与退出相关字段必须一并初始化，
        // 否则 do_wait / do_exit 遍历 cptr 链时会读到未定义指针
        proc->exit_code = 0;
        proc->wait_state = 0;
        proc->cptr = proc->optr = proc->yptr = NULL;
    }
    return proc;
}


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

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
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

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
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

//lab5 function2 2411350 update kernel_thread

// kernel_thread - create a kernel thread and make it run fn(arg)
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    // s0/s1 是内核线程专用的“参数寄存器”：
    // kernel_thread_entry 把 s1 搬到 a0 作为实参，然后 jalr s0 跳 fn
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.epc = (uintptr_t)kernel_thread_entry;
    // 内核线程首次“返回”时处于 S 态（SPP=1），且返回后要开中断（SPIE=1）
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    return do_fork(clone_flags, 0, &tf);
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

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

//lab5 function3 YOUR CODE update copy_mm

// =====================================================================
// 以下两个函数按小组分工由刘国全（2411822）主责：copy_mm / do_fork。
// 本文件此处保留原始标记，等待 lab5-memory 分支合并，刘爽不修改该区域。
// =====================================================================

//lab5 function2 YOUR CODE（刘国全：copy_mm 的辅助/合并占位，勿动）


//lab5 function3 YOUR CODE（刘国全：do_fork 的合并占位，勿动）


//lab5 function4 2411350 copy_thread + proc_init

// copy_thread - fill the new proc's trapframe and context so that
//               after switch_to() it can start running from tf->epc
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    // trapframe 放在内核栈顶，紧贴 KSTACKSIZE 上方边界向下生长
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;
    // 子进程从 do_fork 返回 0；父进程走另一条 return 路径拿子进程 pid
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
    // context 只保存 callee-saved 寄存器：ra 指向 forkret，
    // sp 指向刚构造好的 trapframe，于是首次 switch_to 后直接落到 forkret
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

static int init_main(void *arg);
static int user_main(void *arg);
static int load_icode(struct mm_struct *mm, unsigned char *binary, size_t size);

// proc_init - set up the first kernel thread idleproc "idle" by itself and
//           - create the second kernel thread init_main "init"
void proc_init(void)
{
    int i, pid;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    // 0 号进程 idleproc，直接复用 bootloader 给的内核栈 bootstack
    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    // idleproc 的 parent 为 NULL，不能走 set_links（那里会解引用 parent）
    nr_process++;
    list_add(&proc_list, &(idleproc->list_link));
    hash_proc(idleproc);

    current = idleproc;

    // 1 号进程 initproc，运行 init_main
    pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }
    initproc = find_proc(pid);
    set_proc_name(initproc, "init");
}


#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)

//lab5 function2 2411350 init_main / user_main / kernel_execve / do_execve / load_icode
//lab5 function3 2411350 do_wait
//lab5 function4 2411350 do_exit / do_yield / do_kill / cpu_idle / proc_run

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");

    // initproc 自己也是内核线程（mm == NULL），它先派生一个跑 user_main 的内核线程，
    // user_main 再通过 kernel_execve 把自己“变成”用户进程
    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    // 只要还有孩子就继续睡 / 继续收；do_wait 返回 0 表示尚未回收完
    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }
    cprintf("all user-mode processes have quit.\n");
    panic("init check memory pass.");
    return 0;
}

// user_main - kernel thread used to execute one user program
static int
user_main(void *arg)
{
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(hello);
#endif
    panic("user_main execve failed.\n");
    return 0;
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main
//               它不能像普通函数那样 return，必须构造用户 Trapframe 后
//               直接跳 __trapret，由 sret 把 CPU 从 S 态切到 U 态
int kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int ret = do_execve(name, strlen(name), binary, size);
    if (ret != 0)
    {
        return ret;
    }
    // do_execve/load_icode 已经把 current->tf 改造成用户态现场：
    //   tf->epc = elF 入口, tf->gpr.sp = USTACKTOP, tf->status.SPP = 0
    // 只要把 sp 指向这个 tf 再跳 __trapret，就等价于“从中断返回用户态”
    uintptr_t tf_va = (uintptr_t)current->tf;
    asm volatile("mv sp, %0\n\t"
                 "j __trapret\n" ::"r"(tf_va));
    return 0;
}

// do_execve - free the old user memory space, load the new ELF, refresh mm/pgdir/name
//           - 名字合法性检查由调用方 sys_exec 负责，此处只做截断拷贝，
//             这样 kernel_execve 传内核态字符串也能复用
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    struct mm_struct *mm = current->mm;
    int ret;

    if (len > PROC_NAME_LEN)
    {
        len = PROC_NAME_LEN;
    }
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    // 1. 释放旧用户地址空间：先解绑 current，切回内核页表，
    //    再按“页 -> 页目录 -> mm”顺序释放（否则 satp 还指着将被释放的页目录）
    if (mm != NULL)
    {
        current->mm = NULL;
        current->pgdir = boot_pgdir_pa;
        lsatp(boot_pgdir_pa);
        asm volatile("sfence.vma");
        set_mm_count(mm, 0); // exit_mmap 里有 assert(mm_count(mm) == 0)
        exit_mmap(mm);
        put_pgdir(mm);
        mm_destroy(mm);
    }

    // 2. 新建 mm 与页目录
    struct mm_struct *new_mm;
    if ((new_mm = mm_create()) == NULL)
    {
        return -E_NO_MEM;
    }
    if (setup_pgdir(new_mm) != 0)
    {
        mm_destroy(new_mm);
        return -E_NO_MEM;
    }

    // 3. 装载 ELF（load_icode 会改 current->tf，所以必须在 mm 已就位时做）
    lock_mm(new_mm);
    ret = load_icode(new_mm, binary, size);
    unlock_mm(new_mm);
    if (ret != 0)
    {
        set_mm_count(new_mm, 0);
        exit_mmap(new_mm);
        put_pgdir(new_mm);
        mm_destroy(new_mm);
        return ret;
    }

    // 4. 生效：PCB 存物理地址，同时把 satp 切到新页目录，
    //    否则 kernel_execve 跳到用户地址时用的还是内核页表
    current->mm = new_mm;
    current->pgdir = PADDR(new_mm->pgdir);
    lsatp(current->pgdir);
    asm volatile("sfence.vma");
    set_proc_name(current, local_name);
    return 0;
}

// load_icode - load the ELF image into the new mm and rebuild current->tf
static int
load_icode(struct mm_struct *mm, unsigned char *binary, size_t size)
{
    struct elfhdr *elf = (struct elfhdr *)binary;
    if (elf->e_magic != ELF_MAGIC)
    {
        return -E_INVAL_ELF;
    }
    if (elf->e_phoff == 0 || elf->e_phnum == 0)
    {
        return -E_INVAL_ELF;
    }

    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);

    for (int i = 0; i < elf->e_phnum; i++, ph++)
    {
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz)
        {
            return -E_INVAL_ELF;
        }

        uint32_t vm_flags = 0;
        if (ph->p_flags & ELF_PF_X)
            vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W)
            vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R)
            vm_flags |= VM_READ;

        // 建立 VMA：权限来自 ELF，长度用 memsz（把 bss 一并圈进去）
        if (mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL) != 0)
        {
            return -E_INVAL;
        }

        uint32_t perm = PTE_U | PTE_V;
        if (vm_flags & VM_WRITE)
            perm |= PTE_W;
        if (vm_flags & VM_READ)
            perm |= PTE_R;
        if (vm_flags & VM_EXEC)
            perm |= PTE_X;

        // 逐页分配：按页对齐遍历（用户程序链接在 0x800020，p_va 并非页对齐，
        // 必须按页内偏移拷贝，否则整段内容会前移或后移若干字节）；
        // filesz 区间从 ELF 拷贝，filesz~memsz 区间保持清零（bss）
        uintptr_t seg_va = ROUNDDOWN(ph->p_va, PGSIZE);
        uintptr_t seg_end = ph->p_va + ph->p_memsz;
        uintptr_t file_end = ph->p_va + ph->p_filesz;
        for (uintptr_t va = seg_va; va < seg_end; va += PGSIZE)
        {
            struct Page *page = alloc_page();
            if (page == NULL)
            {
                return -E_NO_MEM;
            }
            uint8_t *kva = page2kva(page);
            memset(kva, 0, PGSIZE);

            // 本页与 [p_va, p_va + p_filesz) 的交集才需要从 ELF 拷贝
            uintptr_t cpy_start = (va > ph->p_va) ? va : ph->p_va;
            uintptr_t cpy_end = (va + PGSIZE < file_end) ? (va + PGSIZE) : file_end;
            if (cpy_start < cpy_end)
            {
                memcpy(kva + (cpy_start - va),
                       binary + ph->p_offset + (cpy_start - ph->p_va),
                       cpy_end - cpy_start);
            }

            if (page_insert(mm->pgdir, page, va, perm) != 0)
            {
                free_page(page);
                return -E_NO_MEM;
            }
        }
    }

    // 用户栈：USTACKTOP 向下 USTACKPAGE 页，整段映射并清零
    if (mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE,
               VM_READ | VM_WRITE | VM_EXEC, NULL) != 0)
    {
        return -E_INVAL;
    }
    uint32_t stack_perm = PTE_U | PTE_V | PTE_R | PTE_W;
    for (int i = 0; i < USTACKPAGE; i++)
    {
        struct Page *page = alloc_page();
        if (page == NULL)
        {
            return -E_NO_MEM;
        }
        memset(page2kva(page), 0, PGSIZE);
        if (page_insert(mm->pgdir, page, USTACKTOP - (i + 1) * PGSIZE, stack_perm) != 0)
        {
            free_page(page);
            return -E_NO_MEM;
        }
    }

    // 构造“用户态现场”：SPP=0 让 sret 回到 U 态，SPIE=1 表示返回后开中断，
    // SIE=0 保证 sret 之前不会再被打断
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->epc = elf->e_entry;
    tf->gpr.sp = USTACKTOP;
    tf->status = (read_csr(sstatus) & ~SSTATUS_SPP) | SSTATUS_SPIE;
    tf->gpr.a0 = 0;
    return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE, and return its pid
//         - no child            -> -E_BAD_PROC
//         - child alive         -> sleep on WT_CHILD
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;

    if (code_store != NULL)
    {
        if (mm == NULL)
        {
            return -E_INVAL;
        }
        lock_mm(mm);
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            unlock_mm(mm);
            return -E_INVAL;
        }
        unlock_mm(mm);
    }

repeat:
    {
        bool intr_flag;
        struct proc_struct *proc = NULL, *zombie = NULL;
        int pid_ret = -1, exit_code = 0;

        local_intr_save(intr_flag);
        {
            // 在 proc_list 里筛自己名下的孩子（比手工遍历 cptr 链更稳）
            list_entry_t *le = &proc_list;
            while ((le = list_next(le)) != &proc_list)
            {
                struct proc_struct *p = le2proc(le, list_link);
                if (p->parent != current)
                {
                    continue;
                }
                if (pid != 0 && p->pid != pid)
                {
                    continue;
                }
                if (p->state == PROC_ZOMBIE)
                {
                    zombie = p;
                    break;
                }
                if (proc == NULL)
                {
                    proc = p; // 记下“存在但还活着”的孩子
                }
            }

            if (zombie != NULL)
            {
                // 先把 pid / 退出码存到栈上，再释放 PCB
                pid_ret = zombie->pid;
                exit_code = zombie->exit_code;
                unhash_proc(zombie);
                remove_links(zombie);
                put_kstack(zombie);
                kfree(zombie);
            }
        }
        local_intr_restore(intr_flag);

        if (zombie != NULL)
        {
            if (code_store != NULL)
            {
                copy_to_user(mm, code_store, &exit_code, sizeof(int));
            }
            return pid_ret;
        }

        if (proc == NULL)
        {
            return -E_BAD_PROC; // 一个孩子都没有
        }

        // 有孩子但都还活着：挂起等它退出
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
}




// do_exit - called by sys_exit, free the user mm and turn current into zombie
int do_exit(int error_code)
{
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }

    // 1. 回收用户地址空间
    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        current->mm = NULL;
        current->pgdir = boot_pgdir_pa;
        lsatp(boot_pgdir_pa);
        asm volatile("sfence.vma");
        set_mm_count(mm, 0);
        exit_mmap(mm);
        put_pgdir(mm);
        mm_destroy(mm);
    }

    current->flags |= PF_EXITING;
    current->exit_code = error_code;

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // 2. 把孩子过继给 initproc，由它负责最终回收
        struct proc_struct *proc;
        while ((proc = current->cptr) != NULL)
        {
            current->cptr = proc->optr;
            proc->parent = initproc;
            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE)
            {
                wakeup_proc(initproc);
            }
        }

        // 3. 唤醒可能在 WT_CHILD 上睡着的父进程
        struct proc_struct *parent = current->parent;
        if (parent != NULL && parent->state == PROC_SLEEPING &&
            (parent->wait_state & WT_CHILD))
        {
            parent->wait_state &= ~WT_CHILD;
            wakeup_proc(parent);
        }

        // 4. 自己变僵尸，等父进程 do_wait 来收
        current->state = PROC_ZOMBIE;
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!!\n");
    return 0;
}

// do_yield - ask current to release CPU
int do_yield(void)
{
    current->need_resched = 1;
    schedule();
    return 0;
}

// do_kill - set PF_EXITING on target proc
int do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)
    {
        if (!(proc->flags & PF_EXITING))
        {
            proc->flags |= PF_EXITING;
            if (proc->state == PROC_SLEEPING)
            {
                wakeup_proc(proc);
            }
            return 0;
        }
    }
    return -E_INVAL;
}

// cpu_idle - when no process is runnable, idleproc loops here
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

// proc_run - switch to proc: update satp with its page table then switch_to
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;

        local_intr_save(intr_flag);
        {
            current = proc;
            // Sv39 模式位 + 页目录物理页号（<< 12 位）
            uintptr_t satp = SATP64_MODE | (next->pgdir >> 12);
            asm volatile("csrw satp, %0" ::"r"(satp));
            asm volatile("sfence.vma");

            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
