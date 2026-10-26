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

int read_all(int sst_fd, char* ptr, size_t total_bytes) {

	ssize_t read_ret;
	size_t bytes_to_read = total_bytes;
	size_t bytes_read = 0;

	while (bytes_to_read > 0) {
		read_ret = read(sst_fd, ptr + bytes_read, bytes_to_read);

		if (read_ret == 0) {
			return -1;
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

int flush_sst(mmt_inst* cur_inst, int sst_fd) {

	char w_buffer[5*1024];

	char* ptr = w_buffer;
	
	node* cur_node = cur_inst->headers[0];

	size_t total_size = 0;

	while (cur_node != NULL) {
		total_size = (2*sizeof(size_t) + sizeof(int) + cur_node->key_len + cur_node->val_len);

		if (sizeof(w_buffer) < total_size) return -1;

		memcpy(ptr, &cur_node->key_len, sizeof(size_t));
		memcpy(ptr + sizeof(size_t), &cur_node->val_len, sizeof(size_t));
		memcpy(ptr + 2*sizeof(size_t), &cur_node->del_status, sizeof(int));
		memcpy(ptr + 2*sizeof(size_t) + sizeof(int), cur_node->buff_ptr, cur_node->key_len);
		memcpy(ptr + 2*sizeof(size_t) + sizeof(int) + cur_node->key_len, cur_node->buff_ptr + cur_node->key_len, cur_node->val_len);

		write_all(sst_fd, ptr, total_size);

		cur_node = cur_node->next_ptrs[0];
	}	

	if (fsync(sst_fd) != -1) return -1;

	return 0;
}

int read_sst(int sst_fd) {

	char r_buffer[5*1024];

	char* ptr = r_buffer;

	size_t key_len = 0;
	size_t val_len = 0;
	int del_status;

	while (1) {

		int read_all_ret = read_all(sst_fd, ptr, 20);
		if (read_all_ret == -1) return -1;
 
		memcpy(&key_len, ptr, sizeof(size_t));
		memcpy(&val_len, (ptr + sizeof(size_t)), sizeof(size_t));
		memcpy(&del_status, (ptr + 2*sizeof(size_t)), sizeof(int));

		if (key_len <= 0) return -1;

		char key[key_len];
		char* key_ptr = key;
		int read_all_ret_k = read_all(sst_fd, key_ptr, key_len);
		if (read_all_ret_k == -1) return -1;
		
		if (val_len == 0) {
			//node* cur_node = put_node(key, key_len, NULL, val_len, del_status, cur_inst);
			printf("\nkey: %.*s\n", (int)key_len, key_ptr);
			printf("del_status: %d\n", del_status);
			continue;
		} else if (val_len < 0) {
			return 0;
		}

		char val[val_len];
		char* val_ptr = val;
		int read_all_ret_v = read_all(sst_fd, val_ptr, val_len);
		if (read_all_ret_v == -1) return -1;

		// node* cur_node = put_node(key, key_len, val, val_len, del_status, cur_inst);

		printf("\nkey: %.*s\n", (int)key_len, key_ptr);
		printf("value: %.*s\n", (int)val_len, val_ptr);
		printf("del_status: %d\n", del_status);

	}

	return 0;
}
