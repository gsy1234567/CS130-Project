#pragma once

#include <stdint.h>
#include "threads/thread.h"

/*
  Evict Manager:
    - 简介:用来管理内核中已经分配好的用户能访问的页
    - 功能:当物理内存不足时,选择`受害者`页
*/

struct evict_entry {
  uint32_t *pte;
  void *kpage, *upage;
  struct thread *owner;
};

/**
 * \brief 初始化`受害者管理器`.
*/
void evict_manager_init(void);

/**
 * \brief 追踪`kpage`页.
 * \param kpage 属于 [3GB, 4GB) 空间
 * \param pte 页表项
 * \attention 需要页表项的目的时当将此页对用的数据设置为`受害者`时, 
 * 需要让对应进程的页表项标记为`unmapped`
*/
void trace_page(struct evict_entry entry);

/**
 * \brief 获得一个`受害者`页, 取消对这个受害页的追踪.
*/
struct evict_entry get_evict(void);

/**
 * \brief 取消追踪所有`t`拥有的内核页.
*/
void untrace_all(struct thread *t);