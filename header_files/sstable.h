#ifndef SSTABLE_H
#define SSTABLE_H

#include <unistd.h>
#include <error.h>

#include "memtable.h"

int write_all(int sst_fd, char* ptr, size_t total_size);

int read_all(int sst_fd, char* ptr, size_t total_bytes);

int flush_sst(mmt_inst* cur_inst, int sst_fd);

int read_sst(int sst_fd);

#endif
