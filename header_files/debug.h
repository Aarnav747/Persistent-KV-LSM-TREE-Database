#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "memtable.h"

typedef struct fuzz_status_struct {
	bool mod_flag;
	bool del_flag;
	size_t val_len;
	char val_buff[100];
} fuzz_stat;

void check_all(mmt_inst* cur_inst, fuzz_stat f_stat[], int key_cnt, int step);

void diff_fuzz(mmt_inst* cur_inst);

#endif
