#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define IO_TYPE_SET_PUT 0 // to define 'io_type' as PUT
#define IO_TYPE_SET_DEL 1 // to define 'io_type' as DEL
#define MAX_LAYER_LEVEL 16

struct wal_struct {
	size_t total_len; // total length of the struct
	int io_type; // io operation type (PUT/DELETE)
	size_t key_len; // key length
	size_t val_len; // val length
	char buffer[]; // stores key and val bytes
	
	/* key + val are added later in 'buffer' during the call to wal_write() */
};

typedef struct memtable_struct {
	node* headers[MAX_LAYER_LEVEL];	
	arena
} mmt_inst;

typedef struct skip_list_node_struct {
	int layer_cnt; // number of total layers this node appears in
	char* buff_ptr; // skips past the pointer array in buffer and points to where the key bytes begin 
	node** next_ptrs; // points to the buffer's pointer array part. the pointer array size is (layer_cnt * sizeof(node*))
	size_t key_len;
	size_t val_len;
	char buffer[]; // stores key and val bytes, aswell as the 'next' pointers of different layers. buffer layout: [pointer array][key bytes][value bytes]
} node;

typedef struct arena_struct {
	char* mem_ptr;
	size_t offset;
	size_t total_size;
} arena;

typedef struct memtable_struct {
	node* headers[MAX_LAYER_LEVEL]; // dummy nodes that help us traverse each of the layer sequentially, because they act as empty headers
	arena* arena_mem;
} mmt_inst;

int pick_layercnt() {

	int rand_ret = rand();

	int layer_limit = (RAND_MAX / 2);
	int layer_cnt = 1; // layer 1 corresponds to the first layer, which is the linked list itself
	if (rand_ret <= layer_limit) {
		while (rand_ret < layer_limit) {
			if (layer_cnt == MAX_LAYER_LEVEL) break;

			layer_cnt++;
			rand_ret = rand();
		}
	} else {
		while (rand_ret > layer_limit) {				if (layer_cnt == MAX_LAYER_LEVEL) break;

			layer_cnt++;
			rand_ret = rand();
		}
	}

	return layer_cnt;
}

char* alloc_mem(arena* arena_mem, size_t size_req) {

	char* new_mem_ptr = NULL;

	if ((arena_mem->total_size - arena_mem->offset) >= size_req) {
		new_mem_ptr = arena_mem->mem_ptr + arena_mem->offset;
		arena_mem->offset += size_req;
	}

	return new_mem_ptr;
}

node* create_node(char key[], size_t key_len, char val[], size_t val_len, mmt_inst cur_inst) {

	int layer_cnt = pick_layer();
	node* cur_node = (node*)alloc_mem(cur_inst->arena_mem, (sizeof(node) + key_len + val_len + (layer_cnt * sizeof(node*))));	
	if (cur_node == NULL) return NULL;

	cur_node->layer_cnt = layer_cnt;
	cur_node->key_len = key_len;
	cur_node->val_len = val_len;

	cur_node->buff_ptr = cur_node->buffer + (layer_cnt * sizeof(node*));
	memcpy(cur_node->buff_ptr, key, cur_node->key_len);
	memcpy(cur_node->buff_ptr + cur_node->key_len, val, (cur_node->val_len));

	cur_node->next_ptrs = (node**)cur_node->buffer;

	return cur_node;
}

void traverse_layers(char key[], size_t key_len) {

	int memcmp_ret;
	for (int n = (MAX_LAYER_LEVEL - 1); n >= 0; n--) {
		node* cur_node = mmt_inst->headers[n];

		while (cur_node != NULL) {
			memcmp_ret = memcmp(cur_node, key, key_len);
			if (memcmp_ret > 0) {
				cur_node = cur_node->next_ptrs[n];
			} else if () {
				
			}
		}

	}

	/*int cur_layer_cnt = head_node->layer_cnt;
	node* layer_node = head_node->next_ptrs[cur_layer_cnt];
	char key[cur_node->key_len];
	char layer_key[layer_node->key_len];

	while (1) {

		memcpy(buff_ptr, cur_node->key_len, key);
		memcpy(layer_node->buff_ptr, layer_node->key_len, layer_key);

		if ((int strcmp_ret = strcmp()) < ) {
			cur_layer_cnt--;
			if (head_node->next_ptrs[cur_layer_cnt] == NULL) return;
			layer_node = head_node->next_ptrs[cur_layer_cnt];
		} 
	}*/

}

int read_all(int fd, struct wal_struct *wal_record, size_t struct_size) {
	
	int read_bytes = read(fd, wal_record, struct_size);

	if (read_bytes < struct_size) {
		size_t rem_bytes = (struct_size - read_bytes);
		int total_read_bytes = read_bytes;
		while (total_read_bytes != 0) {	
			read_bytes = read(fd, (wal_record - total_read_bytes), rem_bytes);
			total_read_bytes += read_bytes;
			rem_bytes = (struct_size - read_bytes);
		}
	}

}

// runs every new io operation performed
int wal_write(int wal_fd, char key[], char val[], int io_type) /* used to save the operations that led to the current conditions */ {

	size_t buffer_len = (strlen(key) + strlen(val));

	struct wal_struct *wal_record = Malloc(sizeof(struct wal_struct) + buffer_len);
	
	wal_record->total_len = (buffer_len + sizeof(struct wal_struct));
	wal_record->io_type = io_type;
	wal_record->key_len = strlen(key);
	wal_record->val_len = strlen(val);	
	// fill wal_record's buffer
	memcpy(wal_record->buffer, key, wal_record->key_len);
	memcpy(wal_record->buffer[wal_record + wal_record->key_len], val, wal_record->val_len);

	int written_bytes = Write(wal_fd, wal_record, wal_record->total_len);

	if (written_bytes < wal_record->total_len) {
		int total_written = written_bytes;
		unsigned char* cur_pos; // a pointer at the starting index of the unwritten data in the struct
		while (total_written < wal_record->total_len) {			
			cur_pos = ((unsigned char*) wal_record + total_written);
			written_bytes = Write(wal_fd, cur_pos, (wal_record->total_len - total_written));
			total_written += written_bytes;
		}
	}

	Fsync(wal_fd); // flush all of the unsaved changes to the disk space

	Free(wal_record);

	return 0;

}

int wal_read(int wal_fd) {

	while (1) {

		struct wal_struct *wal_record_temp = Malloc(sizeof(struct wal_struct));

		int read_bytes1 = Read(wal_fd, wal_record_temp, sizeof(struct wal_struct));

		if (read_bytes1 == 0) {
			Free(wal_record_temp);
			break;
		}

		struct wal_struct *wal_record = Realloc(wal_record_temp, wal_record_temp->total_len); // reallocating the size of the struct because of the buffer

		int read_bytes2 = Read(wal_fd, wal_record->buffer, (wal_record_temp->total_len - sizeof(struct wal_struct) /* the size of the wal_record buffer */));

		Free(wal_record);

	}

	return 0;

}

avl_tree* find_node(avl_tree* cur_node, char key[]) {

	if (cur_node == NULL) return NULL;

	int strcmp_ret = strcmp(cur_node->key, key);

	if (strcmp_ret == 0) return cur_node;
	
	if (strcmp_ret < 0) return find_node(cur_node->left, key);
	
	return find_node(cur_node->right, key);

}

void put_list() {

	

}

int main() {

	int wal_fd /* file for WAL logging */ = Open("wal_log.txt", O_APPEND | O_CREAT | O_RDWR, 0644):

}
