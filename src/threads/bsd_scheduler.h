#pragma once

#include "list.h"
#include "fix_point.h"
#include "thread.h"

#define CAPACITY 64U

typedef unsigned (*priority_update_func)(struct list_elem *, void *);

void bsd_scheduler_init(void);

bool bsd_scheduler_empty(void);

void bsd_scheduler_insert(struct list_elem *elem, unsigned priority);

struct list_elem *bsd_scheduler_pop(void);

void bsd_scheduler_priority_update(priority_update_func calc, void *aux);

void bsd_scheduler_foreach(list_foreach_func func, void *aux);

int bsd_update_priority(fp1714_t recent_cpu, int nice);

fp1714_t bsd_get_load_avg(void);

unsigned bsd_next_highest_priority(void);

void bsd_timer_callback(int64_t cur_ticks);