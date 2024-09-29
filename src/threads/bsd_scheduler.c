#include "bsd_scheduler.h"
#include "debug.h"
#include "interrupt.h"
#include "thread.h"
#include "devices/timer.h"
#include "list.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static fp1714_t load_avg;

struct multilevel_queue{
    struct list queue[CAPACITY];
    unsigned head;
} scheduler_queue[2];

static unsigned flag;

static unsigned _get_local_idx(unsigned priority) {
    return (CAPACITY - 1U) - priority;
}

static unsigned _get_idx_priority(unsigned idx)
    {
        return (CAPACITY -1U) - idx;
    }

static unsigned _get_current_idx(void) {
    return flag & 1U;
}

static unsigned _get_next_idx(void) {
    return (~flag) & 1U;
}

static void _swap_scheduler(void) { flag = ~flag; }

static void _single_queue_init(unsigned idx) {
    scheduler_queue[idx].head = CAPACITY;
    for(unsigned i = 0 ; i < CAPACITY ; ++i)
        list_init(&scheduler_queue[idx].queue[i]);
}

static bool _bsd_scheduler_empty(unsigned idx) {
    return scheduler_queue[idx].head == CAPACITY;
}

static void _bsd_scheduler_insert(unsigned idx, struct list_elem *elem, unsigned priority) {
    list_push_back(&scheduler_queue[idx].queue[_get_local_idx(priority)], elem);
    scheduler_queue[idx].head = MIN(scheduler_queue[idx].head, _get_local_idx(priority));
}

static unsigned _bsd_scheduler_next_head(unsigned idx) {
    unsigned ret = scheduler_queue[idx].head;
    for(; ret < CAPACITY && list_empty(&scheduler_queue[idx].queue[ret]) ; ++ret);
    return ret;
}

static fp1714_t _update_recent_cpu(fp1714_t load_avg, fp1714_t recent_cpu, int nice)
    {
        return fp_int_add(
            fp2_mul(
                fp2_div(
                    fp_int_mul(load_avg, 2), 
                    fp_int_add(fp_int_mul(load_avg, 2), 1)
                ),
                recent_cpu
            ),
            nice
        );
    }

static fp1714_t _update_load_avg(fp1714_t old_load_avg, int ready_threads) {
    return fp2_add(
        fp2_mul(
            fp_int_div(
                to_fix_point(59), 
                60
            ), 
            old_load_avg
        ), 
        fp_int_div(
            to_fix_point(ready_threads), 
            60
        )
    );
}

static struct list_elem *_bsd_scheduler_pop(unsigned idx) {
    if(scheduler_queue[idx].head == CAPACITY)
        return NULL;
    struct list_elem *ret = list_begin(&scheduler_queue[idx].queue[scheduler_queue[idx].head]); 
    list_remove(ret);
    scheduler_queue[idx].head = _bsd_scheduler_next_head(idx);
    return ret;
}

void bsd_scheduler_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);
    flag = 0U;
    load_avg = to_fix_point(0);
    _single_queue_init(_get_current_idx());
    _single_queue_init(_get_next_idx());
}

bool bsd_scheduler_empty(void) {
    ASSERT(intr_get_level() == INTR_OFF);
    return _bsd_scheduler_empty(_get_current_idx());
}

void bsd_scheduler_insert(struct list_elem *elem, unsigned priority) {
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(priority < CAPACITY);
    _bsd_scheduler_insert(_get_current_idx(), elem, priority);
}

struct list_elem *bsd_scheduler_pop(void) {
    ASSERT(intr_get_level() == INTR_OFF);
    return _bsd_scheduler_pop(_get_current_idx());
}

void bsd_scheduler_priority_update(priority_update_func calc, void *aux) {
    ASSERT(intr_get_level() == INTR_OFF);
    unsigned cur_idx = _get_current_idx(), next_idx = _get_next_idx();
    ASSERT(_bsd_scheduler_empty(next_idx));
    while(!_bsd_scheduler_empty(cur_idx)) {
        struct list_elem *elem = _bsd_scheduler_pop(cur_idx);
        _bsd_scheduler_insert(next_idx, elem, calc(elem, aux));
    }
    _swap_scheduler();
}

void bsd_scheduler_foreach(list_foreach_func func, void *aux) {
    ASSERT(intr_get_level() == INTR_OFF);
    unsigned cur_idx = _get_current_idx();
    for(unsigned cur_head = scheduler_queue[cur_idx].head ; 
                 cur_head != CAPACITY; 
                 ++cur_head) {
        list_foreach(&scheduler_queue[cur_idx].queue[cur_head], func, aux);
    }
}

fp1714_t bsd_get_load_avg(void) { return load_avg; }

unsigned bsd_next_highest_priority(void)
  {
    ASSERT(intr_get_level() == INTR_OFF);
    unsigned ret = 0;
    if(!bsd_scheduler_empty())
      {
        ret = _get_idx_priority(scheduler_queue[_get_current_idx()].head);
      }
    return ret;
  }

int bsd_update_priority(fp1714_t recent_cpu, int nice)
  {
    int ret = round_to_zero(
                fp_int_sub(
                    fp2_sub(
                        to_fix_point(PRI_MAX), 
                        fp_int_div(
                            recent_cpu, 
                            4
                        )
                    ), 
                    nice * 2
                )
            );
    ret = MAX(MIN(ret, PRI_MAX), 0);
    return ret;
  }

static unsigned list_elem_priority_update_func(struct list_elem *elem, void *aux UNUSED)
  {
    ASSERT(intr_get_level() == INTR_OFF);
    struct thread *t = list_entry(elem, struct thread, elem);
    ASSERT(t != idle_thread);
    return bsd_update_priority(t->recent_cpu, t->nice);
  }

static void list_elem_recent_cpu_update_func(struct list_elem *elem, void *aux UNUSED)
  {
    ASSERT(intr_get_level() == INTR_OFF);
    struct thread *t = list_entry(elem, struct thread, elem);
    t->recent_cpu = _update_recent_cpu(load_avg, t->recent_cpu, t->nice);
  }

static void ready_thread_calc_func(struct thread *t, void *aux)
  {
    int *p_cnt = aux;
    if(t != idle_thread)
      {
        if(t->status == THREAD_READY || t->status == THREAD_RUNNING)
          {
            *p_cnt = *p_cnt + 1;
          }
      }
  }

void bsd_timer_callback(int64_t cur_ticks)
  {
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(thread_mlfqs);

    struct thread *cur = thread_current();
    
    if(cur != idle_thread)
      {
        cur->recent_cpu = fp_int_add(cur->recent_cpu, 1);
      }

    if(cur_ticks % 4 == 0)
      {
        bsd_scheduler_priority_update(&list_elem_priority_update_func, NULL);
        if(cur != idle_thread)
          {
            cur->priority = bsd_update_priority(cur->recent_cpu, cur->nice);
          }
      }
    
    if(cur_ticks % TIMER_FREQ == 0)
      {
        bsd_scheduler_foreach(&list_elem_recent_cpu_update_func, NULL);
        cur->recent_cpu = _update_recent_cpu(load_avg, cur->recent_cpu, cur->nice);
        int ready_num = 0;
        thread_foreach(&ready_thread_calc_func, &ready_num);
        load_avg = _update_load_avg(load_avg, (int)ready_num);
      }
  }
