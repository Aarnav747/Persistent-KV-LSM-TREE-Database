#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "arena.h"
#include "node.h"                                       #include "memtable.h"

typedef struct fuzz_status_struct {
	bool mod_flag;
	bool del_flag;
	size_t val_len;
	char val_buff[100];
} fuzz_stat;

void diff_fuzz(mmt_inst* cur_inst);

#endif
