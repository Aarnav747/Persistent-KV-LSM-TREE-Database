#ifndef ARENA_H
#define ARENA_H

#include "tags.h"
#include <stddef.h>

typedef struct arena_struct {
	char* mem_ptr;
	size_t offset;
	size_t total_size;
} arena;

char* alloc_mem(arena* arena_mem, size_t size_req, size_t alignment);

#endif
