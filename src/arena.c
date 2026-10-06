#include "arena.h"

char* alloc_mem(arena* arena_mem, size_t size_req, size_t alignment /* the proper alignment needed for the aligned offset, majority of the times it's just '_Alignof(node)' */) /* function to allocate an arbitrary size */ {

	char* new_mem_ptr = NULL;

	size_t aligned_offset = (arena_mem->offset + alignment - 1) & ~(alignment - 1); /* This is basically '(offset + 7) & ~7', we assume that the alignment will be 8 (hence _Alignof(node)) because that's exactly what caused the runtime error previously when we didn't adjust for the alignment, and this does exactly that. I previously used this similar trick back in my memory allocator project. */

	if ((arena_mem->total_size - aligned_offset) >= size_req) {
		new_mem_ptr = arena_mem->mem_ptr + aligned_offset;
		arena_mem->offset = aligned_offset + size_req;
	}

	return new_mem_ptr;
}
