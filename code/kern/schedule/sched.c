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

    local_intr_save(intr_flag);
    {
        current->need_resched = 0;

        list_entry_t *head = &proc_list;
        list_entry_t *start;

        /*
         * Round-robin search starts after the current process.  If the
         * current process is the last node, start from the first node
         * instead of stopping at the list head.
         */
        if (current == idleproc) {
            start = list_next(head);
        } else {
            start = list_next(&(current->list_link));
            if (start == head) {
                start = list_next(head);
            }
        }

        list_entry_t *le = start;
        do {
            if (le == head) {
                le = list_next(head);
                if (le == start) {
                    break;
                }
                continue;
            }

            struct proc_struct *p = le2proc(le, list_link);
            /*
             * idleproc is the fallback only.  Do not let it preempt a
             * real runnable thread, and do not select the current thread.
             */
            if (p != idleproc && p != current && p->state == PROC_RUNNABLE) {
                next = p;
                break;
            }
            le = list_next(le);
        } while (le != start);

        /* If no other thread is runnable, fall back to idleproc. */
        if (next == current) {
            local_intr_restore(intr_flag);
            return;
        }
        proc_run(next);
    }
    local_intr_restore(intr_flag);
}
