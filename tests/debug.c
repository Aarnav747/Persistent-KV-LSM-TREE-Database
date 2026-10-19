#include "debug.h"

void check_all(mmt_inst* cur_inst, fuzz_stat f_stat[], int key_count, int step) {
    char key[100];

    for (int k = 0; k < key_count; k++) {
        size_t key_len = snprintf(key, sizeof(key), "K%d", k);
        node* n = lookup_node(key, key_len, cur_inst);
        fuzz_stat* f = &f_stat[k];

        if (!f->mod_flag) {
            /* never written: must be NULL */
            if (n != NULL) {
                printf("step %d: K%d invented (never written)\n", step, k);
                assert(0);
            }
        } else if (f->del_flag) {
            /* deleted: must be a tombstone with empty value */
            if (n == NULL || n->del_status != IO_TYPE_SET_DEL || n->val_len != 0) {
                printf("step %d: K%d should be a tombstone\n", step, k);
                printf("n=%p", (void*)n);
                if (n) printf(" del_status=%d val_len=%zu", n->del_status, n->val_len);
                printf("\n");
                assert(0);
            }
        } else {
            /* live: must exist, be a PUT, and match value */
            if (n == NULL || n->del_status != IO_TYPE_SET_PUT ||
                n->val_len != f->val_len ||
                memcmp(n->buff_ptr + key_len, f->val_buff, f->val_len) != 0) {
                printf("step %d: K%d live value wrong or missing\n", step, k);
                assert(0);
            }
        }
    }
}

void diff_fuzz(mmt_inst* cur_inst) {

	fuzz_stat f_stat[16] = {0};

	node* found_node = NULL;
	node* cur_node = NULL;

	char key_f[100];
	char val_f[100];

	for (int i = 0; i < 100000; i++) {
		int rand_no = (rand() % 16);

		size_t key_len = snprintf(key_f, sizeof(key_f), "K%d", rand_no);
		size_t val_len = snprintf(val_f, sizeof(val_f), "V%d_%d", rand_no, i);

		int op = (rand() % 3);

		if (op == 0) /* get/lookup */ {
			if (f_stat[rand_no].mod_flag == false) {
				check_all(cur_inst, f_stat, 16, i);
				continue;
			}

			found_node = lookup_node(key_f, key_len, cur_inst);
			if (found_node == NULL) {
				printf("Node wasn't found at %d position\n", i);
				printf("Value in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);
				printf("K%d mod_flag=%d del_flag=%d\n", rand_no, f_stat[rand_no].mod_flag, f_stat[rand_no].del_flag);
				assert(0);
			}

			if (found_node->del_status == IO_TYPE_SET_DEL) {
				if (f_stat[rand_no].del_flag != IO_TYPE_SET_DEL) {
					printf("The delete flag of node was different than in f_stat\n");
					return;
				} else {
					check_all(cur_inst, f_stat, 16, i);
					continue;
				}
			}

			int memcmp_val = memcmp(found_node->buff_ptr + key_len, f_stat[rand_no].val_buff, f_stat[rand_no].val_len);

			if (memcmp_val != 0 || found_node->val_len != f_stat[rand_no].val_len) {
				printf("Node was different than the original at %d position\n", i);
				printf("Node in f_stat array: %.*s\n", (int)f_stat[rand_no].val_len, f_stat[rand_no].val_buff);
				printf("Node in skip list: %.*s\n", (int)found_node->val_len, found_node->buff_ptr + key_len);
				printf("Node val_len in f_stat array: %zu\n", f_stat[rand_no].val_len);
				printf("Node val_len in skip list: %zu\n", found_node->val_len);
				assert(0);
			}	

		} else if (op == 1) /* delete */ {
			fuzz_stat* f_entry = &f_stat[rand_no];
			int ret = delete_node(key_f, key_len, cur_inst);

			if (ret != -1) /* Check if delete failed, because of arena getting full */ {
				f_entry->mod_flag = true;
				f_entry->del_flag = true;
				f_entry->val_len = 0;
				memset(f_entry->val_buff, '\0', f_entry->val_len);
			} else {
				printf("\nDeletion failed at %d position\n", i);
				printf("Key was: %.*s\n", key_len, key_f);
				check_all(cur_inst, f_stat, 16, i);
				continue;
			}

		} else /* put */ {
			fuzz_stat* f_entry = &f_stat[rand_no];

			cur_node = put_node(key_f, key_len, val_f, val_len, IO_TYPE_SET_PUT, cur_inst);

			if (cur_node == NULL) break;

			f_entry->mod_flag = true;
			f_entry->del_flag = false;
			f_entry->val_len = val_len;
			memcpy(f_entry->val_buff, val_f, val_len);

			/*if (cur_inst->headers[0] != NULL) {
				printf("\n headers[0]: %.*s", cur_inst->headers[0]->key_len, cur_inst->headers[0]->buff_ptr);
			} else {
				printf("\nheaders[0] is NULL\n");
			}*/
		
		}

		check_all(cur_inst, f_stat, 16, i);

	}

	//printf("\n headers[0]: %.*s", cur_inst->headers[0]->key_len, cur_inst->headers[0]->buff_ptr);

	/*if (found_node != NULL) {
		printf("\nfound_node key: %.*s\n", found_node->key_len, found_node->buff_ptr);
		if (found_node->del_status == IO_TYPE_SET_DEL) {
			printf("The node is deleted, so no value\n");
			return;
		}
		printf("found_node value: %.*s\n", found_node->val_len, found_node->buff_ptr + found_node->key_len);
	} else if (found_node == NULL && cur_node != NULL) {
		printf("\ncur_node key: %.*s\n", cur_node->key_len, cur_node->buff_ptr);
		printf("cur_node value: %.*s\n", cur_node->val_len, cur_node->buff_ptr + cur_node->key_len);
	}*/

	printf("\nfuzz passed\n");
}
