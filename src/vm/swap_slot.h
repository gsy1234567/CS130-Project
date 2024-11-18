#pragma once

#include "lib/kernel/hash.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "devices/block.h"


/**
 * \brief 初始化磁盘的交换空间
*/
void swap_slot_init(void);

/**
 * \brief 将[`kpage`, `kpage` + `PGSIZE`)的内存写进磁盘, 
 * 这段空间的id为`upage`和`cur`->tid组合而成.
 * \param kpage 属于 [3GB, 4GB) 空间
 * \param upage 属于 [0, 3GB)   空间
 * \param cur   当前线程
 * \param perm  此块内存的访问权限
*/
void swap_in(
    void *kpage, 
    void *upage, 
    struct thread *cur, 
    struct permission perm
);

/**
 * \brief 查询磁盘空间中是否有以`upage`和`cur`->tid组合而成的id对应的数据,
 * 如果有则返回`true`, 如果没有则返回`false`.
*/
bool swap_search(void *upage, struct thread* cur);

/**
 * \brief 查询磁盘空间中是否有以`upage`和`cur`->tid组合而成的id对应的数据,
 * 如果有则返回`true`, 将这些数据写入内存中[`kpage`, `kpage`+`PGSIZE`)对应的
 * 空间中,并将磁盘中这些数据释放掉.如果没有则返回`false`.
 * \param kpage 属于 [3GB, 4GB) 空间
 * \param upage 属于 [0, 3GB)   空间
 * \param cur   当前线程
 * \param perm  存储此块内存的访问权限
*/
bool swap_out(void *kpage, void *upage, struct thread* cur, struct permission *perm);

/**
 * \brief 查询磁盘空间中是否有以`upage`和`cur`->tid组合而成的id对应的数据,
 * 如果有则返回`true`, 没有则返回`false`.
*/
bool swap_free(void *upage, struct thread* cur);

/**
 * \brief 释放所有`cur`线程获取的磁盘交换空间.
*/
void swap_free_all(struct thread* cur);
