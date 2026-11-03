#include "sstable.h"

int write_all(int sst_fd, char* ptr, size_t total_size) {

	size_t bytes_to_write = total_size;
	size_t bytes_written = 0;
	ssize_t write_ret;

	while (bytes_to_write > 0) {
		write_ret = write(sst_fd, ptr + bytes_written, bytes_to_write);

		if (write_ret != -1) {
			bytes_to_write -= write_ret;
			bytes_written += write_ret;
		} else {
			if (errno == EINTR) continue;
			return -1;
		}
	}

	return bytes_written;
}

ssize_t read_all(int sst_fd, char* ptr, size_t total_bytes) {

	ssize_t read_ret;
	size_t bytes_to_read = total_bytes;
	ssize_t bytes_read = 0;

	while (bytes_to_read > 0) {
		read_ret = read(sst_fd, ptr + bytes_read, bytes_to_read);

		if (read_ret == 0) {
			break;
		} else if (read_ret != -1) {
			bytes_to_read -= read_ret;
			bytes_read += read_ret;
		} else {
			if (errno == EINTR) continue;
			return -1;
		}
	}

	return bytes_read;
}

int flush_sst(mmt_inst* cur_inst, int sst_fd, sst_arena* sst_mem) {

	char* ptr = sst_mem->mem_ptr;
	
	node* cur_node = cur_inst->headers[0];

	size_t total_size = 0;

	while (cur_node != NULL) {
		total_size = (SST_KEYLEN_SIZE + SST_VALLEN_SIZE + SST_IOTYPE_SIZE + cur_node->key_len + cur_node->val_len);

		if ((MAX_KEY_SIZE + MAX_VAL_SIZE) < total_size) {
			return -1;
		}

		memcpy(ptr, &cur_node->key_len, SST_KEYLEN_SIZE);
		memcpy(ptr + SST_KEYLEN_SIZE, &cur_node->val_len, SST_VALLEN_SIZE);
		memcpy(ptr + 2*SST_KEYLEN_SIZE, &cur_node->del_status, SST_IOTYPE_SIZE);
		memcpy(ptr + 2*SST_KEYLEN_SIZE + SST_IOTYPE_SIZE, cur_node->buff_ptr, cur_node->key_len);
		memcpy(ptr + 2*SST_KEYLEN_SIZE + SST_IOTYPE_SIZE + cur_node->key_len, cur_node->buff_ptr + cur_node->key_len, cur_node->val_len);

		write_all(sst_fd, ptr, total_size);

		cur_node = cur_node->next_ptrs[0];

		memset(sst_mem->mem_ptr, 0, (MAX_KEY_SIZE + MAX_VAL_SIZE));

	}	

	int fsync_ret = fsync(sst_fd);

	if (fsync_ret == -1) return -1;

	return 0;
}

void cmp_read(size_t key_len, char key[], size_t val_len, char val[], uint8_t del_status, node* cur_node) {

	if (key_len != cur_node->key_len) {
		printf("key_len different\n");
		printf("key_len: %zu, cur_node: %u\n", key_len, cur_node->key_len);
		assert(0);
	}
	if (memcmp(key, cur_node->buff_ptr, key_len) != 0) {
		printf("key different\n");
		printf("key: %.*s\n", (int)key_len, key);
		printf("cur_node: %.*s\n", (int)cur_node->key_len, cur_node->buff_ptr);
		assert(0);
	}
	if (del_status != cur_node->del_status) {
		printf("tombstone different\n");
		assert(0);
	}
	if (val_len != cur_node->val_len) {
		printf("val_len different\n");
		printf("val_len: %zu, cur_node: %u\n", val_len, cur_node->val_len);
		assert(0);
	}
	if (memcmp(val, cur_node->buff_ptr + key_len, val_len) != 0) {
		printf("value different\n");
		assert(0);
	}

}

int read_sst(int sst_fd, mmt_inst* cur_inst, sst_arena* sst_mem) {

	char* ptr = sst_mem->mem_ptr;

	size_t key_len = 0;
	size_t val_len = 0;
	int del_status;

	node* cur_node = cur_inst->headers[0];

	while (1) {

		int read_all_ret = read_all(sst_fd, ptr, (2*SST_KEYLEN_SIZE + SST_IOTYPE_SIZE));
		if (read_all_ret < 0) {
			printf("read_all error 1\n");
			return -1;
		} else if (read_all_ret == 0) {
			break;
		}
 
		memcpy(&key_len, ptr, SST_KEYLEN_SIZE);
		memcpy(&val_len, (ptr + SST_KEYLEN_SIZE), SST_KEYLEN_SIZE);
		memcpy(&del_status, (ptr + 2*SST_KEYLEN_SIZE), SST_IOTYPE_SIZE);

		char key[key_len];
		char* key_ptr = key;
		int read_all_ret_k = read_all(sst_fd, key_ptr, key_len);
		if (read_all_ret_k != (int)key_len) {
			return -1;
		} else if (read_all_ret_k == 0) {
			break;
		}
		
		if (val_len == 0) {
			cmp_read(key_len, key, val_len, NULL, del_status, cur_node);

			cur_node = cur_node->next_ptrs[0];
			continue;
		}

		char val[val_len];
		char* val_ptr = val;
		int read_all_ret_v = read_all(sst_fd, val_ptr, val_len);

		if (read_all_ret_v < 0 || read_all_ret_v != (int)val_len) {
			printf("Value read error\n");
			return -1;
		} else if (read_all_ret_v == 0) {
			break;
		}

		if (val_len > 0) cmp_read(key_len, key, val_len, val, del_status, cur_node);

		cur_node = cur_node->next_ptrs[0];
	}

	if (cur_node != NULL) {
		printf("Sstable file does not contain the last node\n");
		assert(0);
	}

	return 0;
}
