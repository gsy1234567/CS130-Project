#include "swap_slot.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#define BLOCKS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct _disk_entry {
  struct hash_elem elem;
  void *upage;
  struct thread* t;
  struct permission perm;
  block_sector_t start;
};

static struct lock lock;
static struct hash disk_entry_hash;
static struct block *block_swap;
static struct bitmap *block_swap_manager;

static unsigned disk_entry_hash_func(const struct hash_elem*, void *);
static bool disk_entry_less_func(const struct hash_elem*, const struct hash_elem*, void *);

void swap_slot_init(void) {
  lock_init(&lock);
  ASSERT(hash_init(&disk_entry_hash, &disk_entry_hash_func, &disk_entry_less_func, NULL));
  ASSERT((block_swap = block_get_role(BLOCK_SWAP)) != NULL);
  ASSERT((block_swap_manager = bitmap_create(block_size(block_swap))) != NULL);
  bitmap_set_all(block_swap_manager, false);
}

void swap_in(void *kpage, void *upage, struct thread *cur, struct permission perm) {
  ASSERT(is_kernel_vaddr(kpage));
  ASSERT(is_user_vaddr(upage));
  ASSERT(is_thread(cur));
  ASSERT(pg_ofs(kpage) == 0);
  ASSERT(pg_ofs(upage) == 0);

  struct disk_upage_entry *disk_upage_entry = malloc(sizeof *disk_upage_entry);
  struct _disk_entry *disk_entry = malloc(sizeof *disk_entry);
  block_sector_t start;

  disk_entry->upage = upage;
  disk_entry->upage = upage;
  disk_entry->t = cur;
  disk_entry->perm = perm;

  ASSERT(hash_find(&cur->disk_upages, disk_upage_entry->upage) == NULL);
  ASSERT(hash_find(&disk_entry_hash, disk_entry->upage) == NULL);

  lock_acquire(&lock);
  ASSERT((start = bitmap_scan_and_flip(block_swap_manager, 0, BLOCKS_PER_PAGE, false)));
  disk_entry->start = start;
  hash_insert(&disk_entry_hash, &disk_entry->elem);
  lock_release(&lock);

  hash_insert(&cur->disk_upages, &disk_upage_entry->elem);
  for(block_sector_t off = 0 ; off < BLOCKS_PER_PAGE ; ++off)
    block_write(block_swap, start + off, kpage + off * BLOCK_SECTOR_SIZE);

}

bool swap_search(void *upage, struct thread* cur) {
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(is_thread(cur));

  struct _disk_entry de;
  bool ret = false;
  de.upage = upage;
  de.t = cur;
  lock_acquire(&lock);
  ret = (hash_find(&disk_entry_hash, &de.elem) != NULL);
  lock_release(&lock);
  return ret;
}

bool swap_out(void *kpage, void *upage, struct thread* cur, struct permission *perm) {
  ASSERT(is_kernel_vaddr(kpage));
  ASSERT(is_user_vaddr(upage));
  ASSERT(pg_ofs(kpage) == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(is_thread(cur));
  ASSERT(perm != NULL);

  struct _disk_entry de;
  struct hash_elem *hash_elem;
  struct _disk_entry *result;

  struct disk_upage_entry due;

  de.t = cur;
  de.upage = upage;
  due.upage = upage;

  lock_acquire(&lock);
  hash_elem = hash_delete(&disk_entry_hash, &de.elem);
  if(hash_elem != NULL) {
    result = hash_entry(hash_elem, struct _disk_entry, elem);
    bitmap_set_multiple(block_swap_manager, result->start, BLOCKS_PER_PAGE, false);
  }
  lock_release(&lock);

  if(hash_elem == NULL)
    return false;
  
  *perm = result->perm;
  for(block_sector_t off = 0 ; off < BLOCKS_PER_PAGE ; ++off)
    block_read(block_swap, result->start + off, kpage + off * BLOCK_SECTOR_SIZE);
  free(result);
  hash_elem = hash_delete(&cur->disk_upages, &due.elem);
  ASSERT(hash_elem != NULL);
  free(hash_entry(hash_elem, struct disk_upage_entry, elem));
  return true;
}

bool swap_free(void *upage, struct thread* cur) {
  ASSERT(is_user_vaddr(upage));
  ASSERT(is_thread(cur));

  struct _disk_entry de;
  struct hash_elem *hash_elem;
  struct _disk_entry *result;

  struct disk_upage_entry due;

  de.upage = upage;
  de.t = cur;

  due.upage = upage;
  
  lock_acquire(&lock);
  hash_elem = hash_delete(&disk_entry_hash, &de.elem);
  if(hash_elem != NULL) {
    result = hash_entry(hash_elem, struct _disk_entry, elem);
    bitmap_set_multiple(block_swap_manager, result->start, BLOCKS_PER_PAGE, false);
  }
  lock_release(&lock);

  if(hash_elem != NULL) {
    free(result);
    hash_elem = hash_delete(&cur->disk_upages, &due.elem);
    ASSERT(hash_elem != NULL);
    free(hash_entry(hash_elem, struct disk_upage_entry, elem));
  }

  return hash_elem != NULL;
}

static void swap_free_helper(struct hash_elem * e, void *aux) {
  ASSERT(swap_free(hash_entry(e, struct disk_upage_entry, elem)->upage, aux));
}

void swap_free_all(struct thread* cur) {
  ASSERT(is_thread(cur));

  hash_apply_aux(&cur->disk_upages, &swap_free_helper, cur);
}

static unsigned disk_entry_hash_func(const struct hash_elem* hash_elem, void * aux) {
  return hash_bytes(hash_entry(hash_elem, struct _disk_entry, elem)->t, sizeof(void*));
}

static bool disk_entry_less_func(const struct hash_elem* a, const struct hash_elem* b, void *aux) {
  struct _disk_entry *a_ = hash_entry(a, struct _disk_entry, elem);
  struct _disk_entry *b_ = hash_entry(b, struct _disk_entry, elem);
  return a_->t < b_->t || (a_->t == b_->t && a_->upage < b_->upage);
}
