#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "colosseum.h"

struct col_arena_free_entry {
	size_t size;
	void *next;
};

struct col_block_header {
	size_t size;
	struct col_arena *arena;
	size_t offset;
};

struct col_arena {
	size_t size;
	size_t capacity;
	struct col_arena *next;
	struct col_arena_free_entry *head;
	void *mem;
};

struct col_pool {
	struct col_arena *head;
};

static struct col_pool pool = {NULL};

// This is our alligment size, it will be 16 for a 64 bit architecture and 8 for a 32 bit architecture.
static size_t alignment_size = 2*sizeof(void*);

size_t alignment_offset(void *p) {
	return (size_t)p % alignment_size;
}

struct col_arena *initialize_arena(size_t size) {
	int page_size = getpagesize();
	size_t num_pages = 1 + (size + sizeof(struct col_arena) + sizeof(struct col_arena_free_entry))/page_size;

	void *mem = mmap(NULL, num_pages*page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (mem == MAP_FAILED) {
		return NULL;
	}

	// Place arena in memory
	struct col_arena *arena = mem;
	void *arena_mem = mem + sizeof(struct col_arena);
	arena->mem = arena_mem;

	struct col_arena_free_entry *entry = arena->mem;
	entry->size = num_pages*page_size;
	arena->mem += sizeof(struct col_arena_free_entry);
	arena->head = entry;
	arena->size = num_pages*page_size;
	arena->capacity = arena->size;
	return arena;
}

void initialize_pool(size_t arena_size) {
	if (pool.head != NULL) {
		exit(1);
	}

	struct col_arena *arena = initialize_arena(arena_size);
	if (arena == NULL) {
		exit(1);
	}
	pool.head = arena;
}

// This is relatively good alloc function, could be better. 
// We use first fit for our arena and then best fit for the block selection within the arena.
void *col_alloc(size_t size) {
	size += sizeof(struct col_block_header) + alignment_size;
	if (pool.head == NULL) {
		initialize_pool(size);
	}

	// 1. Select arena (first fit)
	struct col_arena *a_head = pool.head;
	struct col_arena *prev_arena = NULL;
	while (a_head != NULL) {
		if (a_head->size >= size) {
			break;
		}
		prev_arena = a_head;
		a_head = a_head->next;
	}

	if (a_head == NULL) {
		a_head = initialize_arena(size);
		prev_arena->next = a_head;
	}

	// 2. Select free block from arena (best fit)
	struct col_arena_free_entry *f_head = a_head->head;
	struct col_arena_free_entry *prev = NULL;
	struct col_arena_free_entry *f_min = NULL;
	size_t min = 0;
	while (f_head != NULL) {
		if (f_head->size >= size) {
			if (f_head->size < min || min == 0) {
				f_min = f_head;
				min = f_min->size;
			}
		}
		prev = f_head;
		f_head = f_head->next;
	}

	if (f_min == NULL) {
		return NULL;
	}

	// 4. Align block
	size_t offset = alignment_offset(f_head);

	// 5. Create new free entry if needed
	void *block_start = f_min;
	size_t block_size = size - sizeof(struct col_block_header) - alignment_size;
	if (f_min->size - block_size > sizeof(struct col_arena_free_entry)) {
		struct col_arena_free_entry *e = (struct col_arena_free_entry *)(f_min+block_size);
		e->size = f_min->size - block_size;
		e->next = f_min->next;
		if (prev == a_head->head) {
			a_head->head = e;
		} else {
			prev->next = e;
		}
	}
	// 6. Create block header
	struct col_block_header *h = block_start;
	h->size = block_size;
	h->arena = a_head;
	h->offset = offset;
	block_start += sizeof(struct col_block_header);

	// 7. Update arena size
	a_head->size -= (block_size + offset);

	return block_start;
}

// Naive free - No remapping/grouping, will lead to fragmentation.
void col_free(void *p) {
	// 1. Get block header
	struct col_block_header *h = p - sizeof(struct col_block_header);

	// 2. Get arena from header and update arena size
	struct col_arena *a = h->arena;
	a->size += h->size + h->offset;

	// 3. Create new free entry at unaligned block start and append it to start of free list
	struct col_arena_free_entry *f = (struct col_arena_free_entry *)(h - h->offset);
	f->size = h->size + h->offset;
	f->next = a->head;
	a->head = f;
}
