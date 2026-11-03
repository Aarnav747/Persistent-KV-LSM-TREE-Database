#ifndef SSTABLE_H
#define SSTABLE_H

#include <unistd.h>
#include <error.h>
#include <assert.h>

#include "memtable.h"
#include "debug.h"

typedef struct sst_arena_struct {
	char* mem_ptr;
	size_t total_size;
} sst_arena;

int write_all(int sst_fd, char* ptr, size_t total_size);

ssize_t read_all(int sst_fd, char* ptr, size_t total_bytes);

int flush_sst(mmt_inst* cur_inst, int sst_fd, sst_arena* sst_mem);

void cmp_fstat_read(size_t key_len, char key[], size_t val_len, char val[], uint8_t del_status, node* cur_node);

int read_sst(int sst_fd, mmt_inst* cur_inst, sst_arena* sst_mem);

#endif
