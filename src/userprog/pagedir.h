#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"

uint32_t *pagedir_create (void);
void pagedir_destroy (uint32_t *pd);
bool pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool rw);
void *pagedir_get_page (uint32_t *pd, const void *upage);
void pagedir_clear_page (uint32_t *pd, void *upage);
bool pagedir_is_dirty (uint32_t *pd, const void *upage);
void pagedir_set_dirty (uint32_t *pd, const void *upage, bool dirty);
bool pagedir_is_accessed (uint32_t *pd, const void *upage);
void pagedir_set_accessed (uint32_t *pd, const void *upage, bool accessed);
void pagedir_activate (uint32_t *pd);
uint32_t *lookup_page (uint32_t *pd, const void *vaddr, bool create);
struct permission pagedir_get_perm (uint32_t *pd, const void *upage);
uint32_t *pagedir_get_pte(uint32_t *pd, const void *upage);

bool pagedir_get_writable(uint32_t *pd, const void *upage);
bool pagedir_get_present(uint32_t *pd, const void *upage);
void pagedir_set_writable(uint32_t *pd, const void *upage, bool writable);
void pagedir_set_present(uint32_t *pd, const void *upage, bool present);


#endif /* userprog/pagedir.h */
