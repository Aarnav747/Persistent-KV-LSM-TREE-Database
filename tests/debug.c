#include "debug.h"

void diff_fuzz(mmt_inst* cur_inst) {

	fuzz_stat f_stat[100] = {0};

	char key_f[100];
	char val_f[100];

	for (int i = 0; i < 218; i++) {
		int rand_no = (rand() % 100);

		size_t key_len = snprintf(key_f, sizeof(key_f), "K%d", rand_no);
		size_t val_len = snprintf(val_f, sizeof(val_f), "V%d_%d", rand_no, i);

		printf("val_f array: %.*s\n", (int)val_len, val_f);

		int op = (rand() % 3);

		if (op == 0) /* get/lookup */ {
			if (f_stat[rand_no].mod_flag == false) continue;

			node* found_node = lookup_node(key_f, key_len, cur_inst);
			if (found_node == NULL) {
				printf("Node wasn't found at %d position\n", i);
				printf("Node in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);
				assert(0);
			}

			if (found_node->del_status == IO_TYPE_SET_DEL) continue;

			int memcmp_val = memcmp(found_node->buff_ptr + key_len, f_stat[rand_no].val_buff, f_stat[rand_no].val_len);

			if (memcmp_val != 0 || found_node->val_len != f_stat[rand_no].val_len) {
				printf("Node was different than the original at %d position\n", i);
				printf("Node in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);
				printf("Node in skip list: %.*s\n", (int)found_node->val_len, found_node->buff_ptr + key_len);
				printf("Node val_len in f_stat array: %zu\n", f_stat[rand_no].val_len);
				printf("Node val_len in skip list: %zu\n", found_node->val_len);
				assert(0);
			}	

			printf("Node in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);

		} else if (op == 1) /* delete */ {
			fuzz_stat* f_entry = &f_stat[rand_no];
			int ret = delete_node(key_f, key_len, cur_inst);
			if (ret != -1) {
				f_entry->mod_flag = true;
				f_entry->del_flag = true;
				f_entry->val_len = 0;
				memset(f_entry->val_buff, '\0', f_entry->val_len);
			}
		} else /* put */ {
			fuzz_stat* f_entry = &f_stat[rand_no];

			node* cur_node = put_node(key_f, key_len, val_f, val_len, IO_TYPE_SET_PUT, cur_inst);

			if (cur_node == NULL) break;

			f_entry->mod_flag = true;
			f_entry->del_flag = false;
			f_entry->val_len = val_len;
			memcpy(f_entry->val_buff, val_f, val_len);

			printf("Node in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);
		
		}

	}

	printf("fuzz passed\n");
}
