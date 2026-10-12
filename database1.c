#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <assert.h>
#include <stddef.h>

#define IO_TYPE_SET_PUT 0 // to define 'io_type' as PUT
#define IO_TYPE_SET_DEL 1 // to define 'io_type' as DEL
#define MAX_LAYER_LEVEL 16

struct wal_struct {
	size_t total_len; // total length of the struct
	int io_type; // io operation type (PUT/DELETE)
	size_t key_len; // key length
	size_t val_len; // value/data length
	char buffer[]; // stores key and val bytes. layout: [key bytes][value bytes]
	
	/* key + val are added later in 'buffer' during the call to wal_write() */
};

typedef struct skip_list_node_struct node;

struct skip_list_node_struct {
	int layer_cnt; // number of total layers this node appears in
	char* buff_ptr; // skips past the pointer array in buffer and points to where the key bytes begin 
	node** next_ptrs; // points to the buffer's pointer array part. the pointer array size is (layer_cnt * sizeof(node*))
	size_t key_len;
	size_t val_len;
	int del_status; // shows the current status of the node, could be either active (0 / IO_TYPE_SET_PUT) or deleted (1 / IO_TYPE_SET_DEL)
	_Alignas(node*) char buffer[]; // stores key and val bytes, aswell as the 'next' pointers of different layers. buffer layout: [pointer array][key bytes][value bytes]
};

typedef struct arena_struct {
	char* mem_ptr;
	size_t offset;
	size_t total_size;
} arena;

typedef struct memtable_struct {
	node* headers[MAX_LAYER_LEVEL]; // pointers that help us traverse each of the layer sequentially, because they act as point to the first nodes of each layer
	arena* arena_mem;
} mmt_inst;

int pick_layercnt() /* a function that is used to select the no. of layers for each node */ {

	int rand_ret = rand();

	int layer_limit = (RAND_MAX / 2);
	int layer_cnt = 0; // layer 0 corresponds to the first layer, which is the linked list itself
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

char* alloc_mem(arena* arena_mem, size_t size_req, size_t alignment /* the proper alignment needed for the aligned offset, majority of the times its just '_Alignof(node)' */) /* function to allocate an arbitrary size */ {

	char* new_mem_ptr = NULL;

	size_t aligned_offset = (arena_mem->offset + alignment - 1) & ~(alignment - 1); // this is basically '(offset + 7) & ~7', we assume that the alignment will be 8 (hence _Alignof(node)) because that's exactly what caused the runtime error previously when we didn't adjust for the alignment, and this does exactly that. I previously used this similar trick back in my memory allocator project

	if ((arena_mem->total_size - aligned_offset) >= size_req) {
		new_mem_ptr = arena_mem->mem_ptr + aligned_offset;
		arena_mem->offset = (aligned_offset + size_req);
	}

	return new_mem_ptr;
}

node* create_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, mmt_inst* cur_inst) {

	int layer_cnt = pick_layercnt();

	node* cur_node = (node*)alloc_mem(cur_inst->arena_mem, (sizeof(node) + key_len + val_len + (layer_cnt * sizeof(node*))), _Alignof(node) /* align 8 bytes */);	
	if (cur_node == NULL) return NULL;

	cur_node->layer_cnt = layer_cnt;
	cur_node->key_len = key_len;
	cur_node->val_len = val_len;
	cur_node->del_status = io_type;

	cur_node->buff_ptr = cur_node->buffer + (layer_cnt * sizeof(node*));

	memcpy(cur_node->buff_ptr, key, key_len);

	if (val_len > 0) {
		memcpy((cur_node->buff_ptr + key_len), val, val_len);		
	}

	cur_node->next_ptrs = (node**)cur_node->buffer;
	memset(cur_node->next_ptrs, 0, layer_cnt * sizeof(node*));

	return cur_node;
}

void traverse_layers(char key[], size_t key_len, node* past_nodes[], mmt_inst* cur_inst, int stop_before_match /* this parameter helps traverse differentiate between what the caller wants. if its,say, lookup calling, we most definetly will need the same node (if it exists in the list), but if its another function calling , we might instead want to skip whenever iterating at that layer, and if we didnt, it could cause problems while unlinking in put_node later on (which it actually did, and thats why we added this parameter */) {

	int memcmp_ret;
	node* cur_node = NULL;
	node* past_node = NULL;
	int len;
	for (int n = (MAX_LAYER_LEVEL - 1); n >= 0; n--) {
		if (past_node == NULL) {
			cur_node = cur_inst->headers[n];
		} else {
			cur_node = past_node->next_ptrs[n];
		}

		while (cur_node != NULL) {
			len = MIN(cur_node->key_len, key_len); // used MIN() function instead of the if/else branch, because, well, its convinient

			memcmp_ret = memcmp(cur_node->buff_ptr, key, len);
			if (memcmp_ret == 0) {
				if (cur_node->key_len > key_len) {
					cur_node = NULL;
				} else if (cur_node->key_len == key_len) {

					if (stop_before_match == 1) {
						cur_node = NULL;
					} else {
						past_node = cur_node;
						cur_node = cur_node->next_ptrs[n];
					}
				} else /* added a check for the condition when cur_node's key length is smaller, because when we didnt have it, it previously caused improper inserts which caused a problem in actually sorting the skip list */ {
					past_node = cur_node;
					cur_node = cur_node->next_ptrs[n];
				}
			} else if (memcmp_ret < 0) {
				past_node = cur_node;
				cur_node = cur_node->next_ptrs[n];
			} else {
				cur_node = NULL;
			}
		}

		past_nodes[n] = past_node;

	}


}

void insert_node(node* cur_node, mmt_inst* cur_inst) {

	node* past_nodes[MAX_LAYER_LEVEL];

	traverse_layers(cur_node->buff_ptr, cur_node->key_len, past_nodes, cur_inst, 1);

	node* past_node = NULL;
	node* next_node = NULL;
	for (int n = (cur_node->layer_cnt - 1); n >= 0; n--) {
		past_node = past_nodes[n];

		if (past_node == NULL) /* no node key smaller than the selected key exists in this layer, so make the new node (cur_node) the header of that list */ {
			next_node = cur_inst->headers[n];
			cur_inst->headers[n] = cur_node;
			cur_node->next_ptrs[n] = next_node;
			continue;
		} 

		next_node = past_node->next_ptrs[n];
		past_node->next_ptrs[n] = cur_node;
		cur_node->next_ptrs[n] = next_node;
	}

}

node* lookup_node(char key[], size_t key_len, mmt_inst* cur_inst) {

	node *past_nodes[MAX_LAYER_LEVEL];

	traverse_layers(key, key_len, past_nodes, cur_inst, -1 /* we do this in lookup, because while using lookup we might also want to actually search for the node to see if its in the list, thats what lookup is even for, so passing -1 as a parameter executes the branch where the same node is added to the past_nodes array aswell */); // by traversing the skip list and storing the previous nodes it becomes easier and faster to find the node with the key, we're searching for, compared to traversing the original skip list (which might take O(n)), this approach can possibly take O(log(n)) instead. and we can confirm that the node, if it exists, will be at layer 1 (the original skip list), so it will be at past_nodes[0]

	if (past_nodes[0] == NULL) return NULL;

	int memcmp_ret = memcmp(key, past_nodes[0]->buff_ptr, key_len); // compares both keys, just in case

	if (memcmp_ret == 0 && past_nodes[0]->key_len == key_len) return past_nodes[0];

	return NULL;
}

node* put_node(char key[], size_t key_len, char val[], size_t val_len, int io_type, mmt_inst* cur_inst) {

	node* old_node = lookup_node(key, key_len, cur_inst);

	printf("");

	node* cur_node = create_node(key, key_len, val, val_len, io_type, cur_inst);

	if (cur_node == NULL) {
		return NULL;
	} else if (old_node != NULL) {
		node* past_nodes[MAX_LAYER_LEVEL];
		node* past_node;
		node* next_node;

		traverse_layers(key, key_len, past_nodes, cur_inst, 1);

		int layer_cnt = MAX(old_node->layer_cnt, cur_node->layer_cnt);

		for (int n = (layer_cnt - 1); n>=0; n--) {
			past_node = past_nodes[n];

			if (n < old_node->layer_cnt && n < cur_node->layer_cnt) /* both nodes exists at this layer */ {
				if (past_node == NULL) /* we previously did not have this condition check, and it did cause segfaults when we tried to access past_node,but it turned out to be null. so here whenever it is the case, we just make the cur_node the header of that layer, cause no other node exists there yet */ {
					cur_inst->headers[n] = cur_node;
					continue;
				}

				next_node = old_node->next_ptrs[n];
				past_node->next_ptrs[n] = cur_node;
				cur_node->next_ptrs[n] = next_node;
			} else if (n < old_node->layer_cnt && n >= cur_node->layer_cnt) /* only old_node exists at this layer */ {
				if (past_node == NULL) /* since cur_node obviously does not exist at this layer, and there is no past_node aswell,meaning old_node is the header, so we unlink it from that layer and make the header NULL, and that makes the layer empty */ {
					cur_inst->headers[n] = NULL;
					continue;
				}

				next_node = old_node->next_ptrs[n];
				past_node->next_ptrs[n] = next_node;
			} else if (n < cur_node->layer_cnt && n >= old_node->layer_cnt) /* only cur_node exists at this layer */ {
				if (past_node == NULL) {
					cur_inst->headers[n] = cur_node;
					continue;
				}

				next_node = past_node->next_ptrs[n];
				past_node->next_ptrs[n] = cur_node;
				cur_node->next_ptrs[n] = next_node;

			}

		}

		// both nodes not existing at a layer cant really happen because we start the iteration from either layer_cnt of old_node or cur_node, so this just means we've finished iterating
		return cur_node;
	}

	insert_node(cur_node, cur_inst);

	return cur_node;
}

int delete_node(char key[], size_t key_len, mmt_inst* cur_inst) {

	char val[] = "";

	node* cur_node = put_node(key, key_len, val, 0, IO_TYPE_SET_DEL, cur_inst); 

	if (cur_node == NULL) return -1;

	return 0;
}

/* int read_all(int fd, struct wal_struct *wal_record, size_t struct_size) {
	
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

} */

// runs every new io operation performed (basically when PUT/DELETE instructions are ran)
int wal_write(int wal_fd, char key[], char val[], int io_type) /* used to save the operations that led to the current conditions */ {

	size_t buffer_len = (strlen(key) + strlen(val));

	struct wal_struct *wal_record = malloc(sizeof(struct wal_struct) + buffer_len);
	
	wal_record->total_len = (buffer_len + sizeof(struct wal_struct));
	wal_record->io_type = io_type;
	wal_record->key_len = strlen(key);
	wal_record->val_len = strlen(val);	
	// fill wal_record's buffer
	memcpy(wal_record->buffer, key, wal_record->key_len);
	char* val_ptr = wal_record->buffer + wal_record->key_len;
	memcpy(val_ptr, val, wal_record->val_len);

	ssize_t written_bytes = write(wal_fd, wal_record, wal_record->total_len);
	if (written_bytes < 0) return -1;

	if ((size_t)written_bytes < wal_record->total_len) {
		size_t total_written = written_bytes;
		unsigned char* cur_pos; // a pointer at the starting index of the unwritten data in the struct
		while (total_written < wal_record->total_len) {			
			cur_pos = ((unsigned char*) wal_record + total_written);
			written_bytes = write(wal_fd, cur_pos, (wal_record->total_len - total_written));
			total_written += written_bytes;
		}
	}

	fsync(wal_fd); // flush all of the unsaved changes to the disk space

	free(wal_record);

	return 0;

}

int wal_read(int wal_fd) {

	while (1) {

		struct wal_struct *wal_record_temp = malloc(sizeof(struct wal_struct));

		int read_bytes1 = read(wal_fd, wal_record_temp, sizeof(struct wal_struct));

		if (read_bytes1 == 0) {
			free(wal_record_temp);
			break;
		}

		struct wal_struct *wal_record = realloc(wal_record_temp, wal_record_temp->total_len); // reallocating the size of the struct because of the buffer

		/* int read_bytes2 = read(wal_fd, wal_record->buffer, (wal_record_temp->total_len - sizeof(struct wal_struct))); */

		free(wal_record);

	}

	return 0;

}

void print_level0(mmt_inst* mt) {
	    for (node* c = mt->headers[0]; c; c = c->next_ptrs[0]) {
		            printf("[%.*s] ", (int)c->key_len, c->buff_ptr);
			        }
	        printf("\n");
}

int main() {
	
	/* int wal_fd = open("wal_log.txt", O_APPEND | O_CREAT | O_RDWR, 0644); */



srand(1); // fixed seed so failures reproduce

	arena a;
	a.total_size = 1024 * 1024;
	a.offset = 0;
	a.mem_ptr = malloc(a.total_size);

	mmt_inst mt;
	memset(mt.headers, 0, sizeof(mt.headers));
	mt.arena_mem = &a;

	// put + lookup
	assert(put_node("a", 1, "1", 1, IO_TYPE_SET_PUT, &mt) != NULL);
	node* n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->val_len == 1);
	assert(memcmp(n->buff_ptr + n->key_len, "1", 1) == 0);

	//print_level0(&mt);

	// overwrite with a longer value
	put_node("a", 1, "hello", 5, IO_TYPE_SET_PUT, &mt);
	n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->val_len == 5);
	assert(memcmp(n->buff_ptr + n->key_len, "hello", 5) == 0);

	//print_level0(&mt);

	// prefix keys
	put_node("cat", 3, "x", 1, IO_TYPE_SET_PUT, &mt);
	put_node("cats", 4, "y", 1, IO_TYPE_SET_PUT, &mt);
	print_level0(&mt);
	assert(lookup_node("cat", 3, &mt) != NULL);
	assert(lookup_node("cats", 4, &mt) != NULL);
	assert(lookup_node("ca", 2, &mt) == NULL);

	//print_level0(&mt);

	// delete
	assert(delete_node("a", 1, &mt) == 0);
	n = lookup_node("a", 1, &mt);
	assert(n != NULL && n->del_status == IO_TYPE_SET_DEL);

	//print_level0(&mt);

	// level 0 must be sorted
	for (node* c = mt.headers[0]; c && c->next_ptrs[0]; c = c->next_ptrs[0]) {
		node* nx = c->next_ptrs[0];
     		size_t m = c->key_len < nx->key_len ? c->key_len : nx->key_len;
        	int r = memcmp(c->buff_ptr, nx->buff_ptr, m);
        	assert(r < 0 || (r == 0 && c->key_len < nx->key_len));
    }

	//print_level0(&mt);

	// fill the arena: should return NULL, not crash
	char key[16];
	int i = 0;
	while (1) {
        	int len = snprintf(key, sizeof(key), "k%d", i++);
        	if (put_node(key, len, "v", 1, IO_TYPE_SET_PUT, &mt) == NULL) break;
	}

	//print_level0(&mt);

	printf("all tests passed\n");
	free(a.mem_ptr);
	return 0;

}
