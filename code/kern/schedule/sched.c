#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2413423
// schedule - 选择下一个可运行线程并在必要时完成切换
void
schedule(void) {
    bool intr_flag;
    struct proc_struct *next = idleproc;
    current->need_resched = 0;

    local_intr_save(intr_flag);
    {
        list_entry_t *le;
        if (current != idleproc) {
            le = list_next(&(current->list_link));
        } else {
            le = list_next(&proc_list);
        }
        // 从起点循环遍历整个 proc_list，找第一个 PROC_RUNNABLE 的线程
        while (le != &proc_list) {
            struct proc_struct *p = le2proc(le, list_link);
            if (p->state == PROC_RUNNABLE) {
                next = p;
                break;
            }
            le = list_next(le);
        }
        // 整个链表无可运行线程时，next 保持为 idleproc
        if (next == current) {
            local_intr_restore(intr_flag);
            return;
        }
        // runs++ 由 proc_run() 负责（分工 B5 第 4 步），此处不重复计数
        proc_run(next);
    }
    local_intr_restore(intr_flag);
}
