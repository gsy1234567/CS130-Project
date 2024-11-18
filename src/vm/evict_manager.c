#include "evict_manager.h"
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct _evict_entry {
  struct list_elem list_elem;
  struct hash_elem hash_elem;
  struct evict_entry entry;
};

static struct lock lock;
static struct list evict_entry_list;
static struct hash evict_entry_hash;
static unsigned evict_entry_hash_func(const struct hash_elem *, void *aux);
static bool evict_entry_hash_less(const struct hash_elem *, const struct hash_elem *, void *aux);
void evict_manager_init() {
  lock_init(&lock);
  list_init(&evict_entry_list);
  ASSERT(hash_init(&evict_entry_hash, &evict_entry_hash_func, &evict_entry_hash_less, NULL));
}

void trace_page(struct evict_entry entry) {
  ASSERT(is_kernel_vaddr(entry.kpage));
  ASSERT(is_user_vaddr(entry.upage));
  ASSERT(pg_ofs(entry.kpage) == 0);
  ASSERT(pg_ofs(entry.upage) == 0);

  struct _evict_entry *e = (struct _evict_entry*)malloc(sizeof *e);
  ASSERT(e != NULL);
  e->entry = entry;
  lock_acquire(&lock);
  list_push_back(&evict_entry_list, &e->list_elem);
  hash_insert(&evict_entry_hash, &e->hash_elem);
  lock_release(&lock);
}

struct evict_entry get_evict(void) {
  struct evict_entry ret;
  struct _evict_entry *e;
  lock_acquire(&lock);
  e = list_entry(list_pop_front(&evict_entry_list), struct _evict_entry, list_elem);
  ASSERT(hash_delete(&evict_entry_hash, &e->hash_elem) == &e->hash_elem);
  lock_release(&lock);
  ret = e->entry;
  free(e);
  return ret;
}

void untrace_all(struct thread *t) {
  ASSERT(is_thread(t));

  struct _evict_entry e;
  struct hash_delete_prev_info prev_info;
  struct _evict_entry *evict_entry;
  struct hash_elem *hash_elem;
  

  e.entry.owner = t;
  hash_delete_prev_info_init(&prev_info);
  lock_acquire(&lock);
  while((hash_elem = hash_delete_continuos(&evict_entry_hash, &e.hash_elem, &prev_info)) != NULL) {
    evict_entry = hash_entry(hash_elem, struct _evict_entry, hash_elem);
    list_remove(&evict_entry->list_elem);
    free(evict_entry);
  }
  lock_release(&lock);
}

static unsigned evict_entry_hash_func(const struct hash_elem * hash_elem, void *aux) {
  return hash_bytes(hash_entry(hash_elem, struct _evict_entry, hash_elem)->entry.owner, sizeof(void *));
}
static bool evict_entry_hash_less(const struct hash_elem *hash_elem_a, const struct hash_elem *hash_elem_b, void *aux) {
  return hash_entry(hash_elem_a, struct _evict_entry, hash_elem)->entry.owner < hash_entry(hash_elem_b, struct _evict_entry, hash_elem)->entry.owner;
}
